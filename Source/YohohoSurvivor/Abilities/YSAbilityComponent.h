#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "YSAbilityComponent.generated.h"

class UTexture2D;

/**
 *  Base des capacités du joueur : temps de recharge, activation réseau et données d'UI.
 *  Flux : le client local appelle TryActivate -> RPC serveur -> le serveur vérifie la recharge
 *  et exécute ExecuteAbility. Le temps de recharge est prédit côté client puis corrigé par réplication.
 *  Les paramètres sont BlueprintReadWrite pour pouvoir être modifiés par les upgrades.
 */
UCLASS(Abstract, ClassGroup=(YS))
class UYSAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Nom affiché */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability")
	FText DisplayName;

	/** Temps de recharge */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Ability", meta=(ClampMin="0", Units="Seconds"))
	float Cooldown = 5.0f;

	/** Icône affichée dans l'UI (optionnelle : sans icône, un carré de couleur IconTint) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability|UI")
	TObjectPtr<UTexture2D> Icon;

	/** Teinte de l'icône */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability|UI")
	FLinearColor IconTint = FLinearColor::White;

protected:

	/** Instant (en temps serveur) où la capacité redevient disponible */
	UPROPERTY(Replicated)
	double ReadyServerTime = 0.0;

public:

	UYSAbilityComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Tente d'activer la capacité vers un point du monde (à appeler sur le client qui contrôle le pawn) */
	UFUNCTION(BlueprintCallable, Category="Ability")
	void TryActivate(const FVector& TargetLocation);

	/** Secondes restantes avant que la capacité soit disponible (0 = prête) */
	UFUNCTION(BlueprintPure, Category="Ability")
	float GetCooldownRemaining() const;

	UFUNCTION(BlueprintPure, Category="Ability")
	bool IsReady() const { return GetCooldownRemaining() <= 0.0f; }

	/** Secondes depuis la dernière activation vue par cette machine (très grand si jamais activée) ; sert aux effets visuels */
	UFUNCTION(BlueprintPure, Category="Ability")
	float GetTimeSinceActivation() const;

protected:

	/** Effets visuels de l'activation sur toutes les machines (le client qui tire les a déjà joués par prédiction) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastActivated();

	/** Note l'instant de l'activation pour les effets visuels locaux */
	void NotifyActivated();

	/** Vrai sur le client qui contrôle le tireur (hors serveur) : il joue les effets sans attendre le serveur */
	bool IsLocallyPredicted() const;

private:

	/** Instant (temps local) de la dernière activation vue par cette machine */
	double LastActivationTime = -1.0e6;

protected:

	/** Effet de la capacité, exécuté uniquement sur le serveur */
	virtual void ExecuteAbility(const FVector& TargetLocation) {}

	UFUNCTION(Server, Reliable)
	void ServerActivate(FVector_NetQuantize TargetLocation);

	/** Vérifie la recharge, la relance et exécute la capacité (serveur) */
	void ActivateOnServer(const FVector& TargetLocation);

	/** Temps du monde synchronisé avec le serveur */
	double GetServerTime() const;
};
