#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSCoupDPetoirAbility.h"
#include "YSAutoShotAbility.generated.h"

/**
 *  « Auto shot » : Auto atk du Battle Petoir (Gun, shoot system PewPew).
 *  Serveur : dès qu'elle est prête et qu'un NME est dans le cône de visée assistée (depuis le regard du personnage),
 *  tire une rafale de Bullets Per Burst balles (3) réparties sur Burst Duration (1 s), chacune vers la cible actuelle,
 *  puis anime le rechargement et attend sa recharge (Cooldown).
 *  Akimbo : la main principale déclenche ; les balles alternent entre les deux armes (main principale d'abord)
 *  sans être doublées (3 balles = droite, gauche, droite), et les deux armes jouent le rechargement.
 *  Pas d'upgrade d'arme d'Ability (Ricochet) : seulement les upgrades d'Auto atk.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Auto shot"))
class UYSAutoShotAbility : public UYSCoupDPetoirAbility
{
	GENERATED_BODY()

public:

	UYSAutoShotAbility();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual bool IsAutomatic() const override { return true; }

	/** Rafale puis recharge (l'alternance akimbo est dans la rafale : pas de délai akimbo en plus) */
	virtual float GetActivationLockDuration() const override { return GetCooldownDuration(); }

protected:

	/** Chaque balle vise la cible actuelle (droit devant si elle a disparu) */
	virtual FVector GetBurstTargetLocation(int32 BulletIndex) const override;

	/** Akimbo : balles paires = cette arme (main principale), impaires = l'autre */
	virtual UYSCoupDPetoirAbility* GetBulletShooter(int32 BulletIndex) const override;

private:

	/** Cible actuelle dans le cône de visée assistée (depuis le regard du personnage) */
	AActor* FindAutoTarget() const;

	/** En akimbo : l'Auto shot de l'autre main si celle-ci est la principale */
	UYSAutoShotAbility* GetAkimboPartner() const;
};
