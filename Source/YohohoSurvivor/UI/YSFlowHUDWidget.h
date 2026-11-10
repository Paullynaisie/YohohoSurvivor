#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/YSGameState.h"
#include "YSFlowHUDWidget.generated.h"

class UProgressBar;
class USizeBox;
class UTextBlock;
class UVerticalBox;

/** Objectif en mer à afficher à la place de celui de l'île (calculé par le PlayerController pour son joueur) */
struct FYSFlowSeaObjective
{
	/** Le joueur n'est pas sur l'île qui affiche son objectif (ou aucune île n'en affiche) */
	bool bShow = false;
	/** Une bataille navale est en cours : « Coule le Galion » */
	bool bNavalBattle = false;
	int32 CompassesRemaining = 0;
	int32 CompassTotal = 0;
	/** Partie 3 : combat contre le Kraken */
	bool bPart3 = false;
};

/**
 *  HUD du flow (plein écran, ne capte pas la souris) : chronomètre du temps de partie tout en haut à droite ; en haut au milieu (symétrique des
 *  icônes d'actions) le texte d'objectif, dessous la barre du temps restant de la phase (« OVERTIME » qui clignote à sa place quand la nuit est
 *  écoulée) et, pendant l'évacuation, le compteur de joueurs à bord.
 *  Arbre construit en C++ ; réglages (catégorie Layout) dans le Widget Blueprint enfant WBP_FlowHUD, choisi par AYSPlayerController::FlowHUDClass.
 */
UCLASS(meta=(PrioritizeCategories="Layout"))
class UYSFlowHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Distance au coin haut droit (X vers la gauche, Y vers le bas) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Chrono")
	FVector2D ChronoMargin = FVector2D(24.0f, 16.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Chrono", meta=(ClampMin="1"))
	int32 ChronoFontSize = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Chrono")
	FLinearColor ChronoColor = FLinearColor::White;

	/** Distance du texte d'objectif au haut de l'écran (même valeur que la marge du bas des icônes d'actions) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Objective")
	float ObjectiveTopMargin = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Objective", meta=(ClampMin="1"))
	int32 ObjectiveFontSize = 28;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Objective")
	FLinearColor ObjectiveColor = FLinearColor(1.0f, 0.92f, 0.7f);

	/** Texte de victoire (partie 2 du flow) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Objective", meta=(ClampMin="1"))
	int32 VictoryFontSize = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Objective")
	FLinearColor VictoryColor = FLinearColor(1.0f, 0.8f, 0.1f);

	/** Barre du temps restant de la phase, sous le texte */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Phase Bar")
	FVector2D BarSize = FVector2D(420.0f, 18.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Phase Bar")
	float BarSpacing = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Phase Bar")
	FLinearColor DayBarColor = FLinearColor(1.0f, 0.78f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Phase Bar")
	FLinearColor NightBarColor = FLinearColor(0.35f, 0.5f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Phase Bar")
	FLinearColor EvacuationBarColor = FLinearColor(0.95f, 0.35f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Phase Bar")
	FLinearColor BarBackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.75f);

	/** Secondes restantes écrites dans la barre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Phase Bar")
	bool bShowRemainingTime = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Phase Bar", meta=(ClampMin="1"))
	int32 RemainingFontSize = 12;

	/** À la place de la barre quand la nuit est écoulée */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Overtime")
	FText OvertimeText = NSLOCTEXT("YS", "FlowOvertime", "OVERTIME");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Overtime", meta=(ClampMin="1"))
	int32 OvertimeFontSize = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Overtime")
	FLinearColor OvertimeColor = FLinearColor(1.0f, 0.15f, 0.1f);

	/** Clignotements par seconde */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Overtime", meta=(ClampMin="0.1"))
	float OvertimeBlinkFrequency = 2.0f;

	/** Part de chaque clignotement où le texte est visible */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Overtime", meta=(ClampMin="0.05", ClampMax="1"))
	float OvertimeVisibleRatio = 0.6f;

	/** Évacuation ({0} = joueurs à bord, {1} = joueurs vivants) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Boarding")
	FText BoardingFormat = NSLOCTEXT("YS", "FlowBoarding", "Joueurs sur le bateau : {0} / {1}");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Boarding", meta=(ClampMin="1"))
	int32 BoardingFontSize = 22;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Boarding")
	FLinearColor BoardingColor = FLinearColor(1.0f, 0.85f, 0.3f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Boarding")
	float BoardingSpacing = 6.0f;

	/** Partie 2 : grand timer sous l'objectif de l'île ({0} = temps restant avant l'overtime) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Part 2")
	FText Part2Format = NSLOCTEXT("YS", "FlowPart2Timer", "{0}");

	/** À la place du timer une fois le temps de la partie 2 écoulé (clignote comme l'overtime de la nuit) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Part 2")
	FText Part2OvertimeText = NSLOCTEXT("YS", "FlowPart2Overtime", "OVERTIME");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Part 2", meta=(ClampMin="1"))
	int32 Part2FontSize = 48;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Part 2")
	FLinearColor Part2Color = FLinearColor::White;

	/** Écart avec l'objectif de l'île au-dessus */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Part 2")
	float Part2Spacing = 10.0f;

	/** En mer : boussoles à trouver ({0} = boussoles restantes) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Sea")
	FText CompassObjectiveFormat = NSLOCTEXT("YS", "FlowSeaCompasses", "Trouve les {0} boussoles");

	/** En mer, une seule boussole restante */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Sea")
	FText CompassLastObjectiveText = NSLOCTEXT("YS", "FlowSeaLastCompass", "Trouve la dernière boussole");

	/** En mer, toutes les boussoles trouvées (pointillés vers la zone du Kraken) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Sea")
	FText CompassDoneText = NSLOCTEXT("YS", "FlowSeaFindCreature", "Trouvez la créature");

	/** Bataille navale en cours, jusqu'à ce que son Galion soit coulé */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Sea")
	FText NavalBattleObjectiveText = NSLOCTEXT("YS", "FlowSeaNavalBattle", "Coule le Galion");

	/** Partie 3 : combat contre le Kraken (pas de timer) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Sea")
	FText KrakenObjectiveText = NSLOCTEXT("YS", "FlowKraken", "Affrontez le Kraken");

	virtual bool Initialize() override;

	/**
	 *  Chaque image : état du flow, temps restant de la phase, temps de partie (chrono caché avant le début de la partie),
	 *  partie 2 (temps écoulé, -1 = pas commencée ; durée avant l'overtime), objectif en mer (remplace celui de l'île quand bShow)
	 */
	void Refresh(const FYSFlowStatus& InStatus, float PhaseRemaining, float MatchTime, bool bMatchStarted, float Part2Elapsed, float Part2Duration,
		const FYSFlowSeaObjective& Sea);

private:

	void BuildWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ChronoLabel;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ObjectiveColumn;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ObjectiveLabel;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> BarBox;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RemainingLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OvertimeLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BoardingLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Part2Label;

	int32 DisplayedPart2Seconds = TNumericLimits<int32>::Min();

	int32 DisplayedChronoSeconds = INDEX_NONE;
	int32 DisplayedRemainingSeconds = INDEX_NONE;
	EYSFlowObjective DisplayedObjective = EYSFlowObjective::None;
	FString DisplayedObjectiveText;
	int32 DisplayedBoarded = INDEX_NONE;
	int32 DisplayedBoardingTotal = INDEX_NONE;
};
