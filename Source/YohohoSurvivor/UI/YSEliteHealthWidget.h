#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSEliteHealthWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 *  Barre de vie de l'élite dans le HUD (haut droite, symétrique des PV du joueur) : nom + barre « X / Y ».
 *  Affichée par AYSPlayerController quand l'élite est hors de l'écran (sinon sa barre est au-dessus de sa tête).
 *  Construit en C++ (pas de Widget Blueprint).
 */
UCLASS()
class UYSEliteHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FVector2D BarSize = FVector2D(320.0f, 26.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FLinearColor BarColor = FLinearColor(0.6f, 0.05f, 0.75f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FLinearColor BarBackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);

	virtual bool Initialize() override;

	void SetElite(const FText& Name, float Health, float MaxHealth);

private:

	void BuildWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameLabel;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ValueLabel;
};
