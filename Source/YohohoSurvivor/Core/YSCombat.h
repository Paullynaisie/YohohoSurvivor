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

	/**
	 *  Force du pushback d'un coup de Damage sur une cible de TargetMaxHealth PV max, selon la part de vie retirée :
	 *  sous MinRatio rien, à partir de MaxRatio MaxForce, entre les deux de MinForce à MaxForce (linéaire). Joueurs et NME.
	 */
	float ComputePushbackForce(float Damage, float TargetMaxHealth, float MinRatio, float MaxRatio, float MinForce, float MaxForce);
}
