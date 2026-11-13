#include "Core/YSGameMode.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/GameSession.h"
#include "EngineUtils.h"
#include "Core/YSGameState.h"
#include "Core/YSPlayerState.h"
#include "Ship/YSBoatCrew.h"
#include "Enemies/YSEnemySpawnRule.h"
#include "Enemies/YSFoeCharacter.h"
#include "Enemies/YSFoeGalion.h"
#include "Enemies/YSFoeJollyShip.h"
#include "Interaction/YSTreasureMap.h"
#include "Interaction/YSInteractiveObject.h"
#include "Interaction/YSPlayerCorpse.h"
#include "Core/YSCombat.h"
#include "LandscapeProxy.h"
#include "Map/YSOcean.h"
#include "NPC/YSBrookerNPC.h"
#include "Ship/YSPirateShip.h"
#include "Ship/YSShipStation.h"
#include "Summons/YSHiredCrewComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Player/YSPlayerCharacter.h"
#include "Player/YSPlayerController.h"
#include "Upgrades/YSShardUpgrade.h"
#include "Weapons/YSWeapon.h"

AYSGameMode::AYSGameMode()
{
	GameStateClass = AYSGameState::StaticClass();
	PlayerStateClass = AYSPlayerState::StaticClass();
	PlayerControllerClass = AYSPlayerController::StaticClass();

	PrimaryActorTick.bCanEverTick = true;

	SpawnRules.Add(CreateDefaultSubobject<UYSTimedWaveSpawnRule>(TEXT("TimedWaveSpawnRule")));

	// 1 à 4 joueurs : NME x1 / x1.5 / x1.8 / x2, PV x1 / x1.35 / x1.65 / x2, élites PV x nombre de joueurs
	for (const FVector& Row : { FVector(1.0f, 1.0f, 1.0f), FVector(1.5f, 1.35f, 2.0f), FVector(1.8f, 1.65f, 3.0f), FVector(2.0f, 2.0f, 4.0f) })
	{
		FYSPlayerCountScaling& Scaling = PlayerCountScaling.AddDefaulted_GetRef();
		Scaling.SpawnMultiplier = Row.X;
		Scaling.HealthMultiplier = Row.Y;
		Scaling.EliteHealthMultiplier = Row.Z;
	}

	// overtime de l'île de départ : Skull niveau jour + 3, puis jour + 5 30 s plus tard (NME réglés dans le Blueprint)
	FYSOvertimeWave& FirstWave = OvertimeWaves.AddDefaulted_GetRef();
	FirstWave.LevelOffset = 3;
	FYSOvertimeWave& SecondWave = OvertimeWaves.AddDefaulted_GetRef();
	SecondWave.Delay = 45.0f;
	SecondWave.LevelOffset = 5;

	// Hired crew : BP enfant de Mouss (Tools/EditorScripts/setup_hired_mouss.py)
	HiredMoussClass = TSoftClassPtr<AYSPlayerCharacter>(FSoftObjectPath(TEXT("/Game/Placeable/Character/Player/PlayerCharacter_HiredMouss.PlayerCharacter_HiredMouss_C")));
}

void AYSGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || !YSGameState->IsMatchStarted())
	{
		return;
	}

	// fin du jour : la nuit commence (elle ne se termine qu'à la mort de l'élite)
	if (bDayNightCycle && !YSGameState->IsNight() && !IsGalionPhase() && YSGameState->GetDayElapsedTime() >= DayDuration)
	{
		StartNight();
	}

	for (UYSEnemySpawnRule* Rule : SpawnRules)
	{
		if (Rule)
		{
			Rule->Tick(DeltaSeconds);
		}
	}

	TickOvertime();
	TickShipBoarding();
	TickJollyShips();
	TickGalions();
	TickPhoenixAutoRevive();
	TickAllPlayersDead();
	TickHiredCrew();

	// apparition ratée au lancement : le joueur restait « mort » pour l'hôte
	if (MissingPawnCheckInterval > 0.0f && !YSGameState->IsMatchEnded() && !GetWorld()->IsPaused())
	{
		MissingPawnTimer += DeltaSeconds;
		if (MissingPawnTimer >= MissingPawnCheckInterval)
		{
			MissingPawnTimer = 0.0f;
			RespawnMissingPlayers(TEXT("vérification périodique"), false);
		}
	}
}

void AYSGameMode::ResyncPlayer(APlayerController* Player, const TCHAR* Reason)
{
	if (!Player)
	{
		return;
	}
	APawn* Pawn = Player->GetPawn();
	Player->ForceNetUpdate();
	if (Pawn)
	{
		Pawn->ForceNetUpdate();
	}
	if (APlayerState* PlayerState = Player->GetPlayerState<APlayerState>())
	{
		PlayerState->ForceNetUpdate();
	}
	if (AGameStateBase* State = GetGameState<AGameStateBase>())
	{
		State->ForceNetUpdate();
	}
	GetWorldSettings()->ForceNetUpdate();

	// client qui n'a pas confirmé son personnage actuel : possession renvoyée
	const bool bResendPossession = Pawn && !Player->IsLocalController() && Player->AcknowledgedPawn != Pawn;
	if (bResendPossession)
	{
		Player->ClientRestart(Pawn);
	}
	UE_LOG(LogTemp, Log, TEXT("[YSResync] %s (%s) : état renvoyé, personnage %s%s"), *Player->GetName(), Reason, Pawn ? *Pawn->GetName() : TEXT("aucun"),
		bResendPossession ? TEXT(", possession renvoyée (pas confirmée par le client)") : TEXT(""));
}

void AYSGameMode::PlayerRequestedUpgradeResync(APlayerController* Player)
{
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	AYSPlayerState* PlayerState = Player ? Player->GetPlayerState<AYSPlayerState>() : nullptr;
	const AYSPlayerCharacter* Character = Player ? Cast<AYSPlayerCharacter>(Player->GetPawn()) : nullptr;
	if (!YSGameState || !YSGameState->IsUpgradeSelectionActive() || !PlayerState || !PlayerState->IsChoosingUpgrade())
	{
		return;
	}

	// nouvelles propositions du même type (la réplication les renvoie : le client les affiche s'il a son personnage)
	if (Character && Character->IsAlive())
	{
		EYSUpgradeSelectionKind PlayerKind = CurrentSelectionKind;
		const TArray<FYSUpgradeOffer> Offers = PickOffersForKind(Character, CurrentSelectionKind, PlayerKind);
		if (Offers.Num() > 0)
		{
			const int32 RerollsLeft = PlayerState->GetUpgradeRerollsLeft();
			PlayerState->SetUpgradeOffers(Offers);
			PlayerState->SetUpgradeRerollsLeft(RerollsLeft);
		}
		UE_LOG(LogTemp, Warning, TEXT("[YSResync] %s n'affiche pas ses upgrades : %d nouvelle(s) proposition(s)"), *PlayerState->GetPlayerName(), Offers.Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSResync] %s n'affiche pas ses upgrades et n'a pas de personnage vivant sur le serveur"), *PlayerState->GetPlayerName());
	}
	ResyncPlayer(Player, TEXT("upgrades pas affichées"));
}

void AYSGameMode::RespawnMissingPlayers(const TCHAR* Reason, bool bResyncOthers)
{
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || !YSGameState->IsMatchStarted() || YSGameState->IsMatchEnded())
	{
		return;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Player = It->Get();
		const AYSPlayerState* PlayerState = Player ? Player->GetPlayerState<AYSPlayerState>() : nullptr;
		if (!PlayerState || !PlayerState->IsReady() || PlayerState->IsDead())
		{
			continue;
		}
		if (!HasCharacter(Player))
		{
			UE_LOG(LogTemp, Warning, TEXT("[YSResync] %s : prêt, pas mort, mais sans personnage (%s) : il apparaît"), *PlayerState->GetPlayerName(), Reason);
			RestartPlayer(Player);
			ResyncPlayer(Player, Reason);
		}
		else if (bResyncOthers)
		{
			ResyncPlayer(Player, Reason);
		}
	}
}

void AYSGameMode::SetHiredCrew(bool bEnabled)
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || YSGameState->IsMatchStarted() || YSGameState->IsHiredCrew() == bEnabled)
	{
		return;
	}
	YSGameState->SetHiredCrew(bEnabled);
	UE_LOG(LogTemp, Log, TEXT("[YSHiredCrew] Hired crew : %s"), bEnabled ? TEXT("oui") : TEXT("non"));
}

AYSPlayerCharacter* AYSGameMode::GetHiredCrewLeader() const
{
	// l'hôte (premier contrôleur du serveur) ; mort : un autre joueur vivant
	const APlayerController* Host = GetWorld()->GetFirstPlayerController();
	AYSPlayerCharacter* HostCharacter = Host ? Cast<AYSPlayerCharacter>(Host->GetPawn()) : nullptr;
	if (HostCharacter && HostCharacter->IsAlive() && !HostCharacter->IsDying())
	{
		return HostCharacter;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AYSPlayerCharacter* Character = It->Get() ? Cast<AYSPlayerCharacter>(It->Get()->GetPawn()) : nullptr;
		if (Character && Character->IsAlive() && !Character->IsDying())
		{
			return Character;
		}
	}
	return nullptr;
}

void AYSGameMode::DebugHiredStatus() const
{
	const AYSPlayerCharacter* Leader = GetHiredCrewLeader();
	UE_LOG(LogTemp, Log, TEXT("[YSHiredCrew] %d place(s), chef %s"), HiredCrew.Num(), Leader ? *Leader->GetName() : TEXT("aucun"));
	for (const FYSHiredCrewSlot& Slot : HiredCrew)
	{
		const AYSPlayerCharacter* Mouss = Slot.Mouss;
		UE_LOG(LogTemp, Log, TEXT("[YSHiredCrew]   Hired Mouss %d : %s, PV %.0f / %.0f, stamina %.0f, %d shard(s), porte %s, poste %s, à %.0f cm du chef"), Slot.Number,
			IsValid(Mouss) ? *Mouss->GetName() : TEXT("mort"), IsValid(Mouss) ? Mouss->GetHealth() : 0.0f, IsValid(Mouss) ? Mouss->GetMaxHealth() : 0.0f,
			IsValid(Mouss) ? Mouss->GetStamina() : 0.0f, IsValid(Mouss) ? Mouss->GetShardLevels().Num() : Slot.ShardLevels.Num(),
			IsValid(Mouss) && Mouss->GetCarriedObject() ? *Mouss->GetCarriedObject()->GetName() : TEXT("rien"),
			IsValid(Mouss) && Mouss->GetCurrentStation() ? *Mouss->GetCurrentStation()->GetName() : TEXT("aucun"),
			IsValid(Mouss) && Leader ? FVector::Dist(Mouss->GetActorLocation(), Leader->GetActorLocation()) : -1.0f);
	}
}

void AYSGameMode::DebugKillHired(int32 Number)
{
	for (FYSHiredCrewSlot& Slot : HiredCrew)
	{
		if (IsValid(Slot.Mouss) && (Number <= 0 || Slot.Number == Number))
		{
			UE_LOG(LogTemp, Log, TEXT("[YSHiredCrew] Debug : Hired Mouss %d tué"), Slot.Number);
			Slot.Mouss->TakeDamage(Slot.Mouss->GetHealth() + Slot.Mouss->GetArmor() + 100000.0f, FDamageEvent(), nullptr, nullptr);
		}
	}
}

