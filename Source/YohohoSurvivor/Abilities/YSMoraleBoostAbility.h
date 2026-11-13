#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSMoveActionAbility.h"
#include "YSMoraleBoostAbility.generated.h"

/**
 *  « Booster de moral » (MoveAction) : coûte Stamina Cost (65) ; le personnage se fige Raise Hands Duration (0.5 s) en levant les
 *  mains ; tous les joueurs ET minions vivants (HiredMouss compris, lui aussi si Include Self) à moins de Radius (15 m, à plat) gagnent
 *  x Speed Multiplier de vitesse pendant Duration (3 s, + 1 s par Booster de moral ++) : AYSPlayerCharacter::ApplyMoraleBoost, pas
 *  cumulable (un boost actif garde le plus grand bonus et la fin la plus lointaine), FX au-dessus des personnages boostés.
 *  Golden (ordre des variantes de DA_Up_MoveAction_MoraleBoost), pour tous les boostés :
 *  0 Motivé pour cogner : dégâts x (1 + Golden Damage Bonus) ;  1 Motivé pour tuer : leur prochaine attaque auto est critique ;
 *  2 Motivé pour survivre : soin de Golden Heal Per Second (5 %) des PV max par seconde ;
 *  3 Motivé pour bombarder : recharge des Abilities x Golden Cooldown Speed (2) plus rapide ;
 *  4 Motivé pour courir : infliger des dégâts relance le boost (Duration).
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Booster de moral"))
class UYSMoraleBoostAbility : public UYSMoveActionAbility
{
	GENERATED_BODY()

public:

	/** Portée (à plat) autour du personnage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morale Boost", meta=(ClampMin="0", Units="Centimeters"))
	float Radius = 1500.0f;

	/** Vitesse de déplacement x ce multiplicateur (1.35 = +35 %) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morale Boost", meta=(ClampMin="0"))
	float SpeedMultiplier = 1.35f;

	/** Durée du bonus (+ bonus des Booster de moral ++) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morale Boost", meta=(ClampMin="0", Units="Seconds"))
	float Duration = 3.0f;

	/** Le personnage profite aussi du bonus */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morale Boost")
	bool bIncludeSelf = true;

	/** À l'activation : le personnage se fige ce temps, mains levées */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morale Boost|Anim", meta=(ClampMin="0", Units="Seconds"))
	float RaiseHandsDuration = 0.5f;

	/** Mains levées : pose de la main droite (repère du corps ; la gauche est symétrique) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morale Boost|Anim")
	FVector RaisedHandLocation = FVector(10.0f, 45.0f, 110.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morale Boost|Anim")
	FRotator RaisedHandRotation = FRotator(80.0f, 0.0f, 0.0f);

	/** Part de la durée pour lever (puis redescendre) les mains */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morale Boost|Anim", meta=(ClampMin="0.01", ClampMax="0.5"))
	float RaiseBlendFraction = 0.25f;

	/** Motivé pour cogner : dégâts en plus (0.35 = +35 %) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morale Boost|Golden", meta=(ClampMin="0"))
	float GoldenDamageBonus = 0.35f;

	/** Motivé pour survivre : part des PV max soignée par seconde (0.05 = 5 %) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morale Boost|Golden", meta=(ClampMin="0"))
	float GoldenHealPerSecond = 0.05f;

	/** Motivé pour bombarder : vitesse de la recharge des Abilities (2 = 2x plus vite) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morale Boost|Golden", meta=(ClampMin="1"))
	float GoldenCooldownSpeed = 2.0f;

	/** Index des variantes Golden (ordre de l'asset DA_Up_MoveAction_MoraleBoost) */
	static constexpr int32 GoldenDamageIndex = 0;
	static constexpr int32 GoldenCriticalIndex = 1;
	static constexpr int32 GoldenHealIndex = 2;
	static constexpr int32 GoldenCooldownIndex = 3;
	static constexpr int32 GoldenRunIndex = 4;

	UYSMoraleBoostAbility();

	/** Durée du boost, Booster de moral ++ compris */
	float GetBoostDuration() const { return Duration + GetMoveActionBonus(); }

	virtual float GetMoveActionBonusDisplayValue(float BonusTotal) const override { return Duration + BonusTotal; }

	/** Mains levées : en cours (bloque l'attaque auto) */
	virtual float GetActionDuration() const override { return RaiseHandsDuration; }

	virtual bool IsUninterruptible() const override { return true; }

	/** Mains levées : les armes attendent (attaque auto interrompue, Ability après) */
	virtual bool BlocksAllWeapons() const override { return RaiseHandsDuration > 0.0f; }

	/** Figé pendant les mains levées */
	virtual float GetMoveSpeedMultiplier() const override;

	virtual bool EvaluateHandPose(float& OutIdleWeight, FVector& OutLocation, FQuat& OutRotation) const override;

protected:

	virtual void ExecuteMoveAction(const FVector& TargetLocation) override;
};
