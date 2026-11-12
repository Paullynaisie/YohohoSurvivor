#include "Enemies/YSDavyJonesArena.h"
#include "Components/CapsuleComponent.h"
#include "Core/YSFlowMode.h"
#include "Core/YSGameState.h"
#include "Ship/YSBoatCrew.h"
#include "Enemies/YSEnemySpawnRule.h"
#include "Enemies/YSFoeCharacter.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interaction/YSInteractiveObject.h"
#include "Interaction/YSSuperTreasureMap.h"
#include "Map/YSOcean.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "Ship/YSPirateShip.h"
#include "Ship/YSShipHelm.h"

namespace YSDavyJones
{
	/** Saut : marge après l'heure d'arrivée prévue avant de le poser exactement (collisions en vol) */
	constexpr double JumpLandingGrace = 0.3;

	/** Davy Jones tombé à l'eau (pieds à plus de ça sous la surface) : reposé sur le pont du bateau des joueurs */
	constexpr float FallenInSeaDepth = 150.0f;

	/** Hauteur au-dessus des pieds pour poser un acteur (évite le sol) */
	constexpr float SpawnLift = 5.0f;
}

AYSDavyJonesArena::AYSDavyJonesArena()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AYSDavyJonesArena::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AYSDavyJonesArena, State);
}

AYSDavyJonesArena* AYSDavyJonesArena::Find(const UWorld* World)
{
	for (TActorIterator<AYSDavyJonesArena> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AYSDavyJonesArena::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] Zone du world boss en %s (rayon %.0f m)"), *GetActorLocation().ToCompactString(), TriggerRadius / 100.0f);
}

void AYSDavyJonesArena::OnRep_State()
{
	UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] État %d (client)"), static_cast<int32>(State));
}

void AYSDavyJonesArena::SetState(EYSDavyJonesState NewState)
{
	State = NewState;
	StateStartTime = GetWorld()->GetTimeSeconds();
	ForceNetUpdate();
}

float AYSDavyJonesArena::GetSeaLevel() const
{
	const AYSOcean* Ocean = AYSOcean::Find(GetWorld());
	return Ocean ? Ocean->GetSeaLevel() : GetActorLocation().Z;
}

bool AYSDavyJonesArena::IsDavyAlive() const
{
	return IsValid(Davy) && !Davy->IsDying() && Davy->GetHealth() > 0.0f;
}

void AYSDavyJonesArena::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		TickFight();
	}
}

