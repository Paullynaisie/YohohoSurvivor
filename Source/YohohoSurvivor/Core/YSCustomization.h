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

	/**
	 *  Émissif du joueur (m_PH_emissive_turbo* ; rouge et noir : blanc) : composant Coloration de ses armes et mesh de toutes ses AMMO
	 *  (canons compris). Vide = matériaux du Blueprint.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Customization")
	TObjectPtr<UMaterialInterface> EmissiveMaterial;

	/**
	 *  Couleur de cet émissif, pour les VFX du joueur dessinés en couleur de sommets / paramètres : traînée de slash, disques, mèche
	 *  et sphère d'explosion des bombes / mines / AMMO explosives, prévisus des postes de combat
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Customization")
	FLinearColor EmissiveColor = FLinearColor::White;
};

class AActor;
class AYSPlayerCharacter;

/** Couleur du joueur pour ses armes, AMMO et VFX */
namespace YSPlayerColor
{
	/**
	 *  Joueur dont vient Source : lui-même, sinon en remontant son Instigator / son Owner (arme -> porteur, AMMO / bombe -> tireur) ;
	 *  un minion du Captain -> son Captain. nullptr pour un NME ou un acteur sans joueur.
	 */
	const AYSPlayerCharacter* FindPlayer(const AActor* Source);

	/** Couleur de personnalisation (ligne de DT_CustomizationColors) du joueur dont vient Source, ou nullptr */
	const FYSCustomizationColor* Find(const AActor* Source);
}

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