void AYSGameMode::TickHiredCrew()
{
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || !YSGameState->IsHiredCrew() || YSGameState->IsMatchEnded() || GetWorld()->IsPaused())
	{
		return;
	}

	// places vides jusqu'à Hired Crew Team Size (un joueur qui arrive reprend une place, un joueur qui part en libère une)
	const int32 Wanted = FMath::Max(0, HiredCrewTeamSize - YSGameState->PlayerArray.Num());
	while (HiredCrew.Num() > Wanted)
	{
		FYSHiredCrewSlot& Removed = HiredCrew.Last();
		if (IsValid(Removed.Mouss))
		{
			Removed.Mouss->DropCarriedObject(TEXT("place rendue à un joueur"));
			Removed.Mouss->LeaveStation(TEXT("place rendue à un joueur"), true);
			Removed.Mouss->Destroy();
		}
		UE_LOG(LogTemp, Log, TEXT("[YSHiredCrew] Hired Mouss %d part : %d joueur(s)"), Removed.Number, YSGameState->PlayerArray.Num());
		HiredCrew.Pop();
	}
	while (HiredCrew.Num() < Wanted)
	{
		FYSHiredCrewSlot& Slot = HiredCrew.AddDefaulted_GetRef();
		Slot.Number = NextHiredNumber++;
		// couleur : la première qui n'est ni à un joueur ni à un autre HiredMouss ; visage au hasard
		const int32 ColorCount = YSGameState->GetCustomizationColors().Num();
		Slot.ColorIndex = 0;
		for (int32 Color = 0; Color < ColorCount; ++Color)
		{
			const bool bHiredTaken = HiredCrew.ContainsByPredicate([Color, &Slot](const FYSHiredCrewSlot& Other) { return &Other != &Slot && Other.ColorIndex == Color; });
			if (!IsColorTaken(Color, nullptr) && !bHiredTaken)
			{
				Slot.ColorIndex = Color;
				break;
			}
		}
		const int32 FaceCount = YSGameState->GetCustomizationFaces().Num();
		Slot.FaceIndex = FaceCount > 0 ? FMath::RandRange(0, FaceCount - 1) : 0;
		UE_LOG(LogTemp, Log, TEXT("[YSHiredCrew] Place %d : Hired Mouss %d (couleur %d, visage %d)"), HiredCrew.Num(), Slot.Number, Slot.ColorIndex, Slot.FaceIndex);
	}

	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 Index = 0; Index < HiredCrew.Num(); ++Index)
	{
		FYSHiredCrewSlot& Slot = HiredCrew[Index];
		if (IsValid(Slot.Mouss))
		{
			continue;
		}
		Slot.Mouss = nullptr;
		// mort : corps disparu depuis Hired Revive Delay (le premier passage note l'heure)
		if (Slot.bSpawned)
		{
			if (Slot.DeathTime < 0.0)
			{
				Slot.DeathTime = Now;
			}
			if (Now - Slot.DeathTime < HiredReviveDelay)
			{
				continue;
			}
		}
		SpawnHiredMouss(Slot, Index);
	}
}

bool AYSGameMode::SpawnHiredMouss(FYSHiredCrewSlot& Slot, int32 SlotIndex)
{
	UClass* MoussClass = HiredMoussClass.LoadSynchronous();
	if (!MoussClass)
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG(LogTemp, Error, TEXT("[YSHiredCrew] Pas de Hired Mouss Class (%s) : lancer Tools/EditorScripts/setup_hired_mouss.py"), *HiredMoussClass.ToString());
		}
		return false;
	}

	// point d'apparition : réanimation = à côté du core du bateau (sinon du Brooker de l'île) ; début de partie = chaloupe de l'intro, sinon à côté du chef
	FTransform SpawnTransform = FTransform::Identity;
	AYSShipStation* IntroSeat = nullptr;
	AYSPlayerCharacter* Leader = GetHiredCrewLeader();
	if (Slot.bSpawned)
	{
		FVector Ground;
		if (!FindPhoenixRevivePoint(Leader ? static_cast<const AActor*>(Leader) : this, SlotIndex + 1, Ground))
		{
			return false;
		}
		SpawnTransform.SetLocation(Ground);
	}
	else if ((IntroSeat = FindHiredIntroSeat(SpawnTransform)) == nullptr)
	{
		if (!Leader)
		{
			return false;
		}
		const FVector Side = FVector::ForwardVector.RotateAngleAxis(90.0f + SlotIndex * 120.0f, FVector::UpVector);
		const FVector Ground = AYSInteractiveObject::FindGround(GetWorld(), Leader->GetActorLocation() + Side * HiredSpawnDistance, Leader);
		SpawnTransform = FTransform(FRotator(0.0f, Leader->GetActorRotation().Yaw, 0.0f), Ground);
	}
	const float HalfHeight = MoussClass->GetDefaultObject<AYSPlayerCharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	SpawnTransform.AddToTranslation(FVector(0.0f, 0.0f, HalfHeight + 2.0f));
	SpawnTransform.SetScale3D(FVector::OneVector);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.bDeferConstruction = true;
	AYSPlayerCharacter* Mouss = GetWorld()->SpawnActor<AYSPlayerCharacter>(MoussClass, SpawnTransform, Params);
	if (!Mouss)
	{
		return false;
	}
	Mouss->FinishSpawning(SpawnTransform);
	UYSHiredCrewComponent* Crew = Mouss->GetHiredCrew();
	if (!Crew)
	{
		UE_LOG(LogTemp, Error, TEXT("[YSHiredCrew] %s n'a pas de composant YSHiredCrewComponent : détruit"), *MoussClass->GetName());
		Mouss->Destroy();
		return false;
	}
	Crew->InitHired(Slot.Number, Slot.ColorIndex, Slot.FaceIndex);
	if (!Mouss->GetController())
	{
		Mouss->SpawnDefaultController();
	}

	const bool bRevive = Slot.bSpawned;
	if (bRevive)
	{
		Mouss->RestoreUpgrades(Slot.UpgradePool, Slot.UpgradeProgress);
		Mouss->RestoreShards(Slot.ShardLevels);
		Mouss->SetHealthRatio(HiredReviveHealthRatio);
	}
	if (IntroSeat)
	{
		Mouss->EnterStation(IntroSeat);
	}
	Slot.Mouss = Mouss;
	Slot.bSpawned = true;
	Slot.DeathTime = -1.0;
	UE_LOG(LogTemp, Log, TEXT("[YSHiredCrew] Hired Mouss %d %s en %s (%s)"), Slot.Number, bRevive ? TEXT("réanimé (sans plume)") : TEXT("apparaît"),
		*SpawnTransform.GetLocation().ToCompactString(), IntroSeat ? TEXT("chaloupe de l'intro") : bRevive ? TEXT("bateau / Brooker") : TEXT("à côté du chef"));

	// level up passés pendant sa mort
	for (; Slot.MissedLevelUps > 0; --Slot.MissedLevelUps)
	{
		ApplyRandomHiredUpgrade(Mouss);
	}
	return true;
}

void AYSGameMode::OnHiredMoussDied(AYSPlayerCharacter* Mouss)
{
	for (FYSHiredCrewSlot& Slot : HiredCrew)
	{
		if (Slot.Mouss == Mouss)
		{
			Slot.UpgradePool = Mouss->GetUpgradePool();
			Slot.UpgradeProgress = Mouss->GetAllUpgradeProgress();
			Slot.ShardLevels = Mouss->GetShardLevels();
			UE_LOG(LogTemp, Log, TEXT("[YSHiredCrew] Hired Mouss %d meurt : %d shard(s), %d upgrade(s) gardées, réanimé au bateau"), Slot.Number, Slot.ShardLevels.Num(), Slot.UpgradeProgress.Num());
			return;
		}
	}
}

void AYSGameMode::GiveHiredCrewLevelUp()
{
	for (FYSHiredCrewSlot& Slot : HiredCrew)
	{
		if (IsValid(Slot.Mouss) && Slot.Mouss->IsAlive() && !Slot.Mouss->IsDying())
		{
			ApplyRandomHiredUpgrade(Slot.Mouss);
		}
		else if (Slot.bSpawned)
		{
			++Slot.MissedLevelUps;
		}
	}
}

void AYSGameMode::ApplyRandomHiredUpgrade(AYSPlayerCharacter* Mouss)
{
	// même tirage qu'un joueur pour un level up, puis une des propositions au hasard, prise quoi qu'il arrive
	EYSUpgradeSelectionKind Kind = EYSUpgradeSelectionKind::LevelUp;
	const TArray<FYSUpgradeOffer> Offers = PickOffersForKind(Mouss, EYSUpgradeSelectionKind::LevelUp, Kind);
	if (Offers.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSHiredCrew] %s : aucune upgrade à prendre"), *Mouss->GetName());
		return;
	}
	const int32 Picked = FMath::RandRange(0, Offers.Num() - 1);
	Mouss->ApplyUpgrade(Offers[Picked]);
	UE_LOG(LogTemp, Log, TEXT("[YSHiredCrew] %s prend l'upgrade %d / %d (%s, rareté %d)"), *Mouss->GetName(), Picked + 1, Offers.Num(),
		Offers[Picked].UpStats ? *Offers[Picked].UpStats->GetName() : *FString::Printf(TEXT("pool %d"), Offers[Picked].PoolIndex), static_cast<int32>(Offers[Picked].Rarity));
}

void AYSGameMode::TickAllPlayersDead()
{
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	// encore une plume (ou la réanimation gratuite du Medic de l'équipage) : TickPhoenixAutoRevive s'en charge
	if (!YSGameState || YSGameState->IsMatchEnded() || YSGameState->GetUsablePhoenixFeathers() > 0 || AYSBoatCrew::FindFreeRevive(GetWorld()) || GetWorld()->IsPaused())
	{
		return;
	}
	// tous morts, corps au sol (la dernière mort a fini son ragdoll : plus de personnage contrôlé), comme la réanimation automatique
	int32 Dead = 0;
	for (const APlayerState* Player : YSGameState->PlayerArray)
	{
		const AYSPlayerState* YSPlayer = Cast<AYSPlayerState>(Player);
		if (!YSPlayer || Cast<AYSPlayerCharacter>(Player->GetPawn()) || !YSPlayer->IsDead())
		{
			return;
		}
		++Dead;
	}
	if (Dead > 0)
	{
		GameOver(*FString::Printf(TEXT("les %d joueur(s) sont morts %s"), Dead, YSGameState->IsHardcore() ? TEXT("(Hardcore : plumes de phoenix inutilisables)") : TEXT("sans plume de phoenix")));
	}
}

bool AYSGameMode::IsGalionPhase() const
{
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	return bGalionWaves && GalionClass && YSGameState && YSGameState->IsShipPresent();
}

