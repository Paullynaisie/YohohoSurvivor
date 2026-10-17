#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSDayBannerWidget.generated.h"

class UTextBlock;

/**
 *  Annonce « Jour X » au milieu de l'écran au début d'un nouveau jour (affichée quelques secondes par AYSPlayerController).
 *  Construit en C++ (pas de Widget Blueprint).
 */
UCLASS()
class UYSDayBannerWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Texte affiché ({0} = numéro du jour) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Day Banner")
	FText Format = NSLOCTEXT("YS", "DayBanner", "Jour {0}");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Day Banner")
	int32 FontSize = 72;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Day Banner")
	FLinearColor Color = FLinearColor(1.0f, 0.85f, 0.35f);

	/** Durée d'affichage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Day Banner", meta=(ClampMin="0", Units="s"))
	float DisplayDuration = 3.0f;

	virtual bool Initialize() override;

	void SetDay(int32 Day);

private:

	void BuildWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;
};
