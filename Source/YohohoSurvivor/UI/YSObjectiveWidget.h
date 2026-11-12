#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSObjectiveWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 *  Jauge d'objectif (commune à toute l'équipe) : doublons gagnés en revendant des coffres au Brooker, « X / Objectif ».
 *  HUD haut droite, sous l'emplacement de la barre de vie de l'élite. Arbre construit en C++ ; réglages
 *  (placement compris) dans les Class Defaults du Widget Blueprint enfant WBP_Objective (catégorie Layout),
 *  choisi par BP_YSPlayerController (ObjectiveWidgetClass).
 */
UCLASS()
class UYSObjectiveWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Distance au coin haut droit de l'écran (X vers la gauche, Y vers le bas) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FVector2D ScreenOffset = FVector2D(24.0f, 130.0f);

	/** Titre au-dessus de la jauge */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FText Title = NSLOCTEXT("YS", "ObjectiveTitle", "Doublons");

	/** Titre une fois le bateau arrivé ({0} = niveau du bateau) : les doublons sont l'XP du bateau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FText ShipLevelTitleFormat = NSLOCTEXT("YS", "ObjectiveShipLevelTitle", "Doublons - Bateau niv. {0}");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout", meta=(ClampMin="1"))
	int32 TitleFontSize = 18;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout", meta=(ClampMin="1"))
	int32 ValueFontSize = 14;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FVector2D BarSize = FVector2D(320.0f, 22.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor BarColor = FLinearColor(1.0f, 0.72f, 0.08f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor BarBackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);

	/** Couleur de la jauge une fois l'objectif atteint */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor CompletedBarColor = FLinearColor(0.2f, 0.9f, 0.3f);

	/** Espace entre le titre et la jauge */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	float TitleSpacing = 2.0f;

	/** Stock de plumes de phoenix, sous la jauge ({0} = nombre) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Phoenix")
	FText FeatherFormat = NSLOCTEXT("YS", "ObjectiveFeathers", "Plumes de phoenix : {0}");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Phoenix", meta=(ClampMin="1"))
	int32 FeatherFontSize = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Phoenix")
	FLinearColor FeatherColor = FLinearColor(1.0f, 0.55f, 0.15f);

	/** Espace entre la jauge et le stock de plumes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Phoenix")
	float FeatherSpacing = 4.0f;

	/** Stock de CoreBoats (partie 2 du flow), sous les plumes : {0} = stock, {1} = CoreBoats pour l'étape suivante du bateau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|CoreBoat")
	FText CoreBoatFormat = NSLOCTEXT("YS", "ObjectiveCoreBoats", "CoreBoat : {0} / {1}");

	/** Sans étape suivante (bateau au maximum) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|CoreBoat")
	FText CoreBoatMaxFormat = NSLOCTEXT("YS", "ObjectiveCoreBoatsMax", "CoreBoat : {0}");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|CoreBoat", meta=(ClampMin="1"))
	int32 CoreBoatFontSize = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|CoreBoat")
	FLinearColor CoreBoatColor = FLinearColor(0.45f, 0.85f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|CoreBoat")
	float CoreBoatSpacing = 2.0f;

	/** PV du bateau des joueurs, au-dessus des doublons ({0} = PV, {1} = PV max) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Ship Health")
	FText ShipHealthFormat = NSLOCTEXT("YS", "ObjectiveShipHealth", "Bateau : {0} / {1}");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Ship Health", meta=(ClampMin="1"))
	int32 ShipHealthFontSize = 14;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Ship Health")
	FVector2D ShipHealthBarSize = FVector2D(320.0f, 22.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Ship Health")
	FLinearColor ShipHealthColor = FLinearColor(0.85f, 0.15f, 0.12f);

	/** PV négatifs (bateau hors d'usage) : barre de cette couleur, remplie d'autant */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Ship Health")
	FLinearColor ShipNegativeHealthColor = FLinearColor(0.35f, 0.05f, 0.4f);

	/** Espace entre la jauge des PV et le titre des doublons */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Ship Health")
	float ShipHealthSpacing = 8.0f;

	virtual bool Initialize() override;

	/** PV du bateau des joueurs (bVisible faux = cachée : pas de bateau) */
	void SetShipHealth(bool bVisible, float Health, float MaxHealth);

	void SetProgress(int32 Doubloons, int32 Goal);

	void SetPhoenixFeathers(int32 Feathers);

	/** bVisible faux = ligne cachée ; NextCost <= 0 = pas d'étape suivante */
	void SetCoreBoats(bool bVisible, int32 Stock, int32 NextCost);

	/** Niveau du bateau (0 = pas encore arrivé : titre Title) */
	void SetShipLevel(int32 Level);

private:

	void BuildWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ValueLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FeatherLabel;

	int32 DisplayedFeathers = TNumericLimits<int32>::Min();

	UPROPERTY(Transient)
	TObjectPtr<class USizeBox> ShipHealthBox;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ShipHealthBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ShipHealthLabel;

	int32 DisplayedShipHealth = MIN_int32;
	int32 DisplayedShipMaxHealth = MIN_int32;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CoreBoatLabel;

	int32 DisplayedCoreBoats = INDEX_NONE;
	int32 DisplayedCoreBoatCost = INDEX_NONE;

	int32 DisplayedShipLevel = INDEX_NONE;

	int32 DisplayedDoubloons = INDEX_NONE;
	int32 DisplayedGoal = INDEX_NONE;
};
