#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSAbilityComponent.h"
#include "Weapons/YSProjectile.h"
#include "YSCoupDPetoirAbility.generated.h"

class AYSProjectile;
class AYSPlayerBomb;

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

	/**
	 *  Shoot system (attaque auto : ce qu'améliore l'upgrade Shoot system et la règle des critiques) :
	 *  PewPew = balles par rafale, critique une balle sur N ; Pierce = les balles traversent les NME, critique sur un NME bumpé ;
	 *  Explosif = chaque balle explose à l'impact (Explosion Class), rayon des explosions
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoot System")
	EYSShootSystem ShootSystem = EYSShootSystem::PewPew;

	/**
	 *  Tir en éventail (Shotgun, Dragon breath) : à chaque tir, Spread Ammo Count AMMO partent d'un coup, réparties à angles égaux
	 *  entre -Spread et +Spread autour de la direction de la cible (pas de rafale) ; sinon rafale de Bullets Per Burst balles
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoot System")
	bool bSpreadFire = false;

	/** Rafale : balles tirées par activation (en akimbo, réparties en alternance entre les deux armes, pas doublées) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoot System", meta=(ClampMin="1", EditCondition="!bSpreadFire", EditConditionHides))
	int32 BulletsPerBurst = 1;

	/** Rafale : durée (de la première à la dernière balle, réparties uniformément) ; la recharge commence après */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoot System", meta=(ClampMin="0", Units="Seconds", EditCondition="!bSpreadFire", EditConditionHides))
	float BurstDuration = 0.0f;

	/** Éventail : AMMO tirées d'un coup à chaque tir (+ Gros sel ++) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoot System", meta=(DisplayName="Spread Ammo Count", ClampMin="1", EditCondition="bSpreadFire", EditConditionHides))
	int32 ShotgunAmmoPerShot = 5;

	/** Éventail : angle de chaque côté de la direction de la cible ; les AMMO sont réparties à angles égaux de -Spread à +Spread */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoot System", meta=(DisplayName="Spread", ClampMin="0", ClampMax="180", Units="Degrees", EditCondition="bSpreadFire", EditConditionHides))
	float ShotgunSpread = 20.0f;

	/** Pierce : NME touchés par chaque balle avant qu'elle disparaisse (2 = traverse une fois ; + upgrade Shoot system) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoot System", meta=(ClampMin="1", EditCondition="ShootSystem==EYSShootSystem::Pierce", EditConditionHides))
	int32 PierceMaxHits = 2;

	/** Explosif : explosion à l'impact de la balle (BP enfant de BP_CannoneerBomb : rayon, effet ; elle explose tout de suite) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoot System|Explosive", meta=(EditCondition="ShootSystem==EYSShootSystem::Explosive", EditConditionHides))
	TSubclassOf<AYSPlayerBomb> ExplosionClass;

	/** Explosif : dégâts de l'explosion à chaque NME touché (x Damage bonus, comme les dégâts d'impact) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoot System|Explosive", meta=(ClampMin="0", EditCondition="ShootSystem==EYSShootSystem::Explosive", EditConditionHides))
	float ExplosionDamage = 50.0f;

	/** Explosif + upgrade MineShooter : mine collée au NME touché à la place de l'explosion (BP enfant de BP_CannoneerBomb) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoot System|Explosive", meta=(EditCondition="ShootSystem==EYSShootSystem::Explosive", EditConditionHides))
	TSubclassOf<AYSPlayerBomb> StickyMineClass;

	/** MineShooter : délai entre la dernière mine collée sur un NME et l'explosion de toutes ses mines */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoot System|Explosive", meta=(ClampMin="0", Units="Seconds", EditCondition="ShootSystem==EYSShootSystem::Explosive", EditConditionHides))
	float StickyMineFuse = 5.0f;

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

	/** Courbe du coup (EaseOut : 1 = vitesse constante, plus grand = départ plus sec) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin="1"))
	float RecoilKickEaseExponent = 2.0f;

	/** Courbe du retour en place (EaseInOut : 1 = vitesse constante, plus grand = plus doux au début et à la fin) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin="1"))
	float RecoilRecoverEaseExponent = 2.0f;

	/**
	 *  Rafale : si la balle suivante de la même arme part avant la fin du recul (coup + retour), le recul est accéléré
	 *  pour se jouer entièrement d'ici là (l'écart entre les tirs ne change pas). La dernière balle joue le recul normal.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil")
	bool bFitRecoilToFireRate = true;

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
	virtual float GetCooldownDuration() const override { return GetBurstTime() + Cooldown * GetCooldownMultiplier(); }

	/** Durée de la rafale (Burst Duration ; éventail : 0, tout part d'un coup) */
	float GetBurstTime() const { return bSpreadFire ? 0.0f : BurstDuration; }

	/** Attaque auto de Gun : Shoot System */
	virtual bool GetShootSystem(EYSShootSystem& OutSystem) const override;

	/** Priorités : le tir est « en cours » pendant la rafale puis l'animation de recul de la dernière balle */
	virtual float GetActionDuration() const override { return RecoilKickDuration + RecoilRecoverDuration; }

	virtual bool IsActionInProgress() const override;

	/** Bloque les actions moins prioritaires pendant la rafale seulement (pas pendant le recul de la dernière balle) */
	virtual float GetPriorityBlockDuration() const override { return GetBurstTime(); }

	virtual bool IsBlockingLowerPriority() const override;

	/**
	 *  Balles par activation : rafale = Bullets Per Burst (+ upgrade Shoot system en PewPew) ;
	 *  éventail = Spread Ammo Count (+ Gros sel ++ pour l'attaque auto)
	 */
	int32 GetBulletsPerBurst() const;

	/** Pierce : NME touchés par chaque balle = Pierce Max Hits + upgrade Shoot system ; 1 pour les autres shoot systems */
	int32 GetMaxHitsPerBullet() const;

	/** Démarre une rafale vers TargetLocation (serveur ; ExecuteAbility, ou l'autre arme en akimbo) */
	void StartBurst(const FVector& TargetLocation);

	/**
	 *  Tire une balle vers TargetLocation (serveur) : projectile + recul sur toutes les machines ;
	 *  TimeToNextShot = délai avant la balle suivante de cette arme (0 = aucune : recul complet)
	 */
	void FireBullet(const FVector& TargetLocation, int32 BulletIndex, float TimeToNextShot = 0.0f, float BurstRemaining = 0.0f);

	/** Rafale en cours d'après la dernière balle vue par cette machine (toutes les machines ; ralentissement de l'attaque auto) */
	bool IsBurstInProgressLocally() const;

