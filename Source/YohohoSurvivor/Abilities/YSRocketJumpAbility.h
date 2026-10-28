#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSMeteorJumpAbility.h"
#include "YSRocketJumpAbility.generated.h"

class AYSPlayerBomb;

/**
 *  « Rocket jump » : Ability du HandCanon de l'Artilleur (Cannoneer). Au départ, une grosse explosion aux pieds du joueur
 *  (Explosion Class : par défaut la bombe du Cannoneer, explosion immédiate, Explosion Damage x Damage bonus de l'Ability),
 *  puis exactement le saut du Meteor Jump (montée, apex, chute ; réglages Meteor Jump|Jump / Landing) mais sans coupe :
 *  pas de hitbox, pas de bump, pas d'animation de la main.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Rocket jump"))
class UYSRocketJumpAbility : public UYSMeteorJumpAbility
{
	GENERATED_BODY()

public:

	/** Explosion au départ (BP enfant de AYSPlayerBomb ; explose tout de suite, sans mèche) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket jump")
	TSubclassOf<AYSPlayerBomb> ExplosionClass;

	/** Dégâts de l'explosion à chaque NME touché (x Damage bonus de l'Ability) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rocket jump", meta=(ClampMin="0"))
	float ExplosionDamage = 150.0f;

	UYSRocketJumpAbility();

	virtual float GetBaseDamage() const override { return ExplosionDamage; }

	/** Pas de coupe : les mains gardent leur pose */
	virtual bool EvaluateHandPose(float& OutIdleWeight, FVector& OutLocation, FQuat& OutRotation) const override { return false; }

#if WITH_EDITOR
	virtual void GetUnusedProperties(TArray<FName>& OutNames) const override;
#endif

protected:

	/** Serveur : explosion aux pieds, puis le saut */
	virtual void ExecuteAbility(const FVector& TargetLocation) override;

	/** Pas de coups (ni hitbox ni bump) */
	virtual void DealSwingDamage(AActor* Target, float FinalDamage, const FVector& ShotDirection, AController* InstigatorController, bool bCritical) override {}
};
