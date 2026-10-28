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
	/** Arme d'invocation (ex. Crochet du capitaine) : son Ability invoque des alliés (upgrade Summon system) */
	Summon,
};

/** Shoot system d'une attaque auto de Gun : définit ce qu'améliore l'upgrade Shoot system et la règle des critiques */
UENUM(BlueprintType)
enum class EYSShootSystem : uint8
{
	/** Rafale de balles réparties sur Burst Duration ; Shoot system = balles en plus ; critique : une balle sur N reçue par un même NME */
	PewPew		UMETA(DisplayName="PewPew"),
	/** Chaque balle traverse les NME (Pierce Max Hits, + Shoot system) ; critique : toujours sur un NME en train d'être bumpé */
	Pierce		UMETA(DisplayName="Pierce"),
	/** Balles / bombes qui explosent (zone) ; Shoot system = rayon des explosions x (1 + bonus) */
	Explosive	UMETA(DisplayName="Explosif"),
};

class UTexture2D;

/** Activation forcée d'une Ability par un autre personnage (« Allez-y mes minions ! » du Cursed Hook) */
USTRUCT(BlueprintType)
struct FYSForcedActivation
{
	GENERATED_BODY()

	/** Dégâts de l'action forcée x ce multiplicateur (You're all my minions ++) */
	UPROPERTY()
	float DamageMultiplier = 1.0f;

	/** Golden : la recharge de l'Ability est rendue en plus (sinon elle reste telle quelle : l'action forcée ne la déclenche pas) */
	UPROPERTY()
	bool bResetCooldown = false;
};