void AYSGameMode::TickGalions()
{
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!IsGalionPhase())
	{
		return;
	}
	const float Now = YSGameState->GetMatchElapsedTime();
	if (NextGalionTime < 0.0f)
	{
		NextGalionTime = Now + GalionFirstDelay;
		return;
	}
	if (Now < NextGalionTime)
	{
		return;
	}
	if (!SpawnGalion(TEXT("vague")))
	{
		NextGalionTime = Now + GalionRetryDelay;
		return;
	}
	NextGalionTime = Now + GalionWaveInterval;
	if (bGalionAdvancesDay)
	{
		AdvanceDay();
		UE_LOG(LogTemp, Log, TEXT("[YSGalion] Nouveau Galion : jour %d"), YSGameState->GetDayNumber());
	}
}

void AYSGameMode::TickJollyShips()
{
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!bJollyShipWaves || !JollyShipClass || !YSGameState || !YSGameState->IsShipPresent())
	{
		return;
	}
	const float Now = YSGameState->GetMatchElapsedTime();
	if (NextJollyShipWaveTime >= 0.0f && Now < NextJollyShipWaveTime)
	{
		return;
	}
	NextJollyShipWaveTime = Now + JollyShipWaveInterval;

	int32 Count = ScaleEnemyCount(JollyShipsPerWave);
	if (JollyShipMaxAlive > 0)
	{
		int32 Alive = 0;
		for (TActorIterator<AYSFoeJollyShip> It(GetWorld()); It; ++It)
		{
			++Alive;
		}
		Count = FMath::Min(Count, JollyShipMaxAlive - Alive);
	}
	int32 Spawned = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Spawned += SpawnJollyShip(TEXT("vague")) ? 1 : 0;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSJollyShip] Vague : %d / %d"), Spawned, Count);
}

UClass* AYSGameMode::GetGalionClass() const
{
	return GalionClass.Get();
}

UClass* AYSGameMode::GetJollyShipClass() const
{
	return JollyShipClass.Get();
}

AYSFoeJollyShip* AYSGameMode::SpawnJollyShip(const TCHAR* Reason)
{
	AYSFoeJollyShip* Ship = AYSFoeJollyShip::SpawnAtSea(GetWorld(), JollyShipClass, JollyShipMinSpawnDistance, JollyShipMaxSpawnDistance, JollyShipSpawnStep, JollyShipSpawnAttempts);
	if (!Ship)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSJollyShip] Apparition (%s) : pas de classe ou pas d'emplacement"), Reason);
	}
	return Ship;
}

AYSFoeGalion* AYSGameMode::SpawnGalion(const TCHAR* Reason)
{
	AYSFoeGalion* Ship = AYSFoeGalion::SpawnAtSea(GetWorld(), GalionClass, GalionMinSpawnDistance, GalionMaxSpawnDistance);
	UE_LOG(LogTemp, Log, TEXT("[YSGalion] Apparition (%s) : %s"), Reason, Ship ? *Ship->GetName() : TEXT("pas de classe ou pas d'emplacement"));
	return Ship;
}

void AYSGameMode::OnDayStarted(int32 Day)
{
	// l'overtime commence à la tombée de la nuit d'Overtime Day (StartNight) : le jour lui-même est normal
}

void AYSGameMode::StartOvertime()
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || !YSGameState->IsMatchStarted() || OvertimeStartTime >= 0.0f)
	{
		return;
	}
	OvertimeStartTime = YSGameState->GetMatchElapsedTime();
	LastOvertimeWaveTime = OvertimeStartTime;
	NextOvertimeWave = 0;
	YSGameState->SetOvertime();
	UE_LOG(LogTemp, Log, TEXT("[YSOvertime] Nuit %d : l'île passe en overtime (pas d'élite, %d circle spawn(s), l'île coulera %.0f s après le dernier)"),
		YSGameState->GetDayNumber(), OvertimeWaves.Num(), IslandSinkDelayAfterLastWave);
}

void AYSGameMode::TickOvertime()
{
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (OvertimeStartTime < 0.0f || !YSGameState)
	{
		return;
	}

	// toutes les vagues parties : l'île coule Island Sink Delay After Last Wave secondes après la dernière
	if (!OvertimeWaves.IsValidIndex(NextOvertimeWave))
	{
		if (!YSGameState->IsIslandSinking() && YSGameState->GetMatchElapsedTime() - LastOvertimeWaveTime >= IslandSinkDelayAfterLastWave)
		{
			StartIslandSink();
		}
		return;
	}
	const FYSOvertimeWave& Wave = OvertimeWaves[NextOvertimeWave];
	if (YSGameState->GetMatchElapsedTime() - OvertimeStartTime < Wave.Delay)
	{
		return;
	}

	// circle spawn sur la plage, niveau = jour en cours + décalage (nombre selon les joueurs de la partie, comme les vagues)
	const int32 Level = YSGameState->GetFoeDayNumber() + Wave.LevelOffset;
	SpawnCircle(Wave.EnemyClasses, ScaleEnemyCount(Wave.EnemiesPerPlayer), FString::Printf(TEXT("overtime %d, niveau %d"), NextOvertimeWave + 1, Level), Level);
	++NextOvertimeWave;
	// sans vague (liste vide) : l'île coule le même délai après le début de l'overtime
	LastOvertimeWaveTime = YSGameState->GetMatchElapsedTime();
}

void AYSGameMode::StartIslandSink()
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || YSGameState->IsIslandSinking())
	{
		return;
	}
	YSGameState->StartIslandSink(IslandSinkDuration, IslandSinkDepth, IslandSinkEaseExponent);
	UE_LOG(LogTemp, Log, TEXT("[YSOvertime] Nuit %d : l'île coule (%.0f cm en %.0f s)"), YSGameState->GetDayNumber(), IslandSinkDepth, IslandSinkDuration);
}

void AYSGameMode::ShipArrives(const TCHAR* Reason)
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (YSGameState && !YSGameState->IsShipPresent())
	{
		YSGameState->SetShipPresent(true);
		// plus de nuit avec le bateau : celle en cours se termine (même jour), son élite reste un NME normal ; un overtime commencé continue
		if (IsGalionPhase() && YSGameState->IsNight() && OvertimeStartTime < 0.0f)
		{
			YSGameState->StartDay(YSGameState->GetDayNumber());
			for (UYSEnemySpawnRule* Rule : SpawnRules)
			{
				if (Rule)
				{
					Rule->NotifyPhaseChanged(false);
				}
			}
			UE_LOG(LogTemp, Log, TEXT("[YSGalion] Le bateau arrive pendant la nuit : elle se termine"));
		}
		// arrivé sans l'objectif (debug) : niveau 1, seuil du niveau suivant
		if (YSGameState->GetShipLevel() == 0)
		{
			YSGameState->SetShipLevel(1);
			YSGameState->SetDoubloons(0, GetDoubloonGoalForShipLevel(1));
		}
		UE_LOG(LogTemp, Log, TEXT("[YSShip] Le bateau arrive (%s)"), Reason);
	}
}

void AYSGameMode::TickShipBoarding()
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!bShipBoardingVictory || !YSGameState || !YSGameState->IsShipPresent() || YSGameState->IsVictory())
	{
		return;
	}

	// à bord = debout sur le bateau (base de mouvement), hors planche d'embarquement ; joueurs comptés = vivants
	int32 Total = 0;
	int32 Aboard = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AYSPlayerCharacter* Character = It->Get() ? Cast<AYSPlayerCharacter>(It->Get()->GetPawn()) : nullptr;
		if (!Character || !Character->IsAlive())
		{
			continue;
		}
		++Total;
		const UPrimitiveComponent* Base = Cast<UPrimitiveComponent>(Character->GetCharacterMovement()->GetMovementBaseObject());
		const AYSPirateShip* Ship = Base ? AYSPirateShip::GetShipOf(Base->GetOwner()) : nullptr;
		const bool bOnShip = Ship && Ship->IsObjectiveShip() && !Base->GetName().Contains(TEXT("Gangplank"));
		Aboard += bOnShip ? 1 : 0;
	}

	if (Aboard != YSGameState->GetShipAboardPlayers() || Total != YSGameState->GetShipTotalPlayers())
	{
		YSGameState->SetShipBoarding(Aboard, Total);
		UE_LOG(LogTemp, Log, TEXT("[YSShip] %d / %d joueur(s) sur le bateau"), Aboard, Total);
	}
	if (Total > 0 && Aboard == Total)
	{
		YSGameState->SetVictory(VictoryReturnDelay);
		UE_LOG(LogTemp, Log, TEXT("[YSShip] Tous les joueurs sont sur le bateau : BRAVO, retour au menu dans %.0f s"), VictoryReturnDelay);
	}
}

void AYSGameMode::InitGameState()
{
	Super::InitGameState();

	if (AYSGameState* FeatherState = GetGameState<AYSGameState>())
	{
		FeatherState->SetPhoenixFeathers(StartingPhoenixFeathers);
	}

	// personnalisation : les Data Tables (ordre des lignes) remplacent les listes
	if (CustomizationColorTable)
	{
		CustomizationColors.Reset();
		CustomizationColorTable->ForeachRow<FYSCustomizationColor>(TEXT("YSCustomizationColors"), [this](const FName&, const FYSCustomizationColor& Row)
		{
			CustomizationColors.Add(Row);
		});
	}
	if (CustomizationFaceTable)
	{
		CustomizationFaces.Reset();
		CustomizationFaceTable->ForeachRow<FYSCustomizationFace>(TEXT("YSCustomizationFaces"), [this](const FName&, const FYSCustomizationFace& Row)
		{
			if (Row.Face)
			{
				CustomizationFaces.Add(Row.Face);
			}
		});
	}
	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Personnalisation : %d couleur(s) (%s), %d visage(s) (%s)"), CustomizationColors.Num(),
		CustomizationColorTable ? *CustomizationColorTable->GetName() : TEXT("liste"), CustomizationFaces.Num(), CustomizationFaceTable ? *CustomizationFaceTable->GetName() : TEXT("liste"));

#if !WITH_EDITOR
	// build packagée : personnages de test retirés (les index de sélection suivent la liste filtrée, sur toutes les machines)
	const int32 CharacterCount = SelectableCharacters.Num();
	SelectableCharacters.RemoveAll([](const TSubclassOf<AYSPlayerCharacter>& CharacterClass)
	{
		return CharacterClass && CharacterClass->GetDefaultObject<AYSPlayerCharacter>()->IsEditorOnlyCharacter();
	});
	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Personnages de test retirés de la sélection : %d"), CharacterCount - SelectableCharacters.Num());
#endif

	// la liste est répliquée par le GameState pour que les clients puissent construire leurs boutons
	if (AYSGameState* YSGameState = GetGameState<AYSGameState>())
	{
		YSGameState->SetSelectableCharacters(SelectableCharacters);
		YSGameState->SetCustomizationOptions(CustomizationColors, CustomizationFaces);
		YSGameState->SetTeamProgress(0, 0.0f, GetExperienceForLevel(0));
		YSGameState->SetDoubloons(0, DoubloonGoal);
		// bateau : là dès le début, ou seulement à l'objectif de doublons (GameMap)
		YSGameState->SetShipPresent(!bShipArrivesAtDoubloonGoal);
		// bateau là dès le début : niveau 1
		if (!bShipArrivesAtDoubloonGoal)
		{
			YSGameState->SetShipLevel(1);
			YSGameState->SetDoubloons(0, GetDoubloonGoalForShipLevel(1));
		}
	}
}

