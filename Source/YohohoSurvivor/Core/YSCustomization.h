#pragma once

#include "CoreMinimal.h"
#include "YSCustomization.generated.h"

class UMaterialInterface;

/** Couleur proposée à l'écran de sélection : bouton + matériau appliqué au « Head Feature » du personnage */
USTRUCT(BlueprintType)
struct FYSCustomizationColor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Customization")
	FText Name;

	/** Couleur du bouton de l'écran de sélection */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Customization")
	FLinearColor ButtonColor = FLinearColor::White;

	/** Matériau appliqué au composant « Head Feature » et à tous ses sous-composants */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Customization")
	TObjectPtr<UMaterialInterface> Material;
};
