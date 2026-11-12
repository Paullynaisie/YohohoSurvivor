#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSNameplateWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 *  Au-dessus du personnage d'un allié : son pseudo et sa barre de PV (le joueur local a les siens dans le HUD).
 *  Construit en C++ (pas de Widget Blueprint) ; affiché par le WidgetComponent « Nameplate » du personnage.
 */
UCLASS()
class UYSNameplateWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Nameplate")
	FVector2D BarSize = FVector2D(110.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Nameplate")
	FLinearColor BarColor = FLinearColor(0.15f, 0.8f, 0.2f);

	/** Barre de stamina sous celle des PV (HiredMouss) : taille, couleur, écart */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Nameplate")
	FVector2D StaminaBarSize = FVector2D(110.0f, 6.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Nameplate")
	FLinearColor StaminaBarColor = FLinearColor(1.0f, 0.85f, 0.1f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Nameplate")
	float StaminaBarSpacing = 2.0f;

	virtual bool Initialize() override;

	/** Stamina Ratio < 0 : pas de barre de stamina */
	void SetPlayer(const FText& Name, float Health, float MaxHealth, float StaminaRatio = -1.0f);

private:

	void BuildWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameLabel;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> StaminaBar;

	UPROPERTY(Transient)
	TObjectPtr<class USizeBox> StaminaBox;
};