/**
 *  Base des capacités du joueur : temps de recharge, activation réseau et données d'UI.
 *  Attaque active (ex. Coup d'Petoir) : le client local appelle TryActivate -> RPC serveur -> le serveur
 *  vérifie la recharge et exécute ExecuteAbility. Le temps de recharge est prédit côté client puis corrigé par réplication.
 *  Attaque automatique (ex. Auto swing) : déclenchée par le serveur lui-même (IsAutomatic).
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

	/** À l'activation, la recharge de l'attaque auto de toutes les armes tenues est rendue (après Auto Attack Reset Delay) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability|Auto Atk Reset")
	bool bResetAutoAttackOnUse = false;

	/** Délai entre l'activation et la recharge rendue (ex. Dragon breath : après le recul) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability|Auto Atk Reset", meta=(ClampMin="0", Units="Seconds", EditCondition="bResetAutoAttackOnUse"))
	float AutoAttackResetDelay = 0.0f;

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

	/** Multiplicateur de recharge du type d'action (CD reduction ; toujours 1 pour l'Auto atk d'un Gun ; Auto swing : x Bloodlust) */
	virtual float GetCooldownMultiplier() const;

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

	/** Attaque auto de Gun : son shoot system (upgrade Shoot system) ; faux si l'action n'en a pas */
	virtual bool GetShootSystem(EYSShootSystem& OutSystem) const { return false; }

	/** Shoot system Explosif : rayon des explosions x (1 + bonus Shoot system du porteur) ; 1 sinon */
	float GetExplosiveRadiusMultiplier() const;

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
	 *  Priorités des actions d'une arme (des deux armes en akimbo) : voir ActionPriority dans le .cpp (action forcée, Ability, puis Auto atk).
	 *  Une action ne démarre pas tant qu'une action plus prioritaire est en cours ; quand elle démarre, elle interrompt
	 *  les actions moins prioritaires en cours (CancelAction).
	 */

	/** Durée pendant laquelle l'action est « en cours » après son déclenchement (animation) */
	virtual float GetActionDuration() const { return 0.0f; }

	/** En cours sur cette machine, d'après la dernière activation vue ; en akimbo, la main principale l'est aussi pendant AkimboFireDelay */
	virtual bool IsActionInProgress() const;

	/**
	 *  Partie de l'action qui bloque les actions moins prioritaires (CanStartByPriority) : par défaut toute l'action ; sans le recul
	 *  des tirs ni le retour à la pose des coups (l'attaque auto peut repartir dès la dernière balle / la fin de la coupe)
	 */
	virtual float GetPriorityBlockDuration() const { return GetActionDuration(); }

	/** Bloque les actions moins prioritaires sur cette machine (GetPriorityBlockDuration depuis l'activation, + AkimboFireDelay en akimbo) */
	virtual bool IsBlockingLowerPriority() const;

	/** Rang de priorité du type de cette action (0 = la plus prioritaire) */
	int32 GetPriorityRank() const;

	/** Aucune action plus prioritaire de l'arme (des deux armes en akimbo) n'est en cours, et le personnage a les mains libres (pas d'objet porté) */
	bool CanStartByPriority() const;

	/** Interrompt l'action (serveur) : animation arrêtée sur toutes les machines, recharge rendue, main suivante de l'akimbo annulée */
	void CancelAction();

	/**
	 *  Arrête l'action en cours sur cette machine seulement, recharge inchangée (ex. arme locale d'un NME bumpé : chaque machine
	 *  arrête la sienne) : plus d'animation ni de coups, main suivante de l'akimbo annulée
	 */
	void StopActionLocally();

	/**
	 *  Le clic ne peut pas lancer l'action maintenant alors que sa recharge est finie (action plus prioritaire en cours, armes rangées,
	 *  action forcée en cours, activation sans effet comme Summon the crew au maximum de minions) : icône grisée
	 */
	bool IsActivationBlocked() const;

	/** Recharge terminée tout de suite (serveur, ex. level up) */
	void ResetCooldown();

	/** Recharge en cours raccourcie de Seconds (serveur, ex. Golden Marquage) */
	void ReduceCooldown(float Seconds);

	/** Appelé par CancelAction (serveur) : arrêter ce que l'action a en cours (ex. rafale) */
	virtual void OnActionCancelled() {}

	/** L'action bloque et interrompt aussi les actions moins prioritaires de l'autre arme (ex. Rolling shot : tout le personnage roule) */
	virtual bool BlocksAllWeapons() const { return false; }

	/** Le personnage ne subit aucun dégât en ce moment (ex. roulade ; serveur) */
	virtual bool IsGrantingInvulnerability() const { return false; }

	/** Multiplicateur de la vitesse de déplacement du personnage en ce moment (ex. 0 pendant la visée du Rolling shot) */
	virtual float GetMoveSpeedMultiplier() const { return 1.0f; }

	/**
	 *  Attaque auto d'un joueur en train de frapper (Melee : préparation + coupe ; Gun : rafale) : ralentissement du PJ
	 *  (OutMultiplier, Auto Atk Move Speed Multiplier) ; faux hors attaque. Le personnage applique le plus fort, une seule fois.
	 */
	virtual bool GetAutoAttackSlowdown(float& OutMultiplier) const { return false; }

	/** Attaque auto autorisée par le porteur : un joueur maintient le clic gauche ; NME et minions : toujours */
	bool IsAutoAttackAllowedByHolder() const;

	/** Joueur qui maintient le clic gauche : l'attaque auto part même sans cible (NME et minions : il leur faut une cible) */
	bool IsAutoAttackTargetOptional() const;

	/** Déplacement forcé en cours quand le joueur n'appuie sur aucune direction (ex. roulade : à l'opposé du curseur) */
	virtual bool GetForcedMoveDirection(FVector& OutDirection) const { return false; }

	/** Animation du corps en cours (ex. roulade) : angle de rotation (degrés, vers la direction du mouvement) et décalage vertical */
	virtual bool GetBodyRoll(float& OutAngle, float& OutHeightOffset) const { return false; }

	/** L'action impose sa trajectoire en ce moment : l'input de déplacement (ZQSD) est ignoré, seul GetForcedMoveDirection compte */
	virtual bool OverridesMoveInput() const { return false; }

	/** Le regard du personnage est figé dans cette direction (le curseur n'est plus suivi), ex. Dashing Slash */
	virtual bool GetLockedAimDirection(FVector& OutDirection) const { return false; }

	/**
	 *  Pose animée d'une main (exprimée pour la main droite, relative au corps) pendant l'action ; false hors animation.
	 *  OutIdleWeight = part de la pose normale de la main à mélanger (1 = pose normale).
	 */
	virtual bool EvaluateHandPose(float& OutIdleWeight, FVector& OutLocation, FQuat& OutRotation) const { return false; }

	/** La pose animée est celle de l'AUTRE main (arme à deux mains : la main posée sur l'Off Hand Socket), pas celle qui tient l'arme */
	virtual bool AnimatesOffHand() const { return false; }

	/** L'activation aurait un effet (ex. Summon the crew : faux si les minions sont déjà au maximum) ; sinon le clic ne fait rien, sans recharge */
	virtual bool CanActivateNow() const { return true; }

	/**
	 *  Activation forcée (« Allez-y mes minions ! ») : le serveur l'accorde (et le client qui contrôle le personnage, par RPC du
	 *  personnage), puis la machine qui contrôle l'active vers son curseur avec TryForcedActivate. Elle passe avant toutes les autres
	 *  actions (priorité Forced : celles en cours sont interrompues), ignore la recharge et ne la déclenche pas (Reset Cooldown : la rend).
	 */
	void GrantForcedActivation(const FYSForcedActivation& Forced);

	/** Active l'activation forcée accordée (machine qui contrôle le personnage) ; rien sans accord */
	void TryForcedActivate(const FVector& TargetLocation);

	/** La dernière activation était forcée et l'action est encore en cours : le clic ne la relance pas */
	bool IsForcedActionInProgress() const { return bLastActivationForced && IsActionInProgress(); }

