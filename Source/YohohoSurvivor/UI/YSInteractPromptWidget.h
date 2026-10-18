#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSInteractPromptWidget.generated.h"

/**
 *  Invite d'interaction : la touche « E » dans un carré arrondi, affichée au-dessus d'un objet interactif
 *  (WidgetComponent écran de AYSInteractiveObject). Arbre construit en C++ ; réglages dans les Class Defaults
 *  du Widget Blueprint enfant WBP_InteractPrompt (catégorie Layout).
 */
UCLASS()
class UYSInteractPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Texte de la touche */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FText KeyText = NSLOCTEXT("YS", "InteractKey", "E");

	/** Taille du carré */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout", meta=(ClampMin="4"))
	FVector2D KeySize = FVector2D(44.0f, 44.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout", meta=(ClampMin="0"))
	float CornerRadius = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout", meta=(ClampMin="1"))
	int32 FontSize = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor BackgroundColor = FLinearColor(0.95f, 0.95f, 0.95f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout", meta=(ClampMin="0"))
	float OutlineWidth = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor TextColor = FLinearColor(0.05f, 0.05f, 0.05f);

	virtual bool Initialize() override;

private:

	void BuildWidgetTree();
};
