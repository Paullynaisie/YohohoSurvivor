#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSAbilityComponent.h"
#include "YSCoupDPetoirAbility.generated.h"

class AYSProjectile;

/** Shoot system d'une action de Gun */
UENUM(BlueprintType)
enum class EYSShootSystem : uint8
{
	/** Tire toutes ses balles une par une, réparties uniformément sur Burst Duration, puis recharge (Cooldown) */
	PewPew		UMETA(DisplayName="PewPew"),
	/** À venir */
	Shotgun		UMETA(Hidden),
	/** À venir */
	Piercing	UMETA(Hidden),
};

/** Axe de rotation de l'animation de rechargement (par rapport à la main) */
UENUM(BlueprintType)
enum class EYSReloadSpinAxis : uint8
{
	/** Le canon passe par-dessus (bascule avant / arrière) */
	Pitch,
	/** Rotation à plat, autour de la verticale */
	Yaw,
	/** Rotation autour de l'axe du canon */
	Roll,
};

/**
 *  Base des actions de Gun (WeaponType Gun) : « Coup d'Petoir » (Ability du pistolet), et l'Auto shot (sous-classe).
 *  Chaque tir lance un projectile depuis la bouche de l'arme, parallèle au sol, vers le point visé — ou la cible de la
 *  visée assistée s'il y en a une. Les dégâts appartiennent à l'action (et aux upgrades), pas au projectile.
 *  Shoot system PewPew : une activation = une rafale de Bullets Per Burst balles réparties uniformément sur Burst Duration
 *  (première tout de suite, dernière à la fin), puis l'animation de rechargement et la recharge (Cooldown).
 *  Chaque balle joue le recul (Recoil) sur toutes les machines.
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

	/** Shoot system (PewPew pour l'instant ; Shotgun et Piercing à venir) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoot System")
	EYSShootSystem ShootSystem = EYSShootSystem::PewPew;

	/** PewPew : balles tirées par activation (en akimbo, réparties en alternance entre les deux armes, pas doublées) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoot System", meta=(ClampMin="1"))
	int32 BulletsPerBurst = 1;

	/** PewPew : durée de la rafale (de la première à la dernière balle, réparties uniformément) ; la recharge commence après */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoot System", meta=(ClampMin="0", Units="Seconds"))
	float BurstDuration = 0.0f;

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

	/** Intensité actuelle du recul (0 = au repos, 1 = pic), d'après la dernière balle */
	UFUNCTION(BlueprintPure, Category="Recoil")
	float GetRecoilAlpha() const;

	/** Rechargement (fin de rafale) : chaque arme qui a tiré fait des tours sur elle-même */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reload Anim")
	bool bPlayReloadAnim = true;

	/** Durée de l'animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reload Anim", meta=(ClampMin="0.01", Units="Seconds"))
	float ReloadSpinDuration = 0.35f;

	/** Nombre de tours (1 = 360°) ; négatif = dans l'autre sens */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reload Anim")
	float ReloadSpinTurns = 1.0f;

	/** Axe de rotation, par rapport à la main */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reload Anim")
	EYSReloadSpinAxis ReloadSpinAxis = EYSReloadSpinAxis::Pitch;

	/** Courbe EaseInOut de la rotation (1 = vitesse constante) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reload Anim", meta=(ClampMin="1"))
	float ReloadSpinEaseExponent = 2.0f;

	/** Rotation actuelle de l'animation de rechargement (nulle au repos), à ajouter à celle de la main */
	FRotator GetReloadSpin() const;

	/** Secondes depuis le dernier début de rechargement vu par cette machine */
	float GetTimeSinceReload() const;

public:

	UYSCoupDPetoirAbility();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual float GetBaseDamage() const override { return Damage; }

	/** Indisponible pendant la rafale puis pendant la recharge (CD reduction : seulement la recharge) */
	virtual float GetCooldownDuration() const override { return BurstDuration + Cooldown * GetCooldownMultiplier(); }

	/** Priorités : le tir est « en cours » pendant la rafale puis l'animation de recul de la dernière balle */
	virtual float GetActionDuration() const override { return RecoilKickDuration + RecoilRecoverDuration; }

	virtual bool IsActionInProgress() const override;

	/** Tire une balle vers TargetLocation (serveur) : projectile + recul sur toutes les machines */
	void FireBullet(const FVector& TargetLocation, int32 BulletIndex);

protected:

	/** Démarre la rafale (serveur) */
	virtual void ExecuteAbility(const FVector& TargetLocation) override;

	virtual void OnActionCancelled() override;

	/** Point visé par la balle numéro BulletIndex de la rafale (par défaut : le point visé à l'activation) */
	virtual FVector GetBurstTargetLocation(int32 BulletIndex) const { return BurstTargetLocation; }

	/** Arme qui tire la balle BulletIndex : en akimbo (attaque auto), alternance main principale / autre main */
	virtual UYSCoupDPetoirAbility* GetBulletShooter(int32 BulletIndex) const { return const_cast<UYSCoupDPetoirAbility*>(this); }

	/** Recul d'une balle sur toutes les machines (le tireur qui a prédit la première ne la rejoue pas) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBulletFired(int32 BulletIndex);

	/** Animation de rechargement sur toutes les machines */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastReloadStarted();

private:

	/** Tire la balle suivante de la rafale, programme la suivante ou lance le rechargement (serveur) */
	void FireNextBullet();

	/** Point visé à l'activation */
	FVector BurstTargetLocation = FVector::ZeroVector;

	/** Balles déjà tirées dans la rafale en cours */
	int32 BulletsFired = 0;

	/** Armes qui ont tiré dans la rafale en cours (pour l'animation de rechargement) */
	TArray<TWeakObjectPtr<UYSCoupDPetoirAbility>> BurstShooters;

	FTimerHandle BurstTimer;

	/** Dernier début de rechargement vu par cette machine (temps local) */
	double LastReloadTime = -1.0e6;
};