protected:

	/** Démarre la rafale (serveur) */
	virtual void ExecuteAbility(const FVector& TargetLocation) override;

	/** Client qui active : recul de la première balle (prédite) adapté à la cadence de la rafale */
	virtual void OnActivationPredicted(const FVector& TargetLocation) override;

	virtual void OnActionCancelled() override;

	/** La première balle part à l'activation (le client qui active la prédit) ; faux si le tir vient plus tard (Rolling shot) */
	virtual bool PredictsFirstBullet() const { return true; }

	/** Réglages en plus d'une balle avant son apparition (serveur ; ex. Roll, bomb and shot : fait exploser les bombes) */
	virtual void ConfigureProjectile(AYSProjectile* Projectile) const {}

	/** Point visé par la balle numéro BulletIndex de la rafale (par défaut : le point visé à l'activation) */
	virtual FVector GetBurstTargetLocation(int32 BulletIndex) const { return BurstTargetLocation; }

	/** Arme qui tire la balle BulletIndex : en akimbo (attaque auto), alternance main principale / autre main */
	virtual UYSCoupDPetoirAbility* GetBulletShooter(int32 BulletIndex) const { return const_cast<UYSCoupDPetoirAbility*>(this); }

	/** Recul d'une balle sur toutes les machines (le tireur qui a prédit la première ne la rejoue pas) ; BurstRemaining = temps avant la dernière balle de la rafale */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBulletFired(int32 BulletIndex, float TimeToNextShot, float BurstRemaining);

	/** Animation de rechargement sur toutes les machines */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastReloadStarted();

private:

	/** Tire la balle suivante de la rafale, programme la suivante ou lance le rechargement (serveur) */
	void FireNextBullet();

	/** Éventail : toutes les AMMO d'un coup vers TargetLocation, réparties à angles égaux, un seul recul (serveur) */
	void FireShotgun(const FVector& TargetLocation);

	/** Gros sel de l'éventail en cours (dégâts en plus par AMMO sur une même cible, critique à partir de N) */
	TSharedPtr<FYSShotVolley> CurrentVolley;

	/**
	 *  Départ et direction (à plat) d'un tir vers TargetLocation : bouche de l'arme, visée assistée (un NME proche de la
	 *  direction visée remplace le point visé) ; faux si l'action ne peut pas tirer
	 */
	bool ComputeShot(const FVector& TargetLocation, FVector& OutMuzzle, FVector& OutDirection) const;

	/** Lance un projectile depuis Muzzle dans Direction, avec les dégâts et les upgrades de l'action (serveur) */
	void SpawnProjectile(const FVector& Muzzle, const FVector& Direction);

	/** Animation de rechargement des armes qui ont tiré (fin de rafale / tir) */
	void PlayReloadAnims();

	/** Délai entre la balle BulletIndex et la suivante tirée par la même arme dans une rafale de BulletCount balles (0 = aucune) */
	float GetTimeToNextShot(int32 BulletIndex, int32 BulletCount) const;

	/** Accélération du recul (1 = normal, < 1 = plus rapide) pour qu'il finisse avant la balle suivante */
	float ComputeRecoilTimeScale(float TimeToNextShot) const;

	/** Accélération du recul de la dernière balle vue par cette machine */
	float RecoilTimeScale = 1.0f;

	/** Point visé à l'activation */
	FVector BurstTargetLocation = FVector::ZeroVector;

	/** Balles déjà tirées dans la rafale en cours */
	int32 BulletsFired = 0;

	/** Balles de la rafale en cours (fixé au départ) */
	int32 BurstBulletCount = 1;

	/** Armes qui ont tiré dans la rafale en cours (pour l'animation de rechargement) */
	TArray<TWeakObjectPtr<UYSCoupDPetoirAbility>> BurstShooters;

	FTimerHandle BurstTimer;

	/** Dernier début de rechargement vu par cette machine (temps local) */
	double LastReloadTime = -1.0e6;

	/** Dernière balle vue par cette machine (temps local ; recul) */
	double LastBulletTime = -1.0e6;

	/** Fin de la rafale en cours vue par cette machine (temps local) */
	double BurstEndTime = -1.0e6;
};
