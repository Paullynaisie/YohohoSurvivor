#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSBombThrowerAbility.h"
#include "YSAutoBombThrowAbility.generated.h"

/**
 *  « BombThrow » : attaque auto de la Battle Bomb du Saboteur (Cannoneer), shoot system Explosif.
 *  Même lancer en cloche que le BombThrower (réglages BombThrower|Throw, animation, explosion au sol), mais automatique (serveur) :
 *  clic gauche maintenu, lancée vers le NME le plus proche dans le cône (Auto Target Angle, à moins de Max Throw Distance),
 *  sinon droit devant à Max Throw Distance. Akimbo : la main principale lance, l'autre Akimbo Fire Delay plus tard.
 *  Upgrade Shoot system : rayon d'explosion des bombes x (1 + bonus).
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="BombThrow (auto)"))
class UYSAutoBombThrowAbility : public UYSBombThrowerAbility
{
	GENERATED_BODY()

public:

	/** Cône de recherche de la cible, de chaque côté du regard du personnage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Auto", meta=(ClampMin="0", ClampMax="180", Units="Degrees"))
	float AutoTargetAngle = 45.0f;

	/** Vitesse du PJ x ce multiplicateur pendant le lancer (préparation + lancer) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Auto", meta=(ClampMin="0"))
	float AutoAtkMoveSpeedMultiplier = 0.7f;

	UYSAutoBombThrowAbility();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual bool IsAutomatic() const override { return true; }

	/** Shoot system Explosif */
	virtual bool GetShootSystem(EYSShootSystem& OutSystem) const override { OutSystem = EYSShootSystem::Explosive; return true; }

	virtual bool GetAutoAttackSlowdown(float& OutMultiplier) const override;

	/** Lancer puis recharge ; l'autre main de l'akimbo est lancée par celle-ci */
	virtual float GetActivationLockDuration() const override { return GetCooldownDuration() + (IsAkimboMainHand() ? AkimboFireDelay : 0.0f); }

private:

	/** NME le plus proche dans le cône du regard, à portée de lancer */
	AActor* FindAutoTarget() const;

	/** En akimbo : la BombThrow de l'autre main si celle-ci est la principale */
	UYSAutoBombThrowAbility* GetAkimboPartner() const;
};
