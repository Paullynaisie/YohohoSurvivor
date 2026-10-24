#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSRollAndBombAbility.h"
#include "YSRollBombShotAbility.generated.h"

/**
 *  « Roll, bomb and shot » : Ability du Sniper (Gun). Roll and Bomb + Rolling shot : bombe (ou mine, « Une Mine ! ») posée aux
 *  pieds (Roll and Bomb), roulade, visée (Rolling Shot|Aim), puis tir droit devant (réglages de tir du Coup d'Petoir : Projectile
 *  Class, Damage, rafale, recul). Dégâts de la balle (Damage) et de la bombe (Bomb Damage) x Damage bonus de l'Ability.
 *  Une balle qui passe sur une bombe / mine de son tireur (Bomb Hit Radius du projectile) la fait exploser tout de suite en
 *  critique (dégâts critiques du personnage) et disparaît.
 *  Recharge : roulade + visée + rafale + Cooldown (CD reduction, Mine Cooldown Multiplier).
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Roll, bomb and shot"))
class UYSRollBombShotAbility : public UYSRollAndBombAbility
{
	GENERATED_BODY()

public:

	UYSRollBombShotAbility();

	/** Roulade + visée + rafale, puis la recharge */
	virtual float GetCooldownDuration() const override { return RollDuration + AimDuration + GetBurstTime() + Cooldown * GetCooldownMultiplier(); }

	/** Priorités : roulade + visée + recul du tir (comme le Rolling shot) */
	virtual float GetActionDuration() const override { return UYSRollingShotAbility::GetActionDuration(); }

protected:

	/** Serveur : bombe aux pieds, puis roulade, visée et tir (Rolling shot) */
	virtual void ExecuteAbility(const FVector& TargetLocation) override;

	/** La balle fait exploser (critique) les bombes de son tireur */
	virtual void ConfigureProjectile(AYSProjectile* Projectile) const override;
};