void AYSDavyJonesArena::TickFight()
{
	if (State == EYSDavyJonesState::Defeated)
	{
		return;
	}
	if (State == EYSDavyJonesState::Waiting)
	{
		// partie 2 seulement : le bateau des joueurs entre dans la zone
		const AYSFlowMode* FlowMode = GetWorld()->GetAuthGameMode<AYSFlowMode>();
		const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
		const AYSPirateShip* Boat = YSGameState ? YSGameState->GetGameBoat() : nullptr;
		if (FlowMode && FlowMode->IsPart2Started() && !FlowMode->IsPart3Started() && IsValid(Boat) && !Boat->IsSinking()
			&& FVector::Dist2D(Boat->GetHullCenter(), GetActorLocation()) <= TriggerRadius)
		{
			StartFight(TEXT("le bateau des joueurs entre dans la zone"));
		}
		return;
	}

	// Davy Jones mort : victoire
	if (!IsDavyAlive())
	{
		Victory();
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	const double Elapsed = Now - StateStartTime;
	// tombé à l'eau (hors saut) : reposé sur le pont du bateau des joueurs
	const float FeetZ = Davy->GetActorLocation().Z - Davy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (State != EYSDavyJonesState::Jumping && FeetZ < GetSeaLevel() - YSDavyJones::FallenInSeaDepth && IsValid(PlayerBoat))
	{
		const FVector Feet = FindDescendPoint();
		Davy->TeleportTo(Feet + FVector(0.0f, 0.0f, Davy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + YSDavyJones::SpawnLift), Davy->GetActorRotation());
		Davy->GetCharacterMovement()->StopMovementImmediately();
		UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] %s tombé à l'eau : reposé sur %s"), *Davy->GetName(), *GetNameSafe(PlayerBoat));
	}

	// paquets d'équipage restants (même s'il est déjà descendu)
	TickCrewBatches();

	switch (State)
	{
	case EYSDavyJonesState::Summoning:
		if (Elapsed >= SummonAnimDuration)
		{
			SpawnCrewWave();
		}
		break;

	case EYSDavyJonesState::Crew:
	{
		Crew.RemoveAll([](const TWeakObjectPtr<AYSFoeCharacter>& Foe) { return !Foe.IsValid() || Foe->IsDying() || Foe->GetHealth() <= 0.0f; });
		// Descend Crew Killed Ratio de l'équipage tué (part de tout l'équipage, paquets à venir compris), Descend Max Crew Time écoulé (ou plus
		// de Galion sous ses pieds) : il descend se battre
		const bool bGalionLost = !IsValid(DavyGalion) || DavyGalion->IsSinking();
		const int32 Killed = CrewSpawnedCount - Crew.Num();
		const bool bEnoughKilled = (Crew.Num() == 0 && PendingCrew.Num() == 0) || Killed >= FMath::CeilToInt(CrewPlannedCount * DescendCrewKilledRatio);
		const bool bTimeUp = DescendMaxCrewTime > 0.0f && Elapsed >= DescendMaxCrewTime;
		if (bEnoughKilled || bTimeUp || bGalionLost)
		{
			UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] Phase %d : %s (%d / %d tués, %.0f s), Davy Jones descend"), Phase + 1,
				bGalionLost ? TEXT("son Galion coule") : bEnoughKilled ? TEXT("équipage tué") : TEXT("temps écoulé"), Killed, CrewPlannedCount, Elapsed);
			DavyGalion = nullptr;
			StartJump(FindDescendPoint(), false);
		}
		break;
	}

	case EYSDavyJonesState::Jumping:
		if (Now >= JumpEndTime)
		{
			FinishJump();
		}
		break;

	case EYSDavyJonesState::Fighting:
		if (PhaseHealthThresholds.IsValidIndex(Phase) && Davy->GetHealth() <= Davy->GetMaxHealth() * PhaseHealthThresholds[Phase] + 0.5f)
		{
			StartNextPhase();
		}
		break;

	default:
		break;
	}
}

void AYSDavyJonesArena::StartFight(const TCHAR* Reason)
{
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	PlayerBoat = YSGameState ? YSGameState->GetGameBoat() : nullptr;
	if (!IsValid(PlayerBoat) || !DavyJonesClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSDavyJones] Combat impossible (%s) : bateau des joueurs %s, Davy Jones Class %s"), Reason, *GetNameSafe(PlayerBoat), *GetNameSafe(DavyJonesClass));
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] COMBAT contre Davy Jones (%s) autour de %s"), Reason, *PlayerBoat->GetName());
	RemoveSuperTreasureMaps();

	// le bateau des joueurs est immobilisé, le pilote lâche la barre
	PlayerBoat->SetPilotInput(0.0f, 0.0f);
	if (AYSShipHelm* Helm = PlayerBoat->GetHelm())
	{
		if (AYSPlayerCharacter* Pilot = Helm->GetOccupant())
		{
			Pilot->LeaveStation(TEXT("combat contre Davy Jones"), true);
		}
	}
	PlayerBoat->SetGrappled(true);
	// tout le combat : invulnérable, ignoré par les NME, postes de combat bloqués
	PlayerBoat->SetBossFightLocked(true);

	SpawnGalions();
	Phase = 0;
	SpawnDavy();
	if (AYSGameState* MutableState = GetWorld()->GetGameState<AYSGameState>())
	{
		MutableState->SetDavyJonesState(true, false);
	}
	if (IsDavyAlive())
	{
		BeginSummon();
	}
}

void AYSDavyJonesArena::RemoveSuperTreasureMaps()
{
	TArray<AYSSuperTreasureMap*> Maps;
	for (TActorIterator<AYSSuperTreasureMap> It(GetWorld()); It; ++It)
	{
		Maps.Add(*It);
	}
	for (AYSSuperTreasureMap* Map : Maps)
	{
		if (AYSPlayerCharacter* Carrier = Map->GetCarrier())
		{
			Carrier->DropCarriedObject(TEXT("combat contre Davy Jones"));
		}
		UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] %s disparaît"), *Map->GetName());
		Map->Destroy();
	}
}

