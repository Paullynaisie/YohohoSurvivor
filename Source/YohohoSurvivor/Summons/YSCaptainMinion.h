#pragma once

#include "CoreMinimal.h"
#include "Player/YSPlayerCharacter.h"
#include "YSCaptainMinion.generated.h"

class AYSFoeCharacter;

/**
 *  Minion du Captain (invoqué par « Summon the crew ! » du Crochet du capitaine) : personnage auto-piloté (AAIController, logique
 *  serveur dans Tick), même corps que Mouss avec sa Head Feature : corps blanc comme les joueurs, Head Feature de la couleur de
 *  son Captain, même visage que lui, armes de leur couleur de base. Sort du sol en 0.5 s (fini tout de suite si son Ability est forcée).
 *  Même vitesse que son Captain (sans les bonus d'actions), lean et sautillement comme les joueurs ; set d'arme tiré au hasard à
 *  l'apparition (MinionAutoGun ou MinionSword : attaque auto seulement, 50 PV via le set).
 *  Comportement : un NME à moins de Aggro Range (les NME marqués par le Captain comptent Marked Foe Attraction fois plus près)
 *  -> va l'attaquer (s'arrête à la portée de son arme, tourné vers lui : l'attaque auto part toute seule) ; sinon reste à moins
 *  de Follow Distance de son Captain. Damage bonus de l'Ability du Captain appliqué à ses dégâts. Meurt avec son Captain.
 *  Ennemis : les NME le ciblent (3x plus loin qu'un joueur) et ses PV baissent sous leurs coups (contact, balles, bombes).
 *  Bateau : Captain à bord -> le minion monte aussi (accès du bateau, à pied ou à la nage) et ne quitte plus le pont pour poursuivre un NME ;
 *  Captain à la barre -> marche jusqu'à un canon libre et tire tout seul sur les NME à portée ; barre lâchée -> lâche son canon. Un joueur
 *  peut lui prendre sa place avec E (jamais l'inverse).
 *  Abilities (Dashing Slash de l'épée, Rolling shot du gun) : jamais utilisées d'elles-mêmes, seulement forcées par
 *  « Allez-y mes minions ! » du Cursed Hook (sans recharge), vers sa cible. Sur le NME marqué par la Captain Sword : coups critiques.
 */
UCLASS(meta=(PrioritizeCategories="Minion 3C"))
class AYSCaptainMinion : public AYSPlayerCharacter
{
	GENERATED_BODY()

public:

	/** Distance au Captain au-delà de laquelle le minion revient vers lui (sans cible) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion", meta=(ClampMin="0", Units="Centimeters"))
	float FollowDistance = 500.0f;

	/** En revenant vers le Captain, le minion s'arrête à cette distance de lui */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion", meta=(ClampMin="0", Units="Centimeters"))
	float FollowStopDistance = 250.0f;

	/** Un NME à moins de cette distance du minion devient sa cible (le plus proche) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion", meta=(ClampMin="0", Units="Centimeters"))
	float AggroRange = 1000.0f;

	/** Le NME marqué par le Captain (Captain AMMO) compte ce nombre de fois plus près */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion", meta=(ClampMin="1"))
	float MarkedFoeAttraction = 5.0f;

	/** Un NME plus loin que cette distance du Captain n'est pas poursuivi (le marqué : avec la même attraction) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion", meta=(ClampMin="0", Units="Centimeters"))
	float MaxChaseDistanceFromCaptain = 1500.0f;

	/** Arme de mêlée : le minion s'approche de sa cible jusqu'à cette distance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion", meta=(ClampMin="0", Units="Centimeters"))
	float MeleeStopDistance = 150.0f;

	/** Arme à distance (Gun) : le minion s'arrête à cette distance de sa cible */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion", meta=(ClampMin="0", Units="Centimeters"))
	float RangedStopDistance = 600.0f;

	/** À l'apparition, le minion traverse les autres personnages (joueurs, minions, NME) pendant ce délai : il n'est pas poussé n'importe où */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion", meta=(ClampMin="0", Units="Seconds"))
	float SpawnPassThroughDuration = 1.0f;

	/** Captain à bord : pour rejoindre l'accès du bateau depuis l'autre flanc, le minion passe à cette distance de la proue ou de la poupe */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion|Ship", meta=(ClampMin="0", Units="Centimeters"))
	float BoardingHullMargin = 250.0f;

