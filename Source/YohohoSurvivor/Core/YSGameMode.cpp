#include "Core/YSGameMode.h"
#include "Core/YSGameState.h"
#include "Core/YSPlayerState.h"
#include "Enemies/YSEnemySpawnRule.h"
#include "Enemies/YSFoeCharacter.h"
#include "Interaction/YSTreasureMap.h"
#include "Interaction/YSInteractiveObject.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Player/YSPlayerCharacter.h"
#include "Player/YSPlayerController.h"
#include "Weapons/YSWeapon.h"

AYSGameMode::AYSGameMode()
{
	GameStateClass = AYSGameState::StaticClass();
	PlayerStateClass = AYSPlayerState::StaticClass();
	PlayerControllerClass = AYSPlayerController::StaticClass();

	PrimaryActorTick.bCanEverTick = true;

	SpawnRules.Add(CreateDefaultSubobject<UYSTimedWaveSpawnRule>(TEXT("TimedWaveSpawnRule")));
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
	if (bDayNightCycle && !YSGameState->IsNight() && YSGameState->GetDayElapsedTime() >= DayDuration)
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
}

void AYSGameMode::InitGameState()
{
	Super::InitGameState();

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

	// la liste est répliquée par le GameState pour que les clients puissent construire leurs boutons
	if (AYSGameState* YSGameState = GetGameState<AYSGameState>())
	{
		YSGameState->SetSelectableCharacters(SelectableCharacters);
		YSGameState->SetCustomizationOptions(CustomizationColors, CustomizationFaces);
		YSGameState->SetTeamProgress(0, 0.0f, GetExperienceForLevel(0));
		YSGameState->SetDoubloons(0, DoubloonGoal);
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
	if (!YSGameState || PendingLevelUps <= 0)
	{
		return;
	}

	--PendingLevelUps;
	// niveau atteint par cette sélection (plusieurs niveaux gagnés d'un coup : sélections enchaînées)
	const int32 SelectionLevel = YSGameState->GetTeamLevel() - PendingLevelUps;
	const bool bClassSelection = SelectionLevel == ClassUpgradeLevel;
	YSGameState->SetUpgradeSelectionLevel(SelectionLevel);
	YSGameState->SetUpgradeSelectionActive(true);

	// propositions propres à chaque joueur vivant (selon son personnage) ; les autres ne sont pas attendus
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Player = It->Get();
		AYSPlayerState* PlayerState = Player ? Player->GetPlayerState<AYSPlayerState>() : nullptr;
		const AYSPlayerCharacter* Character = Player ? Cast<AYSPlayerCharacter>(Player->GetPawn()) : nullptr;
		if (PlayerState)
		{
			// niveau de classe : choix de classe (si le personnage en a), sinon upgrades au hasard
			TArray<FYSUpgradeOffer> Offers;
			if (Character && Character->IsAlive())
			{
				Offers = bClassSelection ? Character->PickClassOffers() : TArray<FYSUpgradeOffer>();
				if (Offers.Num() == 0)
				{
					Offers = Character->PickUpgradeOffers(UpgradeChoiceCount);
				}
			}
			PlayerState->SetUpgradeOffers(Offers);
			UE_LOG(LogTemp, Log, TEXT("[YSGameMode] %s (%s, pool de %d, armes G %s / D %s) : %d proposition(s)"), *PlayerState->GetPlayerName(),
				Character ? *Character->GetClass()->GetName() : TEXT("pas de perso"), Character ? Character->GetUpgradePool().Num() : 0,
				Character && Character->GetLeftHandWeapon() ? *Character->GetLeftHandWeapon()->GetName() : TEXT("aucune"),
				Character && Character->GetRightHandWeapon() ? *Character->GetRightHandWeapon()->GetName() : TEXT("aucune"),
				PlayerState->GetUpgradeOffers().Num());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Niveau %d : choix %s (%d niveau(x) en attente)"), SelectionLevel, bClassSelection ? TEXT("de classe") : TEXT("d'upgrade"), PendingLevelUps);

	// tout le monde s'arrête pendant le choix (pause répliquée aux clients)
	if (APlayerController* FirstPlayer = GetWorld()->GetFirstPlayerController())
	{
		SetPause(FirstPlayer);
		// la pause vit sur le WorldSettings (Pauser répliqué) : envoi immédiat, le temps du serveur étant figé
		GetWorldSettings()->ForceNetUpdate();
	}

	TryFinishUpgradeSelection();
}

void AYSGameMode::PlayerChoseUpgrade(APlayerController* Player, int32 OfferIndex)
{
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	AYSPlayerState* PlayerState = Player ? Player->GetPlayerState<AYSPlayerState>() : nullptr;
	AYSPlayerCharacter* Character = Player ? Cast<AYSPlayerCharacter>(Player->GetPawn()) : nullptr;
	if (!YSGameState || !YSGameState->IsUpgradeSelectionActive() || !PlayerState || !Character
		|| !PlayerState->IsChoosingUpgrade() || !PlayerState->GetUpgradeOffers().IsValidIndex(OfferIndex))
	{
		return;
	}

	Character->ApplyUpgrade(PlayerState->GetUpgradeOffers()[OfferIndex]);
	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] %s choisit l'upgrade %d (pool %d, variante %d, rareté %d)"), *PlayerState->GetPlayerName(), OfferIndex, PlayerState->GetUpgradeOffers()[OfferIndex].PoolIndex, PlayerState->GetUpgradeOffers()[OfferIndex].Variant, (int32)PlayerState->GetUpgradeOffers()[OfferIndex].Rarity);
	PlayerState->SetChosenUpgradeOffer(OfferIndex);
	TryFinishUpgradeSelection();
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

	// tout le monde a choisi : on enchaîne s'il reste des niveaux gagnés, sinon la partie reprend
	YSGameState->SetUpgradeSelectionActive(false);
	for (APlayerState* Player : YSGameState->PlayerArray)
	{
		if (AYSPlayerState* YSPlayer = Cast<AYSPlayerState>(Player))
		{
			YSPlayer->SetUpgradeOffers(TArray<FYSUpgradeOffer>());
		}
	}

	if (PendingLevelUps > 0)
	{
		StartUpgradeSelection();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Tous les joueurs ont choisi : la partie reprend"));
		ClearPause();
		GetWorldSettings()->ForceNetUpdate();
	}
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
			ChangeName(NewPlayer, FString::Printf(TEXT("Player %d"), Number), false);
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
	SpawnTreasureMap();
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

	YSGameState->StartDay(YSGameState->GetDayNumber() + 1);
	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Élite %s tué : jour %d"), Elite ? *Elite->GetName() : TEXT("?"), YSGameState->GetDayNumber());
	// nouveau jour : nouvelle carte au trésor
	SpawnTreasureMap();
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
	APawn* Pawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo);
	if (!Pawn)
	{
		return nullptr;
	}

	// gaucher : à connaître avant la construction (OnConstruction crée les armes des mains)
	const AYSPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AYSPlayerState>() : nullptr;
	if (AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(Pawn))
	{
		PlayerCharacter->SetLeftHanded(PlayerState && PlayerState->IsLeftHanded());
	}
	Pawn->FinishSpawning(SpawnTransform);
	return Pawn;
}

void AYSGameMode::AddDoubloons(int32 Amount)
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || Amount <= 0)
	{
		return;
	}

	const bool bWasReached = YSGameState->GetDoubloons() >= YSGameState->GetDoubloonGoal();
	YSGameState->SetDoubloons(YSGameState->GetDoubloons() + Amount, DoubloonGoal);
	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Doublons +%d -> %d / %d"), Amount, YSGameState->GetDoubloons(), DoubloonGoal);
	if (!bWasReached && YSGameState->GetDoubloons() >= DoubloonGoal)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Objectif atteint : %d doublons"), YSGameState->GetDoubloons());
	}
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
			const int32 Count = EnemiesPerPlayer * Rule->GetPlayersInMatchCount();
			const int32 Spawned = Rule->SpawnEnemies(Count);
			UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Vague bonus (%s) : %d / %d NME"), Reason, Spawned, Count);
			return Spawned;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[YSGameMode] Vague bonus (%s) : aucune règle de spawn avec des NME"), Reason);
	return 0;
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
