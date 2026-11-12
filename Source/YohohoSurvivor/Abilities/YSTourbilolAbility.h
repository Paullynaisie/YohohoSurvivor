#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSAutoSwingAbility.h"
#include "YSTourbilolAbility.generated.h"

class AYSPlayerBomb;

/** État du tourbilol en cours, répliqué (le serveur ajoute les tours de Tourbillon infinie) */
USTRUCT()
struct FYSTourbilolState
{
	GENERATED_BODY()

	/** Tours de ce tourbilol (Base Turns + tours achetés en stamina) */
	UPROPERTY()
	int32 TotalTurns = 0;

	/** Golden Tourbillon infinie débloqué à l'activation : saut puis impact à la fin */
	UPROPERTY()
	bool bGolden = false;

	/** Change à chaque activation (réplication même si les valeurs sont identiques) */
	UPROPERTY()
	uint8 Counter = 0;
};

/**
 *  « TOURBILOL ! » : Ability du Boulet et Chaîne (BP_MorningStar ; Auto swing en Ability : dégâts x Damage bonus de l'Ability).
 *  À l'activation, le personnage tourne sur lui-même Base Turns tours (Turn Duration chacun), en partant de son orientation,
 *  en se déplaçant librement à Move Speed Ratio de sa vitesse de base (ignore tous les bonus et ralentissements) ; il ne suit
 *  plus la souris et la caméra se recentre (décentrée à la fin). La hitbox de l'arme (le boulet) frappe pendant tout le
 *  tourbilol : Damage (50) par coup, chaque NME au plus une fois par tour. Pas d'attaque auto pendant le tourbilol (priorité
 *  de l'Ability) ; avec Yoloooooo, la hitbox permanente de l'attaque auto continue en plus. La recharge (Cooldown 10) part à la fin.
 *  Tourbillon infinie (upgrade d'arme) : clic droit maintenu en entrant dans le dernier tour -> un tour de plus pour
 *  Infinite Turn Stamina Cost (- ++) de stamina, tant que le clic est maintenu et la stamina suffit.
 *  Golden : à la fin, saut (Golden Jump Velocity) puis impact à l'atterrissage (Golden Explosion Class, dégâts = stamina consommée
 *  x Golden Damage Per Stamina x Damage bonus de l'Ability).
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="TOURBILOL !"))
class UYSTourbilolAbility : public UYSAutoSwingAbility
{
	GENERATED_BODY()

public:

	/** Tours de base */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tourbilol", meta=(ClampMin="1"))
	int32 BaseTurns = 3;

	/** Durée d'un tour */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tourbilol", meta=(ClampMin="0.05", Units="Seconds"))
	float TurnDuration = 0.5f;

	/** Sens de rotation (vu de dessus) : horaire si vrai */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tourbilol")
	bool bClockwise = true;

	/** Vitesse pendant le tourbilol = vitesse de base du personnage (Move Speed) x ce ratio, sans aucun autre bonus ni ralentissement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tourbilol", meta=(ClampMin="0"))
	float MoveSpeedRatio = 1.0f;

	/** Caméra recentrée sur le personnage pendant le tourbilol */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tourbilol")
	bool bCenterCamera = true;

	/** Pose de la main pendant le tourbilol (sinon pose normale : le boulet tourne au bout de sa chaîne) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tourbilol|Hand Pose")
	bool bUseSpinHandPose = false;

	/** Pose de la main (droite, repère du corps) pendant le tourbilol */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tourbilol|Hand Pose", meta=(EditCondition="bUseSpinHandPose"))
	FVector SpinHandLocation = FVector(40.0f, 40.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tourbilol|Hand Pose", meta=(EditCondition="bUseSpinHandPose"))
	FRotator SpinHandRotation = FRotator::ZeroRotator;

	/** Temps pour prendre / quitter la pose */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tourbilol|Hand Pose", meta=(ClampMin="0", Units="Seconds", EditCondition="bUseSpinHandPose"))
	float SpinHandBlendTime = 0.1f;

	/** Tourbillon infinie : stamina d'un tour en plus (avant la réduction des ++) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tourbilol|Infinite", meta=(ClampMin="0"))
	float InfiniteTurnStaminaCost = 30.0f;

	/** Coût minimum d'un tour en plus (après les ++) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tourbilol|Infinite", meta=(ClampMin="0"))
	float MinInfiniteTurnStaminaCost = 1.0f;

	/** Golden : vitesse verticale du saut à la fin du tourbilol */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tourbilol|Golden", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float GoldenJumpVelocity = 1100.0f;

	/** Golden : dégâts de base de l'impact par point de stamina consommé par ce tourbilol */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tourbilol|Golden", meta=(ClampMin="0"))
	float GoldenDamagePerStamina = 1.0f;

	/** Golden : explosion de l'impact (rayon, couleur, sphère : réglages du Blueprint de bombe) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tourbilol|Golden")
	TSubclassOf<AYSPlayerBomb> GoldenExplosionClass;

	UYSTourbilolAbility();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Durée du tourbilol (tours achetés compris) */
	float GetSpinDuration() const { return FMath::Max(State.TotalTurns, BaseTurns) * TurnDuration; }

	/** En train de tourner sur cette machine */
	bool IsSpinning() const;

	virtual bool IsAutomatic() const override { return false; }

	/** Recharge : après le tourbilol */
	virtual float GetCooldownDuration() const override { return GetSpinDuration() + Cooldown * GetCooldownMultiplier(); }

	virtual float GetBaseCooldown() const override { return Cooldown; }

	virtual float GetRecoverTime() const override { return 0.0f; }

	virtual bool GetLockedAimDirection(FVector& OutDirection) const override;

	virtual bool GetFixedMoveSpeedRatio(float& OutRatio) const override;

	virtual bool CentersCamera() const override { return bCenterCamera && IsSpinning(); }

	virtual bool GetAutoAttackSlowdown(float& OutMultiplier) const override { return false; }

	virtual bool EvaluateHandPose(float& OutIdleWeight, FVector& OutLocation, FQuat& OutRotation) const override;

#if WITH_EDITOR
	virtual void GetUnusedProperties(TArray<FName>& OutNames) const override;
#endif

protected:

	/** Tout le tourbilol est la « coupe » : hitbox active du début à la fin */
	virtual float GetWindupTime() const override { return 0.0f; }
	virtual float GetSwingTime() const override { return GetSpinDuration(); }

	virtual void PrepareActivation(const FVector& TargetLocation) override;

	virtual void OnActivationPredicted(const FVector& TargetLocation) override;

	virtual void ExecuteAbility(const FVector& TargetLocation) override;

private:

	UPROPERTY(Replicated)
	FYSTourbilolState State;

	/** Orientation de départ (machine qui active : client local ou hôte) */
	float StartYaw = 0.0f;

	/** Tour en cours (chaque NME peut être touché une fois par tour) */
	int32 CurrentTurn = -1;

	/** Serveur : tour pour lequel le prolongement a déjà été décidé */
	int32 ExtensionCheckedTurns = 0;

	/** Serveur : stamina consommée par ce tourbilol (Golden) */
	float StaminaSpent = 0.0f;

	/** Le tourbilol tournait à l'image précédente (fin détectée sur la machine qui déplace le personnage) */
	bool bWasSpinning = false;

	/** Golden : en l'air après le saut (serveur : impact à l'atterrissage) */
	bool bGoldenAirborne = false;
	double GoldenLaunchTime = 0.0;
	float GoldenStaminaSpent = 0.0f;

	/** Serveur : un tour de plus si Tourbillon infinie, clic droit maintenu et stamina suffisante */
	void TryExtend();

	/** Fin du tourbilol (serveur et client qui contrôle) : saut Golden */
	void OnSpinEnded();

	/** Serveur : impact Golden */
	void GoldenImpact();
};
