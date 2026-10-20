#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YSPirateShip.generated.h"

/**
 *  Bateau pirate : énorme bateau qui transporte les joueurs ; ils marchent sur le pont pendant qu'il avance
 *  (le pont est une base de mouvement : le CharacterMovementComponent emporte les persos posés dessus).
 *  Répliqué (déplacement serveur), composants mobiles. Pilotage et déplacement : à venir.
 *  Origine de l'acteur = ligne de flottaison (à poser au niveau de la mer). Visuel (primitives) dans le Blueprint
 *  enfant /Game/Placeable/Ship/BP_PirateShip (Tools/EditorScripts/setup_pirate_ship.py).
 */
UCLASS(meta=(PrioritizeCategories="Ship"))
class AYSPirateShip : public AActor
{
	GENERATED_BODY()

private:

	/** Racine mobile : toutes les formes du bateau y sont attachées */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Root;

protected:

	/** Nom affiché */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship")
	FText ShipName;

public:

	AYSPirateShip();

	FText GetShipName() const { return ShipName; }
};
