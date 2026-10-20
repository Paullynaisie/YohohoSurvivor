#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "YSCustomization.generated.h"

class UMaterialInterface;
class UTexture2D;

/**
 *  Couleur proposée à l'écran de sélection : bouton + matériau appliqué au « Head Feature » du personnage.
 *  Ligne de la Data Table /Game/Core/Customization/DT_CustomizationColors (ordre des lignes = ordre des boutons).
 */
USTRUCT(BlueprintType)
struct FYSCustomizationColor : public FTableRowBase
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

/**
 *  Visage proposé à l'écran de sélection : texture du decal de visage.
 *  Ligne de la Data Table /Game/Core/Customization/DT_CustomizationFaces (ordre des lignes = ordre des boutons).
 */
USTRUCT(BlueprintType)
struct FYSCustomizationFace : public FTableRowBase
{
	GENERATED_BODY()

	/** Texture du visage (decal M_FaceDecal), aussi affichée sur le bouton */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Customization")
	TObjectPtr<UTexture2D> Face;
};
