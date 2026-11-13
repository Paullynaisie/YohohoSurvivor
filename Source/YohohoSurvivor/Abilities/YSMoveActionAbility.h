#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSAbilityComponent.h"
#include "YSMoveActionAbility.generated.h"

/**
 *  Base des MoveActions qui ne sont pas une roulade (Run, Booster de moral, Camouflage) : composant du PERSONNAGE
 *  (AYSPlayerCharacter, donné par une upgrade de MoveAction), touche Espace / manette B.
 *  Coûte GetActivationStaminaCost (Stamina Cost par défaut) à l'activation (cran de la barre de stamina, impossible en dessous),
 *  comme le Roll : mains libres (sauf AllowsCarrying), pas dans l'eau, pas à un poste. Le client prédit le coût ; l'effet est joué
 *  par le serveur (ExecuteMoveAction).
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

	virtual float GetMoveActionStaminaCost() const override { return GetActivationStaminaCost(); }

	/** Stamina consommée à l'activation (upgrades comprises) */
	virtual float GetActivationStaminaCost() const { return StaminaCost; }

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

/**
 *  MoveAction maintenue (Run, Camouflage) : l'activation (Stamina Cost) dure au moins Min Duration, puis elle continue tant que
 *  la touche est tenue (Hold To Sustain ; sinon un 2e appui l'arrête) pour Stamina Per Second (x GetSustainCostMultiplier) par seconde.
 *  S'arrête à 0 de stamina, en portant un objet (sauf AllowsCarrying), dans l'eau, à un poste, à la mort, et quand le personnage
 *  ATTAQUE ou lance une Ability (OnCancelledByAttack : effets des Golden).
 *  Réseau : le client qui contrôle la lance / l'arrête tout de suite (prédiction), le serveur fait foi (fin imposée : ClientStopSustain).
 */
UCLASS(Abstract, ClassGroup=(YS))
class UYSSustainedMoveActionAbility : public UYSMoveActionAbility
{
	GENERATED_BODY()

public:

	/** Stamina consommée par seconde de maintien, après Min Duration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move Action", meta=(ClampMin="0"))
	float StaminaPerSecond = 10.0f;

	/** Durée payée par l'activation : la MoveAction dure au moins ce temps, même touche relâchée */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move Action", meta=(ClampMin="0", Units="Seconds"))
	float MinDuration = 1.0f;

	/** Vrai : maintenue tant qu'Espace est tenu ; faux : Espace la lance, un 2e appui l'arrête (après Min Duration) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Move Action")
	bool bHoldToSustain = true;

	UYSSustainedMoveActionAbility();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void OnInputPressed(const FVector& TargetLocation) override;

	virtual void OnInputReleased() override;

	/** Pas de nouvelle activation pendant qu'elle dure */
	virtual bool CanActivateNow() const override;

	/** Le personnage attaque / lance une Ability : fin (serveur) */
	virtual void OnHolderActionStarted(UYSAbilityComponent* Action) override;

	UFUNCTION(BlueprintPure, Category="Move Action")
	bool IsSustained() const { return bSustained; }

	/** Secondes depuis le début (machine locale ; très grand si elle ne dure pas) */
	float GetSustainElapsed() const;

protected:

	virtual void OnActivationPredicted(const FVector& TargetLocation) override;

	virtual void ExecuteMoveAction(const FVector& TargetLocation) override;

	virtual void OnActivationRejected() override;

	/** Coût par seconde x ce multiplicateur (ex. Like a Shadow : x2) */
	virtual float GetSustainCostMultiplier() const { return 1.0f; }

	/** Durée minimale réelle (ex. Impulsion : celle du dash) */
	virtual float GetMinDuration() const { return MinDuration; }

	/** Début sur cette machine (serveur, client qui contrôle, autres à la réplication) */
	virtual void OnSustainStarted() {}

	/** Fin sur cette machine */
	virtual void OnSustainStopped() {}

	/** Serveur : fin provoquée par une attaque (Action), après Consumed Stamina dépensée depuis le début */
	virtual void OnCancelledByAttack(UYSAbilityComponent* Action, float Consumed) {}

	/** Nom pour les logs (« court », « se camoufle ») */
	virtual const TCHAR* GetLogVerb() const { return TEXT("maintient"); }

	/** Serveur : fin ; bNotifyOwner : le client qui contrôle l'arrête aussi (fin imposée par le serveur) */
	void StopSustain(bool bNotifyOwner, const TCHAR* Reason);

private:

	/** En cours (serveur ; client qui contrôle : sa prédiction ; autres machines : répliqué) */
	UPROPERTY(ReplicatedUsing=OnRep_Sustained)
	bool bSustained = false;

	UFUNCTION()
	void OnRep_Sustained();

	/** Début / fin sur cette machine (effets locaux) */
	void SetSustainedLocal(bool bNewSustained);

	/** Début (temps local de cette machine) */
	double SustainStartTime = -1.0e6;

	/** Machine qui contrôle : touche relâchée avant Min Duration, l'arrêt attend la fin de Min Duration */
	bool bStopRequested = false;

	/** Serveur : stamina dépensée depuis le début (activation comprise) */
	float ConsumedStamina = 0.0f;

	/** Machine qui contrôle : arrête (et le serveur) */
	void RequestStop();

	UFUNCTION(Server, Reliable)
	void ServerStopSustain();

	UFUNCTION(Client, Reliable)
	void ClientStopSustain();
};
