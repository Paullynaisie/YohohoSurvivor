#pragma once

#include "CoreMinimal.h"
#include "Interaction/YSInteractiveObject.h"
#include "YSTreasureMap.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USplineComponent;
class UStaticMesh;
class UTexture2D;

/**
 *  Carte au trésor : objet porté comme le coffre (touche E, armes cachées, plus lent, NME attirés), invendable.
 *  À son apparition (serveur), un chemin de SegmentCount segments (longueurs entre MinSegmentLength et MaxSegmentLength,
 *  chaque point au sol) part de la carte. Seul le joueur qui la porte voit le chemin, en pointillés au sol, et seulement
 *  autour de lui (VisibleRadius). Le porteur avance de point en point (ReachRadius) :
 *  au point WaveAtPoint, une grosse vague de NME apparaît (WaveEnemiesPerPlayer par joueur) ;
 *  au dernier point, la carte disparaît et un coffre au trésor (RewardClass) apparaît.
 *  Le GameMode en fait apparaître une au hasard au début de chaque jour.
 *  Classe de base du Blueprint BP_InteractiveObject_TreasureMap.
 */
UCLASS(meta=(PrioritizeCategories="Interaction TreasureMap"))
class AYSTreasureMap : public AYSInteractiveObject
{
	GENERATED_BODY()

private:

	/** Chemin (local, coordonnées monde), reconstruit depuis PathPoints */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USplineComponent> PathSpline;

	/** Pointillés du chemin (local, coordonnées monde, seulement chez le porteur) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInstancedStaticMeshComponent> PathDashes;

protected:

	/** Nombre de segments du chemin */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Path", meta=(ClampMin="1"))
	int32 SegmentCount = 5;

	/** Longueur min d'un segment */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Path", meta=(ClampMin="100", Units="Centimeters"))
	float MinSegmentLength = 1500.0f;

	/** Longueur max d'un segment */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Path", meta=(ClampMin="100", Units="Centimeters"))
	float MaxSegmentLength = 2500.0f;

	/** Angle max entre deux segments qui se suivent (évite les allers-retours) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Path", meta=(ClampMin="0", ClampMax="180", Units="Degrees"))
	float MaxTurnAngle = 100.0f;

	/** Écart de hauteur max entre deux points du chemin (évite le dessus des obstacles) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Path", meta=(ClampMin="0", Units="Centimeters"))
	float MaxHeightDifference = 150.0f;

	/** Essais d'emplacement par point avant de recommencer le chemin */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Path", meta=(ClampMin="1"))
	int32 PointAttempts = 30;

	/** Le porteur atteint un point du chemin à cette distance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Path", meta=(ClampMin="0", Units="Centimeters"))
	float ReachRadius = 250.0f;

	/** Point du chemin (1 = fin du premier segment) qui déclenche la grosse vague */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Wave", meta=(ClampMin="1"))
	int32 WaveAtPoint = 2;

	/** Grosse vague : NME pour 1 joueur (x Spawn Multiplier du nombre de joueurs, AYSGameMode::PlayerCountScaling) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Wave", meta=(ClampMin="0"))
	int32 WaveEnemiesPerPlayer = 10;

	/** Récompense posée au dernier point (coffre au trésor) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Reward")
	TSubclassOf<AYSInteractiveObject> RewardClass;

	/** Pointillés affichés jusqu'à cette distance du porteur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Dashes", meta=(ClampMin="0", Units="Centimeters"))
	float VisibleRadius = 1500.0f;

	/** Les pointillés rétrécissent sur cette distance avant la limite d'affichage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Dashes", meta=(ClampMin="0", Units="Centimeters"))
	float FadeDistance = 300.0f;

	/** Chemin arrondi entre les points (courbe, sol vérifié aussi le long de la courbe : un chemin dont la courbe passe hors du sol
	 *  est refusé) ; décoché = segments droits */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Dashes")
	bool bSmoothPath = true;

	/** Distance entre le début de deux pointillés */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Dashes", meta=(ClampMin="1", Units="Centimeters"))
	float DashSpacing = 70.0f;

	/** Longueur d'un pointillé */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Dashes", meta=(ClampMin="1", Units="Centimeters"))
	float DashLength = 40.0f;

	/** Largeur d'un pointillé */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Dashes", meta=(ClampMin="1", Units="Centimeters"))
	float DashWidth = 14.0f;

	/** Hauteur des pointillés au-dessus du sol */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Dashes", meta=(Units="Centimeters"))
	float DashHeightOffset = 2.0f;

	/** Les pointillés déjà parcourus (avant le dernier point atteint) disparaissent */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Dashes")
	bool bHideTravelledPath = true;

