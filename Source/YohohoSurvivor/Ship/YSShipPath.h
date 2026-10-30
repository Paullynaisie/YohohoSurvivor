#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YSShipPath.generated.h"

class USplineComponent;

/**
 *  Trajet d'un bateau en mode playtest (Ship|Helm|Playtest de AYSPirateShip) : spline fermée posée dans la map, à retoucher dans le
 *  viewport (points de la spline). Par défaut un tour de l'île : bouton Build Around Island (ou Tools/EditorScripts/setup_jolly_ship.py).
 *  Lu seulement par le serveur (pilote automatique), rien de répliqué.
 */
UCLASS(meta=(PrioritizeCategories="Ship Path"))
class AYSShipPath : public AActor
{
	GENERATED_BODY()

private:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USplineComponent> Spline;

protected:

	/** Centre de l'île */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship Path")
	FVector IslandCenter = FVector::ZeroVector;

	/** Build Around Island : nombre de points de la spline */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship Path", meta=(ClampMin="4"))
	int32 PointCount = 24;

	/** Build Around Island : distance entre le premier fond à Min Shore Depth et la spline (doit laisser passer la coque et le ponton) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship Path", meta=(ClampMin="0", Units="Centimeters"))
	float ShoreClearance = 2000.0f;

	/** Build Around Island : profondeur d'eau qui compte comme le bord de l'île */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship Path", meta=(ClampMin="0", Units="Centimeters"))
	float MinShoreDepth = 200.0f;

	/** Build Around Island : recherche du bord depuis le large, depuis cette distance au centre, par pas de Shore Search Step */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship Path", meta=(ClampMin="0", Units="Centimeters"))
	float ShoreSearchRadius = 14000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship Path", meta=(ClampMin="10", Units="Centimeters"))
	float ShoreSearchStep = 100.0f;

public:

	AYSShipPath();

	virtual void BeginPlay() override;

	/** Spline refaite : un tour de l'île à Shore Clearance de son bord (Landscape seulement, à la hauteur de la mer) */
	UFUNCTION(CallInEditor, BlueprintCallable, Category="Ship Path")
	void BuildAroundIsland();

	/** Point de la spline LookAhead cm plus loin que le point le plus proche de From ; faux si la spline est vide */
	bool GetPointAhead(const FVector& From, float LookAhead, FVector& OutPoint) const;
};