void AYSGameMode::AddTeamExperience(float Amount)
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || !YSGameState->IsMatchStarted() || Amount <= 0.0f)
	{
		return;
	}

	// XP commune : tout le monde progresse ensemble
	int32 Level = YSGameState->GetTeamLevel();
	float Experience = YSGameState->GetTeamExperience() + Amount;
	while (Experience >= GetExperienceForLevel(Level))
	{
		Experience -= GetExperienceForLevel(Level);
		++Level;
		++PendingLevelUps;
	}
	YSGameState->SetTeamProgress(Level, Experience, GetExperienceForLevel(Level));

	if (PendingLevelUps > 0 && !YSGameState->IsUpgradeSelectionActive())
	{
		StartUpgradeSelection();
	}
}

void AYSGameMode::StartUpgradeSelection()
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	// partie finie : plus de choix (l'écran des scores est affiché)
	if (!YSGameState || YSGameState->IsMatchEnded() || (PendingLevelUps <= 0 && PendingDoubloonRewards.Num() == 0))
	{
		return;
	}

	// récompenses de la jauge de doublons d'abord (niveau du bateau), puis les level up
	EYSUpgradeSelectionKind Kind = EYSUpgradeSelectionKind::LevelUp;
	int32 SelectionLevel = 0;
	const bool bDoubloonSelection = PendingDoubloonRewards.Num() > 0;
	if (bDoubloonSelection)
	{
		Kind = PendingDoubloonRewards[0];
		PendingDoubloonRewards.RemoveAt(0);
		SelectionLevel = YSGameState->GetShipLevel();
	}
	else
	{
		--PendingLevelUps;
		// niveau atteint par cette sélection (plusieurs niveaux gagnés d'un coup : sélections enchaînées)
		SelectionLevel = YSGameState->GetTeamLevel() - PendingLevelUps;
		// ancien fonctionnement : choix de classe au niveau de classe
		if (!bDoubloonRewards && SelectionLevel == ClassUpgradeLevel)
		{
			Kind = EYSUpgradeSelectionKind::Class;
		}
	}
	CurrentSelectionKind = Kind;
	YSGameState->SetUpgradeSelectionLevel(SelectionLevel);
	// Hired crew : une upgrade au hasard à chaque level up (pas aux récompenses de doublons), prise tout de suite
	if (!bDoubloonSelection)
	{
		GiveHiredCrewLevelUp();
	}
	// level up (même un choix de classe de l'ancien fonctionnement : le menu reconnaît les upgrades de classe) ou récompense de doublons
	YSGameState->SetUpgradeSelectionKind(bDoubloonSelection ? Kind : EYSUpgradeSelectionKind::LevelUp);
	YSGameState->SetUpgradeSelectionActive(true);
	const bool bClassSelection = Kind == EYSUpgradeSelectionKind::Class;

	// propositions propres à chaque joueur vivant (selon son personnage) ; les autres ne sont pas attendus
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Player = It->Get();
		AYSPlayerState* PlayerState = Player ? Player->GetPlayerState<AYSPlayerState>() : nullptr;
		AYSPlayerCharacter* Character = Player ? Cast<AYSPlayerCharacter>(Player->GetPawn()) : nullptr;
		if (Character && bRestoreOnLevelUp && !bDoubloonSelection)
		{
			Character->RestoreOnLevelUp();
		}
		if (PlayerState)
		{
			// type de la sélection (classe, upgrade d'arme...), avec repli si le personnage n'a rien de ce type à proposer
			TArray<FYSUpgradeOffer> Offers;
			EYSUpgradeSelectionKind PlayerKind = Kind;
			if (Character && Character->IsAlive())
			{
				Offers = PickOffersForKind(Character, Kind, PlayerKind);
			}
			PlayerState->SetUpgradeOffers(Offers);
			// SilverCoin upgrade (récompense de doublons) : jamais de relance
			PlayerState->SetUpgradeRerollsLeft(bDoubloonSelection ? 0 : MaxUpgradeRerolls);
			// pendant la pause rien n'est répliqué sans ForceNetUpdate : personnage (nouveau après un changement de classe), contrôleur
			Player->ForceNetUpdate();
			if (APawn* Pawn = Player->GetPawn())
			{
				Pawn->ForceNetUpdate();
			}
			UE_LOG(LogTemp, Log, TEXT("[YSGameMode] %s : sélection %s%s"), *PlayerState->GetPlayerName(), *UEnum::GetValueAsString(Kind),
				PlayerKind != Kind ? *FString::Printf(TEXT(" -> %s (rien à proposer)"), *UEnum::GetValueAsString(PlayerKind)) : TEXT(""));
			UE_LOG(LogTemp, Log, TEXT("[YSGameMode] %s (%s, pool de %d, armes G %s / D %s) : %d proposition(s)"), *PlayerState->GetPlayerName(),
				Character ? *Character->GetClass()->GetName() : TEXT("pas de perso"), Character ? Character->GetUpgradePool().Num() : 0,
				Character && Character->GetLeftHandWeapon() ? *Character->GetLeftHandWeapon()->GetName() : TEXT("aucune"),
				Character && Character->GetRightHandWeapon() ? *Character->GetRightHandWeapon()->GetName() : TEXT("aucune"),
				PlayerState->GetUpgradeOffers().Num());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] %s %d : choix %s (%d niveau(x) et %d récompense(s) de doublons en attente)"), bDoubloonSelection ? TEXT("Bateau niveau") : TEXT("Niveau"), SelectionLevel,
		bClassSelection ? TEXT("de classe") : *UEnum::GetValueAsString(Kind), PendingLevelUps, PendingDoubloonRewards.Num());

	// tout le monde s'arrête pendant le choix (pause répliquée aux clients)
	if (APlayerController* FirstPlayer = GetWorld()->GetFirstPlayerController())
	{
		SetPause(FirstPlayer);
		// la pause vit sur le WorldSettings (Pauser répliqué) : envoi immédiat, le temps du serveur étant figé
		GetWorldSettings()->ForceNetUpdate();
		// le chrono global continue pendant le choix (pas les timers de phase)
		YSGameState->SetClockPaused(true);
	}

	TryFinishUpgradeSelection();
}

TArray<FYSUpgradeOffer> AYSGameMode::PickRandomUpgradeOffers(const AYSPlayerCharacter* Character, int32 Count) const
{
	if (!Character)
	{
		return TArray<FYSUpgradeOffer>();
	}
	if (!bShardUpgrades)
	{
		return Character->PickUpgradeOffers(Count);
	}
	TArray<UYSUpStats*> Replacements;
	for (const TObjectPtr<UYSUpStats>& Replacement : ReplacementUpStats)
	{
		Replacements.Add(Replacement);
	}
	// SilverCoin upgrades : les level up ne proposent plus que le Bonus (++) des upgrades d'arme et de MoveAction déjà débloquées
	YSShardUpgrades::FPickOptions Options;
	if (bDoubloonRewards)
	{
		Options.WeaponTierMask = 0b010;
		Options.MoveActionTierMask = 0b010;
	}
	return YSShardUpgrades::PickOffers(Character, CharaUpgradesTable, Replacements, Count, Options);
}

TArray<FYSUpgradeOffer> AYSGameMode::PickOffersForKind(const AYSPlayerCharacter* Character, EYSUpgradeSelectionKind Kind, EYSUpgradeSelectionKind& OutKind) const
{
	OutKind = Kind;
	if (!Character)
	{
		return TArray<FYSUpgradeOffer>();
	}
	TArray<UYSUpStats*> Replacements;
	for (const TObjectPtr<UYSUpStats>& Replacement : ReplacementUpStats)
	{
		Replacements.Add(Replacement);
	}
	// SilverCoin upgrade (tout sauf le level up) : Silver Coin Choice Count propositions
	const int32 SilverCoinCount = FMath::Max(1, SilverCoinChoiceCount);
	TArray<FYSUpgradeOffer> Offers;
	switch (Kind)
	{
	case EYSUpgradeSelectionKind::Class:
		Offers = Character->PickClassOffers();
		if (Offers.Num() == 0)
		{
			// ancien fonctionnement (niveau de classe) : upgrades au hasard ; SilverCoin upgrade : mythique
			return PickOffersForKind(Character, bDoubloonRewards ? EYSUpgradeSelectionKind::Mythic : EYSUpgradeSelectionKind::LevelUp, OutKind);
		}
		// SilverCoin upgrade : Silver Coin Choice Count classes au hasard parmi celles du personnage
		if (bDoubloonRewards)
		{
			while (Offers.Num() > SilverCoinCount)
			{
				Offers.RemoveAt(FMath::RandRange(0, Offers.Num() - 1));
			}
		}
		return Offers;

	case EYSUpgradeSelectionKind::Weapon:
	case EYSUpgradeSelectionKind::GoldenWeapon:
	case EYSUpgradeSelectionKind::MoveAction:
	case EYSUpgradeSelectionKind::GoldenMoveAction:
	{
		const bool bMoveAction = Kind == EYSUpgradeSelectionKind::MoveAction || Kind == EYSUpgradeSelectionKind::GoldenMoveAction;
		const bool bGolden = Kind == EYSUpgradeSelectionKind::GoldenWeapon || Kind == EYSUpgradeSelectionKind::GoldenMoveAction;
		YSShardUpgrades::FPickOptions Options;
		Options.bStats = false;
		const uint8 TierMask = bGolden ? 0b100 : 0b001;
		Options.WeaponTierMask = bMoveAction ? 0 : TierMask;
		Options.MoveActionTierMask = bMoveAction ? TierMask : 0;
		if (bGolden)
		{
			Options.ForcedRarity = EYSUpgradeRarity::Gold;
		}
		Offers = bShardUpgrades ? YSShardUpgrades::PickOffers(Character, CharaUpgradesTable, Replacements, SilverCoinCount, Options) : TArray<FYSUpgradeOffer>();
		if (Offers.Num() == 0)
		{
			// rien à débloquer / à passer en Golden : Golden -> Déblocage -> mythique
			const EYSUpgradeSelectionKind Next = Kind == EYSUpgradeSelectionKind::GoldenWeapon ? EYSUpgradeSelectionKind::Weapon
				: Kind == EYSUpgradeSelectionKind::GoldenMoveAction ? EYSUpgradeSelectionKind::MoveAction
				: EYSUpgradeSelectionKind::Mythic;
			return PickOffersForKind(Character, Next, OutKind);
		}
		return Offers;
	}

	case EYSUpgradeSelectionKind::Mythic:
	{
		YSShardUpgrades::FPickOptions Options;
		Options.WeaponTierMask = 0b010;
		Options.MoveActionTierMask = 0b010;
		Options.ForcedRarity = EYSUpgradeRarity::Mythic;
		Offers = bShardUpgrades ? YSShardUpgrades::PickOffers(Character, CharaUpgradesTable, Replacements, SilverCoinCount, Options) : TArray<FYSUpgradeOffer>();
		if (Offers.Num() == 0)
		{
			return PickOffersForKind(Character, EYSUpgradeSelectionKind::LevelUp, OutKind);
		}
		return Offers;
	}

	default:
		return PickRandomUpgradeOffers(Character, UpgradeChoiceCount);
	}
}

