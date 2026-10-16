#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "YSPlayerState.generated.h"

/**
 *  État d'un joueur partagé avec tout le monde : personnage choisi dans le menu de sélection
 *  (index dans AYSGameState::SelectableCharacters, INDEX_NONE tant qu'aucun choix).
 */
UCLASS()
class AYSPlayerState : public APlayerState
{
	GENERATED_BODY()

protected:

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Character Select")
	int32 SelectedCharacterIndex = INDEX_NONE;

	/** Le personnage du joueur est mort (pas de réapparition) : écran « You're dead », compte toujours pour les vagues */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Health")
	bool bIsDead = false;

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	int32 GetSelectedCharacterIndex() const { return SelectedCharacterIndex; }

	bool HasSelectedCharacter() const { return SelectedCharacterIndex != INDEX_NONE; }

	bool IsDead() const { return bIsDead; }

	/** Serveur uniquement */
	void SetSelectedCharacterIndex(int32 Index) { SelectedCharacterIndex = Index; }

	/** Serveur uniquement */
	void SetDead() { bIsDead = true; }
};
