#include "Core/YSGameMode.h"
#include "Core/YSGameState.h"
#include "Core/YSPlayerState.h"
#include "Enemies/YSEnemySpawnRule.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Player/YSPlayerCharacter.h"
#include "Player/YSPlayerController.h"

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
	YSGameState->SetUpgradeSelectionActive(true);

	// propositions propres à chaque joueur vivant (selon son personnage) ; les autres ne sont pas attendus
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Player = It->Get();
		AYSPlayerState* PlayerState = Player ? Player->GetPlayerState<AYSPlayerState>() : nullptr;
		const AYSPlayerCharacter* Character = Player ? Cast<AYSPlayerCharacter>(Player->GetPawn()) : nullptr;
		if (PlayerState)
		{
			PlayerState->SetUpgradeOffers(Character && Character->IsAlive() ? Character->PickUpgradeOffers(UpgradeChoiceCount) : TArray<int32>());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Niveau %d : choix d'upgrade (%d niveau(x) en attente)"), YSGameState->GetTeamLevel(), PendingLevelUps);

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
	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] %s choisit l'upgrade %d (pool %d)"), *PlayerState->GetPlayerName(), OfferIndex, PlayerState->GetUpgradeOffers()[OfferIndex]);
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
			YSPlayer->SetUpgradeOffers(TArray<int32>());
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
	// pas de pawn tant que le joueur n'a pas choisi (et que la partie n'a pas démarré) : il voit le menu
	const AYSPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AYSPlayerState>() : nullptr;
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (PlayerState && PlayerState->HasSelectedCharacter() && YSGameState && YSGameState->IsMatchStarted())
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

	PlayerState->SetSelectedCharacterIndex(CharacterIndex);

	if (YSGameState->IsMatchStarted())
	{
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
		if (!YSPlayer || !YSPlayer->HasSelectedCharacter())
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
