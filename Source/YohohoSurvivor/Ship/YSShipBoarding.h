#pragma once

#include "CoreMinimal.h"
#include "Interaction/YSInteractiveObject.h"
#include "YSShipBoarding.generated.h"

class AYSPirateShip;
class UMaterialInterface;

/**
 *  Accès du bateau (acteur enfant BoardingAccess du bateau, à l'ouverture du bastingage / planche d'embarquement) : un joueur
 *  à côté (à pied, sur le ponton, ou EN NAGEANT) appuie sur E et est reposé sur le pont (Board Point).
 *  Zone d'interaction = rectangle à plat devant l'acteur : de son origine (flanc du bateau) jusqu'à Access Reach vers +X
 *  (le large), sur +/- Access Half Width ; + Interaction Distance du personnage autour. Pas pour un joueur déjà sur ce bateau.
 *  Indisponible tant que le bateau est absent. Serveur autoritaire.
 */
UCLASS(meta=(PrioritizeCategories="Boarding Interaction"))
class AYSShipBoarding : public AYSInteractiveObject
{
	GENERATED_BODY()

private:

	/** Où le joueur est reposé (ses pieds, sur le pont), tourné vers son +X */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> BoardPoint;

	/** Échelle de corde (Show Ladder) : les deux cordes et les barreaux, contre le flanc */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<class UInstancedStaticMeshComponent> LadderRopes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<class UInstancedStaticMeshComponent> LadderRungs;

protected:

	/**
	 *  Arrivée à bord : Board Point s'il est libre, sinon la première place libre sur le dessus du bateau (sol tracé depuis Landing Search
	 *  Height au-dessus de la flottaison, capsule du personnage qui y tient), en avançant vers l'intérieur jusqu'à Landing Search Depth
	 *  et le long de la coque, par pas de Landing Search Step (échelles du fond : sinon dans la cabine, joueur bloqué)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boarding", meta=(ClampMin="0", Units="Centimeters"))
	float LandingSearchHeight = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boarding", meta=(ClampMin="0", Units="Centimeters"))
	float LandingSearchDepth = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boarding", meta=(ClampMin="10", Units="Centimeters"))
	float LandingSearchStep = 100.0f;

public:

	/** Point d'arrivée des pieds (serveur) */
	FVector FindLandingPoint(const AYSPlayerCharacter* Character) const;

	/** Debug : le personnage tiendrait-il debout au point prévu (Board Point) */
	bool IsBoardPointFree(const AYSPlayerCharacter* Character) const;

	FVector GetBoardPointLocation() const;

protected:

	/** Échelle de corde affichée contre le flanc, de la mer au-dessus du bastingage (bateaux des joueurs ; visuel seulement) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boarding|Ladder")
	bool bShowLadder = false;

	/** Écart de l'échelle devant le flanc (+X, vers le large) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boarding|Ladder", meta=(Units="Centimeters", EditCondition="bShowLadder"))
	float LadderOffset = 8.0f;

	/** Demi-largeur (entre les deux cordes) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boarding|Ladder", meta=(ClampMin="1", Units="Centimeters", EditCondition="bShowLadder"))
	float LadderHalfWidth = 35.0f;

	/** Bas et haut de l'échelle (hauteur depuis la mer ; pont à +210, bastingage à +240) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boarding|Ladder", meta=(Units="Centimeters", EditCondition="bShowLadder"))
	float LadderBottom = -60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boarding|Ladder", meta=(Units="Centimeters", EditCondition="bShowLadder"))
	float LadderTop = 250.0f;

	/** Écart entre deux barreaux */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boarding|Ladder", meta=(ClampMin="5", Units="Centimeters", EditCondition="bShowLadder"))
	float RungSpacing = 32.0f;

	/** Diamètre des cordes, épaisseur des barreaux */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boarding|Ladder", meta=(ClampMin="0.5", Units="Centimeters", EditCondition="bShowLadder"))
	float RopeDiameter = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boarding|Ladder", meta=(ClampMin="0.5", Units="Centimeters", EditCondition="bShowLadder"))
	float RungThickness = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boarding|Ladder", meta=(EditCondition="bShowLadder"))
	TObjectPtr<UMaterialInterface> RopeMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boarding|Ladder", meta=(EditCondition="bShowLadder"))
	TObjectPtr<UMaterialInterface> RungMaterial;

	/** Cordes et barreaux reconstruits (éditeur et jeu, local) */
	void BuildLadder();

	/** Longueur de la zone d'accès depuis le flanc vers le large (+X de l'acteur) : couvre la planche d'embarquement */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boarding", meta=(ClampMin="0", Units="Centimeters"))
	float AccessReach = 600.0f;

	/** Demi-largeur de la zone d'accès (le long du flanc) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boarding", meta=(ClampMin="0", Units="Centimeters"))
	float AccessHalfWidth = 350.0f;

	/** Un joueur qui nage peut monter à bord */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boarding")
	bool bAllowSwimmers = true;

	/** Un joueur à pied (ponton, planche, plage) peut aussi monter d'un coup avec E */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boarding")
	bool bAllowWalkers = true;

public:

	AYSShipBoarding();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;

	/** Bateau présent, joueur vivant, mains libres, à aucun poste, pas déjà sur ce bateau */
	virtual bool CanInteract(const AYSPlayerCharacter* Character) const override;

	/** Le joueur est reposé sur Board Point (serveur) */
	virtual void Interact(AYSPlayerCharacter* Character) override;

	/** Point le plus proche de la zone d'accès */
	virtual FVector GetInteractLocation(const FVector& From) const override;

	/** Priorité entre objets à portée : point le plus proche du flanc (bord de la zone côté bateau) */
	virtual FVector GetInteractPriorityLocation(const FVector& From) const override;

	/** Zone large le long du flanc : pas besoin de la regarder */
	virtual bool RequiresFacing() const override { return false; }

	AYSPirateShip* GetShip() const;
};
