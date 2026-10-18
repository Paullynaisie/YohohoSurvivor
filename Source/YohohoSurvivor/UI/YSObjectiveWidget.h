#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSObjectiveWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 *  Jauge d'objectif (commune à toute l'équipe) : doublons gagnés en revendant des coffres au Brooker, « X / Objectif ».
 *  HUD haut droite, sous l'emplacement de la barre de vie de l'élite. Arbre construit en C++ ; réglages
 *  (placement compris) dans les Class Defaults du Widget Blueprint enfant WBP_Objective (catégorie Layout),
 *  choisi par BP_YSPlayerController (ObjectiveWidgetClass).
 */
UCLASS()
class UYSObjectiveWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Distance au coin haut droit de l'écran (X vers la gauche, Y vers le bas) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FVector2D ScreenOffset = FVector2D(24.0f, 100.0f);

	/** Titre au-dessus de la jauge */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FText Title = NSLOCTEXT("YS", "ObjectiveTitle", "Doublons");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout", meta=(ClampMin="1"))
	int32 TitleFontSize = 18;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout", meta=(ClampMin="1"))
	int32 ValueFontSize = 14;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FVector2D BarSize = FVector2D(320.0f, 22.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor BarColor = FLinearColor(1.0f, 0.72f, 0.08f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor BarBackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);

	/** Couleur de la jauge une fois l'objectif atteint */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor CompletedBarColor = FLinearColor(0.2f, 0.9f, 0.3f);

	/** Espace entre le titre et la jauge */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	float TitleSpacing = 2.0f;

	virtual bool Initialize() override;

	void SetProgress(int32 Doubloons, int32 Goal);

private:

	void BuildWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ValueLabel;

	int32 DisplayedDoubloons = INDEX_NONE;
	int32 DisplayedGoal = INDEX_NONE;
};
