#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "YSGroundZoneComponent.generated.h"

/**
 *  Disque posé sur le relief (zone de rachat du Brooker, impact des bombes) : maillage généré autour du composant,
 *  chaque sommet placé sur le sol par une trace verticale (WorldStatic), au-dessus de la mer s'il y a un océan.
 *  Le sol est suivi de proche en proche depuis le centre : un obstacle plus haut que MaxStepHeight (tronc, mur)
 *  est ignoré au lieu de faire monter la zone dessus. Sans collision ni ombre ; matériau donné par le propriétaire
 *  (M_BrookerZone : bord calculé depuis la distance au centre du composant).
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent))
class UYSGroundZoneComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:

	/** Écart entre deux sommets (rayon et circonférence) : plus petit = suit mieux le relief, plus de traces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground Zone", meta=(ClampMin="10", Units="Centimeters"))
	float VertexSpacing = 100.0f;

	/** Montée max du sol entre deux sommets voisins ; au-delà c'est un obstacle, la zone reste à la hauteur précédente */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground Zone", meta=(ClampMin="0", Units="Centimeters"))
	float MaxStepHeight = 150.0f;

	/** Profondeur max cherchée sous le sommet précédent */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground Zone", meta=(ClampMin="0", Units="Centimeters"))
	float MaxStepDown = 2000.0f;

	/** Au-dessus de l'eau, la zone reste à la surface de la mer (AYSOcean) au lieu de plonger sur le fond */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground Zone")
	bool bClampAboveSea = true;

	UYSGroundZoneComponent(const FObjectInitializer& ObjectInitializer);

	/** Reconstruit le disque de rayon Radius centré sous le composant, HeightAboveGround au-dessus du sol */
	void BuildZone(float Radius, float HeightAboveGround);

private:

	/** Sol sous (X, Y) cherché entre ReferenceZ + MaxStepHeight et ReferenceZ - MaxStepDown ; ReferenceZ sans sol */
	float FindGroundZ(float X, float Y, float ReferenceZ) const;
};
