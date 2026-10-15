#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSAbilityComponent.h"
#include "YSCoupDPetoirAbility.generated.h"

class AYSProjectile;

/**
 *  « Coup d'Petoir » : tir de pistolet (action de BP_Pistolet). Lance un projectile depuis la bouche de l'arme,
 *  parallèle au sol, en direction du point visé. Les dégâts appartiennent à la capacité
 *  (et seront modifiés par les upgrades), pas au projectile.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Coup d'Petoir"))
class UYSCoupDPetoirAbility : public UYSAbilityComponent
{
	GENERATED_BODY()

public:

	/** Blueprint du projectile tiré */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Coup d'Petoir")
	TSubclassOf<AYSProjectile> ProjectileClass;

	/** Dégâts infligés par chaque projectile */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Coup d'Petoir", meta=(ClampMin="0"))
	float Damage = 5.0f;

public:

	UYSCoupDPetoirAbility();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void ExecuteAbility(const FVector& TargetLocation) override;
};
