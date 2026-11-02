#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSStaminaBarWidget.generated.h"

class UImage;
class UOverlaySlot;
class UProgressBar;

/**
 *  Barre de stamina (bas de l'écran, sous les icônes d'actions) : jaune quand la MoveAction peut partir, jaune gris sinon,
 *  avec un cran à son coût. Arbre construit en C++ ; réglages (placement compris) dans les Class Defaults du
 *  Widget Blueprint enfant WBP_StaminaBar (catégorie Layout), choisi par BP_YSPlayerController.
 */
UCLASS()
class UYSStaminaBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Taille de la barre (par défaut : la largeur des 3 icônes) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FVector2D BarSize = FVector2D(320.0f, 16.0f);

	/** Distance entre la barre et le bas de l'écran */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	float BottomMargin = 16.0f;

	/** Couleur quand il y a assez de stamina pour la MoveAction */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor ReadyColor = FLinearColor(1.0f, 0.8f, 0.05f);

	/** Couleur quand il n'y en a pas assez */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor NotEnoughColor = FLinearColor(0.42f, 0.38f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.7f);

	/** Cran au coût de la MoveAction : couleur et largeur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor NotchColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout", meta=(ClampMin="1"))
	float NotchWidth = 3.0f;

	virtual bool Initialize() override;

	/** Stamina actuelle, maximum (0 = caché), coût de la MoveAction */
	void Refresh(float Stamina, float MaxStamina, float MoveActionCost);

private:

	void BuildWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Notch;

	UPROPERTY(Transient)
	TObjectPtr<UOverlaySlot> NotchSlot;
};
