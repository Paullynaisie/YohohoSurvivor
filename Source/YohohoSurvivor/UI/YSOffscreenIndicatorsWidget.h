#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSOffscreenIndicatorsWidget.generated.h"

class UCanvasPanel;
class UTexture2D;
class UYSPlayerIconWidget;

/** Un indicateur : position (unités Slate, centre de l'icône) et icône du joueur */
struct FYSOffscreenIndicator
{
	FVector2D Position = FVector2D::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	UTexture2D* Face = nullptr;
};

/**
 *  Plein écran : icônes des autres joueurs hors de l'écran, collées au bord dans leur direction.
 *  Positions calculées par AYSPlayerController. Construit en C++ (pas de Widget Blueprint).
 */
UCLASS()
class UYSOffscreenIndicatorsWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Diamètre des icônes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Indicators")
	float IconSize = 48.0f;

	virtual bool Initialize() override;

	/** Affiche ces indicateurs (les icônes en trop sont cachées) */
	void SetIndicators(const TArray<FYSOffscreenIndicator>& Indicators);

private:

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Canvas;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UYSPlayerIconWidget>> Icons;
};
