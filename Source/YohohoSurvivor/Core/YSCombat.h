#pragma once

#include "CoreMinimal.h"

class ACharacter;
class AActor;
class AController;
class UPrimitiveComponent;
class UWorld;

/**
 *  Collisions de combat (requêtes uniquement, aucun blocage : le mouvement reste géré par la capsule) :
 *  - Hurtbox : zone où une entité reçoit les coups = le cylindre du corps (BodyMesh). Type Pawn, détecte les WorldDynamic.
 *  - Hitbox : zone qui inflige des coups (contact des NME). Type WorldDynamic, détecte les Pawn
 *    (filtrer ensuite sur la hurtbox de la cible : la capsule de mouvement est aussi de type Pawn).
 *  Les projectiles (WorldDynamic, détectent les Pawn) touchent donc aussi les hurtbox ; l'épée interroge les Pawn.
 */
/**
 *  Canal d'objet « Foe » (DefaultEngine.ini, réponse par défaut Block) : capsule de mouvement des NME. Les NME l'ignorent
 *  entre eux (Foes Block Each Other faux : séparation douce à la place), tout le reste la bloque comme une capsule Pawn.
 */
inline constexpr ECollisionChannel ECC_YSFoe = ECC_GameTraceChannel2;

namespace YSCombat
{
	/** Capsule d'un personnage qui traverse (ou plus) les autres personnages : joueurs / minions (Pawn) et NME (Foe) */
	void SetPassThroughCharacters(UPrimitiveComponent* Capsule, bool bPassThrough);

	void ConfigureHurtbox(UPrimitiveComponent* Component);

	void ConfigureHitbox(UPrimitiveComponent* Component);

	/**
	 *  Force du pushback d'un coup de Damage sur une cible de TargetMaxHealth PV max, selon la part de vie retirée :
	 *  sous MinRatio rien, à partir de MaxRatio MaxForce, entre les deux de MinForce à MaxForce (linéaire). Joueurs et NME.
	 */
	float ComputePushbackForce(float Damage, float TargetMaxHealth, float MinRatio, float MaxRatio, float MinForce, float MaxForce);

	/**
	 *  Position à l'instant Time d'un vol en cloche de Start à Target en Duration : sommet ApexHeight au-dessus du plus haut des
	 *  deux points, gravité déduite (bombes lancées, bump des NME). Calcul identique sur toutes les machines.
	 */
	FVector GetBellLocation(const FVector& Start, const FVector& Target, float Duration, float ApexHeight, float Time);

	/** Cet acteur est le sol d'une île : Landscape (maps fixes) ou île générée (AYSIsland) */
	bool IsIslandGroundActor(const AActor* Actor);

	/** Sol de l'île (Landscape seul : les props touchés sont traversés) sous Point, hors de l'eau ; faux sinon */
	bool FindIslandGround(const UWorld* World, const FVector& Point, float ReferenceZ, const AActor* IgnoredActor, FVector& OutGround);

	/**
	 *  Point d'arrivée d'un lancer / saut de Character vers TargetLocation (BombThrower, MeteorJump) : ramené à MaxDistance,
	 *  puis on recule vers le personnage par pas de RetreatStep jusqu'à un sol de l'île hors de l'eau ; aucun = sous ses pieds.
	 */
	FVector FindLandingPoint(const ACharacter* Character, const FVector& TargetLocation, float MaxDistance, float RetreatStep);

	/**
	 *  Dégâts venant d'un poste de combat d'un bateau (AMMO de canon, obus de mortier, leurs explosions, éperonnage) : vrai pendant
	 *  que ces dégâts sont appliqués (serveur, appels synchrones dans un FBoatDamageScope). Les NME et les postes des bateaux de NME
	 *  prennent alors leur Boat Dmg Multiplier au lieu du Player Dmg Multiplier.
	 */
	bool IsBoatDamage();

	struct FBoatDamageScope
	{
		explicit FBoatDamageScope(bool bInActive);
		~FBoatDamageScope();
		FBoatDamageScope(const FBoatDamageScope&) = delete;
		FBoatDamageScope& operator=(const FBoatDamageScope&) = delete;

	private:
		bool bActive = false;
	};

	/** Multiplicateur des dégâts subis par un NME (ou un poste d'un bateau de NME) selon la source : poste de combat du bateau
	 *  (IsBoatDamage) ou personnage ; 1 pour les dégâts sans auteur (triches de l'arène, mise à mort scriptée) */
	float GetDamageTakenMultiplier(const AController* EventInstigator, float PlayerMultiplier, float BoatMultiplier);
}
