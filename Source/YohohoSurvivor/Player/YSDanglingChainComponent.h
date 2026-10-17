#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "YSDanglingChainComponent.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 *  Chaîne de segments qui pend et flotte derrière le personnage (bandana, plume, queue…).
 *  Simulation physique légère maison (intégration de Verlet + contraintes de longueur) :
 *  gravité, inertie, amortissement et une poussée vers l'arrière du personnage.
 *  Purement visuelle : simulée localement sur chaque machine, rien n'est répliqué.
 *  Le premier point est accroché à ce composant ; les segments sont des meshes (cylindres par défaut).
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent))
class UYSDanglingChainComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	/** Nombre de segments */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain", meta=(ClampMin="1", ClampMax="32"))
	int32 SegmentCount = 5;

	/** Longueur d'un segment */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain", meta=(ClampMin="1", Units="Centimeters"))
	float SegmentLength = 10.0f;

	/** Rayon d'un segment */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain", meta=(ClampMin="0.1", Units="Centimeters"))
	float SegmentRadius = 3.0f;

	/** Mesh d'un segment (aligné sur son axe Z, 100 unités de haut comme les formes de base) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain|Appearance")
	TObjectPtr<UStaticMesh> SegmentMesh;

	/** Matériau des segments (vide = matériau du mesh, teinté avec Color) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain|Appearance")
	TObjectPtr<UMaterialInterface> SegmentMaterial;

	/** Couleur appliquée au paramètre ColorParameterName du matériau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain|Appearance")
	FLinearColor Color = FLinearColor(0.7f, 0.05f, 0.05f);

	UPROPERTY(EditAnywhere, Category="Chain|Appearance")
	FName ColorParameterName = TEXT("Color");

	/** Gravité appliquée aux segments */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain|Physics")
	FVector Gravity = FVector(0.0f, 0.0f, -980.0f);

	/** Poussée vers l'arrière du personnage (fait flotter la chaîne derrière lui) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain|Physics", meta=(Units="CentimetersPerSecondSquared"))
	float TrailAcceleration = 600.0f;

	/** Perte de vitesse par image à 60 fps (0 = aucune, 1 = pas d'inertie) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain|Physics", meta=(ClampMin="0", ClampMax="1"))
	float Damping = 0.08f;

	/** Itérations de résolution des longueurs (plus = chaîne moins élastique) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain|Physics", meta=(ClampMin="1", ClampMax="16"))
	int32 SolverIterations = 4;

public:

	UYSDanglingChainComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Change le matériau de tous les segments (personnalisation du joueur) */
	void SetSegmentMaterial(UMaterialInterface* Material);

protected:

	virtual void OnRegister() override;

	virtual void OnUnregister() override;

	virtual void BeginPlay() override;

	/** Hors jeu (éditeur) : la chaîne suit le composant dans sa pose de repos */
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;

private:

	void BuildSegments();
	void DestroySegments();

	/** Place la chaîne au repos : vers l'arrière et vers le bas */
	void ResetPoints();

	void UpdateSegmentTransforms();

	/** Direction « arrière » du personnage, à plat */
	FVector GetTrailDirection() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Segments;

	TArray<FVector> Points;
	TArray<FVector> PreviousPoints;
};
