#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSOverheatWidget.generated.h"

class UProgressBar;
class USizeBox;
class UTextBlock;

/**
 *  Jauge de chaleur de l'Overheat (Golden du BeamShot) : même placement que les BL stacks du Bloodlust (en bas de l'écran, à droite
 *  de l'icône d'Ability). Barre de chaleur (0 à 100 %) dont la couleur change : normale, critique (à partir du seuil des critiques),
 *  surchauffe (clignote jusqu'au refroidissement) ; le pourcentage écrit à côté. Cachée sans la Golden.
 *  Arbre construit en C++ ; réglages (placement compris) dans les Class Defaults du Widget Blueprint enfant WBP_OverheatGauge
 *  (catégorie Layout), choisi par BP_YSPlayerController.
 */
UCLASS()
class UYSOverheatWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Position : X = distance entre le centre de l'écran et le bord gauche du widget, Y = distance au bas de l'écran */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FVector2D ScreenOffset = FVector2D(176.0f, 40.0f);

	/** Taille de la barre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FVector2D BarSize = FVector2D(140.0f, 18.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor HeatColor = FLinearColor(1.0f, 0.55f, 0.1f);

	/** Couleur quand les tirs sont critiques */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor CriticalColor = FLinearColor(1.0f, 0.85f, 0.1f);

	/** Couleur de la surchauffe (clignote) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor OverheatColor = FLinearColor(1.0f, 0.05f, 0.02f);

	/** Clignotements par seconde pendant la surchauffe (0 = fixe) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout", meta=(ClampMin="0"))
	float OverheatBlinkRate = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.7f);

	/** Pourcentage écrit à droite de la barre : taille du texte, espace */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	bool bShowPercent = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout", meta=(ClampMin="1"))
	int32 PercentFontSize = 14;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout", meta=(ClampMin="0"))
	float PercentSpacing = 6.0f;

	virtual bool Initialize() override;

	/** Golden prise ? (sinon caché), chaleur 0..1, tirs critiques ?, surchauffe ? */
	void Refresh(bool bEnabled, float Heat, bool bCritical, bool bOverheated);

private:

	void BuildWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HeatBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PercentText;
};
