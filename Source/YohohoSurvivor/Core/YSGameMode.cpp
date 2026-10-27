#include "Core/YSGameMode.h"
#include "GameFramework/GameSession.h"
#include "EngineUtils.h"
#include "Core/YSGameState.h"
#include "Core/YSPlayerState.h"
#include "Enemies/YSEnemySpawnRule.h"
#include "Enemies/YSFoeCharacter.h"
#include "Interaction/YSTreasureMap.h"
#include "Interaction/YSInteractiveObject.h"
#include "Interaction/YSPlayerCorpse.h"
#include "LandscapeProxy.h"
#include "Map/YSOcean.h"
#include "Ship/YSPirateShip.h"
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
	SecondWave.Delay = 30.0f;
	SecondWave.LevelOffset = 5;
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

	TickOvertime();
	TickShipBoarding();
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
	const int32 Level = YSGameState->GetDayNumber() + Wave.LevelOffset;
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
		AYSPlayerCharacter* Character = Player ? Cast<AYSPlayerCharacter>(Player->GetPawn()) : nullptr;
		if (Character && bRestoreOnLevelUp)
		{
			Character->RestoreOnLevelUp();
		}
		if (PlayerState)
		{
			// niveau de classe : choix de classe (si le personnage en a), sinon upgrades au hasard
			TArray<FYSUpgradeOffer> Offers;
			if (Character && Character->IsAlive())
			{
				Offers = bClassSelection ? Character->PickClassOffers() : TArray<FYSUpgradeOffer>();
				if (Offers.Num() == 0)
				{
					Offers = PickRandomUpgradeOffers(Character, UpgradeChoiceCount);
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
	return YSShardUpgrades::PickOffers(Character, CharaUpgradesTable, Replacements, Count);
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
		|| !PlayerState->HasUpgradeOffers())
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

	const TArray<FYSUpgradeOffer> Offers = PickRandomUpgradeOffers(Character, UpgradeChoiceCount);
	if (Offers.Num() > 0)
	{
		PlayerState->SetUpgradeOffers(Offers);
		UE_LOG(LogTemp, Log, TEXT("[YSGameMode] %s relance ses upgrades : %d proposition(s)"), *PlayerState->GetPlayerName(), Offers.Num());
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

	YSGameState->StartDay(YSGameState->GetDayNumber() + 1);
	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Élite %s tué : jour %d"), Elite ? *Elite->GetName() : TEXT("?"), YSGameState->GetDayNumber());
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
	}
	YSGameState->SetDoubloons(Total, GetDoubloonGoalForShipLevel(Level));
	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Doublons +%d -> %d / %d (bateau niveau %d)"), Amount, Total, GetDoubloonGoalForShipLevel(Level), Level);
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
	return Foe;
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
		if (Hit.GetActor() && Hit.GetActor()->IsA<ALandscapeProxy>())
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
