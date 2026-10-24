#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSShipBoardingWidget.generated.h"

class UTextBlock;
class UWidget;

/**
 *  Fin de jeu temporaire (plein écran, ne capte pas la souris) : « X/Y sur le bateau » en bas de l'écran pour tout le monde
 *  dès qu'un joueur est à bord, puis « BRAVO » au centre quand ils y sont tous (retour au menu : AYSPlayerController).
 *  Arbre construit en C++ ; placements réglables (catégorie Layout) dans un Widget Blueprint enfant (WBP_ShipBoarding,
 *  Class Defaults) choisi par AYSPlayerController::ShipBoardingWidgetClass.
 */
UCLASS(meta=(PrioritizeCategories="Layout"))
class UYSShipBoardingWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Texte du bas ({0} = joueurs à bord, {1} = joueurs vivants) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Boarding")
	FText BoardingFormat = NSLOCTEXT("YS", "ShipBoarding", "{0}/{1} sur le bateau");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Boarding")
	int32 BoardingFontSize = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Boarding")
	FLinearColor BoardingColor = FLinearColor(1.0f, 0.85f, 0.3f);

	/** Distance au bas de l'écran (au-dessus de l'icône d'Ability) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Boarding")
	float BoardingBottomOffset = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Victory")
	FText VictoryText = NSLOCTEXT("YS", "ShipVictory", "BRAVO");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Victory")
	int32 VictoryFontSize = 120;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Victory")
	FLinearColor VictoryColor = FLinearColor(1.0f, 0.8f, 0.1f);

	/** Voile derrière « BRAVO » */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Victory")
	FLinearColor VictoryBackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);

	virtual bool Initialize() override;

	/** Joueurs à bord / vivants ; caché si personne n'est à bord */
	void SetBoarding(int32 Aboard, int32 Total);

	/** Écran « BRAVO » */
	void SetVictory(bool bVictory);

private:

	void BuildWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BoardingText;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> VictoryPanel;
};
