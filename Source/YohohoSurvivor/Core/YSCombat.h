#pragma once

#include "CoreMinimal.h"

class UPrimitiveComponent;

/**
 *  Collisions de combat (requêtes uniquement, aucun blocage : le mouvement reste géré par la capsule) :
 *  - Hurtbox : zone où une entité reçoit les coups = le cylindre du corps (BodyMesh). Type Pawn, détecte les WorldDynamic.
 *  - Hitbox : zone qui inflige des coups (contact des NME). Type WorldDynamic, détecte les Pawn
 *    (filtrer ensuite sur la hurtbox de la cible : la capsule de mouvement est aussi de type Pawn).
 *  Les projectiles (WorldDynamic, détectent les Pawn) touchent donc aussi les hurtbox ; l'épée interroge les Pawn.
 */
namespace YSCombat
{
	void ConfigureHurtbox(UPrimitiveComponent* Component);

	void ConfigureHitbox(UPrimitiveComponent* Component);
}
