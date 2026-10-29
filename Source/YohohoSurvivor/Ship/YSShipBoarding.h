#pragma once

#include "CoreMinimal.h"
#include "Interaction/YSInteractiveObject.h"
#include "YSShipBoarding.generated.h"

class AYSPirateShip;

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

protected:

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

	/** Bateau présent, joueur vivant, mains libres, à aucun poste, pas déjà sur ce bateau */
	virtual bool CanInteract(const AYSPlayerCharacter* Character) const override;

	/** Le joueur est reposé sur Board Point (serveur) */
	virtual void Interact(AYSPlayerCharacter* Character) override;

	/** Point le plus proche de la zone d'accès */
	virtual FVector GetInteractLocation(const FVector& From) const override;

	/** Zone large le long du flanc : pas besoin de la regarder */
	virtual bool RequiresFacing() const override { return false; }

	AYSPirateShip* GetShip() const;
};