bool AYSDavyJonesArena::SpawnGalions()
{
	if (!GalionClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSDavyJones] Pas de Galion Class : Davy Jones reste sur le bateau des joueurs"));
		return false;
	}
	// triangle autour du bateau des joueurs : bâbord et tribord parallèles à lui, proue en travers ; chaque coque à Galion Gap de la sienne
	const FBox2D Boat = PlayerBoat->GetHullLocalBounds();
	const FBox2D Gal = GalionClass->GetDefaultObject<AYSFoeGalion>()->GetHullLocalBounds();
	if (!Boat.bIsValid || !Gal.bIsValid)
	{
		return false;
	}
	const FTransform BoatTransform(FRotator(0.0f, PlayerBoat->GetActorRotation().Yaw, 0.0f), PlayerBoat->GetActorLocation());
	const float BoatCenterX = (Boat.Min.X + Boat.Max.X) * 0.5f;
	const float GalCenterX = (Gal.Min.X + Gal.Max.X) * 0.5f;
	struct FPlacement
	{
		FVector Local;
		float YawOffset;
	};
	const TArray<FPlacement> Placements = {
		// bâbord (-Y) : son tribord (+Y) contre le bateau
		{ FVector(BoatCenterX - GalCenterX, Boat.Min.Y - GalionGap - Gal.Max.Y, 0.0f), 0.0f },
		// tribord (+Y) : son bâbord (-Y) contre le bateau
		{ FVector(BoatCenterX - GalCenterX, Boat.Max.Y + GalionGap - Gal.Min.Y, 0.0f), 0.0f },
		// proue (+X), tourné de 90° : son X local suit le Y du bateau, son +Y local regarde vers le bateau (-X)
		{ FVector(Boat.Max.X + GalionGap + Gal.Max.Y, -GalCenterX, 0.0f), 90.0f },
	};
	for (const FPlacement& Placement : Placements)
	{
		FVector Location = BoatTransform.TransformPosition(Placement.Local);
		Location.Z = PlayerBoat->GetActorLocation().Z;
		const FTransform Transform(FRotator(0.0f, BoatTransform.Rotator().Yaw + Placement.YawOffset, 0.0f), Location);
		AYSFoeGalion* Galion = GetWorld()->SpawnActorDeferred<AYSFoeGalion>(GalionClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Galion)
		{
			continue;
		}
		Galion->FinishSpawning(Transform);
		Galion->SyncMotionToActor();
		Galion->StartScriptedBoarding(PlayerBoat);
		Galions.Add(Galion);
		UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] %s en %s (yaw %.0f)"), *Galion->GetName(), *Location.ToCompactString(), Transform.Rotator().Yaw);
	}
	return Galions.Num() > 0;
}

void AYSDavyJonesArena::SpawnDavy()
{
	const TArray<AYSFoeGalion*> Live = GetLiveGalions();
	DavyGalion = Live.Num() > 0 ? Live[FMath::RandRange(0, Live.Num() - 1)] : nullptr;
	FVector Feet = FindDescendPoint();
	if (DavyGalion && !DavyGalion->GetPlatformTop(Feet))
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSDavyJones] %s sans plateforme « %s » : Davy Jones posé sur son pont"), *DavyGalion->GetName(), *DavyGalion->PlatformComponentName.ToString());
		Feet = DavyGalion->GetActorLocation() + FVector(0.0f, 0.0f, 250.0f);
	}
	const AYSFoeCharacter* Defaults = DavyJonesClass->GetDefaultObject<AYSFoeCharacter>();
	const float HalfHeight = Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * Defaults->GetSizeMultiplier();
	const FVector Facing = IsValid(PlayerBoat) ? PlayerBoat->GetHullCenter() : GetActorLocation();
	const FTransform Transform(FRotator(0.0f, (Facing - Feet).Rotation().Yaw, 0.0f), Feet + FVector(0.0f, 0.0f, HalfHeight + YSDavyJones::SpawnLift));
	Davy = GetWorld()->SpawnActorDeferred<AYSFoeCharacter>(DavyJonesClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Davy)
	{
		return;
	}
	Davy->MarkArenaFoe();
	Davy->SetScriptedHold(true);
	Davy->SetScriptedInvulnerable(true);
	Davy->FinishSpawning(Transform);
	Davy->SetHudHealthBar(true);
	UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] %s apparaît sur la plateforme de %s (%.0f PV)"), *Davy->GetName(), *GetNameSafe(DavyGalion), Davy->GetMaxHealth());
}