	/** Captain à la barre : le minion s'installe au canon une fois à cette distance de sa place (à plat) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion|Ship", meta=(ClampMin="0", Units="Centimeters"))
	float CannonReachDistance = 100.0f;

	/** En marchant sur le pont : plus lent que cette vitesse pendant Crew Stuck Delay (mât, marche) = bloqué, il contourne sur le côté pendant Crew Sidestep Duration */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion|Ship", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float CrewStuckSpeed = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion|Ship", meta=(ClampMin="0", Units="s"))
	float CrewStuckDelay = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minion|Ship", meta=(ClampMin="0", Units="s"))
	float CrewSidestepDuration = 0.8f;

	AYSCaptainMinion();

	virtual void BeginPlay() override;

	/** Avant FinishSpawning (serveur) : son Captain, son set d'arme et son visage */
	void InitMinion(AYSPlayerCharacter* InCaptain, UYSWeaponSet* InWeaponSet, int32 InFaceIndex);

	AYSPlayerCharacter* GetCaptain() const { return Captain; }

	/** Nouveau Captain (serveur) : changement de classe du joueur, ses minions le suivent au lieu de mourir avec l'ancien personnage */
	void SetCaptain(AYSPlayerCharacter* NewCaptain);

	/** Visage (index dans les visages de personnalisation ; icône du HUD de son Captain) */
	int32 GetFaceIndex() const { return FaceIndex; }

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Couleur du Captain sur la Head Feature, visage du Captain */
	virtual void RefreshCustomization() override;

	/** Dégâts : Damage bonus de l'Ability de son Captain */
	virtual float GetWeaponDamageMultiplier(const AYSWeapon* Weapon, EYSActionSlot Slot) const override;

	/** Attaque auto toujours active (pas de clic gauche), sans ralentissement */
	virtual bool UsesHeldAutoAttack() const override { return false; }

	/** Toujours critique sur le NME marqué par la Captain Sword de son Captain */
	virtual bool HasForcedCriticalAgainst(const AActor* Target) const override;

	/** Dégâts critiques de son Captain (Dégât critique compris) */
	virtual float GetCriticalDamageMultiplier() const override;

protected:

	/** Point visé par son Ability forcée (Allez-y mes minions !) : sa cible, sinon droit devant */
	virtual FVector GetAimLocation() const override;

	/** Mort : simplement détruit (pas de joueur) */
	virtual void Die() override;

	UPROPERTY(Replicated)
	TObjectPtr<AYSPlayerCharacter> Captain;

	/** Visage (index dans les visages de personnalisation) : celui du Captain à l'invocation */
	UPROPERTY(Replicated)
	int32 FaceIndex = 0;

private:

	/** Hps bonus et Armor de son Captain recopiés (serveur, à chaque image ; sans effet s'ils n'ont pas changé) */
	void SyncCaptainUpgrades();

	/** Cible : NME le plus proche (le marqué compte plus près) dans la portée, pas trop loin du Captain ; nullptr sinon */
	AYSFoeCharacter* FindTarget() const;

	/** L'arme tenue attaque à distance (Gun) */
	bool HasRangedWeapon() const;

	/** Une Ability (forcée) est en cours : son déplacement est celui de l'action, pas celui de l'IA */
	bool IsAbilityInProgress() const;

	/**
	 *  Captain à bord d'un bateau : monte à bord (accès du bateau, à pied ou à la nage) ; Captain à la barre : marche jusqu'à un canon libre,
	 *  s'y installe et tire tout seul ; barre lâchée : lâche son canon. Vrai = le minion a fait ce qu'il avait à faire, pas d'IA normale.
	 */
	bool UpdateShipCrew(class AAIController* AIController, float DeltaSeconds);

	/** Au canon : vise le plus proche à portée (NME, module d'un bateau de NME), tire dès que le canon est rechargé */
	void UpdateCannonFire(class AYSShipCannon* Cannon, AAIController* AIController);

	/** Canon libre le plus proche, pas déjà visé par un autre minion (garde le sien s'il est encore libre), ou nullptr */
	AYSShipCannon* FindFreeCannon(const AYSPirateShip* Ship) const;

	/** Marche vers Goal (à plat), regarde où il va, contourne ce qui le bloque */
	void WalkToward(const FVector& Goal, AAIController* AIController, float DeltaSeconds);

	/** Traverse les autres personnages pendant Spawn Pass Through Duration (apparition, montée à bord) */
	void StartPassThrough();

	FTimerHandle PassThroughTimer;

	/** Canon vers lequel il marche */
	TWeakObjectPtr<AYSShipCannon> AssignedCannon;

	/** Bloqué en marchant : temps sous Crew Stuck Speed, temps restant de contournement, côté du contournement */
	float CrewStuckTime = 0.0f;
	float CrewSidestepTime = 0.0f;
	float CrewSidestepSide = 1.0f;

	/** Revient vers le Captain (au-delà de Follow Distance) jusqu'à Follow Stop Distance */
	bool bReturningToCaptain = false;

	/** Cible actuelle (serveur) */
	TWeakObjectPtr<AYSFoeCharacter> CurrentTarget;
};