EYSUpgradeSelectionKind AYSGameMode::GetDoubloonReward(int32 FillIndex) const
{
	return DoubloonRewardCycle.IsValidIndex(FillIndex) ? DoubloonRewardCycle[FillIndex] : DoubloonRewardAfterCycle;
}

void AYSGameMode::QueueDoubloonReward(EYSUpgradeSelectionKind Kind, const TCHAR* Reason)
{
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || !YSGameState->IsMatchStarted())
	{
		return;
	}
	PendingDoubloonRewards.Add(Kind);
	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Récompense de doublons %s (%s)"), *UEnum::GetValueAsString(Kind), Reason);
	if (!YSGameState->IsUpgradeSelectionActive())
	{
		StartUpgradeSelection();
	}
}

UYSUpShard* AYSGameMode::FindShard(const AYSPlayerCharacter* Character, uint8 Stat) const
{
	TArray<UYSUpStats*> Replacements;
	for (const TObjectPtr<UYSUpStats>& Replacement : ReplacementUpStats)
	{
		Replacements.Add(Replacement);
	}
	for (UYSUpShard* Shard : YSShardUpgrades::GetCandidateShards(Character, CharaUpgradesTable, Replacements))
	{
		if (static_cast<uint8>(Shard->Stat) == Stat)
		{
			return Shard;
		}
	}
	return nullptr;
}

UYSUpShard* AYSGameMode::FindShardByName(const AYSPlayerCharacter* Character, const FString& Name) const
{
	TArray<UYSUpStats*> Replacements;
	for (const TObjectPtr<UYSUpStats>& Replacement : ReplacementUpStats)
	{
		Replacements.Add(Replacement);
	}
	for (UYSUpShard* Shard : YSShardUpgrades::GetCandidateShards(Character, CharaUpgradesTable, Replacements))
	{
		if (Shard->GetName().Equals(Name, ESearchCase::IgnoreCase) || Shard->GetName().Equals(TEXT("DA_Up_Shard_") + Name, ESearchCase::IgnoreCase))
		{
			return Shard;
		}
	}
	return nullptr;
}

void AYSGameMode::PlayerChoseUpgrade(APlayerController* Player, int32 OfferIndex)
{
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	AYSPlayerState* PlayerState = Player ? Player->GetPlayerState<AYSPlayerState>() : nullptr;
	AYSPlayerCharacter* Character = Player ? Cast<AYSPlayerCharacter>(Player->GetPawn()) : nullptr;
	if (!YSGameState || !YSGameState->IsUpgradeSelectionActive() || !PlayerState || !Character
		|| !PlayerState->GetUpgradeOffers().IsValidIndex(OfferIndex) || PlayerState->GetChosenUpgradeOffer() == OfferIndex)
	{
		return;
	}

	// choix modifiable tant que tous les joueurs n'ont pas choisi : appliqué à la fin de la sélection (TryFinishUpgradeSelection)
	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] %s choisit l'upgrade %d (pool %d, variante %d, rareté %d)%s"), *PlayerState->GetPlayerName(), OfferIndex, PlayerState->GetUpgradeOffers()[OfferIndex].PoolIndex, PlayerState->GetUpgradeOffers()[OfferIndex].Variant, (int32)PlayerState->GetUpgradeOffers()[OfferIndex].Rarity,
		PlayerState->GetChosenUpgradeOffer() != INDEX_NONE ? *FString::Printf(TEXT(", à la place de %d"), PlayerState->GetChosenUpgradeOffer()) : TEXT(""));
	PlayerState->SetChosenUpgradeOffer(OfferIndex);
	TryFinishUpgradeSelection();
}

void AYSGameMode::PlayerRerolledUpgrades(APlayerController* Player)
{
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	AYSPlayerState* PlayerState = Player ? Player->GetPlayerState<AYSPlayerState>() : nullptr;
	const AYSPlayerCharacter* Character = Player ? Cast<AYSPlayerCharacter>(Player->GetPawn()) : nullptr;
	if (!bAllowUpgradeReroll || !YSGameState || !YSGameState->IsUpgradeSelectionActive() || !PlayerState || !Character || !Character->IsAlive()
		|| !PlayerState->HasUpgradeOffers() || PlayerState->GetUpgradeRerollsLeft() == 0)
	{
		return;
	}

	// jamais pour un choix de classe
	const FYSUpgradeOffer& FirstOffer = PlayerState->GetUpgradeOffers()[0];
	const UYSUpgrade* FirstUpgrade = Character->GetUpgradePool().IsValidIndex(FirstOffer.PoolIndex) ? Character->GetUpgradePool()[FirstOffer.PoolIndex].Get() : nullptr;
	if (!FirstOffer.UpStats && (!FirstUpgrade || FirstUpgrade->GetUpgradeType() == EYSUpgradeType::Class))
	{
		return;
	}

	// même type de sélection (récompense de doublons comprise)
	EYSUpgradeSelectionKind PlayerKind = CurrentSelectionKind;
	const TArray<FYSUpgradeOffer> Offers = PickOffersForKind(Character, CurrentSelectionKind == EYSUpgradeSelectionKind::Class ? EYSUpgradeSelectionKind::LevelUp : CurrentSelectionKind, PlayerKind);
	if (Offers.Num() > 0)
	{
		const int32 RerollsLeft = PlayerState->GetUpgradeRerollsLeft();
		PlayerState->SetUpgradeOffers(Offers);
		PlayerState->SetUpgradeRerollsLeft(RerollsLeft > 0 ? RerollsLeft - 1 : RerollsLeft);
		UE_LOG(LogTemp, Log, TEXT("[YSGameMode] %s relance ses upgrades : %d proposition(s), %d relance(s) restante(s)"), *PlayerState->GetPlayerName(), Offers.Num(), PlayerState->GetUpgradeRerollsLeft());
	}
}

void AYSGameMode::TryFinishUpgradeSelection()
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || !YSGameState->IsUpgradeSelectionActive())
	{
		return;
	}

	for (const APlayerState* Player : YSGameState->PlayerArray)
	{
		const AYSPlayerState* YSPlayer = Cast<AYSPlayerState>(Player);
		if (YSPlayer && YSPlayer->IsChoosingUpgrade())
		{
			return;
		}
	}

	// tout le monde a choisi : choix appliqués, puis on enchaîne s'il reste des niveaux gagnés, sinon la partie reprend
	YSGameState->SetUpgradeSelectionActive(false);
	for (APlayerState* Player : YSGameState->PlayerArray)
	{
		if (AYSPlayerState* YSPlayer = Cast<AYSPlayerState>(Player))
		{
			AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(YSPlayer->GetPawn());
			if (Character && Character->IsAlive() && YSPlayer->GetUpgradeOffers().IsValidIndex(YSPlayer->GetChosenUpgradeOffer()))
			{
				Character->ApplyUpgrade(YSPlayer->GetUpgradeOffers()[YSPlayer->GetChosenUpgradeOffer()]);
				UE_LOG(LogTemp, Log, TEXT("[YSGameMode] %s : upgrade %d appliquée"), *YSPlayer->GetPlayerName(), YSPlayer->GetChosenUpgradeOffer());
			}
			YSPlayer->SetUpgradeOffers(TArray<FYSUpgradeOffer>());
		}
	}

	if (PendingLevelUps > 0 || PendingDoubloonRewards.Num() > 0)
	{
		StartUpgradeSelection();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Tous les joueurs ont choisi : la partie reprend"));
		ClearPause();
		GetWorldSettings()->ForceNetUpdate();
		YSGameState->SetClockPaused(false);
	}
}

void AYSGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	// le moteur refuse un identifiant d'un autre sous-système (incompatible_unique_net_id) : en partie par IP, un hôte
	// avec Steam doit accepter un client sans Steam (sous-système NULL) et inversement
	ErrorMessage = GameSession->ApproveLogin(Options);
	FGameModeEvents::GameModePreLoginEvent.Broadcast(this, UniqueId, ErrorMessage);
}

void AYSGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// premier passage : numéro d'arrivée (hôte = 1), nom « Player N » et première couleur libre
	if (AYSPlayerState* NewPlayerState = NewPlayer ? NewPlayer->GetPlayerState<AYSPlayerState>() : nullptr)
	{
		if (NewPlayerState->GetPlayerNumber() == 0)
		{
			const int32 Number = NextPlayerNumber++;
			NewPlayerState->SetPlayerNumber(Number);
			// joueur Steam : son pseudo Steam (reçu dans l'URL de connexion) ; sinon « Player N »
			const FString SteamName = NewPlayerState->GetPlayerName().TrimStartAndEnd().Left(MaxNicknameLength);
			const bool bSteamPlayer = NewPlayerState->GetUniqueId().IsValid() && NewPlayerState->GetUniqueId().GetType() == FName(TEXT("STEAM"));
			ChangeName(NewPlayer, bSteamPlayer && !SteamName.IsEmpty() ? SteamName : FString::Printf(TEXT("Player %d"), Number), false);
			// couleur par défaut : la première libre (les couleurs sont uniques)
			int32 FreeColor = 0;
			while (FreeColor < CustomizationColors.Num() - 1 && IsColorTaken(FreeColor, NewPlayerState))
			{
				++FreeColor;
			}
			NewPlayerState->SetCustomization(FreeColor, 0);
		}
	}

	// pas de pawn tant que le joueur n'est pas prêt (et que la partie n'a pas démarré) : il voit le menu
	const AYSPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AYSPlayerState>() : nullptr;
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (PlayerState && PlayerState->IsReady() && YSGameState && YSGameState->IsMatchStarted())
	{
		RestartPlayer(NewPlayer);
	}
}

UClass* AYSGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	const AYSPlayerState* PlayerState = InController ? InController->GetPlayerState<AYSPlayerState>() : nullptr;
	if (PlayerState && SelectableCharacters.IsValidIndex(PlayerState->GetSelectedCharacterIndex()))
	{
		if (UClass* Selected = SelectableCharacters[PlayerState->GetSelectedCharacterIndex()])
		{
			return Selected;
		}
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void AYSGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	// un joueur qui part sans avoir choisi ne doit pas bloquer les autres
	TryStartMatch();
	TryFinishUpgradeSelection();
}

void AYSGameMode::PlayerSelectedCharacter(APlayerController* Player, int32 CharacterIndex)
{
	AYSPlayerState* PlayerState = Player ? Player->GetPlayerState<AYSPlayerState>() : nullptr;
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!PlayerState || !YSGameState || !SelectableCharacters.IsValidIndex(CharacterIndex) || HasCharacter(Player))
	{
		return;
	}

	// personnage unique déjà choisi par un autre joueur : refusé
	if (IsCharacterTaken(CharacterIndex, PlayerState))
	{
		UE_LOG(LogTemp, Log, TEXT("[YSGameMode] %s : personnage %d refusé (unique, déjà choisi)"), *PlayerState->GetPlayerName(), CharacterIndex);
		return;
	}

	// simple choix : la partie démarre (ou le joueur apparaît) quand il clique sur « Prêt »
	PlayerState->SetSelectedCharacterIndex(CharacterIndex);
}

