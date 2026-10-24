#pragma once

#include "CoreMinimal.h"
#include "Player/YSPlayerCharacter.h"
#include "YSCaptainMinion.generated.h"

class AYSFoeCharacter;

/**
 *  Minion du Captain (invoqué par « Summon the crew ! » du Crochet du capitaine) : personnage auto-piloté (AAIController, logique
 *  serveur dans Tick), même corps que Mouss sans Head Feature : le CORPS prend la couleur de son Captain, visage tiré au hasard.
 *  Même vitesse que son Captain (sans les bonus d'actions), lean et sautillement comme les joueurs ; set d'arme tiré au hasard à
 *  l'apparition (MinionAutoGun ou MinionSword : attaque auto seulement, 50 PV via le set).
 *  Comportement : un NME à moins de Aggro Range (les NME marqués par le Captain comptent Marked Foe Attraction fois plus près)
 *  -> va l'attaquer (s'arrête à la portée de son arme, tourné vers lui : l'attaque auto part toute seule) ; sinon reste à moins
 *  de Follow Distance de son Captain. Damage bonus de l'Ability du Captain appliqué à ses dégâts. Meurt avec son Captain.
 *  Ennemis : les NME le ciblent (3x plus loin qu'un joueur) et ses PV baissent sous leurs coups (contact, balles, bombes).
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

	AYSCaptainMinion();

	virtual void BeginPlay() override;

	/** Avant FinishSpawning (serveur) : son Captain, son set d'arme et son visage */
	void InitMinion(AYSPlayerCharacter* InCaptain, UYSWeaponSet* InWeaponSet, int32 InFaceIndex);

	AYSPlayerCharacter* GetCaptain() const { return Captain; }

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Couleur du Captain sur le corps, visage tiré au hasard */
	virtual void RefreshCustomization() override;

	/** Dégâts : Damage bonus de l'Ability de son Captain */
	virtual float GetWeaponDamageMultiplier(const AYSWeapon* Weapon, EYSActionSlot Slot) const override;

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

	/** Visage (index dans les visages de personnalisation), tiré au hasard par le serveur */
	UPROPERTY(Replicated)
	int32 FaceIndex = 0;

private:

	/** Cible : NME le plus proche (le marqué compte plus près) dans la portée, pas trop loin du Captain ; nullptr sinon */
	AYSFoeCharacter* FindTarget() const;

	/** L'arme tenue attaque à distance (Gun) */
	bool HasRangedWeapon() const;

	/** Une Ability (forcée) est en cours : son déplacement est celui de l'action, pas celui de l'IA */
	bool IsAbilityInProgress() const;

	/** Revient vers le Captain (au-delà de Follow Distance) jusqu'à Follow Stop Distance */
	bool bReturningToCaptain = false;

	/** Cible actuelle (serveur) */
	TWeakObjectPtr<AYSFoeCharacter> CurrentTarget;
};