void AYSDavyJonesArena::BeginSummon()
{
	Davy->SetScriptedHold(true);
	Davy->SetScriptedInvulnerable(true);
	Davy->SetScriptedHealthFloor(0.0f);
	// tourné vers le bateau des joueurs, crochet (main gauche) levé
	if (IsValid(PlayerBoat))
	{
		Davy->SetActorRotation(FRotator(0.0f, (PlayerBoat->GetHullCenter() - Davy->GetActorLocation()).Rotation().Yaw, 0.0f));
	}
	Davy->MulticastRaiseHand(true, SummonAnimDuration);
	SetState(EYSDavyJonesState::Summoning);
	UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] Phase %d : Davy Jones lève son crochet sur %s"), Phase + 1, *GetNameSafe(DavyGalion));
}

void AYSDavyJonesArena::SpawnCrewWave()
{
	Crew.Reset();
	PendingCrew.Reset();
	CrewSpawnedCount = 0;
	CrewPlannedCount = 0;
	AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	const FYSDavyJonesCrewWave* Wave = CrewWaves.IsValidIndex(Phase) ? &CrewWaves[Phase] : (CrewWaves.Num() > 0 ? &CrewWaves.Last() : nullptr);
	if (Wave && GameMode)
	{
		// nombres pour 1 joueur x Spawn Multiplier du nombre de joueurs
		TArray<FYSGalionCrewGroup> Groups = Wave->Groups;
		for (FYSGalionCrewGroup& Group : Groups)
		{
			Group.Count = GameMode->ScaleEnemyCount(Group.Count);
		}
		// par Galion : classes tirées pour chaque NME, mélangées (chaque paquet mêle les types)
		for (AYSFoeGalion* Galion : GetLiveGalions())
		{
			FPendingCrew Pending;
			Pending.Galion = Galion;
			for (const FYSGalionCrewGroup& Group : Groups)
			{
				TArray<TSubclassOf<AYSFoeCharacter>> Classes = Group.Classes;
				Classes.RemoveAll([](const TSubclassOf<AYSFoeCharacter>& Class) { return !Class; });
				for (int32 Index = 0; Classes.Num() > 0 && Index < Group.Count; ++Index)
				{
					Pending.Classes.Add(UYSEnemySpawnRule::PickEnemyClass(Classes));
				}
			}
			for (int32 K = Pending.Classes.Num() - 1; K > 0; --K)
			{
				Pending.Classes.Swap(K, FMath::RandRange(0, K));
			}
			CrewPlannedCount += Pending.Classes.Num();
			PendingCrew.Add(MoveTemp(Pending));
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] Phase %d : équipage de %d NME sur %d Galion(s), %d à la fois toutes les %.2f s depuis les chalouperies"), Phase + 1,
		CrewPlannedCount, GetLiveGalions().Num(), CrewBatchSize, CrewBatchInterval);
	SetState(EYSDavyJonesState::Crew);
	NextCrewBatchTime = 0.0;
	TickCrewBatches();
}

int32 AYSDavyJonesArena::CountPendingCrew() const
{
	int32 Count = 0;
	for (const FPendingCrew& Pending : PendingCrew)
	{
		Count += Pending.Classes.Num();
	}
	return Count;
}

void AYSDavyJonesArena::TickCrewBatches()
{
	// Galion coulé : ses paquets restants sont perdus
	PendingCrew.RemoveAll([this](const FPendingCrew& Pending)
	{
		const bool bLost = !Pending.Galion.IsValid() || Pending.Galion->IsSinking();
		if (bLost && Pending.Classes.Num() > 0)
		{
			CrewPlannedCount -= Pending.Classes.Num();
			UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] %d NME pas apparus (Galion coulé)"), Pending.Classes.Num());
		}
		return bLost || Pending.Classes.Num() == 0;
	});
	const double Now = GetWorld()->GetTimeSeconds();
	if (PendingCrew.Num() == 0 || Now < NextCrewBatchTime)
	{
		return;
	}
	NextCrewBatchTime = Now + CrewBatchInterval;
	for (FPendingCrew& Pending : PendingCrew)
	{
		const int32 Count = FMath::Min(FMath::Max(CrewBatchSize, 1), Pending.Classes.Num());
		TArray<TSubclassOf<AYSFoeCharacter>> Batch(Pending.Classes.GetData(), Count);
		Pending.Classes.RemoveAt(0, Count);
		const TArray<AYSFoeCharacter*> Spawned = Pending.Galion->SpawnFreeCrewAtLongboat(Batch, CrewBatchRadius);
		for (AYSFoeCharacter* Foe : Spawned)
		{
			Foe->MarkArenaFoe();
			Crew.Add(Foe);
			++CrewSpawnedCount;
		}
		// apparition ratée : pas comptée
		CrewPlannedCount -= Count - Spawned.Num();
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSDavyJones] paquet d'équipage : %d / %d apparus, %d en attente"), CrewSpawnedCount, CrewPlannedCount, CountPendingCrew());
}

