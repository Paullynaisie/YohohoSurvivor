#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "YSAbilityComponent.generated.h"

class UTexture2D;

/**
 *  Base des capacités du joueur : temps de recharge, activation réseau et données d'UI.
 *  Attaque active (ex. Coup d'Petoir) : le client local appelle TryActivate -> RPC serveur -> le serveur
 *  vérifie la recharge et exécute ExecuteAbility. Le temps de recharge est prédit côté client puis corrigé par réplication.
 *  Attaque automatique (ex. Coup d'sabre) : déclenchée par le serveur lui-même (IsAutomatic).
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

	/** Durée totale d'indisponibilité après une activation (par défaut : Cooldown x CD Reduction de la main) */
	virtual float GetCooldownDuration() const { return Cooldown * GetCooldownMultiplier(); }

	/** Recharge de base, sans CD Reduction (affichage des upgrades) */
	virtual float GetBaseCooldown() const { return Cooldown; }

	/** Multiplicateur de recharge de la main qui tient l'arme de cette capacité (CD Reduction ; 1 sinon) */
	float GetCooldownMultiplier() const;

	/** Propriété donnée à l'arme de cette capacité par une upgrade d'arme (Default si aucune) */
	float GetWeaponStat(FName Stat, float Default) const;

	/** Vrai pour une attaque automatique (déclenchée par le serveur, pas par une touche) */
	virtual bool IsAutomatic() const { return false; }

	/** Multiplicateur de dégâts de la main qui tient l'arme de cette capacité (Atk Bonus ; 1 sinon) */
	float GetDamageMultiplier() const;

	/** Dégâts de base de la capacité, sans bonus (0 si elle n'inflige pas de dégâts) */
	virtual float GetBaseDamage() const { return 0.0f; }

	/**
	 *  Cible dans un cône horizontal : le pawn le plus proche portant un des Tags, à moins de MaxDistance
	 *  de Origin et à moins de MaxAngle (de chaque côté) de Direction. Utilisé par la visée assistée et les attaques auto.
	 */
	static AActor* FindTargetInCone(const UWorld* World, const FVector& Origin, const FVector& Direction, float MaxDistance, float MaxAngle, const FGameplayTagContainer& Tags, const AActor* IgnoredActor);

	/** Pawn qui utilise la capacité : le propriétaire direct, ou celui de l'arme qui la porte */
	APawn* GetHolderPawn() const;

	/** Akimbo (même arme dans les deux mains) et cette capacité est sur l'arme de la main droite (principale) */
	bool IsAkimboMainHand() const;

	/**
	 *  Indisponibilité après une activation : GetCooldownDuration, plus en akimbo pour la main droite le délai akimbo
	 *  (la gauche agit ce délai après elle) : recharge unique, les deux mains redeviennent prêtes ensemble,
	 *  une fois les deux actions finies.
	 */
	float GetActivationLockDuration() const;

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
