#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSRollingShotAbility.h"
#include "YSRollAndBombAbility.generated.h"

class AYSPlayerBomb;

/**
 *  « Roll and Bomb » : Ability du Shotgun (Gun). Sous-classe du Rolling shot : exactement la même roulade (réglages
 *  Rolling Shot|Roll et Roll Anim : vitesse, invulnérabilité, traversée des NME, animation), mais pas de tir après.
 *  À l'activation (serveur), une bombe (Bomb Class, Blueprint réglable dans /Game/Placeable/AMMO) est posée au sol aux pieds
 *  du personnage puis explose après sa mèche : Bomb Damage x Damage bonus de l'Ability aux NME dans son rayon.
 *  Recharge : roulade + Cooldown (réduite par CD reduction). Pas de Ricochet (upgrade d'arme du Coup d'Petoir exclue).
 *  Rolling Shot|Aim : pause après la roulade (Aim Duration, 0 par défaut) à la vitesse Aim Move Speed Multiplier, qui est
 *  aussi la vitesse visée en fin de roulade (1 par défaut : la roulade retombe à la vitesse normale).
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Roll and Bomb"))
class UYSRollAndBombAbility : public UYSRollingShotAbility
{
	GENERATED_BODY()

public:

	/** Blueprint de la bombe posée (/Game/Placeable/AMMO/BP_CannoneerBomb) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roll and Bomb")
	TSubclassOf<AYSPlayerBomb> BombClass;

	/** Dégâts de l'explosion à chaque NME touché (x Damage bonus de l'Ability) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roll and Bomb", meta=(ClampMin="0"))
	float BombDamage = 50.0f;

	/** Upgrade « Une Mine ! » : Blueprint posé à la place de la bombe (/Game/Placeable/AMMO/BP_CannoneerMine) ; mêmes dégâts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roll and Bomb")
	TSubclassOf<AYSPlayerBomb> MineClass;

	/** Golden Mine : moment où la seconde mine est posée, en part de la roulade (0.5 = à mi-roulade, 1 = à la fin) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roll and Bomb", meta=(ClampMin="0", ClampMax="1"))
	float SecondMineRollFraction = 0.5f;

	UYSRollAndBombAbility();

	virtual float GetBaseDamage() const override { return BombDamage; }

	/** CD reduction x Mine Cooldown Multiplier de l'upgrade « Une Mine ! » */
	virtual float GetCooldownMultiplier() const override;

	/** Roulade (+ pause éventuelle), puis la recharge (CD reduction : seulement la recharge) */
	virtual float GetCooldownDuration() const override { return RollDuration + AimDuration + Cooldown * GetCooldownMultiplier(); }

	/** Priorités : en cours pendant la roulade (+ pause éventuelle), pas de recul */
	virtual float GetActionDuration() const override { return RollDuration + AimDuration; }

protected:

	/** Serveur : bombe aux pieds puis roulade (pas de tir) */
	virtual void ExecuteAbility(const FVector& TargetLocation) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Golden Mine : seconde mine posée pendant la roulade */
	FTimerHandle SecondMineTimer;

	/** Pose la bombe au sol sous le personnage (serveur) */
	void DropBomb();
};
