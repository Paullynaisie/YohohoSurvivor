#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Upgrades/YSUpgrade.h"
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

	/** Upgrades proposées pendant la sélection en cours (index dans l'UpgradePool du personnage + rareté ; vide = pas concerné) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Upgrades")
	TArray<FYSUpgradeOffer> UpgradeOffers;

	/** Upgrade choisie parmi UpgradeOffers (index dans UpgradeOffers, INDEX_NONE = pas encore choisi) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Upgrades")
	int32 ChosenUpgradeOffer = INDEX_NONE;

	/** Personnalisation (écran de sélection) : index dans AYSGameState::CustomizationColors / CustomizationFaces */
	UPROPERTY(ReplicatedUsing=OnRep_Customization, BlueprintReadOnly, Category="Customization")
	int32 ColorIndex = 0;

	UPROPERTY(ReplicatedUsing=OnRep_Customization, BlueprintReadOnly, Category="Customization")
	int32 FaceIndex = 0;

	/** Numéro d'arrivée dans la partie (hôte = 1 ; serveur) : nom par défaut « Player N » */
	int32 PlayerNumber = 0;

	/** Applique la personnalisation au personnage du joueur */
	UFUNCTION()
	void OnRep_Customization();

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	int32 GetSelectedCharacterIndex() const { return SelectedCharacterIndex; }

	bool HasSelectedCharacter() const { return SelectedCharacterIndex != INDEX_NONE; }

	bool IsDead() const { return bIsDead; }

	/** Serveur uniquement */
	void SetSelectedCharacterIndex(int32 Index) { SelectedCharacterIndex = Index; }

	/** Serveur uniquement */
	void SetDead() { bIsDead = true; }

	const TArray<FYSUpgradeOffer>& GetUpgradeOffers() const { return UpgradeOffers; }

	int32 GetChosenUpgradeOffer() const { return ChosenUpgradeOffer; }

	/** Ce joueur doit encore choisir une upgrade */
	bool IsChoosingUpgrade() const { return UpgradeOffers.Num() > 0 && ChosenUpgradeOffer == INDEX_NONE; }

	/** Serveur uniquement : nouvelles propositions (vide = pas concerné par cette sélection) */
	void SetUpgradeOffers(const TArray<FYSUpgradeOffer>& Offers) { UpgradeOffers = Offers; ChosenUpgradeOffer = INDEX_NONE; }

	/** Serveur uniquement */
	void SetChosenUpgradeOffer(int32 OfferIndex) { ChosenUpgradeOffer = OfferIndex; }

	int32 GetColorIndex() const { return ColorIndex; }
	int32 GetFaceIndex() const { return FaceIndex; }
	int32 GetPlayerNumber() const { return PlayerNumber; }

	/** Serveur uniquement */
	void SetPlayerNumber(int32 Number) { PlayerNumber = Number; }
	void SetCustomization(int32 NewColorIndex, int32 NewFaceIndex);
};