void AYSDavyJonesArena::StartNextPhase()
{
	++Phase;
	Davy->SetScriptedHold(true);
	Davy->SetScriptedInvulnerable(true);
	Davy->SetScriptedHealthFloor(0.0f);

	// un Galion coule : celui où il y a le moins de joueurs (aucun en général), au hasard entre ex aequo ; Davy Jones monte sur un autre
	TArray<AYSFoeGalion*> Live = GetLiveGalions();
	for (int32 K = Live.Num() - 1; K > 0; --K)
	{
		Live.Swap(K, FMath::RandRange(0, K));
	}
	if (Live.Num() >= 2)
	{
		Live.StableSort([this](const AYSFoeGalion& A, const AYSFoeGalion& B) { return CountPlayersOn(&A) < CountPlayersOn(&B); });
		AYSFoeGalion* Sunk = Live[0];
		Live.RemoveAt(0);
		SinkGalion(Sunk, TEXT("Davy Jones change de Galion"));
	}
	UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] Phase %d : Davy Jones à %.0f / %.0f PV, %d Galion(s) à flot"), Phase + 1, Davy->GetHealth(), Davy->GetMaxHealth(), Live.Num());
	if (Live.Num() == 0)
	{
		// plus de Galion : il reste sur les ponts
		Davy->SetScriptedHold(false);
		Davy->SetScriptedInvulnerable(false);
		SetState(EYSDavyJonesState::Fighting);
		return;
	}
	DavyGalion = Live[FMath::RandRange(0, Live.Num() - 1)];
	FVector Top;
	if (!DavyGalion->GetPlatformTop(Top))
	{
		Top = DavyGalion->GetActorLocation() + FVector(0.0f, 0.0f, 250.0f);
	}
	StartJump(Top, true);
}

void AYSDavyJonesArena::StartJump(const FVector& TargetFeet, bool bToPlatform)
{
	Davy->SetScriptedHold(true);
	Davy->SetScriptedInvulnerable(true);
	bJumpToPlatform = bToPlatform;
	const float HalfHeight = Davy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	JumpTarget = TargetFeet + FVector(0.0f, 0.0f, HalfHeight + YSDavyJones::SpawnLift);
	// vol balistique en Jump Duration : z(t) = z0 + vz t + g t² / 2
	UCharacterMovementComponent* Movement = Davy->GetCharacterMovement();
	const float Gravity = Movement->GetGravityZ();
	const FVector Start = Davy->GetActorLocation();
	const float T = FMath::Max(JumpDuration, 0.2f);
	FVector Velocity = (JumpTarget - Start) / T;
	Velocity.Z = (JumpTarget.Z - Start.Z) / T - 0.5f * Gravity * T;
	Movement->StopMovementImmediately();
	Davy->LaunchCharacter(Velocity, true, true);
	JumpEndTime = GetWorld()->GetTimeSeconds() + T + YSDavyJones::JumpLandingGrace;
	SetState(EYSDavyJonesState::Jumping);
	UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] Davy Jones saute vers %s (%s)"), *TargetFeet.ToCompactString(), bToPlatform ? *FString::Printf(TEXT("plateforme de %s"), *GetNameSafe(DavyGalion)) : TEXT("pont des joueurs"));
}

