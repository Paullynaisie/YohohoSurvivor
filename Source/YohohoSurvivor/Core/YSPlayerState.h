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

	/** Upgrades proposées pendant la sélection en cours (index dans l'UpgradePool du personnage ; vide = pas concerné) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Upgrades")
	TArray<int32> UpgradeOffers;

	/** Upgrade choisie parmi UpgradeOffers (index dans UpgradeOffers, INDEX_NONE = pas encore choisi) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Upgrades")
	int32 ChosenUpgradeOffer = INDEX_NONE;

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	int32 GetSelectedCharacterIndex() const { return SelectedCharacterIndex; }

	bool HasSelectedCharacter() const { return SelectedCharacterIndex != INDEX_NONE; }

	bool IsDead() const { return bIsDead; }

	/** Serveur uniquement */
	void SetSelectedCharacterIndex(int32 Index) { SelectedCharacterIndex = Index; }

	/** Serveur uniquement */
	void SetDead() { bIsDead = true; }

	const TArray<int32>& GetUpgradeOffers() const { return UpgradeOffers; }

	int32 GetChosenUpgradeOffer() const { return ChosenUpgradeOffer; }

	/** Ce joueur doit encore choisir une upgrade */
	bool IsChoosingUpgrade() const { return UpgradeOffers.Num() > 0 && ChosenUpgradeOffer == INDEX_NONE; }

	/** Serveur uniquement : nouvelles propositions (vide = pas concerné par cette sélection) */
	void SetUpgradeOffers(const TArray<int32>& Offers) { UpgradeOffers = Offers; ChosenUpgradeOffer = INDEX_NONE; }

	/** Serveur uniquement */
	void SetChosenUpgradeOffer(int32 OfferIndex) { ChosenUpgradeOffer = OfferIndex; }
};
