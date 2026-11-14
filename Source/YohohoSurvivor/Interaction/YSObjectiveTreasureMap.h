#pragma once

#include "CoreMinimal.h"
#include "Enemies/YSFoeGalion.h"
#include "Interaction/YSTreasureMap.h"
#include "YSObjectiveTreasureMap.generated.h"

class AYSIsland;

/**
 *  Carte au trésor d'objectif (partie 2 du flow, Blueprint BP_InteractiveObject_ObjectiveTreasureMap : la carte au trésor avec une croix et des
 *  pointillés verts). Sur une île des zones d'intérêt dont l'objectif est une boussole, la boussole n'est pas posée : quand le bateau des joueurs
 *  accoste, cette carte apparaît sur la plage à côté du point d'accostage. Son chemin est fait de 3 tracés consécutifs posés par l'île :
 *    1) vers un point au hasard dans la moitié de l'île opposée au point d'accostage ; atteint : embuscade (Ambush Groups), 2e tracé visible ;
 *    2) vers un point au hasard dans la moitié opposée à la fin du 1er tracé ; atteint : même embuscade, 3e tracé visible ;
 *    3) vers un point au hasard à moins de Final Center Radius du centre de l'île ; atteint : la boussole (objectif de l'île) et Reward Classes
 *       (2 coffres) apparaissent, la carte disparaît.
 *  Seul le tracé en cours est affiché (pointillés) et seule sa fin compte.
 */
UCLASS(meta=(PrioritizeCategories="Interaction ObjectiveMap TreasureMap"))
class AYSObjectiveTreasureMap : public AYSTreasureMap
{
	GENERATED_BODY()

public:

	AYSObjectiveTreasureMap();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 *  Tracé au sol de From à To (réglages de cette carte, utilisable sur le CDO) : marche guidée par pas de Leg Point Spacing environ vers la
	 *  cible (le point au sol valide qui rapproche le plus, un peu de travers : look de carte ; contourne les massifs), sol vérifié entre les
	 *  points ; sans tracé après Leg Attempts essais : ligne droite (points posés au sol quand même). OutPoints ne contient pas From.
	 */
	void BuildLeg(const UWorld* World, const FVector& From, const FVector& To, TArray<FVector>& OutPoints) const;

	/** Serveur, avant FinishSpawning : chemin complet, index du dernier point de chaque tracé, île et objectif (boussole) à poser à la fin */
	void InitObjectiveMap(const TArray<FVector>& Points, const TArray<int32>& InLegEnds, AYSIsland* InIsland, int32 InObjectiveIndex);

	float GetFinalCenterRadius() const { return FinalCenterRadius; }

	/** Distance minimale entre le début d'un tracé et sa fin (le point au hasard dans la moitié opposée) */
	float GetMinLegLength() const { return MinLegLength; }

	float GetPathScale() const { return PathScale; }

	/** Tracé en cours (0 = premier) */
	int32 GetCurrentLeg() const;

	/** Debug (serveur) : le porteur est posé à la fin du tracé en cours */
	void DebugCompleteLeg();

protected:

	/** Distance visée entre deux points d'un tracé */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ObjectiveMap|Legs", meta=(ClampMin="200", Units="Centimeters"))
	float LegPointSpacing = 2000.0f;

	/** Écart de hauteur toléré entre deux points voisins d'un tracé (relief de l'île) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ObjectiveMap|Legs", meta=(ClampMin="0", Units="Centimeters"))
	float LegMaxHeightDifference = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ObjectiveMap|Legs", meta=(ClampMin="1"))
	int32 LegAttempts = 25;

	/** Distance minimale entre le début d'un tracé et le point au hasard de la moitié opposée */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ObjectiveMap|Legs", meta=(ClampMin="0", Units="Centimeters"))
	float MinLegLength = 3000.0f;

	/**
	 *  Longueur du chemin : les fins des tracés sont tirées comme sur l'île entière (moitiés opposées, Min Leg Length, Final Center Radius)
	 *  puis rapprochées du centre de l'île par ce facteur (1 = chemin d'avant le 2026-11-14 ; 0.4 ≈ moitié moins à marcher, demande de Paul)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ObjectiveMap|Legs", meta=(ClampMin="0.05", ClampMax="1"))
	float PathScale = 0.4f;

	/** 3e tracé : fin à moins de cette distance du centre de l'île (avant Path Scale) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ObjectiveMap|Legs", meta=(ClampMin="0", Units="Centimeters"))
	float FinalCenterRadius = 2500.0f;

	/** Embuscade à la fin des tracés 1 et 2 : NME pour 1 joueur (x Spawn Multiplier du nombre de joueurs) ; défaut BP : 20 Skull, 7 Skull Gun, 3 Skull Bomb */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ObjectiveMap|Ambush")
	TArray<FYSGalionCrewGroup> AmbushGroups;

	/** Embuscade : NME posés en cercle autour de la fin du tracé, entre ces distances */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ObjectiveMap|Ambush", meta=(ClampMin="0", Units="Centimeters"))
	float AmbushMinRadius = 1300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ObjectiveMap|Ambush", meta=(ClampMin="0", Units="Centimeters"))
	float AmbushMaxRadius = 2300.0f;

	/** Index (dans le chemin) du dernier point de chaque tracé */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="ObjectiveMap")
	TArray<int32> LegEnds;

	virtual int32 GetMaxReachablePointIndex() const override;

	virtual int32 GetLastVisiblePointIndex() const override { return GetMaxReachablePointIndex(); }

	/** Jamais avant le début du tracé en cours : les tracés déjà validés (embuscade) ne sont plus affichés */
	virtual int32 GetFirstVisiblePointIndex() const override;

	virtual void OnPointsReached(int32 PreviousReached) override;

	virtual void Complete() override;

private:

	/** Île (serveur) qui pose la boussole à la fin, et index de son objectif */
	TWeakObjectPtr<AYSIsland> OwnerIsland;
	int32 ObjectiveIndex = INDEX_NONE;

	/** Embuscade autour de Center (serveur) */
	void SpawnAmbush(const FVector& Center, int32 Leg);
};