void AYSDavyJonesArena::FinishJump()
{
	// posé exactement (un mât ou une voile a pu dévier le vol)
	if (FVector::Dist(Davy->GetActorLocation(), JumpTarget) > 150.0f)
	{
		Davy->TeleportTo(JumpTarget, Davy->GetActorRotation());
	}
	Davy->GetCharacterMovement()->StopMovementImmediately();
	if (bJumpToPlatform)
	{
		BeginSummon();
		return;
	}
	// sur le pont : il se bat, les PV ne passent pas sous le seuil de la phase suivante
	Davy->SetScriptedHold(false);
	Davy->SetScriptedInvulnerable(false);
	Davy->SetScriptedHealthFloor(PhaseHealthThresholds.IsValidIndex(Phase) ? Davy->GetMaxHealth() * PhaseHealthThresholds[Phase] : 0.0f);
	SetState(EYSDavyJonesState::Fighting);
	UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] Phase %d : Davy Jones se bat (%.0f / %.0f PV, prochain seuil %s)"), Phase + 1, Davy->GetHealth(), Davy->GetMaxHealth(),
		PhaseHealthThresholds.IsValidIndex(Phase) ? *FString::Printf(TEXT("%.0f %%"), PhaseHealthThresholds[Phase] * 100.0f) : TEXT("aucun"));
}

FVector AYSDavyJonesArena::FindDescendPoint() const
{
	if (!IsValid(PlayerBoat))
	{
		return GetActorLocation();
	}
	// bord de la coque le plus proche de Davy Jones, rentré de Descend Inset ; hauteur du pont sous ce point
	const FVector From = IsValid(Davy) ? Davy->GetActorLocation() : PlayerBoat->GetHullCenter();
	FVector Point = PlayerBoat->GetClosestHullPoint(From, DescendInset);
	if (!PlayerBoat->IsInsideHull(Point))
	{
		Point = PlayerBoat->GetHullCenter();
	}
	const float Top = PlayerBoat->GetActorLocation().Z + 1500.0f;
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSDavyJonesDeck), false);
	if (IsValid(Davy))
	{
		Params.AddIgnoredActor(Davy);
	}
	GetWorld()->LineTraceMultiByObjectType(Hits, FVector(Point.X, Point.Y, Top), FVector(Point.X, Point.Y, PlayerBoat->GetActorLocation().Z - 300.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params);
	for (const FHitResult& Hit : Hits)
	{
		if (AYSPirateShip::GetShipOf(Hit.GetActor()) == PlayerBoat)
		{
			return Hit.ImpactPoint;
		}
	}
	return FVector(Point.X, Point.Y, PlayerBoat->GetActorLocation().Z + 210.0f);
}

void AYSDavyJonesArena::Victory()
{
	UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] DAVY JONES VAINCU"));
	SetState(EYSDavyJonesState::Defeated);
	PendingCrew.Reset();
	DavyGalion = nullptr;
	for (AYSFoeGalion* Galion : GetLiveGalions())
	{
		SinkGalion(Galion, TEXT("Davy Jones vaincu"));
	}
	if (IsValid(PlayerBoat))
	{
		PlayerBoat->SetGrappled(false);
		PlayerBoat->SetBossFightLocked(false);
	}
	SpawnLoot();
	if (AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>())
	{
		YSGameState->SetDavyJonesState(false, true);
	}
}

