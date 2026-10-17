#include "Core/YSGameMode.h"
#include "Core/YSGameState.h"
#include "Core/YSPlayerState.h"
#include "Enemies/YSEnemySpawnRule.h"
#include "Enemies/YSFoeCharacter.h"
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
	if (!YSGameState->IsNight() && YSGameState->GetDayElapsedTime() >= DayDuration)
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

	// la liste est répliquée par le GameState pour que les clients puissent construire leurs boutons
	if (AYSGameState* YSGameState = GetGameState<AYSGameState>())
	{
		YSGameState->SetSelectableCharacters(SelectableCharacters);
		YSGameState->SetCustomizationOptions(CustomizationColors, CustomizationFaces);
		YSGameState->SetTeamProgress(0, 0.0f, GetExperienceForLevel(0));
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
	if (!YSGameState || !YSGameState->IsMatchStarted() || YSGameState->IsNight())
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
