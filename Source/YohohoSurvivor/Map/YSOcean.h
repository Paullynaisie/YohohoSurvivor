#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YSOcean.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UMaterialInterface;
class UProceduralMeshComponent;

/** Canal de collision « Water » (DefaultEngine.ini) : le sol de nage de l'océan ; seuls les joueurs (et minions) le bloquent */
inline constexpr ECollisionChannel ECC_YSWater = ECC_GameTraceChannel1;

/**
 *  Océan d'une map : surface bleue sans collision (on passe au travers et on tombe dans la KillZ),
 *  dont la hauteur donne le niveau de la mer. Le sol sous l'eau n'est pas jouable : pas de NME ni de carte au trésor
 *  (IsUnderwater), et les balles et zones au sol restent au-dessus de la surface (ClampAboveSea).
 *  Placé une fois par map (GameMap : Tools/EditorScripts/setup_island.py).
 *  Nage : un sol invisible (SwimFloor, canal Water) à Swim Floor Depth sous la surface porte les joueurs qui vont dans l'eau
 *  profonde (AYSPlayerCharacter::IsSwimming) ; les NME l'ignorent (ils tombent dans la deadzone), les balles et les traces aussi.
 */
UCLASS(meta=(PrioritizeCategories="Ocean"))
class AYSOcean : public AActor
{
	GENERATED_BODY()

private:

	/** Surface de l'eau (plane moteur mis à l'échelle de l'acteur), sans collision */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Surface;

	/** Sol de nage : boîte invisible sous toute la surface, à Swim Floor Depth sous l'eau (canal Water, bloque seulement les Pawns qui bloquent ce canal) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> SwimFloor;

protected:

	/** Un sol à moins de cette hauteur au-dessus de la mer compte comme dans l'eau (apparitions des NME, carte au trésor) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ocean", meta=(ClampMin="0", Units="Centimeters"))
	float ShoreMargin = 20.0f;

	/** Profondeur du sol de nage sous la surface (pieds d'un joueur qui nage) ; le corps est ensuite placé à Swim Immersion du personnage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ocean|Swim", meta=(ClampMin="1", Units="Centimeters"))
	float SwimFloorDepth = 95.0f;

	/**
	 *  Écume (local, chaque machine) : taches blanches posées sur l'eau, FIXES dans le monde (grille de cases, mêmes taches à chaque
	 *  passage) autour de la caméra du joueur local, pour voir qu'on avance (demande de Paul, 2026-11-11)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ocean|Foam")
	bool bFoam = true;

	/** Taille d'une case de la grille ; taches dans les cases à moins de Foam Grid Radius cases de celle de la caméra */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ocean|Foam", meta=(ClampMin="100", Units="Centimeters", EditCondition="bFoam"))
	float FoamCellSize = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ocean|Foam", meta=(ClampMin="1", ClampMax="20", EditCondition="bFoam"))
	int32 FoamGridRadius = 8;

	/** Taches par case (au hasard entre min et max) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ocean|Foam", meta=(EditCondition="bFoam"))
	FIntPoint FoamPerCell = FIntPoint(1, 3);

	/** Rayon d'une tache (au hasard), forme irrégulière */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ocean|Foam", meta=(Units="Centimeters", EditCondition="bFoam"))
	FVector2D FoamRadius = FVector2D(40.0, 180.0);

	/** Allongement des taches (longueur / largeur, au hasard) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ocean|Foam", meta=(EditCondition="bFoam"))
	FVector2D FoamStretch = FVector2D(1.0, 2.5);

	/** Couleur au centre d'une tache (alpha = opacité), bord transparent */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ocean|Foam", meta=(EditCondition="bFoam"))
	FLinearColor FoamColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ocean|Foam", meta=(Units="Centimeters", EditCondition="bFoam"))
	float FoamHeightAboveSea = 2.0f;

	/** Vide = M_ShipPreview (translucide non éclairé, couleur des sommets) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ocean|Foam", meta=(EditCondition="bFoam"))
	TObjectPtr<UMaterialInterface> FoamMaterial;

public:

	AYSOcean();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	float GetSwimFloorDepth() const { return SwimFloorDepth; }

	/** Profondeur d'eau au-dessus de ce point (surface - Z), 0 hors de l'eau ou sans océan */
	static float GetWaterDepth(const UWorld* World, const FVector& Point);

	/** Niveau de la mer = hauteur de l'acteur */
	float GetSeaLevel() const { return GetActorLocation().Z; }

	/** Océan de la map, ou nullptr */
	static const AYSOcean* Find(const UWorld* World);

	/** Vrai si ce point du sol est sous la mer (ou à moins de ShoreMargin au-dessus) ; faux sans océan */
	static bool IsUnderwater(const UWorld* World, const FVector& GroundPoint);

	/** Hauteur ramenée au niveau de la mer si elle est dessous (inchangée sans océan) */
	static float ClampAboveSea(const UWorld* World, float Z);

private:

	/** Sol de nage sous toute la surface (taille et hauteur en coordonnées monde) */
	void ApplySwimFloor();

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> FoamMesh;

	FIntPoint FoamCenterCell = FIntPoint(TNumericLimits<int32>::Max(), 0);

	/** Taches des cases autour de Center (reconstruit quand la caméra change de case) */
	void RebuildFoam(const FIntPoint& Center);
};
