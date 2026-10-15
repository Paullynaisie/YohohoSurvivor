#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "YSGameMode.generated.h"

class AYSPlayerCharacter;

/**
 *  GameMode du jeu (serveur uniquement), avec sélection du personnage :
 *  les joueurs arrivent sans pawn et choisissent un personnage dans le menu (AYSPlayerController).
 *  Quand tous les joueurs connectés ont choisi, la partie démarre et chacun apparaît avec son personnage.
 *  Un joueur qui arrive en cours de partie apparaît dès qu'il a choisi.
 *  Classe de base de BP_TestGameMode.
 */
UCLASS()
class AYSGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:

	/** Personnages proposés dans le menu de sélection (dans l'ordre des boutons) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Select")
	TArray<TSubclassOf<AYSPlayerCharacter>> SelectableCharacters;

public:

	AYSGameMode();

	virtual void InitGameState() override;

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	virtual void Logout(AController* Exiting) override;

	/** Choix d'un joueur (reçu par RPC du PlayerController) */
	void PlayerSelectedCharacter(APlayerController* Player, int32 CharacterIndex);

protected:

	/** Démarre la partie si tous les joueurs connectés ont choisi */
	void TryStartMatch();

	/** Le joueur a-t-il déjà son personnage ? */
	static bool HasCharacter(const AController* Controller);
};
