#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSAbilityComponent.h"
#include "YSSummonCrewAbility.generated.h"

class AYSCaptainMinion;
class AYSPlayerCharacter;
class UYSWeaponSet;

/**
 *  « Summon the crew ! » : Ability du Crochet du capitaine (arme d'invocation, Weapon Type Summon ; pas d'attaque auto).
 *  Coup de crochet vers le haut (animation de la main : préparation basse, crochet vers le haut, retour) ; au sommet du geste
 *  (serveur) invoque Minions Per Summon minions (Minion Class) autour du Captain, chacun avec un set tiré au hasard dans
 *  Minion Weapon Sets (chances égales) et un visage au hasard, sans dépasser le maximum de minions vivants
 *  (Base Max Minions + upgrade Summon system). Déjà au maximum : le clic ne fait rien (pas de recharge).
 *  Le Damage bonus de l'Ability du Captain s'applique aux dégâts de ses minions (AYSCaptainMinion).
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Summon the crew"))
class UYSSummonCrewAbility : public UYSAbilityComponent
{
	GENERATED_BODY()

public:

	/** Blueprint des minions (/Game/Placeable/Character/Summon/CaptainMinion) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summon the crew")
	TSubclassOf<AYSCaptainMinion> MinionClass;

	/** Sets d'arme possibles des minions, tirés au hasard à chances égales à l'apparition */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summon the crew")
	TArray<TObjectPtr<UYSWeaponSet>> MinionWeaponSets;

	/** Minions invoqués à chaque activation (moins s'il ne reste pas assez de place sous le maximum) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summon the crew", meta=(ClampMin="1"))
	int32 MinionsPerSummon = 2;

	/** Minions vivants maximum en même temps (+ upgrade Summon system) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summon the crew", meta=(ClampMin="1"))
	int32 BaseMaxMinions = 2;

	/** Distance d'apparition des minions autour du Captain (sur ses côtés, un peu derrière) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summon the crew", meta=(ClampMin="0", Units="Centimeters"))
	float SpawnDistance = 180.0f;

	/** Dégâts de base des minions affichés par l'upgrade Damage bonus de l'Ability (les vrais dégâts sont ceux de leurs armes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summon the crew", meta=(ClampMin="0"))
	float MinionBaseDamage = 20.0f;

	/** Phase 1 : la main descend et recule (préparation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summon the crew|Hook Anim", meta=(ClampMin="0.01", Units="Seconds"))
	float WindupDuration = 0.2f;

	/** Phase 2 : crochet vers le haut ; les minions apparaissent à la fin de cette phase */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summon the crew|Hook Anim", meta=(ClampMin="0.01", Units="Seconds"))
	float StrikeDuration = 0.12f;

	/** Phase 3 : retour à la pose normale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summon the crew|Hook Anim", meta=(ClampMin="0.01", Units="Seconds"))
	float RecoverDuration = 0.35f;

	/** Pose de préparation (main droite, relative au corps ; X avant, Y droite, Z haut ; symétrique pour la main gauche) : basse, en arrière */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summon the crew|Hook Anim")
	FVector WindupHandLocation = FVector(-5.0f, 55.0f, -40.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summon the crew|Hook Anim")
	FRotator WindupHandRotation = FRotator(40.0f, 0.0f, 0.0f);

	/** Pose au sommet du crochet : devant, en hauteur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summon the crew|Hook Anim")
	FVector StrikeHandLocation = FVector(55.0f, 30.0f, 70.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summon the crew|Hook Anim")
	FRotator StrikeHandRotation = FRotator(-60.0f, 0.0f, 0.0f);

	/** Courbes : préparation EaseOut, crochet EaseOut marqué (cet exposant), retour EaseInOut */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summon the crew|Hook Anim", meta=(ClampMin="1"))
	float StrikeEaseExponent = 3.0f;

	UYSSummonCrewAbility();

	virtual float GetBaseDamage() const override { return MinionBaseDamage; }

	/** Préparation + crochet (les minions apparaissent à la fin) */
	virtual float GetActionDuration() const override { return WindupDuration + StrikeDuration; }

	virtual bool EvaluateHandPose(float& OutIdleWeight, FVector& OutLocation, FQuat& OutRotation) const override;

	/** Encore de la place sous le maximum de minions vivants */
	virtual bool CanActivateNow() const override;

	/** Minions vivants maximum (Base Max Minions + Summon system du porteur) */
	int32 GetMaxMinions() const;

	/** Minions vivants de ce Captain (serveur et clients) */
	static int32 CountMinions(const UWorld* World, const AYSPlayerCharacter* Captain);

protected:

	/** Serveur : les minions apparaissent au sommet du crochet */
	virtual void ExecuteAbility(const FVector& TargetLocation) override;

	virtual void OnActionCancelled() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Invoque les minions qui manquent (jusqu'à Minions Per Summon, sans dépasser le maximum ; serveur) */
	void SpawnMinions();

	FTimerHandle SummonTimer;
};
