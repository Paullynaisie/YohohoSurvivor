#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Upgrades/YSUpgrade.h"
#include "Core/YSMatchStats.h"
#include "YSPlayerState.generated.h"

class AYSPlayerCharacter;

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

	/** Personnage mort gardé (cadavre ou caché) : ses upgrades, repris par une réanimation ; répliqué pour le menu d'upgrade d'un joueur mort */
	UPROPERTY(Replicated)
	TObjectPtr<AYSPlayerCharacter> DeadBody;

	/** Le joueur a cliqué sur « Prêt » (la partie démarre quand tout le monde est prêt) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Character Select")
	bool bReady = false;

	/** Place sur la chaloupe du GameBoat (FlowMod ; INDEX_NONE = aucune) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Character Select")
	int32 BoatSeat = INDEX_NONE;

	/** Upgrades proposées pendant la sélection en cours (index dans l'UpgradePool du personnage + rareté ; vide = pas concerné) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Upgrades")
	TArray<FYSUpgradeOffer> UpgradeOffers;

	/** Upgrade choisie parmi UpgradeOffers (index dans UpgradeOffers, INDEX_NONE = pas encore choisi) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Upgrades")
	int32 ChosenUpgradeOffer = INDEX_NONE;

	/** Relances encore permises pour la sélection en cours (-1 = illimité) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Upgrades")
	int32 UpgradeRerollsLeft = -1;

	/** Personnalisation (écran de sélection) : index dans AYSGameState::CustomizationColors / CustomizationFaces */
	UPROPERTY(ReplicatedUsing=OnRep_Customization, BlueprintReadOnly, Category="Customization")
	int32 ColorIndex = 0;

	UPROPERTY(ReplicatedUsing=OnRep_Customization, BlueprintReadOnly, Category="Customization")
	int32 FaceIndex = 0;

	/** Gaucher (miroir du personnage) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Customization")
	bool bLeftHanded = false;

	/** Statistiques de la partie (serveur ; écran des scores) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Stats")
	FYSMatchStats MatchStats;

	/** Upgrades copiées à la fin de la partie (écran des scores) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Stats")
	FYSScoreUpgrades FinalUpgrades;

	/** Numéro d'arrivée dans la partie (hôte = 1 ; serveur) : nom par défaut « Player N » */
	int32 PlayerNumber = 0;

	/** Applique la personnalisation au personnage du joueur */
	UFUNCTION()
	void OnRep_Customization();

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	int32 GetSelectedCharacterIndex() const { return SelectedCharacterIndex; }

	bool HasSelectedCharacter() const { return SelectedCharacterIndex != INDEX_NONE; }

	/**
	 *  Nom affiché en jeu (HUD, alliés, au-dessus du perso) : nom du personnage puis pseudo, ex. « Mouss Player 1 »
	 *  (personnage contrôlé, sinon celui choisi ; pseudo seul tant qu'aucun n'est choisi)
	 */
	FString GetDisplayName() const;

	bool IsDead() const { return bIsDead; }

	bool IsReady() const { return bReady; }

	/** Serveur uniquement */
	void SetReady(bool bNewReady) { bReady = bNewReady; ForceNetUpdate(); }

	/** Serveur uniquement */
	void SetSelectedCharacterIndex(int32 Index) { SelectedCharacterIndex = Index; }

	int32 GetBoatSeat() const { return BoatSeat; }

	/** Serveur uniquement */
	void SetBoatSeat(int32 Seat) { BoatSeat = Seat; ForceNetUpdate(); }

	/** Serveur uniquement */
	void SetDead() { bIsDead = true; }

	/** Serveur uniquement : ressuscité (Medic, plume de phoenix) */
	void SetAlive() { bIsDead = false; DeadBody = nullptr; ForceNetUpdate(); }

	/** Serveur pour Set : personnage mort qui garde les upgrades du joueur, repris par une réanimation (aussi connu des clients) */
	void SetDeadBody(AYSPlayerCharacter* Body) { DeadBody = Body; ForceNetUpdate(); }
	AYSPlayerCharacter* GetDeadBody() const;

	const TArray<FYSUpgradeOffer>& GetUpgradeOffers() const { return UpgradeOffers; }

	int32 GetChosenUpgradeOffer() const { return ChosenUpgradeOffer; }

	/** Ce joueur doit encore choisir une upgrade */
	bool IsChoosingUpgrade() const { return UpgradeOffers.Num() > 0 && ChosenUpgradeOffer == INDEX_NONE; }

	/** Ce joueur a des propositions (choix modifiable tant que tous les joueurs n'ont pas choisi) */
	bool HasUpgradeOffers() const { return UpgradeOffers.Num() > 0; }

	/** Serveur uniquement : nouvelles propositions (vide = pas concerné par cette sélection) */
	void SetUpgradeOffers(const TArray<FYSUpgradeOffer>& Offers) { UpgradeOffers = Offers; ChosenUpgradeOffer = INDEX_NONE; ForceNetUpdate(); }

	/** Serveur uniquement */
	void SetChosenUpgradeOffer(int32 OfferIndex) { ChosenUpgradeOffer = OfferIndex; ForceNetUpdate(); }

	int32 GetUpgradeRerollsLeft() const { return UpgradeRerollsLeft; }

	/** Serveur uniquement (-1 = illimité) */
	void SetUpgradeRerollsLeft(int32 Count) { UpgradeRerollsLeft = Count; ForceNetUpdate(); }

	int32 GetColorIndex() const { return ColorIndex; }
	int32 GetFaceIndex() const { return FaceIndex; }
	bool IsLeftHanded() const { return bLeftHanded; }
	void SetLeftHanded(bool bNewLeftHanded) { bLeftHanded = bNewLeftHanded; ForceNetUpdate(); }
	int32 GetPlayerNumber() const { return PlayerNumber; }

	/** Serveur uniquement */
	void SetPlayerNumber(int32 Number) { PlayerNumber = Number; }
	void SetCustomization(int32 NewColorIndex, int32 NewFaceIndex);

	const FYSMatchStats& GetMatchStats() const { return MatchStats; }

	/** Serveur uniquement (YSMatchStats) ; répliqué au rythme du PlayerState, envoyé en entier à la fin (CaptureFinalUpgrades) */
	FYSMatchStats& EditMatchStats() { return MatchStats; }

	const FYSScoreUpgrades& GetFinalUpgrades() const { return FinalUpgrades; }

	/** Serveur, fin de partie : upgrades du personnage (vivant, sinon son corps) copiées pour l'écran des scores */
	void CaptureFinalUpgrades();
};