void AYSDavyJonesArena::SpawnLoot()
{
	TArray<TSubclassOf<AYSInteractiveObject>> Loot;
	for (const TSubclassOf<AYSInteractiveObject>& Class : LootClasses)
	{
		if (Class)
		{
			Loot.Add(Class);
		}
	}
	// membres d'équipage tirés au hasard (crewboat), parmi ceux qu'on ne trouve pas ailleurs : ni posés dans les zones d'intérêt, ni déjà en jeu
	AYSFlowMode* FlowMode = GetWorld()->GetAuthGameMode<AYSFlowMode>();
	if (FlowMode)
	{
		TArray<UClass*> InGame;
		for (TActorIterator<AYSBoatCrew> It(GetWorld()); It; ++It)
		{
			InGame.AddUnique(It->GetClass());
		}
		const TArray<TSubclassOf<AYSInteractiveObject>>& ZoneCrew = FlowMode->GetZoneCrewClasses();
		TArray<TSubclassOf<AYSInteractiveObject>> CrewPool = FlowMode->CrewItemClasses;
		CrewPool.RemoveAll([&ZoneCrew, &InGame](const TSubclassOf<AYSInteractiveObject>& Class)
		{
			return !Class || ZoneCrew.Contains(Class) || InGame.ContainsByPredicate([&Class](const UClass* Existing) { return Existing->IsChildOf(Class.Get()); });
		});
		UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] Membres d'équipage possibles pour le butin : %d (sur %d, %d dans les zones d'intérêt, %d déjà en jeu)"), CrewPool.Num(),
			FlowMode->CrewItemClasses.Num(), ZoneCrew.Num(), InGame.Num());
		for (int32 K = CrewPool.Num() - 1; K > 0; --K)
		{
			CrewPool.Swap(K, FMath::RandRange(0, K));
		}
		for (int32 Index = 0; Index < FMath::Min(RandomCrewLootCount, CrewPool.Num()); ++Index)
		{
			Loot.Add(CrewPool[Index]);
		}
	}
	if (!IsValid(PlayerBoat) || Loot.Num() == 0)
	{
		return;
	}
	// à la surface, en cercle autour du bateau des joueurs (à Loot Distance From Hull du bord de sa coque)
	const FVector Center = PlayerBoat->GetHullCenter();
	const float Sea = GetSeaLevel();
	const float StartAngle = FMath::FRandRange(0.0f, 360.0f);
	FString Names;
	for (int32 Index = 0; Index < Loot.Num(); ++Index)
	{
		const FVector Direction = FRotator(0.0f, StartAngle + 360.0f * Index / Loot.Num(), 0.0f).Vector();
		const FVector Edge = PlayerBoat->GetClosestHullPoint(Center + Direction * 100000.0f);
		const FVector Location(Edge.X + Direction.X * LootDistanceFromHull, Edge.Y + Direction.Y * LootDistanceFromHull, Sea);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AYSInteractiveObject* Object = GetWorld()->SpawnActor<AYSInteractiveObject>(Loot[Index], Location, FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), Params))
		{
			// objet sans valeur en doublons (membre d'équipage) : le core du bateau des joueurs le prend comme un objet des zones d'intérêt
			if (FlowMode && Object->GetDoubloonValue() <= 0)
			{
				FlowMode->AddZoneItem(Object);
			}
			Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + Object->GetName();
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] Butin autour de %s : %s"), *PlayerBoat->GetName(), *Names);
}

TArray<AYSFoeGalion*> AYSDavyJonesArena::GetLiveGalions() const
{
	TArray<AYSFoeGalion*> Live;
	for (AYSFoeGalion* Galion : Galions)
	{
		if (IsValid(Galion) && !Galion->IsSinking() && !Galion->IsActorBeingDestroyed())
		{
			Live.Add(Galion);
		}
	}
	return Live;
}

int32 AYSDavyJonesArena::CountPlayersOn(const AYSPirateShip* Ship) const
{
	int32 Count = 0;
	for (TActorIterator<AYSPlayerCharacter> It(GetWorld()); It; ++It)
	{
		Count += It->IsAlive() && It->GetShipUnderFeet() == Ship ? 1 : 0;
	}
	return Count;
}

void AYSDavyJonesArena::SinkGalion(AYSFoeGalion* Galion, const TCHAR* Reason)
{
	if (!IsValid(Galion))
	{
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] %s coule (%s, %d joueur(s) dessus)"), *Galion->GetName(), Reason, CountPlayersOn(Galion));
	Galion->EndScriptedBoarding(Reason);
	Galion->SinkAtSea(Reason);
}

void AYSDavyJonesArena::DebugStartFight()
{
	if (State == EYSDavyJonesState::Waiting)
	{
		StartFight(TEXT("debug"));
	}
}

void AYSDavyJonesArena::DebugSkip()
{
	if (State == EYSDavyJonesState::Crew)
	{
		// paquets à venir annulés
		CrewPlannedCount -= CountPendingCrew();
		PendingCrew.Reset();
		int32 Killed = 0;
		for (const TWeakObjectPtr<AYSFoeCharacter>& Foe : Crew)
		{
			if (Foe.IsValid() && !Foe->IsDying())
			{
				Foe->DebugSetHealth(0.0f);
				++Killed;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] Debug : %d NME de l'équipage tués"), Killed);
		return;
	}
	if (State == EYSDavyJonesState::Fighting && IsDavyAlive())
	{
		// au seuil de la phase suivante, ou tué
		const float Target = PhaseHealthThresholds.IsValidIndex(Phase) ? Davy->GetMaxHealth() * PhaseHealthThresholds[Phase] : 0.0f;
		Davy->DebugSetHealth(Target);
		UE_LOG(LogTemp, Log, TEXT("[YSDavyJones] Debug : Davy Jones à %.0f / %.0f PV"), Davy->GetHealth(), Davy->GetMaxHealth());
	}
}