	/** Point atteint : les pointillés du segment parcouru rétrécissent jusqu'à disparaître pendant cette durée (0 = d'un coup) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Dashes", meta=(ClampMin="0", Units="Seconds", EditCondition="bHideTravelledPath"))
	float TravelledFadeDuration = 0.6f;

	/** Forme d'un pointillé (cube de 100 cm aplati par défaut) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Dashes")
	TObjectPtr<UStaticMesh> DashMesh;

	/** Matériau des pointillés (reçoit DashColor dans DashColorParameterName) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Dashes")
	TObjectPtr<UMaterialInterface> DashMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Dashes")
	FLinearColor DashColor = FLinearColor(1.0f, 0.85f, 0.2f);

	UPROPERTY(EditAnywhere, Category="TreasureMap|Dashes")
	FName DashColorParameterName = TEXT("Color");

	/** Aucun pointillé affiché (porteur trop loin du chemin) : une bulle guide le porteur vers le début du chemin restant
	 *  (dessus s'il est à l'écran, sinon au bord de l'écran dans sa direction) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Guide")
	bool bShowGuideIndicator = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Guide", meta=(EditCondition="bShowGuideIndicator"))
	FLinearColor GuideIndicatorColor = FLinearColor(1.0f, 0.85f, 0.2f);

	/** Icône dans la bulle (vide = rond de couleur seul) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Guide", meta=(EditCondition="bShowGuideIndicator"))
	TObjectPtr<UTexture2D> GuideIndicatorIcon;

	/** Hauteur de la bulle au-dessus du début du chemin */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap|Guide", meta=(Units="Centimeters", EditCondition="bShowGuideIndicator"))
	float GuideIndicatorHeight = 50.0f;

	/** Points du chemin (le premier = position d'apparition de la carte), fixés par le serveur */
	UPROPERTY(ReplicatedUsing=OnRep_PathPoints, VisibleInstanceOnly, BlueprintReadOnly, Category="TreasureMap")
	TArray<FVector> PathPoints;

	/** Dernier point du chemin atteint par un porteur (0 = départ) */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="TreasureMap")
	int32 ReachedPointIndex = 0;

	UFUNCTION()
	void OnRep_PathPoints();

public:

	AYSTreasureMap();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/**
	 *  Chemin au sol depuis Start (réglages de cette carte, utilisable sur le CDO avant l'apparition) ;
	 *  faux si aucun chemin complet n'a été trouvé
	 */
	bool BuildPath(const UWorld* World, const FVector& Start, TArray<FVector>& OutPoints) const;

	/** Chemin arrondi (bSmoothPath) : la courbe entre les points reste sur un sol valide */
	bool IsSmoothPathGrounded(const UWorld* World, const TArray<FVector>& Points) const;

	/** Fixe le chemin (serveur, avant ou juste après l'apparition) */
	void SetPath(const TArray<FVector>& Points);

	const TArray<FVector>& GetPathPoints() const { return PathPoints; }

	int32 GetReachedPointIndex() const { return ReachedPointIndex; }

	float GetMaxHeightDifference() const { return MaxHeightDifference; }

	/** Bulle de guidage (joueur local qui porte la carte, aucun pointillé affiché) : position du début du chemin restant ; faux sinon */
	bool GetGuideIndicatorLocation(FVector& OutLocation) const;

	FLinearColor GetGuideIndicatorColor() const { return GuideIndicatorColor; }

	UTexture2D* GetGuideIndicatorIcon() const { return GuideIndicatorIcon; }

	/** Point au sol (WorldStatic) sous XY, à moins de MaxHeight de ReferenceZ ; faux sinon (vide, dessus d'un obstacle) */
	static bool FindPathGround(const UWorld* World, const FVector& Location, float ReferenceZ, float MaxHeight, FVector& OutGround);

private:

	/** Serveur : le porteur atteint le point suivant (vague au point WaveAtPoint, récompense au dernier) */
	void UpdateProgress();

	/** Dernier point atteint : la carte disparaît, la récompense apparaît (serveur) */
	void Complete();

	/** Spline locale reconstruite depuis PathPoints */
	void RebuildSpline();

	/** Log : le chemin affiché reste-t-il sur le sol vérifié (écart aux segments, pointillés sans sol) */
	void LogDisplayedPathCheck();

	/** Pointillés autour du porteur local (cachés sinon) */
	void UpdateDashes();

	/** Matériau des pointillés avec la couleur */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DashMaterialInstance;

	/** Pointillés affichés à la dernière frame */
	int32 VisibleDashCount = 0;

	/** Dernier point atteint vu par cette machine, et fondu du segment qu'il vient de terminer (temps local, distance sur la spline) */
	int32 LastSeenReachedPointIndex = 0;
	double TravelledFadeStartTime = -1.0e6;
	float TravelledFadeFromDistance = 0.0f;
};
