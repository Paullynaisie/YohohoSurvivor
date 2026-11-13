#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSRollAndBombAbility.h"
#include "YSRollAbility.generated.h"

/**
 *  « Roll » : MoveAction (touche Espace, composant MoveActionAbility du personnage, pas d'une arme ; donnée par une upgrade de MoveAction).
 *  La roulade du Roll and Bomb (réglages Rolling Shot|Roll et Roll Anim : vitesse, invulnérabilité, traversée des NME), sans bombe.
 *  Coûte Stamina Cost de stamina (impossible en dessous ; Roll ++ : - Bonus Gold Value (5) chacun, 75 -> 50), n'a pas de recharge
 *  propre (seulement la durée de la roulade).
 *  Priorités : ne part pas pendant une Ability, interrompt l'attaque auto et bloque l'Ability et l'attaque auto le temps de la roulade.
 *  Impossible en portant un objet, en nageant ou à un poste de bateau.
 *  Golden (ordre des variantes de DA_Up_MoveAction_Roll) :
 *  0 Sleight of hands : la roulade rend la recharge de l'Ability ;  1 Stunts artist : la prochaine attaque auto est critique ;
 *  2 Little Gift : « Bomb and Roll » (bombe Bomb Class aux pieds, Bomb Damage), même coût, sans recharge ;
 *  3 BunnyHop : une roulade sur Bunny Hop Interval (3) est gratuite.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Roll"))
class UYSRollAbility : public UYSRollAndBombAbility
{
	GENERATED_BODY()

public:

	/** Stamina consommée par la roulade (cran de la barre de stamina ; - bonus des Roll ++) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move Action", meta=(ClampMin="0"))
	float StaminaCost = 75.0f;

	/** BunnyHop (Golden) : une roulade sur ce nombre est gratuite (la dernière) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move Action|Golden", meta=(ClampMin="2"))
	int32 BunnyHopInterval = 3;

	/** Index des variantes Golden (ordre de l'asset DA_Up_MoveAction_Roll) */
	static constexpr int32 GoldenSleightOfHands = 0;
	static constexpr int32 GoldenStuntsArtist = 1;
	static constexpr int32 GoldenLittleGift = 2;
	static constexpr int32 GoldenBunnyHop = 3;

	UYSRollAbility();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool IsMoveAction() const override { return true; }

	/** Coût de la prochaine roulade (Roll ++, BunnyHop : 0 pour la roulade gratuite) */
	virtual float GetMoveActionStaminaCost() const override;

	virtual float GetMoveActionBonusDisplayValue(float BonusTotal) const override { return FMath::Max(0.0f, StaminaCost - BonusTotal); }

	/** Conditions communes des MoveActions : assez de stamina (Cost), pas d'objet porté, pas dans l'eau, pas à un poste */
	static bool CanCharacterUseMoveAction(const class AYSPlayerCharacter* Character, float Cost);

	virtual bool IsUninterruptible() const override { return true; }

	/** Assez de stamina, mains libres, pas dans l'eau, pas en pleine roulade */
	virtual bool CanActivateNow() const override;

	virtual float GetCooldownMultiplier() const override { return 1.0f; }

	/** Roulade, puis Cooldown (0 par défaut : seule la stamina limite) */
	virtual float GetCooldownDuration() const override { return GetRollDuration() + AimDuration + Cooldown; }

	virtual float GetBaseDamage() const override { return 0.0f; }

	/** Les ++ et Golden de l'upgrade sont ceux du Roll (faux pour une sous-classe : Shoulder bump, qui a les siens) */
	virtual bool UsesRollUpgrades() const { return true; }

#if WITH_EDITOR
	virtual void GetUnusedProperties(TArray<FName>& OutNames) const override;
#endif

	virtual void OnActionCancelled() override;

protected:

	/** Client : roulade et coût en stamina prédits */
	virtual void OnActivationPredicted(const FVector& TargetLocation) override;

	/** Serveur : stamina consommée, roulade, effets des Golden */
	virtual void ExecuteAbility(const FVector& TargetLocation) override;

	/** Roulades faites (BunnyHop ; serveur, répliqué au propriétaire pour la prédiction du coût) */
	UPROPERTY(Replicated)
	int32 RollCount = 0;
};
