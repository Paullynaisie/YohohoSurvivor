#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSDashingSlashAbility.h"
#include "YSMarkForDeleteAbility.generated.h"

class AYSProjectile;
class AYSFoeCharacter;

/**
 *  Ability de la Sword de « The Gun was better » : un Dashing Slash normal, qui devient « Mark for Delete » avec la Golden
 *  du même nom (propriété d'arme MarkForDelete, répliquée par bMarkForDelete) :
 *  1. Shot Count (3) roulades + tir à la suite, comme le Rolling shot (roulade dans la direction des touches, sinon à l'opposé
 *     du curseur ; visée immobile ; tir droit devant avec visée assistée), Shot Damage (100) x Damage bonus de l'Ability.
 *     Chaque tir garde en mémoire le premier NME touché (jamais deux fois le même ; tué : l'endroit de sa mort) ;
 *  2. le pas en arrière du Dashing Slash (à l'opposé du regard) ;
 *  3. un dash and slash (Damage, 180) par NME de la liste, dans l'ordre, chacun vers ce NME (sa position au début du dash).
 *  Recharge : Mark For Delete Cooldown (20 s, CD reduction), comptée après la durée maximale de l'enchaînement.
 *  Serveur autoritaire (tirs, liste, direction de chaque dash, envoyée aux clients) ; le client qui contrôle prédit les roulades.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Mark for Delete"))
class UYSMarkForDeleteAbility : public UYSDashingSlashAbility
{
	GENERATED_BODY()

public:

	/** Recharge de Mark for Delete (celle du Dashing Slash : Cooldown) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mark for Delete", meta=(ClampMin="0", Units="Seconds"))
	float MarkForDeleteCooldown = 20.0f;

	/** Roulades + tirs, donc dashes au plus */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mark for Delete", meta=(ClampMin="1"))
	int32 ShotCount = 3;

	/** Dégâts de chaque tir (x Damage bonus de l'Ability) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mark for Delete|Shot", meta=(ClampMin="0"))
	float ShotDamage = 100.0f;

	/** Balle tirée (depuis la bouche de l'arme de l'autre main, sinon depuis le personnage) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mark for Delete|Shot")
	TSubclassOf<AYSProjectile> ShotProjectileClass;

	/** Visée assistée du tir : portée et angle de chaque côté du regard */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mark for Delete|Shot", meta=(ClampMin="0", Units="Centimeters"))
	float ShotAssistAimDistance = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mark for Delete|Shot", meta=(ClampMin="0", ClampMax="180", Units="Degrees"))
	float ShotAssistAimAngle = 15.0f;

	/** Roulade : durée, vitesse x multiplicateur au départ (décroît jusqu'à celle de la visée), invulnérable, traverse les NME */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mark for Delete|Roll", meta=(ClampMin="0.01", Units="Seconds"))
	float RollDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mark for Delete|Roll", meta=(ClampMin="0"))
	float RollSpeedMultiplier = 2.0f;

	/** Visée : temps avant le tir, vitesse x multiplicateur (0 = immobile) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mark for Delete|Roll", meta=(ClampMin="0", Units="Seconds"))
	float AimDuration = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mark for Delete|Roll", meta=(ClampMin="0", ClampMax="1"))
	float AimMoveSpeedMultiplier = 0.0f;

	/** Animation de roulade : tours du corps, courbe, hauteur en boule */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mark for Delete|Roll")
	float RollAnimTurns = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mark for Delete|Roll", meta=(ClampMin="1"))
	float RollAnimEaseExponent = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mark for Delete|Roll", meta=(Units="Centimeters"))
	float RollAnimTuckHeight = 40.0f;

	UYSMarkForDeleteAbility();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Mark for Delete actif (Golden) */
	bool IsMarkForDelete() const { return bMarkForDelete; }

	virtual float GetCooldownDuration() const override;
	virtual float GetBaseCooldown() const override;
	virtual float GetActionDuration() const override;
	virtual float GetSwingElapsed() const override;
	virtual bool IsGrantingInvulnerability() const override;
	virtual float GetMoveSpeedMultiplier() const override;
	virtual bool GetForcedMoveDirection(FVector& OutDirection) const override;
	virtual bool OverridesMoveInput() const override;
	virtual bool GetLockedAimDirection(FVector& OutDirection) const override;
	virtual bool GetBodyRoll(float& OutAngle, float& OutHeightOffset) const override;

protected:

	virtual void OnActivationPredicted(const FVector& TargetLocation) override;
	virtual void ExecuteAbility(const FVector& TargetLocation) override;
	virtual void OnActionCancelled() override;

private:

	enum class EPhase : uint8 { None, Roll, Aim, StepBack, Waiting, Dash, Recover };

	/** Phase de l'enchaînement à Elapsed secondes de l'activation (Index : roulade ou dash ; Alpha : avancement) */
	EPhase GetSequencePhase(float Elapsed, int32& OutIndex, float& OutAlpha) const;

	float GetRollCycle() const { return RollDuration + AimDuration; }
	float GetRollsEnd() const { return ShotCount * GetRollCycle(); }
	float GetDashesStart() const { return GetRollsEnd() + StepBackDuration; }
	float GetMaxSequenceDuration() const { return GetDashesStart() + ShotCount * GetDashPhaseDuration(); }

	/** Remise à zéro de l'enchaînement (nouvelle activation) */
	void ResetSequence();

	/** Roulade Index : élan et direction (machines qui pilotent le mouvement) */
	void StartRoll(int32 Index);

	/** Serveur : tir Index */
	void FireShot(int32 Index);

	void OnShotHitFoe(AYSProjectile* Projectile, AYSFoeCharacter* Foe, bool bKilled);

	/** Serveur : fin du pas en arrière, liste figée (nombre de dashes) */
	void FinishStepBack();

	/** Serveur : début du dash Index vers son NME */
	void StartDash(int32 Index);

	/** Dash Index (Count dashes au total ; Index -1 = aucun dash) : direction envoyée à toutes les machines */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastDash(int32 Index, int32 Count, FVector_NetQuantizeNormal Direction);

	/** Propriété d'arme MarkForDelete (serveur), répliquée pour la prédiction et l'animation */
	UPROPERTY(Replicated)
	bool bMarkForDelete = false;

	struct FTarget
	{
		TWeakObjectPtr<AActor> Actor;
		FVector Location = FVector::ZeroVector;
	};

	/** Serveur : NME touchés par les tirs, dans l'ordre */
	TArray<FTarget> Targets;

	/** Dashes : nombre (-1 = pas encore connu) et directions reçues */
	int32 DashCount = -1;
	TArray<FVector> DashDirections;

	int32 RollsStarted = 0;
	int32 ShotsFired = 0;
	int32 DashesStarted = 0;
	bool bStepBackStarted = false;
	bool bStepBackFinished = false;
	bool bCancelled = false;
	float LastElapsed = -1.0f;

	/** Roulade sans touche de direction : direction forcée (à l'opposé du curseur) */
	FVector ForcedRollDirection = FVector::ZeroVector;
};