void AYSGameMode::PlayerSetReady(APlayerController* Player, bool bReady)
{
	AYSPlayerState* PlayerState = Player ? Player->GetPlayerState<AYSPlayerState>() : nullptr;
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!PlayerState || !YSGameState || HasCharacter(Player) || PlayerState->IsDead())
	{
		return;
	}

	// prêt sans avoir cliqué sur un personnage : le premier de la liste (celui de l'aperçu)
	if (bReady && !PlayerState->HasSelectedCharacter())
	{
		PlayerState->SetSelectedCharacterIndex(0);
	}
	PlayerState->SetReady(bReady);

	if (!bReady)
	{
		return;
	}
	if (YSGameState->IsMatchStarted())
	{
		// arrivée en cours de partie : apparaît dès qu'il est prêt
		RestartPlayer(Player);
	}
	else
	{
		TryStartMatch();
	}
}

void AYSGameMode::TryStartMatch()
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || YSGameState->IsMatchStarted() || YSGameState->PlayerArray.Num() == 0)
	{
		return;
	}

	for (const APlayerState* Player : YSGameState->PlayerArray)
	{
		const AYSPlayerState* YSPlayer = Cast<AYSPlayerState>(Player);
		if (!YSPlayer || !YSPlayer->IsReady())
		{
			return;
		}
	}

	YSGameState->SetMatchStarted();
	// jour 1 : carte au trésor
	SpawnDailyTreasureMap();
	for (UYSEnemySpawnRule* Rule : SpawnRules)
	{
		if (Rule)
		{
			Rule->Start(this);
		}
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Player = It->Get();
		if (Player && !HasCharacter(Player))
		{
			RestartPlayer(Player);
		}
	}
	OnAllPlayersReady();
}

bool AYSGameMode::HasCharacter(const AController* Controller)
{
	return Controller && Cast<AYSPlayerCharacter>(Controller->GetPawn()) != nullptr;
}

void AYSGameMode::StartNight()
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!bDayNightCycle || !YSGameState || !YSGameState->IsMatchStarted() || YSGameState->IsNight())
	{
		return;
	}

	YSGameState->StartNight();
	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Nuit %d"), YSGameState->GetDayNumber());
	// nuit d'Overtime Day : overtime = pas d'élite (ni de circle spawn de la nuit), vagues d'overtime puis l'île coule
	if (bOvertime && YSGameState->GetDayNumber() >= OvertimeDay)
	{
		StartOvertime();
		return;
	}
	for (UYSEnemySpawnRule* Rule : SpawnRules)
	{
		if (Rule)
		{
			Rule->NotifyPhaseChanged(true);
		}
	}
}

void AYSGameMode::OnEliteKilled(AYSFoeCharacter* Elite)
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || !YSGameState->IsNight())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Élite %s tué : jour %d"), Elite ? *Elite->GetName() : TEXT("?"), YSGameState->GetDayNumber() + 1);
	AdvanceDay();
}

void AYSGameMode::AdvanceDay()
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState)
	{
		return;
	}
	YSGameState->StartDay(YSGameState->GetDayNumber() + 1);
	// nouveau jour : nouvelle carte au trésor (si la précédente est finie) ; overtime à partir de Overtime Day
	SpawnDailyTreasureMap();
	OnDayStarted(YSGameState->GetDayNumber());
	for (UYSEnemySpawnRule* Rule : SpawnRules)
	{
		if (Rule)
		{
			Rule->NotifyPhaseChanged(false);
		}
	}
}

void AYSGameMode::PlayerSetCustomization(APlayerController* Player, int32 ColorIndex, int32 FaceIndex)
{
	AYSPlayerState* PlayerState = Player ? Player->GetPlayerState<AYSPlayerState>() : nullptr;
	// les visages peuvent être partagés, pas les couleurs
	if (PlayerState && CustomizationColors.IsValidIndex(ColorIndex) && CustomizationFaces.IsValidIndex(FaceIndex) && !IsColorTaken(ColorIndex, PlayerState))
	{
		PlayerState->SetCustomization(ColorIndex, FaceIndex);
	}
}

void AYSGameMode::PlayerSetNickname(APlayerController* Player, const FString& Nickname)
{
	const FString Trimmed = Nickname.TrimStartAndEnd().Left(MaxNicknameLength);
	if (Player && !Trimmed.IsEmpty())
	{
		ChangeName(Player, Trimmed, false);
	}
}

bool AYSGameMode::IsCharacterTaken(int32 CharacterIndex, const APlayerState* Except) const
{
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	return YSGameState && YSGameState->IsCharacterTaken(CharacterIndex, Except);
}

bool AYSGameMode::IsColorTaken(int32 ColorIndex, const APlayerState* Except) const
{
	const AGameStateBase* CurrentGameState = GetGameState<AGameStateBase>();
	if (!CurrentGameState)
	{
		return false;
	}

	for (const APlayerState* Player : CurrentGameState->PlayerArray)
	{
		const AYSPlayerState* YSPlayer = Cast<AYSPlayerState>(Player);
		if (YSPlayer && YSPlayer != Except && YSPlayer->GetPlayerNumber() > 0 && YSPlayer->GetColorIndex() == ColorIndex)
		{
			return true;
		}
	}
	return false;
}

void AYSGameMode::PlayerSetLeftHanded(APlayerController* Player, bool bLeftHanded)
{
	AYSPlayerState* PlayerState = Player ? Player->GetPlayerState<AYSPlayerState>() : nullptr;
	if (PlayerState && !HasCharacter(Player))
	{
		PlayerState->SetLeftHanded(bLeftHanded);
	}
}

APawn* AYSGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	if (!PawnClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;
	SpawnInfo.bDeferConstruction = true;
	// jamais refusé pour une collision (chaloupe : places serrées, un autre joueur ou un HiredMouss à côté) : sans personnage,
	// le joueur restait « mort » pour l'hôte au lancement de la partie
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* Pawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo);
	if (!Pawn)
	{
		UE_LOG(LogTemp, Error, TEXT("[YSGameMode] Personnage %s de %s impossible à faire apparaître en %s"), *PawnClass->GetName(), NewPlayer ? *NewPlayer->GetName() : TEXT("?"), *SpawnTransform.GetLocation().ToCompactString());
		return nullptr;
	}

	// gaucher : à connaître avant la construction (OnConstruction crée les armes des mains)
	const AYSPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AYSPlayerState>() : nullptr;
	if (AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(Pawn))
	{
		PlayerCharacter->SetLeftHanded(PlayerState && PlayerState->IsLeftHanded());
	}
	Pawn->FinishSpawning(SpawnTransform);
	// tests automatisés en réseau : -YSGodAll = aucun joueur ne prend de dégâts (le « god » de la console ne touche que l'hôte)
	if (FParse::Param(FCommandLine::Get(), TEXT("YSGodAll")))
	{
		Pawn->SetCanBeDamaged(false);
	}
	return Pawn;
}

bool AYSGameMode::RevivePlayer(AYSPlayerCorpse* Corpse, const FVector& Ground, float HealthRatio)
{
	AYSPlayerState* PlayerState = Corpse ? Cast<AYSPlayerState>(Corpse->GetOwnerPlayerState()) : nullptr;
	AController* Controller = PlayerState ? PlayerState->GetOwningController() : nullptr;
	AYSPlayerCharacter* Body = Corpse ? Corpse->GetBody() : nullptr;
	// joueur parti, ou déjà revenu avec un autre personnage : le cadavre reste
	if (!PlayerState || !Controller || !IsValid(Body) || Cast<AYSPlayerCharacter>(Controller->GetPawn()))
	{
		UE_LOG(LogTemp, Log, TEXT("[YSMedic] %s : personne à ressusciter (joueur %d, contrôleur %d, corps %d)"), Corpse ? *Corpse->GetName() : TEXT("?"),
			PlayerState != nullptr, Controller != nullptr, IsValid(Body));
		return false;
	}

	// vivant avant la possession (HUD, écran de mort, spectateur)
	PlayerState->SetAlive();
	if (!Body->Revive(Controller, Ground, HealthRatio))
	{
		PlayerState->SetDead();
		return false;
	}
	Corpse->Destroy();
	return true;
}

void AYSGameMode::SetPlayMode(EYSPlayMode Mode)
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || YSGameState->IsMatchStarted())
	{
		return;
	}
	YSGameState->SetPlayMode(Mode, HardcoreFoeDayMultiplier);
	UE_LOG(LogTemp, Log, TEXT("[YSPlayMode] Mode de jeu : %s"), *UEnum::GetValueAsString(Mode));
}

void AYSGameMode::AddPhoenixFeathers(int32 Amount)
{
	if (AYSGameState* YSGameState = GetGameState<AYSGameState>())
	{
		YSGameState->SetPhoenixFeathers(YSGameState->GetPhoenixFeathers() + Amount);
		UE_LOG(LogTemp, Log, TEXT("[YSPhoenix] Plumes %+d -> %d"), Amount, YSGameState->GetPhoenixFeathers());
	}
}

bool AYSGameMode::FindPhoenixRevivePoint(const AActor* Body, int32 Index, FVector& OutGround) const
{
	const FVector From = Body->GetActorLocation();

	// le Brooker du bateau (core) ; bateau absent, de NME ou hors d'usage : le Brooker de l'île ; à défaut (TestMap) tout PNJ qui soigne
	const AYSBrookerNPC* ShipBrooker = nullptr;
	const AYSBrookerNPC* IslandBrooker = nullptr;
	const AYSBrookerNPC* OtherNPC = nullptr;
	for (TActorIterator<AYSBrookerNPC> It(GetWorld()); It; ++It)
	{
		const AYSBrookerNPC* NPC = *It;
		if (!NPC->IsAvailable())
		{
			continue;
		}
		const AYSPirateShip* Ship = AYSPirateShip::GetShipOf(NPC);
		if (Ship && Ship->IsDisabled())
		{
			continue;
		}
		const AYSBrookerNPC*& Slot = Ship ? ShipBrooker : NPC->IsBrooker() ? IslandBrooker : OtherNPC;
		if (!Slot || FVector::DistSquared2D(NPC->GetActorLocation(), From) < FVector::DistSquared2D(Slot->GetActorLocation(), From))
		{
			Slot = NPC;
		}
	}
	const AYSBrookerNPC* Brooker = ShipBrooker ? ShipBrooker : IslandBrooker ? IslandBrooker : OtherNPC;
	if (!Brooker)
	{
		return false;
	}
	const FVector Side = FVector::ForwardVector.RotateAngleAxis(Index * 90.0f, FVector::UpVector);
	OutGround = AYSInteractiveObject::FindGround(GetWorld(), Brooker->GetActorLocation() + Side * PhoenixReviveDistance, Brooker);
	UE_LOG(LogTemp, Log, TEXT("[YSPhoenix] Réanimation chez %s, en %s"), *Brooker->GetName(), *OutGround.ToCompactString());
	return true;
}