#if WITH_EDITOR
	/**
	 *  Éditeur : propriétés (héritées) que cette classe n'utilise pas, cachées dans le panneau Details (FYSAbilityDetails).
	 *  Les sous-classes appellent Super puis ajoutent / retirent des noms. Base : pas d'icône pour une attaque auto (l'UI montre l'Ability).
	 */
	virtual void GetUnusedProperties(TArray<FName>& OutNames) const;
#endif

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

	/** Activation forcée exécutée par le serveur (accord vérifié) ; faux si refusée */
	bool ActivateForcedOnServer(const FVector& TargetLocation);

	UFUNCTION(Server, Reliable)
	void ServerForcedActivate(FVector_NetQuantize TargetLocation);

	/** Effets visuels d'une activation forcée sur toutes les machines */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastForcedActivated();

private:

	/** Instant (temps local) de la dernière activation vue par cette machine */
	double LastActivationTime = -1.0e6;

	/** Activation forcée accordée et pas encore utilisée, avec ses réglages et l'instant de l'accord (temps local) */
	bool bForcedGranted = false;
	FYSForcedActivation PendingForced;
	double ForcedGrantTime = -1.0e6;

	/** La dernière activation était forcée ; ses dégâts x ForcedDamageMultiplier (serveur) */
	bool bLastActivationForced = false;
	float ForcedDamageMultiplier = 1.0f;

protected:

	/** Avant l'activation, sur la machine qui active (client local ou hôte) : préparer des données locales (ex. direction de roulade) */
	virtual void PrepareActivation(const FVector& TargetLocation) {}

	/** Client qui prédit l'activation (pas l'hôte) : effets locaux immédiats (ex. roulade prédite) */
	virtual void OnActivationPredicted(const FVector& TargetLocation) {}

	/** Effet de la capacité, exécuté uniquement sur le serveur */
	virtual void ExecuteAbility(const FVector& TargetLocation) {}

	UFUNCTION(Server, Reliable)
	void ServerActivate(FVector_NetQuantize TargetLocation);

	/** Vérifie la recharge, la relance et exécute la capacité (serveur) ; faux si refusée */
	bool ActivateOnServer(const FVector& TargetLocation);

	/**
	 *  Activation prédite par le client mais refusée par le serveur : recharge du serveur reprise et effets prédits arrêtés
	 *  (sinon le client garde sa prédiction, la recharge du serveur n'ayant pas changé elle n'est pas répliquée à nouveau)
	 */
	UFUNCTION(Client, Reliable)
	void ClientActivationRejected(double ServerReadyTime);

	/** Temps du monde synchronisé avec le serveur */
	double GetServerTime() const;

	/** Reset Auto Attack On Use : recharge de l'attaque auto des armes tenues rendue, tout de suite ou après le délai (serveur) */
	void ScheduleAutoAttackReset();

	FTimerHandle AutoAttackResetTimer;
};
