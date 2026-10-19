#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSAbilityComponent.h"
#include "GameplayTagContainer.h"
#include "YSAutoSwingAbility.generated.h"

/** Catégorie d'une attaque de mêlée (comme le Shoot system des Guns) : définit son comportement face aux upgrades */
UENUM(BlueprintType)
enum class EYSMeleeSystem : uint8
{
	/** CD reduction de l'Auto atk : réduit la recharge (Auto Atk CD) ET le retour (Recover Duration), dans la même proportion */
	Swing	UMETA(DisplayName="Swing"),
	/** À venir */
	Bonk	UMETA(Hidden),
};

/**
 *  « Auto swing » : attaque automatique de l'épée (action de BP_Epee).
 *  Le serveur la déclenche seul dès qu'une cible est dans le cône de détection (même principe que la visée
 *  assistée du Coup d'Petoir), quand l'attaque est disponible.
 *  Animation (jouée sur toutes les machines par la main qui tient l'arme) : préparation (main sur le flanc,
 *  lame parallèle au sol), coupe très rapide jusqu'à la pose de fin, retour à la pose normale.
 *  Pendant la coupe, la hitbox de l'arme (son UBoxComponent) inflige Damage une fois par cible.
 *  Les poses sont exprimées pour la main droite, symétrisées pour la main gauche.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Auto swing"))
class UYSAutoSwingAbility : public UYSAbilityComponent
{
	GENERATED_BODY()

public:

	/** Dégâts infligés à chaque cible touchée par la coupe */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Auto swing", meta=(ClampMin="0"))
	float Damage = 1.0f;

	/** Délai avant que l'attaque puisse se redéclencher, compté à la fin de l'animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Auto swing", meta=(DisplayName="Auto Atk CD", ClampMin="0", Units="Seconds"))
	float AutoAtkCooldown = 0.8f;

	/** Catégorie de l'attaque (Swing pour l'instant ; Bonk à venir) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Melee System")
	EYSMeleeSystem MeleeSystem = EYSMeleeSystem::Swing;

	/** Détection : distance max de la cible, dans la direction du curseur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Detection", meta=(ClampMin="0", Units="Centimeters"))
	float DetectionDistance = 300.0f;

	/** Détection : écart d'angle max (de chaque côté) entre la direction du curseur et la cible */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Detection", meta=(ClampMin="0", ClampMax="180", Units="Degrees"))
	float DetectionAngle = 45.0f;

	/** Acteurs détectés et touchés par la coupe (tags portés) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Detection")
	FGameplayTagContainer HitTags;

	/** TEMPORAIRE (en attendant le visuel) : affiche la hitbox en rouge transparent pendant la coupe (builds non Shipping) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Debug")
	bool bShowHitbox = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Debug")
	FColor HitboxColor = FColor(255, 0, 0, 90);

	/** Phase 1 : la main se place sur le flanc */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Animation", meta=(ClampMin="0.01", Units="Seconds"))
	float WindupDuration = 0.15f;

	/** Phase 2 : la coupe (rapide = brutale) ; la hitbox est active pendant cette phase */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Animation", meta=(ClampMin="0.01", Units="Seconds"))
	float SwingDuration = 0.07f;

	/** Phase 3 : retour à la pose normale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Animation", meta=(ClampMin="0.01", Units="Seconds"))
	float RecoverDuration = 0.3f;

	/** Pose de préparation de la main (main droite, relative au corps) : sur le flanc */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Animation")
	FVector WindupHandLocation = FVector(-20.0f, 90.0f, 0.0f);

	/** Rotation de préparation (main droite) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Animation")
	FRotator WindupHandRotation = FRotator(180.0f, -115.0f, -90.0f);

	/** Pose de fin de coupe (main droite, relative au corps) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Animation")
	FVector SwingEndHandLocation = FVector(40.0f, -65.0f, 0.0f);

	/**
	 *  Rotation de fin de coupe (main droite). Interpolée valeur par valeur depuis WindupHandRotation :
	 *  des angles au-delà de 360° (ex. Yaw -400) donnent plus d'un tour, dans le sens indiqué.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Animation")
	FRotator SwingEndHandRotation = FRotator(180.0f, -400.0f, -90.0f);

	/** Pendant la coupe, la main suit un arc de cercle autour du corps (sinon une ligne droite) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Animation")
	bool bCircularSwing = true;

	/** Courbes : préparation EaseOut, coupe EaseOut très marqué (brutal), retour EaseInOut */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Animation", meta=(ClampMin="1"))
	float WindupEaseExponent = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Animation", meta=(ClampMin="1"))
	float SwingEaseExponent = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Animation", meta=(ClampMin="1"))
	float RecoverEaseExponent = 2.0f;