bool AYSGameMode::PhoenixRevivePlayer(AController* Controller, int32 Index)
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	AYSPlayerState* PlayerState = Controller ? Controller->GetPlayerState<AYSPlayerState>() : nullptr;
	// Medic dans l'équipage : réanimation gratuite (sans plume) quand elle est prête, utilisée avant les plumes
	AYSBoatCrew* FreeReviver = AYSBoatCrew::FindFreeRevive(GetWorld());
	if (!YSGameState || !PlayerState || !PlayerState->IsDead() || (YSGameState->GetUsablePhoenixFeathers() <= 0 && !FreeReviver) || Cast<AYSPlayerCharacter>(Controller->GetPawn()))
	{
		return false;
	}

	AYSPlayerCorpse* Corpse = nullptr;
	for (TActorIterator<AYSPlayerCorpse> It(GetWorld()); It && !Corpse; ++It)
	{
		Corpse = It->GetOwnerPlayerState() == PlayerState ? *It : nullptr;
	}
	// porté par un joueur : lâché d'abord (dans la zone d'un Medic, il vient de le ressusciter : la plume reste)
	if (AYSPlayerCharacter* Carrier = Corpse ? Corpse->GetCarrier() : nullptr)
	{
		Carrier->DropCarriedObject(TEXT("réanimation par plume de phoenix"));
		if (!PlayerState->IsDead())
		{
			return false;
		}
	}

	AYSPlayerCharacter* Body = PlayerState->GetDeadBody();
	FVector Ground;
	if (!IsValid(Body) || !FindPhoenixRevivePoint(Body, Index, Ground))
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSPhoenix] %s : réanimation impossible (corps %d)"), *PlayerState->GetPlayerName(), IsValid(Body));
		return false;
	}

	// vivant avant la possession (HUD, écran de mort, spectateur)
	PlayerState->SetAlive();
	if (!Body->Revive(Controller, Ground, PhoenixReviveHealthRatio))
	{
		PlayerState->SetDead();
		PlayerState->SetDeadBody(Body);
		return false;
	}
	if (Corpse)
	{
		Corpse->Destroy();
	}
	if (FreeReviver)
	{
		FreeReviver->ConsumeFreeRevive();
		UE_LOG(LogTemp, Log, TEXT("[YSPhoenix] %s ressuscité gratuitement par %s : %d plume(s) gardée(s)"), *PlayerState->GetPlayerName(), *FreeReviver->GetName(), YSGameState->GetPhoenixFeathers());
		return true;
	}
	YSGameState->SetPhoenixFeathers(YSGameState->GetPhoenixFeathers() - 1);
	UE_LOG(LogTemp, Log, TEXT("[YSPhoenix] %s ressuscité : reste %d plume(s)"), *PlayerState->GetPlayerName(), YSGameState->GetPhoenixFeathers());
	return true;
}

void AYSGameMode::TickPhoenixAutoRevive()
{
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || (YSGameState->GetUsablePhoenixFeathers() <= 0 && !AYSBoatCrew::FindFreeRevive(GetWorld())) || GetWorld()->IsPaused())
	{
		return;
	}

	// tout le monde est mort (corps au sol : la dernière mort a fini son ragdoll) : autant de joueurs que de plumes, dans l'ordre d'arrivée
	TArray<AController*> DeadPlayers;
	for (const APlayerState* Player : YSGameState->PlayerArray)
	{
		const AYSPlayerState* YSPlayer = Cast<AYSPlayerState>(Player);
		if (Player && Cast<AYSPlayerCharacter>(Player->GetPawn()))
		{
			return;
		}
		if (YSPlayer && YSPlayer->IsDead() && YSPlayer->GetOwningController())
		{
			DeadPlayers.Add(YSPlayer->GetOwningController());
		}
	}
	int32 Revived = 0;
	for (AController* Controller : DeadPlayers)
	{
		Revived += PhoenixRevivePlayer(Controller, Revived) ? 1 : 0;
	}
	if (Revived > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSPhoenix] Tous les joueurs étaient morts : %d / %d ressuscité(s) automatiquement"), Revived, DeadPlayers.Num());
	}
}

void AYSGameMode::AddDoubloons(int32 Amount)
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || Amount <= 0)
	{
		return;
	}

	// XP du bateau : chaque seuil franchi = un niveau (le premier le fait arriver), la jauge repart de 0 avec le surplus
	int32 Level = YSGameState->GetShipLevel();
	int32 Total = YSGameState->GetDoubloons() + Amount;
	while (Total >= GetDoubloonGoalForShipLevel(Level))
	{
		Total -= GetDoubloonGoalForShipLevel(Level);
		++Level;
		YSGameState->SetShipLevel(Level);
		if (Level == 1)
		{
			UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Objectif atteint : bateau niveau 1"));
			ShipArrives(TEXT("objectif de doublons atteint"));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[YSShip] Le bateau monte au niveau %d"), Level);
		}
		// jauge remplie : récompense pour tous (classe, upgrade d'arme, Golden, puis mythique)
		if (bDoubloonRewards)
		{
			PendingDoubloonRewards.Add(GetDoubloonReward(DoubloonFillCount));
			UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Jauge de doublons n°%d remplie : récompense %s"), DoubloonFillCount + 1, *UEnum::GetValueAsString(PendingDoubloonRewards.Last()));
		}
		++DoubloonFillCount;
	}
	YSGameState->SetDoubloons(Total, GetDoubloonGoalForShipLevel(Level));
	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Doublons +%d -> %d / %d (bateau niveau %d)"), Amount, Total, GetDoubloonGoalForShipLevel(Level), Level);
	if (PendingDoubloonRewards.Num() > 0 && YSGameState->IsMatchStarted() && !YSGameState->IsUpgradeSelectionActive())
	{
		StartUpgradeSelection();
	}
}

int32 AYSGameMode::GetPlayersInMatchCount() const
{
	const AGameStateBase* CurrentGameState = GetGameState<AGameStateBase>();
	if (!CurrentGameState)
	{
		return 0;
	}

	int32 Count = 0;
	for (const APlayerState* Player : CurrentGameState->PlayerArray)
	{
		const AYSPlayerState* YSPlayer = Cast<AYSPlayerState>(Player);
		Count += (Player && Cast<AYSPlayerCharacter>(Player->GetPawn())) || (YSPlayer && YSPlayer->IsDead()) ? 1 : 0;
	}
	return Count;
}

float AYSGameMode::GetFoeLevelGainMultiplier(const UWorld* World)
{
	const AYSGameMode* GameMode = World ? World->GetAuthGameMode<AYSGameMode>() : nullptr;
	return GameMode ? GameMode->FoeLevelGainMultiplier : 1.0f;
}

FYSPlayerCountScaling AYSGameMode::GetPlayerCountScaling() const
{
	if (PlayerCountScaling.Num() == 0)
	{
		return FYSPlayerCountScaling();
	}
	return PlayerCountScaling[FMath::Clamp(GetPlayersInMatchCount() - 1, 0, PlayerCountScaling.Num() - 1)];
}

int32 AYSGameMode::ScaleEnemyCount(int32 BaseCount) const
{
	return BaseCount > 0 ? FMath::Max(1, FMath::RoundToInt(BaseCount * GetPlayerCountScaling().SpawnMultiplier)) : 0;
}

