#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSRollAndBombAbility.h"
#include "YSRollAbility.generated.h"

/**
 *  « Roll » : MoveAction de base de tous les joueurs (touche Espace, composant MoveActionAbility du personnage, pas d'une arme).
 *  La roulade du Roll and Bomb (réglages Rolling Shot|Roll et Roll Anim : vitesse, invulnérabilité, traversée des NME), sans bombe.
 *  Coûte Stamina Cost de stamina (impossible en dessous), n'a pas de recharge propre (seulement la durée de la roulade).
 *  Priorités : ne part pas pendant une Ability, interrompt l'attaque auto et bloque l'Ability et l'attaque auto le temps de la roulade.
 *  Impossible en portant un objet, en nageant ou à un poste de bateau.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Roll"))
class UYSRollAbility : public UYSRollAndBombAbility
{
	GENERATED_BODY()

public:

	/** Stamina consommée par la roulade (cran de la barre de stamina) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move Action", meta=(ClampMin="0"))
	float StaminaCost = 75.0f;

	UYSRollAbility();

	virtual bool IsMoveAction() const override { return true; }

	virtual float GetMoveActionStaminaCost() const override { return StaminaCost; }

	/** Conditions communes des MoveActions : assez de stamina (Cost), pas d'objet porté, pas dans l'eau, pas à un poste */
	static bool CanCharacterUseMoveAction(const class AYSPlayerCharacter* Character, float Cost);

	virtual bool IsUninterruptible() const override { return true; }

	/** Assez de stamina, mains libres, pas dans l'eau */
	virtual bool CanActivateNow() const override;

	virtual float GetCooldownMultiplier() const override { return 1.0f; }

	/** Roulade, puis Cooldown (0 par défaut : seule la stamina limite) */
	virtual float GetCooldownDuration() const override { return RollDuration + AimDuration + Cooldown; }

	virtual float GetBaseDamage() const override { return 0.0f; }

#if WITH_EDITOR
	virtual void GetUnusedProperties(TArray<FName>& OutNames) const override;
#endif

	virtual void OnActionCancelled() override;

protected:

	/** Client : roulade et coût en stamina prédits */
	virtual void OnActivationPredicted(const FVector& TargetLocation) override;

	/** Serveur : stamina consommée, roulade */
	virtual void ExecuteAbility(const FVector& TargetLocation) override;
};
