#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSCoupDPetoirAbility.h"
#include "YSPewPewPewAbility.generated.h"

/**
 *  « Pew pew pew » (upgrade d'arme Pew Pew Pew du Gunslinger, 2026-11-13) : Ability des pistolets, en sommeil tant que l'upgrade
 *  n'est pas débloquée (le Rolling shot s'endort alors). Cycles de 4 balles : nord, sud, est, ouest (nord = haut de l'écran hors
 *  bateau), chacune avec une visée assistée de +/- Cardinal Assist Angle, qui font une CircleBullet de Circle Bullet On Impact à
 *  chaque NME touché. First Shot Interval entre deux tirs, moins Interval Reduction Per Cycle à chaque cycle passé (minimum
 *  Min Shot Interval). Free Cycles (+ ++) cycles, puis tant que la touche est maintenue, un cycle de plus par Extra Cycle Stamina
 *  Cost de stamina (pas avec Death Blossom, où le level des CircleBullets = numéro du cycle). En akimbo, les tirs alternent
 *  entre les deux pistolets. Pendant le Pew pew pew : caméra centrée, vitesse au moins Min Move Speed Ratio (bonus gardés).
 *  La recharge (Cooldown 20) part à la fin.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Pew pew pew"))
class UYSPewPewPewAbility : public UYSCoupDPetoirAbility
{
	GENERATED_BODY()

public:

	/** Cycles de 4 tirs sans stamina (+ Pew Pew Pew ++) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pew pew pew", meta=(ClampMin="1"))
	int32 FreeCycles = 3;

	/** Écart entre deux tirs au premier cycle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pew pew pew", meta=(ClampMin="0.01", Units="Seconds"))
	float FirstShotInterval = 0.35f;

	/** Écart en moins à chaque cycle passé */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pew pew pew", meta=(ClampMin="0", Units="Seconds"))
	float IntervalReductionPerCycle = 0.02f;

	/** Écart minimal entre deux tirs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pew pew pew", meta=(ClampMin="0.01", Units="Seconds"))
	float MinShotInterval = 0.05f;

	/** Stamina d'un cycle de plus (touche maintenue après les cycles gratuits) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pew pew pew", meta=(ClampMin="0"))
	float ExtraCycleStaminaCost = 10.0f;

	/** Level de la CircleBullet déclenchée à chaque NME touché (Death Blossom : numéro du cycle) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pew pew pew", meta=(ClampMin="0", ClampMax="5"))
	int32 CircleBulletOnImpact = 1;

	/** Visée assistée de chaque tir : écart max de chaque côté de la direction (nord, sud, est, ouest) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pew pew pew", meta=(ClampMin="0", ClampMax="90", Units="Degrees"))
	float CardinalAssistAngle = 45.0f;

	/** Vitesse minimale pendant le Pew pew pew (1 = 100 % de la vitesse de base ; les bonus restent) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pew pew pew", meta=(ClampMin="0"))
	float MinMoveSpeedRatio = 1.0f;

	/** Caméra recentrée sur le personnage pendant le Pew pew pew */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pew pew pew")
	bool bCenterCamera = true;

	UYSPewPewPewAbility();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Upgrade Pew Pew Pew débloquée (répliqué) : sinon en sommeil */
	bool IsPewPewPewActive() const { return bPewPewPewActive; }

	virtual bool IsActionAvailable() const override { return bPewPewPewActive; }

	/** Écart entre les tirs du cycle Cycle (0 = le premier) */
	float GetShotInterval(int32 Cycle) const { return FMath::Max(MinShotInterval, FirstShotInterval - IntervalReductionPerCycle * Cycle); }

	/** Durée des cycles From (inclus) à To (exclu) */
	float GetCyclesDuration(int32 From, int32 To) const;

	/** Cycles gratuits (+ ++) */
	int32 GetFreeCycleCount() const;

	/** En train de tirer, vu par cette machine */
	bool IsPewing() const;

	/** Recharge : après les cycles gratuits (les cycles payés en stamina la repoussent) */
	virtual float GetCooldownDuration() const override { return GetCyclesDuration(0, GetFreeCycleCount()) + Cooldown * GetCooldownMultiplier(); }

	virtual float GetBaseCooldown() const override { return Cooldown; }

	/** Pas de délai akimbo en plus : les tirs alternent dans la séquence */
	virtual float GetActivationLockDuration() const override { return GetCooldownDuration(); }

	virtual bool IsActionInProgress() const override;

	virtual bool IsBlockingLowerPriority() const override;

	virtual bool CentersCamera() const override { return bCenterCamera && IsPewing(); }

	virtual bool GetMinMoveSpeedRatio(float& OutRatio) const override;

	virtual bool GetAutoAttackSlowdown(float& OutMultiplier) const override { return false; }

protected:

	virtual void ExecuteAbility(const FVector& TargetLocation) override;

	virtual void OnActionCancelled() override;

	/** Le premier tir n'est pas prédit (direction : nord) */
	virtual bool PredictsFirstBullet() const override { return false; }

private:

	UPROPERTY(Replicated)
	bool bPewPewPewActive = false;

	/** Fin des tirs (temps serveur, répliqué : caméra et vitesse chez les clients) */
	UPROPERTY(Replicated)
	double PewEndServerTime = 0.0;

	/** Serveur : séquence en cours */
	int32 TotalCycles = 0;
	int32 CycleIndex = 0;
	int32 ShotInCycle = 0;
	int32 ShotCount = 0;
	bool bDeathBlossom = false;
	FTimerHandle PewTimer;

	/** Serveur : tir suivant, ou cycle payé en stamina, ou fin */
	void FireNextPewShot();

	/** Serveur : fin des tirs (rechargement des pistolets) */
	void EndPew();

	/** Pistolet du tir ShotIndex : en akimbo, alternance main principale / autre main */
	UYSPewPewPewAbility* GetPewShooter(int32 ShotIndex) const;
};