public:

	UYSAutoSwingAbility();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Indisponible pendant l'animation puis pendant Auto Atk CD */
	virtual float GetCooldownDuration() const override { return GetAttackDuration() + AutoAtkCooldown * GetCooldownMultiplier(); }

	/** Valeur concernée par la CD reduction (affichage) : Auto Atk CD, + Recover Duration en Swing */
	virtual float GetBaseCooldown() const override { return AutoAtkCooldown + (IsRecoverReducedByCooldown() ? RecoverDuration : 0.0f); }

	/** Swing (attaque auto) : la CD reduction réduit aussi le retour, dans la même proportion que la recharge */
	bool IsRecoverReducedByCooldown() const { return IsAutomatic() && MeleeSystem == EYSMeleeSystem::Swing; }

	/** Durée effective du retour (Recover Duration x CD reduction en Swing) */
	float GetRecoverTime() const { return RecoverDuration * (IsRecoverReducedByCooldown() ? GetCooldownMultiplier() : 1.0f); }

	/** Priorités : l'attaque est « en cours » pendant toute son animation */
	virtual float GetActionDuration() const override { return GetAttackDuration(); }

	virtual bool IsAutomatic() const override { return true; }

	virtual float GetBaseDamage() const override { return Damage; }


	/** Durée totale de l'animation (préparation + coupe + retour) */
	float GetAttackDuration() const { return GetWindupTime() + GetSwingTime() + GetRecoverTime(); }

	/** Durées effectives de la préparation et de la coupe (Auto swing : Windup / Swing Duration ; Dashing Slash : pas en arrière / dash) */
	virtual float GetWindupTime() const { return WindupDuration; }
	virtual float GetSwingTime() const { return SwingDuration; }

	/** Cible actuelle de la détection (dans la direction du curseur), ou nullptr */
	UFUNCTION(BlueprintPure, Category="Auto swing")
	AActor* FindAutoTarget() const;

	/**
	 *  Pose animée de la main (main droite, relative au corps) pendant l'attaque.
	 *  OutIdleWeight = part de la pose normale de la main à mélanger (1 = pose normale). Retourne false hors attaque.
	 */
	bool EvaluateHandPose(float& OutIdleWeight, FVector& OutLocation, FQuat& OutRotation) const;

	/** Déclenche l'attaque (serveur) si elle est disponible */
	void StartAttack();

protected:

	/** Temps écoulé dans l'animation de coupe (préparation au début) ; par défaut depuis l'activation */
	virtual float GetSwingElapsed() const { return GetTimeSinceActivation(); }

	/** Hitbox active : uniquement pendant la phase de coupe (ni la préparation, ni le retour) */
	bool IsInSwingWindow(float SwingElapsed) const { return SwingElapsed >= GetWindupTime() && SwingElapsed < GetWindupTime() + GetSwingTime(); }

	/** Pendant la coupe : applique les dégâts aux cibles dans la hitbox de l'arme (serveur) */
	void ApplySwingHits();

	/** Cibles déjà touchées par la coupe en cours */
	TSet<TWeakObjectPtr<AActor>> HitThisSwing;

private:

	/** Vrai en akimbo si cette épée est dans l'autre main que la principale (elle suit la principale) */
	bool IsOffHandOfAkimbo() const;

	/** En akimbo, l'attaque de l'autre main quand celle-ci est la principale */
	UYSAutoSwingAbility* GetAkimboPartner() const;
};
