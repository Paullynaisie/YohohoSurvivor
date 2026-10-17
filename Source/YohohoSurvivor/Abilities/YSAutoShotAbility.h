#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSCoupDPetoirAbility.h"
#include "YSAutoShotAbility.generated.h"

/**
 *  « Auto shot » : Auto atk du Battle Petoir, version automatique du Coup d'Petoir (même projectile, même visée assistée).
 *  Serveur : dès qu'elle est prête et qu'un NME est dans le cône de visée assistée (depuis le regard du personnage),
 *  tire sur lui, puis attend sa recharge (Cooldown) avant de retirer.
 *  Akimbo : la main principale tire, l'autre suit après AkimboFireDelay (recharge unique, comme le Coup d'sabre).
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

private:

	/** Cible actuelle dans le cône de visée assistée (depuis le regard du personnage) */
	AActor* FindAutoTarget() const;

	/** En akimbo : l'Auto shot de l'autre main si celle-ci est la principale */
	UYSAutoShotAbility* GetAkimboPartner() const;

	/** Tire sur la cible actuelle si elle existe (serveur) */
	void FireAtTarget();
};
