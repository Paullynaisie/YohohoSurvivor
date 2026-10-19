#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "YSAbilityComponent.generated.h"

/** Type d'action d'une arme : Ability (active, déclenchée par le joueur) ou Auto atk (passive, automatique) */
UENUM(BlueprintType)
enum class EYSActionSlot : uint8
{
	Ability,
	AutoAtk	UMETA(DisplayName="Auto atk"),
};

/** Type d'action d'arme : définit certains comportements face aux upgrades */
UENUM(BlueprintType)
enum class EYSWeaponType : uint8
{
	Melee,
	Gun,
};

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

	/** Type de l'action (Melee / Gun) : comportements face aux upgrades */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability")
	EYSWeaponType WeaponType = EYSWeaponType::Melee;

	/** Temps de recharge */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Ability", meta=(ClampMin="0", Units="Seconds"))
	float Cooldown = 5.0f;

	/** Icône affichée dans l'UI (optionnelle : sans icône, un carré de couleur IconTint) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability|UI")
	TObjectPtr<UTexture2D> Icon;

	/** Teinte de l'icône */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability|UI")
	FLinearColor IconTint = FLinearColor::White;

	/** Akimbo (la même arme dans les deux mains) : délai entre l'action de la main principale et celle de l'autre main */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Akimbo", meta=(ClampMin="0", Units="Seconds"))
	float AkimboFireDelay = 0.5f;

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

	/** Multiplicateur de recharge du type d'action (CD reduction ; toujours 1 pour l'Auto atk d'un Gun) */
	float GetCooldownMultiplier() const;

	/** Propriété donnée à l'arme de cette capacité par une upgrade d'arme (Default si aucune) */
	float GetWeaponStat(FName Stat, float Default) const;

	/** Vrai pour une attaque automatique (déclenchée par le serveur, pas par une touche) */
	virtual bool IsAutomatic() const { return false; }

	/** Type d'action pour les upgrades : Ability (déclenchée par le joueur) ou Auto atk (automatique) */
	EYSActionSlot GetActionSlot() const { return IsAutomatic() ? EYSActionSlot::AutoAtk : EYSActionSlot::Ability; }

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
	virtual float GetActivationLockDuration() const;

	/**
	 *  Priorités des actions d'une arme (des deux armes en akimbo) : voir ActionPriority dans le .cpp (Ability, puis Auto atk).
	 *  Une action ne démarre pas tant qu'une action plus prioritaire est en cours ; quand elle démarre, elle interrompt
	 *  les actions moins prioritaires en cours (CancelAction).
	 */

	/** Durée pendant laquelle l'action est « en cours » après son déclenchement (animation) */
	virtual float GetActionDuration() const { return 0.0f; }

	/** En cours sur cette machine, d'après la dernière activation vue ; en akimbo, la main principale l'est aussi pendant AkimboFireDelay */
	virtual bool IsActionInProgress() const;

	/** Rang de priorité du type de cette action (0 = la plus prioritaire) */
	int32 GetPriorityRank() const;

	/** Aucune action plus prioritaire de l'arme (des deux armes en akimbo) n'est en cours, et le personnage a les mains libres (pas d'objet porté) */
	bool CanStartByPriority() const;

	/** Interrompt l'action (serveur) : animation arrêtée sur toutes les machines, recharge rendue, main suivante de l'akimbo annulée */
	void CancelAction();

	/** Appelé par CancelAction (serveur) : arrêter ce que l'action a en cours (ex. rafale) */
	virtual void OnActionCancelled() {}

	/** L'action bloque et interrompt aussi les actions moins prioritaires de l'autre arme (ex. Rolling shot : tout le personnage roule) */
	virtual bool BlocksAllWeapons() const { return false; }

	/** Le personnage ne subit aucun dégât en ce moment (ex. roulade ; serveur) */
	virtual bool IsGrantingInvulnerability() const { return false; }

	/** Multiplicateur de la vitesse de déplacement du personnage en ce moment (ex. 0 pendant la visée du Rolling shot) */
	virtual float GetMoveSpeedMultiplier() const { return 1.0f; }

	/** Déplacement forcé en cours quand le joueur n'appuie sur aucune direction (ex. roulade : à l'opposé du curseur) */
	virtual bool GetForcedMoveDirection(FVector& OutDirection) const { return false; }

	/** Animation du corps en cours (ex. roulade) : angle de rotation (degrés, vers la direction du mouvement) et décalage vertical */
	virtual bool GetBodyRoll(float& OutAngle, float& OutHeightOffset) const { return false; }

	/** L'action impose sa trajectoire en ce moment : l'input de déplacement (ZQSD) est ignoré, seul GetForcedMoveDirection compte */
	virtual bool OverridesMoveInput() const { return false; }

	/** Le regard du personnage est figé dans cette direction (le curseur n'est plus suivi), ex. Dashing Slash */
	virtual bool GetLockedAimDirection(FVector& OutDirection) const { return false; }

protected:

	/** Effets visuels de l'activation sur toutes les machines (le client qui tire les a déjà joués par prédiction) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastActivated();

	/** Note l'instant de l'activation pour les effets visuels locaux */
	void NotifyActivated();

	/** Actions de l'arme de cette capacité, et de l'autre arme en akimbo */
	void GetPriorityGroup(TArray<UYSAbilityComponent*>& OutActions) const;

	/** À l'activation (serveur) : interrompt les actions moins prioritaires du groupe */
	void CancelLowerPriorityActions();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastCancelled();

	/** Déclenchement différé de l'autre main en akimbo (attaques auto) */
	FTimerHandle AkimboTimer;

	/** Vrai sur le client qui contrôle le tireur (hors serveur) : il joue les effets sans attendre le serveur */
	bool IsLocallyPredicted() const;

private:

	/** Instant (temps local) de la dernière activation vue par cette machine */
	double LastActivationTime = -1.0e6;

protected:

	/** Avant l'activation, sur la machine qui active (client local ou hôte) : préparer des données locales (ex. direction de roulade) */
	virtual void PrepareActivation(const FVector& TargetLocation) {}

	/** Client qui prédit l'activation (pas l'hôte) : effets locaux immédiats (ex. roulade prédite) */
	virtual void OnActivationPredicted(const FVector& TargetLocation) {}

	/** Effet de la capacité, exécuté uniquement sur le serveur */
	virtual void ExecuteAbility(const FVector& TargetLocation) {}

	UFUNCTION(Server, Reliable)
	void ServerActivate(FVector_NetQuantize TargetLocation);

	/** Vérifie la recharge, la relance et exécute la capacité (serveur) */
	void ActivateOnServer(const FVector& TargetLocation);

	/** Temps du monde synchronisé avec le serveur */
	double GetServerTime() const;
};