int32 AYSGameMode::SpawnBonusWave(int32 EnemiesPerPlayer, const TCHAR* Reason)
{
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || !YSGameState->IsMatchStarted())
	{
		return 0;
	}

	// mêmes NME et même placement hors écran que la première règle de spawn active
	for (UYSEnemySpawnRule* Rule : SpawnRules)
	{
		if (Rule && Rule->bEnabled && Rule->EnemyClasses.Num() > 0)
		{
			const int32 Count = ScaleEnemyCount(EnemiesPerPlayer);
			const int32 Spawned = Rule->SpawnEnemies(Count);
			UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Vague bonus (%s) : %d / %d NME"), Reason, Spawned, Count);
			return Spawned;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[YSGameMode] Vague bonus (%s) : aucune règle de spawn avec des NME"), Reason);
	return 0;
}

void AYSGameMode::SpawnDailyTreasureMap()
{
	if (bOneTreasureMapAtATime)
	{
		// une seule île pour l'instant : n'importe quelle carte encore là (posée ou portée) compte
		for (TActorIterator<AYSTreasureMap> It(GetWorld()); It; ++It)
		{
			if (!It->IsActorBeingDestroyed())
			{
				UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Carte au trésor du jour : aucune, %s n'est pas encore finie"), *It->GetName());
				return;
			}
		}
	}
	SpawnTreasureMap();
}

AYSTreasureMap* AYSGameMode::SpawnTreasureMap()
{
	const AYSTreasureMap* Settings = TreasureMapClass ? TreasureMapClass->GetDefaultObject<AYSTreasureMap>() : nullptr;
	if (!Settings)
	{
		return nullptr;
	}

	for (int32 Attempt = 0; Attempt < TreasureMapSpawnAttempts; ++Attempt)
	{
		const FVector Candidate = TreasureMapAreaCenter + FVector(FMath::FRandRange(-TreasureMapAreaExtent.X, TreasureMapAreaExtent.X),
			FMath::FRandRange(-TreasureMapAreaExtent.Y, TreasureMapAreaExtent.Y), 0.0f);
		FVector Ground;
		if (AYSTreasureMap::FindPathGround(GetWorld(), Candidate, TreasureMapAreaCenter.Z, Settings->GetMaxHeightDifference(), Ground))
		{
			if (AYSTreasureMap* TreasureMap = SpawnTreasureMapAt(Ground))
			{
				return TreasureMap;
			}
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[YSGameMode] Carte au trésor : aucun emplacement avec un chemin complet après %d essais"), TreasureMapSpawnAttempts);
	return nullptr;
}

AYSTreasureMap* AYSGameMode::SpawnTreasureMapAt(const FVector& Location)
{
	const AYSTreasureMap* Settings = TreasureMapClass ? TreasureMapClass->GetDefaultObject<AYSTreasureMap>() : nullptr;
	TArray<FVector> Path;
	if (!Settings || !Settings->BuildPath(GetWorld(), Location, Path))
	{
		return nullptr;
	}

	const FTransform Transform(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), Location);
	AYSTreasureMap* TreasureMap = GetWorld()->SpawnActorDeferred<AYSTreasureMap>(TreasureMapClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TreasureMap)
	{
		return nullptr;
	}
	TreasureMap->SetPath(Path);
	TreasureMap->FinishSpawning(Transform);

	float Length = 0.0f;
	for (int32 Index = 1; Index < Path.Num(); ++Index)
	{
		Length += FVector::Dist2D(Path[Index - 1], Path[Index]);
	}
	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Carte au trésor %s à (%.0f, %.0f) : %d segments, %.0f cm, arrivée (%.0f, %.0f)"), *TreasureMap->GetName(),
		Location.X, Location.Y, Path.Num() - 1, Length, Path.Last().X, Path.Last().Y);
	return TreasureMap;
}

AYSFoeCharacter* AYSGameMode::SpawnNumpadFoe(const APlayerController* Player, int32 Key)
{
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!bNumpadFoeSpawn || !Pawn)
	{
		return nullptr;
	}

	// NME marin : sur l'eau, pas devant le personnage
	if (Key == NumpadJollyShipKey)
	{
		const AYSFoeJollyShip* Ship = SpawnJollyShip(TEXT("pavé numérique"));
		UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Pavé numérique %d : %s"), Key, Ship ? *Ship->GetName() : TEXT("échec"));
		return nullptr;
	}

	if (Key == NumpadGalionKey)
	{
		SpawnGalion(TEXT("pavé numérique"));
		return nullptr;
	}

	for (const TSubclassOf<AYSFoeCharacter>& FoeClass : NumpadFoeClasses)
	{
		if (FoeClass && FoeClass->GetDefaultObject<AYSFoeCharacter>()->GetNumpadSpawnKey() == Key)
		{
			AYSFoeCharacter* Foe = SpawnFoeInFront(FoeClass, Pawn, NumpadSpawnDistance);
			UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Pavé numérique %d : %s"), Key, Foe ? *Foe->GetName() : TEXT("échec"));
			return Foe;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Pavé numérique %d : aucun NME sur cette touche"), Key);
	return nullptr;
}

AYSFoeCharacter* AYSGameMode::SpawnFoeInFront(UClass* FoeClass, const APawn* Pawn, float Distance)
{
	if (!FoeClass || !Pawn || !FoeClass->IsChildOf(AYSFoeCharacter::StaticClass()))
	{
		return nullptr;
	}

	// au sol devant le personnage, tourné vers lui
	const FVector Forward = Pawn->GetActorForwardVector().GetSafeNormal2D();
	const FVector Ground = AYSInteractiveObject::FindGround(GetWorld(), Pawn->GetActorLocation() + Forward * Distance, Pawn);
	const AYSFoeCharacter* FoeDefaults = FoeClass->GetDefaultObject<AYSFoeCharacter>();
	const float HalfHeight = FoeDefaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * FoeDefaults->GetSizeMultiplier();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	return GetWorld()->SpawnActor<AYSFoeCharacter>(FoeClass, Ground + FVector(0.0f, 0.0f, HalfHeight + 2.0f), (-Forward).Rotation(), Params);
}

int32 AYSGameMode::SpawnNumpadCircle(int32 Key)
{
	if (!bNumpadFoeSpawn)
	{
		return 0;
	}

	for (const TSubclassOf<AYSFoeCharacter>& FoeClass : NumpadFoeClasses)
	{
		if (FoeClass && FoeClass->GetDefaultObject<AYSFoeCharacter>()->GetNumpadSpawnKey() == Key)
		{
			return SpawnCircle({ FoeClass }, NumpadCircleCount, FString::Printf(TEXT("Ctrl + pavé numérique %d"), Key));
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Ctrl + pavé numérique %d : aucun NME sur cette touche"), Key);
	return 0;
}

int32 AYSGameMode::SpawnCircle(const TArray<TSubclassOf<AYSFoeCharacter>>& Classes, int32 Count, const FString& Reason, int32 ForcedLevel, int32 LevelBonus)
{
	TArray<TSubclassOf<AYSFoeCharacter>> ValidClasses = Classes;
	ValidClasses.RemoveAll([](const TSubclassOf<AYSFoeCharacter>& Class) { return !Class; });
	if (Count <= 0 || ValidClasses.Num() == 0)
	{
		return 0;
	}

	// emplacements à intervalles réguliers tout autour de l'île (départ au hasard), tournés vers le centre
	const double StartTime = FPlatformTime::Seconds();
	const float StartAngle = FMath::FRandRange(0.0f, 2.0f * PI);
	int32 Spawned = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FVector Ground;
		if (FindShorePoint(StartAngle + 2.0f * PI * Index / Count, Ground))
		{
			Spawned += SpawnFoeOnGround(UYSEnemySpawnRule::PickEnemyClass(ValidClasses), Ground, IslandCenter, ForcedLevel, LevelBonus) ? 1 : 0;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Circle spawn (%s) : %d / %d NME sur la plage (%.1f ms)"), *Reason, Spawned, Count, (FPlatformTime::Seconds() - StartTime) * 1000.0);
	return Spawned;
}

AYSFoeCharacter* AYSGameMode::SpawnFoeOnGround(UClass* FoeClass, const FVector& Ground, const FVector& FacingTarget, int32 ForcedLevel, int32 LevelBonus)
{
	if (!FoeClass || !FoeClass->IsChildOf(AYSFoeCharacter::StaticClass()))
	{
		return nullptr;
	}

	// la taille du NME (élite x3) est appliquée à son BeginPlay : on la prévoit pour le poser sur le sol
	const AYSFoeCharacter* FoeDefaults = FoeClass->GetDefaultObject<AYSFoeCharacter>();
	const float HalfHeight = FoeDefaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * FoeDefaults->GetSizeMultiplier();
	const FVector Location = Ground + FVector(0.0f, 0.0f, HalfHeight + 2.0f);
	const FRotator Rotation(0.0f, (FacingTarget - Location).Rotation().Yaw, 0.0f);

	// niveau imposé avant le BeginPlay (overtime)
	const FTransform SpawnTransform(Rotation, Location);
	AYSFoeCharacter* Foe = GetWorld()->SpawnActorDeferred<AYSFoeCharacter>(FoeClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (Foe)
	{
		Foe->SetForcedLevel(ForcedLevel);
		Foe->SetLevelBonus(LevelBonus);
		Foe->FinishSpawning(SpawnTransform);
	}
	// place prise : FinishSpawning le détruit
	return IsValid(Foe) ? Foe : nullptr;
}

bool AYSGameMode::FindIslandGround(const FVector& Point, FVector& OutGround) const
{
	// seul le Landscape (l'île) compte : un prop touché (ponton, bateau, palmier) est traversé
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSIslandGround), false);
	const FVector Start(Point.X, Point.Y, Point.Z + 5000.0f);
	const FVector End(Point.X, Point.Y, Point.Z - 5000.0f);
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			return false;
		}
		if (YSCombat::IsIslandGroundActor(Hit.GetActor()))
		{
			OutGround = Hit.ImpactPoint;
			return !AYSOcean::IsUnderwater(GetWorld(), OutGround);
		}
		Params.AddIgnoredActor(Hit.GetActor());
	}
	return false;
}

void AYSGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (bRandomHarbor)
	{
		PlaceHarbor();
	}
}

void AYSGameMode::PlaceHarbor()
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	const AActor* Pivot = AYSGameState::FindHarborPivot(GetWorld());
	if (!YSGameState || !Pivot)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSHarbor] Pas de port à placer (aucun acteur tagué %s)"), *AYSGameState::HarborPivotTag.ToString());
		return;
	}

	// acteurs du port et leurs contraintes (sur la terre ferme / entièrement en mer)
	TArray<const AActor*> LandActors;
	TArray<const AActor*> SeaActors;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(AYSGameState::HarborLandTag))
		{
			LandActors.Add(*It);
		}
		if (It->ActorHasTag(AYSGameState::HarborSeaTag))
		{
			SeaActors.Add(*It);
		}
	}

	const FVector OldPivot = Pivot->GetActorLocation();
	const float OldAngle = FMath::Atan2(OldPivot.Y - IslandCenter.Y, OldPivot.X - IslandCenter.X);
	for (int32 Attempt = 1; Attempt <= HarborPlacementAttempts; ++Attempt)
	{
		const float Angle = FMath::FRandRange(-PI, PI);
		FVector Edge;
		if (!FindShorePoint(Angle, Edge, 0.0f))
		{
			continue;
		}
		const FVector NewPivot(Edge.X, Edge.Y, OldPivot.Z);
		const float YawDelta = FMath::RadiansToDegrees(Angle - OldAngle);

		bool bValid = true;
		for (const AActor* Actor : LandActors)
		{
			FVector Ground;
			const FTransform Placed = AYSGameState::GetHarborTransform(Actor->GetActorTransform(), OldPivot, NewPivot, YawDelta);
			bValid &= FindIslandGround(Placed.GetLocation(), Ground);
		}
		for (const AActor* Actor : SeaActors)
		{
			// centre et coins de la boîte englobante (à plat) : aucun sol de l'île hors de l'eau dessous
			FVector Origin, Extent;
			Actor->GetActorBounds(false, Origin, Extent);
			for (const FVector2D Corner : { FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, -1.0f), FVector2D(-1.0f, 1.0f), FVector2D(-1.0f, -1.0f) })
			{
				FVector Ground;
				const FVector Point(Origin.X + Corner.X * Extent.X, Origin.Y + Corner.Y * Extent.Y, Origin.Z);
				const FTransform Placed = AYSGameState::GetHarborTransform(FTransform(Point), OldPivot, NewPivot, YawDelta);
				bValid &= !FindIslandGround(Placed.GetLocation(), Ground);
			}
		}
		if (bValid)
		{
			UE_LOG(LogTemp, Log, TEXT("[YSHarbor] Port placé (essai %d) : direction %.0f°, ponton en (%.0f, %.0f), rotation %.0f°"),
				Attempt, FMath::RadiansToDegrees(Angle), NewPivot.X, NewPivot.Y, YawDelta);
			YSGameState->SetHarborPlacement(NewPivot, YawDelta);
			return;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[YSHarbor] Aucune direction valable en %d essais : le port reste à sa place"), HarborPlacementAttempts);
}

bool AYSGameMode::FindShorePoint(float Angle, FVector& OutGround, float Inset) const
{
	const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
	const float Step = FMath::Max(ShoreSearchStep, 10.0f);

	// du large vers le centre : premier sol de l'île hors de l'eau = bord de la plage
	for (float Distance = ShoreSearchRadius; Distance > 0.0f; Distance -= Step)
	{
		FVector Shore;
		if (FindIslandGround(IslandCenter + Direction * Distance, Shore))
		{
			// un peu à l'intérieur pour avoir les pieds au sec (sinon le bord lui-même)
			FVector Inland;
			OutGround = Inset > 0.0f && FindIslandGround(IslandCenter + Direction * FMath::Max(Distance - Inset, 0.0f), Inland) ? Inland : Shore;
			return true;
		}
	}
	return false;
}

void AYSGameMode::GameOver(const TCHAR* Reason)
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || YSGameState->IsGameOver() || YSGameState->IsVictory())
	{
		return;
	}
	YSGameState->SetGameOver(GameOverReturnDelay);
	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] GAME OVER (%s) : écran des scores dans %.0f s"), Reason, GameOverReturnDelay);
}

void AYSGameMode::TriggerVictory(const TCHAR* Reason, float ReturnDelay)
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || YSGameState->IsGameOver() || YSGameState->IsVictory())
	{
		return;
	}
	const float Delay = ReturnDelay >= 0.0f ? ReturnDelay : VictoryReturnDelay;
	YSGameState->SetVictory(Delay);
	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] BRAVO (%s) : écran des scores dans %.0f s"), Reason, Delay);
}

TSubclassOf<AYSInteractiveObject> AYSGameMode::PickRandomCombatPostItem() const
{
	TArray<TSubclassOf<AYSInteractiveObject>> Valid;
	for (const TSubclassOf<AYSInteractiveObject>& Class : CombatPostItemClasses)
	{
		if (Class)
		{
			Valid.Add(Class);
		}
	}
	return Valid.Num() > 0 ? Valid[FMath::RandRange(0, Valid.Num() - 1)] : nullptr;
}
