#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSHealthBarWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 *  Barre de vie au-dessus d'un personnage : remplissage proportionnel aux PV,
 *  fond pour la partie vide et texte « X / Y » par-dessus.
 *  Cachée quand les PV sont au maximum. Construite en C++ (pas de Widget Blueprint nécessaire).
 */
UCLASS()
class UYSHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Taille de la barre en pixels */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health Bar")
	FVector2D BarSize = FVector2D(120.0f, 16.0f);

	/** Couleur de la partie remplie */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health Bar")
	FLinearColor FillColor = FLinearColor(0.85f, 0.05f, 0.05f);

	/** Couleur de la partie vide */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health Bar")
	FLinearColor BackgroundColor = FLinearColor::Black;

	/** Taille du texte « X / Y » */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health Bar")
	int32 FontSize = 10;

	virtual bool Initialize() override;

	/** Met à jour la barre ; cachée si Current >= Max (ou PV pas encore connus) */
	void SetHealth(float Current, float Max);

	/** Barre affichée même à PV pleins (élites) */
	bool bAlwaysVisible = false;

private:

	void BuildWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;
};
