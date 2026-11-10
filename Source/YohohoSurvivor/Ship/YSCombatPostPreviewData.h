#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "YSCombatPostPreviewData.generated.h"

class UMaterialInterface;

/**
 *  Prévisus d'un poste de combat (canon, Gatling), affichés seulement pour le joueur installé au poste (demande de Paul du 2026-11-10) :
 *  deux traits aux limites d'angle du poste, l'espace entre eux rempli de la couleur du joueur qui s'estompe avec la distance, et un trait
 *  vers l'endroit précis où le poste va tirer. Data Asset /Game/Core/Ship/DA_CombatPostPreview (Preview Data des postes).
 */
UCLASS(BlueprintType)
class UYSCombatPostPreviewData : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Matériau translucide non éclairé qui affiche la couleur des sommets (alpha = opacité) : /Game/Core/Ship/M_ShipPreview */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview")
	TObjectPtr<UMaterialInterface> Material;

	/** Hauteur des prévisus au-dessus de l'origine du poste (sol du pont) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview", meta=(Units="Centimeters"))
	float HeightOffset = 8.0f;

	/** Traits des limites d'angle : longueur, largeur, couleur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview|Angle Lines", meta=(ClampMin="0", Units="Centimeters"))
	float AngleLineLength = 3500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview|Angle Lines", meta=(ClampMin="0", Units="Centimeters"))
	float AngleLineWidth = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview|Angle Lines")
	FLinearColor AngleLineColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.9f);

	/** Trait de visée (où le poste va tirer) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview|Aim Line", meta=(ClampMin="0", Units="Centimeters"))
	float AimLineLength = 3500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview|Aim Line", meta=(ClampMin="0", Units="Centimeters"))
	float AimLineWidth = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview|Aim Line")
	FLinearColor AimLineColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	/** Remplissage entre les traits, couleur du joueur au poste : opacité Fill Start Alpha au poste, 0 à Fill Fade Distance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview|Fill", meta=(ClampMin="0", ClampMax="1"))
	float FillStartAlpha = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview|Fill", meta=(ClampMin="1", Units="Centimeters"))
	float FillFadeDistance = 2000.0f;

	/** Courbe de l'estompe (1 = linéaire, 2 = s'efface plus vite près du poste) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview|Fill", meta=(ClampMin="0.1"))
	float FillFadeExponent = 1.0f;

	/** Découpage du remplissage : secteurs en angle, anneaux en distance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview|Fill", meta=(ClampMin="2", ClampMax="128"))
	int32 FillAngleSegments = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview|Fill", meta=(ClampMin="1", ClampMax="64"))
	int32 FillRings = 12;

	/** Mortier : cercle de l'explosion sous le point visé (bord et intérieur) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview|Mortar", meta=(ClampMin="8", ClampMax="256"))
	int32 MortarCircleSegments = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview|Mortar", meta=(ClampMin="1", Units="Centimeters"))
	float MortarCircleWidth = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview|Mortar")
	FLinearColor MortarCircleColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Preview|Mortar")
	FLinearColor MortarFillColor = FLinearColor(1.0f, 0.4f, 0.1f, 0.25f);
};
