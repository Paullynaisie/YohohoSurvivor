#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSAbilityComponent.h"
#include "YSMoveActionAbility.generated.h"

/**
 *  Base des MoveActions qui ne sont pas une roulade (Run, Booster de moral) : composant ajouté au PERSONNAGE dans son Blueprint
 *  (remplace le Roll de base, voir AYSPlayerCharacter::ActiveMoveAction), touche Espace / manette B.
 *  Coûte Stamina Cost à l'activation (cran de la barre de stamina, impossible en dessous), comme le Roll : mains libres,
 *  pas dans l'eau, pas à un poste. Le client prédit le coût ; l'effet est joué par le serveur (ExecuteMoveAction).
 */
UCLASS(Abstract, ClassGroup=(YS))
class UYSMoveActionAbility : public UYSAbilityComponent
{
	GENERATED_BODY()

public:

	/** Stamina consommée à l'activation (cran de la barre de stamina) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move Action", meta=(ClampMin="0"))
	float StaminaCost = 20.0f;

	UYSMoveActionAbility();

	virtual bool IsMoveAction() const override { return true; }

	virtual float GetMoveActionStaminaCost() const override { return StaminaCost; }

	virtual bool CanActivateNow() const override;

	/** Pas de CD reduction : seule la stamina limite (Cooldown 0 par défaut) */
	virtual float GetCooldownMultiplier() const override { return 1.0f; }

	virtual void OnActionCancelled() override;

#if WITH_EDITOR
	virtual void GetUnusedProperties(TArray<FName>& OutNames) const override;
#endif

protected:

	/** Client : coût en stamina prédit */
	virtual void OnActivationPredicted(const FVector& TargetLocation) override;

	/** Serveur : stamina consommée, puis ExecuteMoveAction */
	virtual void ExecuteAbility(const FVector& TargetLocation) override;

	/** Effet de la MoveAction (serveur), après le coût en stamina */
	virtual void ExecuteMoveAction(const FVector& TargetLocation) {}

	class AYSPlayerCharacter* GetCharacter() const;
};
