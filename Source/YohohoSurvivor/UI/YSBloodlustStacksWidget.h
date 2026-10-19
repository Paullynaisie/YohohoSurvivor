#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSBloodlustStacksWidget.generated.h"

class UHorizontalBox;
class UImage;
class UProgressBar;
class USizeBox;

/**
 *  BL stacks du Bloodlust (en bas de l'écran, à droite de l'icône d'Ability) : une pastille par stack possible
 *  (pleines = stacks obtenues) et, dessous, une barre du délai restant avant leur perte.
 *  Caché tant que le Bloodlust n'est pas débloqué. Arbre construit en C++ ; réglages (placement compris) dans les
 *  Class Defaults du Widget Blueprint enfant WBP_BloodlustStacks (catégorie Layout), choisi par BP_YSPlayerController.
 */
UCLASS()
class UYSBloodlustStacksWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Position : X = distance entre le centre de l'écran et le bord gauche du widget, Y = distance au bas de l'écran */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FVector2D ScreenOffset = FVector2D(64.0f, 40.0f);

	/** Taille d'une pastille de stack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FVector2D PipSize = FVector2D(22.0f, 22.0f);

	/** Espace entre deux pastilles */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout", meta=(ClampMin="0"))
	float PipSpacing = 6.0f;

	/** Couleur d'une stack obtenue */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor FilledColor = FLinearColor(0.85f, 0.05f, 0.05f);

	/** Couleur des stacks quand les Auto swing sont critiques (Golden bloodlust au maximum) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor CriticalColor = FLinearColor(1.0f, 0.75f, 0.1f);

	/** Couleur d'une stack manquante */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor EmptyColor = FLinearColor(0.08f, 0.08f, 0.08f, 0.85f);

	/** Barre du délai restant : hauteur, espace au-dessus, couleurs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout", meta=(ClampMin="1"))
	float TimerBarHeight = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout", meta=(ClampMin="0"))
	float TimerBarSpacing = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor TimerBarColor = FLinearColor(0.95f, 0.3f, 0.3f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor TimerBarBackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.7f);

	virtual bool Initialize() override;

	/** Stacks actuelles / maximum (0 = caché), délai restant (0..1), critiques ? */
	void Refresh(int32 Stacks, int32 MaxStacks, float TimeRatio, bool bCritical);

private:

	void BuildWidgetTree();

	/** Recrée les pastilles si le maximum change */
	void RebuildPips(int32 Count);

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> PipRow;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> TimerBox;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> TimerBar;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> Pips;
};
