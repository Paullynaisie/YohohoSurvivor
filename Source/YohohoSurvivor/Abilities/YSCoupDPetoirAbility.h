#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSAbilityComponent.h"
#include "YSCoupDPetoirAbility.generated.h"

class AYSProjectile;

/**
 *  « Coup d'Petoir » : tir de pistolet (action de BP_Pistolet). Lance un projectile depuis la bouche de l'arme,
 *  parallèle au sol, en direction du point visé — ou de la cible de la visée assistée s'il y en a une.
 *  Les dégâts appartiennent à la capacité (et seront modifiés par les upgrades), pas au projectile.
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

	/** Visée assistée : portée max (depuis le tireur) d'une cible pour être prise automatiquement (0 = désactivée) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Coup d'Petoir|Assist Aim", meta=(ClampMin="0", Units="Centimeters"))
	float AssistAimDistance = 1000.0f;

	/** Visée assistée : écart d'angle max (de chaque côté) entre la direction de la souris et la cible */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Coup d'Petoir|Assist Aim", meta=(ClampMin="0", ClampMax="180", Units="Degrees"))
	float AssistAimAngle = 15.0f;

	/**
	 *  Cible de la visée assistée : parmi les acteurs touchables par le projectile (ses HitTags),
	 *  le plus proche à moins de AssistAimDistance de Origin et à moins de AssistAimAngle de AimDirection.
	 */
	UFUNCTION(BlueprintPure, Category="Coup d'Petoir|Assist Aim")
	AActor* FindAssistTarget(const FVector& Origin, const FVector& AimDirection) const;

	/** Recul à chaque tir : la main se cabre (muzzle vers le haut) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(Units="Degrees"))
	float RecoilPitch = 25.0f;

	/** Recul à chaque tir : la main recule le long de l'axe de l'arme */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin="0", Units="Centimeters"))
	float RecoilKickBack = 10.0f;

	/** Durée du coup (montée rapide, courbe EaseOut) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin="0.001", Units="Seconds"))
	float RecoilKickDuration = 0.05f;

	/** Durée du retour en place (courbe EaseInOut) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin="0.001", Units="Seconds"))
	float RecoilRecoverDuration = 0.25f;

	/** Intensité actuelle du recul (0 = au repos, 1 = pic), d'après la dernière activation */
	UFUNCTION(BlueprintPure, Category="Recoil")
	float GetRecoilAlpha() const;

public:

	UYSCoupDPetoirAbility();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual float GetBaseDamage() const override { return Damage; }

	/** Priorités : le tir est « en cours » pendant l'animation de recul */
	virtual float GetActionDuration() const override { return RecoilKickDuration + RecoilRecoverDuration; }


protected:

	virtual void ExecuteAbility(const FVector& TargetLocation) override;
};
