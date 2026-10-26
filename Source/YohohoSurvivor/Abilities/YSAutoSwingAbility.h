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

/** Animation jouée par une attaque : swing principal, second swing (une attaque sur deux), estoc du coup critique */
UENUM(BlueprintType)
enum class EYSSwingAnim : uint8
{
	Main,
	Second,
	Critical,
};

/** Poses d'une animation de coupe (main droite, relatives au corps ; symétrisées pour la main gauche) */
USTRUCT(BlueprintType)
struct FYSSwingPoses
{
	GENERATED_BODY()

	/** Pose de préparation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swing")
	FVector WindupHandLocation = FVector(-20.0f, 90.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swing")
	FRotator WindupHandRotation = FRotator(180.0f, -115.0f, -90.0f);

	/** Pose de fin de coupe (rotation interpolée valeur par valeur : au-delà de 360° = plus d'un tour) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swing")
	FVector SwingEndHandLocation = FVector(40.0f, -65.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swing")
	FRotator SwingEndHandRotation = FRotator(180.0f, -400.0f, -90.0f);

	/** Pendant la coupe, la main suit un arc de cercle autour du corps (sinon une ligne droite) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swing")
	bool bCircularSwing = true;
};

/** Animation complète d'une coupe : poses + durées + courbes (second swing) */
USTRUCT(BlueprintType)
struct FYSSwingAnim : public FYSSwingPoses
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swing", meta=(ClampMin="0.01", Units="Seconds"))
	float WindupDuration = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swing", meta=(ClampMin="0.01", Units="Seconds"))
	float SwingDuration = 0.07f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swing", meta=(ClampMin="0.01", Units="Seconds"))
	float RecoverDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swing", meta=(ClampMin="1"))
	float WindupEaseExponent = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swing", meta=(ClampMin="1"))
	float SwingEaseExponent = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swing", meta=(ClampMin="1"))
	float RecoverEaseExponent = 2.0f;
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

	/** Vitesse du PJ x ce multiplicateur pendant la préparation et la coupe de l'attaque auto (joueurs seulement ; 0.7 = ralenti de 30 %) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto Atk", meta=(ClampMin="0"))
	float AutoAtkMoveSpeedMultiplier = 0.7f;

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

	/**
	 *  La hitbox de l'arme frappe pendant TOUTE l'animation (préparation et retour compris), pas seulement pendant la coupe
	 *  (MorningStar : le boulet) ; chaque cible au plus une fois par attaque. Hors de la coupe : dégâts secondaires
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Detection")
	bool bHitboxDuringWholeAnimation = false;

	/**
	 *  Dégâts secondaires = coups de la hitbox hors de la coupe (préparation, retour, hitbox permanente du Yoloooooo) : Damage x ce multiplicateur.
	 *  Jamais critiques et jamais comptés (Atk critique, BloodRage, BL stacks) : seuls les coups de la coupe le sont
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Detection", meta=(ClampMin="0"))
	float SecondaryDamageMultiplier = 0.3f;

	/** Hitbox permanente (Yoloooooo) hors attaque : une même cible est touchée au plus une fois par cet intervalle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Detection", meta=(ClampMin="0.05", Units="Seconds"))
	float IdleRehitInterval = 0.5f;

	/** Captain Sword : le NME touché (s'il survit) devient le NME marqué du porteur (un seul à la fois, comme le Captain AMMO) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Mark")
	bool bMarksTarget = false;

	/** Marque : les minions du porteur font des critiques sur le NME marqué */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Mark", meta=(EditCondition="bMarksTarget"))
	bool bMarkCritsMinions = false;

	/** Marque : à la mort du NME marqué, un minion du porteur apparaît à sa place s'il reste de la place */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Mark", meta=(EditCondition="bMarksTarget"))
	bool bMarkSpawnsMinion = false;

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

	/** Une attaque auto sur deux joue le second swing (pour différencier les coups) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Second Swing")
	bool bAlternateSecondSwing = true;

	/** Second swing : poses, durées et courbes propres (par défaut copiées du swing principal) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Second Swing", meta=(EditCondition="bAlternateSecondSwing"))
	FYSSwingAnim SecondSwing;

	/** Le coup critique d'Atk critique (la 5e attaque auto) joue l'estoc */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Critical Thrust")
	bool bCriticalThrust = true;

	/** Estoc du coup critique : poses (durées et courbes = celles du swing principal) ; défaut : main reculée puis piquée droit devant */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto swing|Critical Thrust", meta=(EditCondition="bCriticalThrust"))
	FYSSwingPoses CriticalThrust;

public:

	UYSAutoSwingAbility();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Indisponible pendant l'animation puis pendant Auto Atk CD */
	virtual float GetCooldownDuration() const override { return GetAttackDuration() + AutoAtkCooldown * GetCooldownMultiplier(); }

	/** Valeur concernée par la CD reduction (affichage) : Auto Atk CD, + Recover Duration en Swing */
	virtual float GetBaseCooldown() const override { return AutoAtkCooldown + (IsRecoverReducedByCooldown() ? RecoverDuration : 0.0f); }

	/** Attaque auto : Atk speed (CD reduction) puis BL stacks du Bloodlust (appliquées après) */
	virtual float GetCooldownMultiplier() const override;

	/** Swing (attaque auto) : la CD reduction réduit aussi le retour, dans la même proportion que la recharge */
	bool IsRecoverReducedByCooldown() const { return IsAutomatic() && MeleeSystem == EYSMeleeSystem::Swing; }

	/** Durée effective du retour (Recover Duration x CD reduction en Swing) */
	float GetRecoverTime() const { return (CurrentAnim == EYSSwingAnim::Second ? SecondSwing.RecoverDuration : RecoverDuration) * (IsRecoverReducedByCooldown() ? GetCooldownMultiplier() : 1.0f); }

	/** Priorités : l'attaque est « en cours » pendant toute son animation */
	virtual float GetActionDuration() const override { return GetAttackDuration(); }

	virtual bool IsAutomatic() const override { return true; }

	/** Attaque auto : ralentit le PJ pendant la préparation et la coupe */
	virtual bool GetAutoAttackSlowdown(float& OutMultiplier) const override;

	virtual float GetBaseDamage() const override { return Damage; }


	/** Durée totale de l'animation (préparation + coupe + retour) */
	float GetAttackDuration() const { return GetWindupTime() + GetSwingTime() + GetRecoverTime(); }

	/** Durées effectives de la préparation et de la coupe (Auto swing : Windup / Swing Duration ; Dashing Slash : pas en arrière / dash) */
	virtual float GetWindupTime() const { return CurrentAnim == EYSSwingAnim::Second ? SecondSwing.WindupDuration : WindupDuration; }
	virtual float GetSwingTime() const { return CurrentAnim == EYSSwingAnim::Second ? SecondSwing.SwingDuration : SwingDuration; }

	/** Cible actuelle de la détection (dans la direction du curseur), ou nullptr */
	UFUNCTION(BlueprintPure, Category="Auto swing")
	AActor* FindAutoTarget() const;

	/**
	 *  Pose animée de la main (main droite, relative au corps) pendant l'attaque.
	 *  OutIdleWeight = part de la pose normale de la main à mélanger (1 = pose normale). Retourne false hors attaque.
	 */
	virtual bool EvaluateHandPose(float& OutIdleWeight, FVector& OutLocation, FQuat& OutRotation) const override;

	/** Déclenche l'attaque (serveur) si elle est disponible ; vrai si elle part */
	bool StartAttack();

	/** Arme de NME (non répliquée) : joue l'animation de l'attaque sur cette machine (NetMulticast du NME) */
	void PlayAttackLocally(EYSSwingAnim Anim);

	/** Animation de l'attaque en cours (ou de la dernière) */
	EYSSwingAnim GetCurrentAnim() const { return CurrentAnim; }

	/** Attaque lancée par son porteur (NME : sa hitbox de contact), pas par la détection en cône du joueur */
	bool IsTriggeredByHolder() const;

	/** Logique de combat : sur la machine qui fait autorité sur le PORTEUR (serveur), pas sur l'arme (arme de NME locale partout) */
	bool HasHolderAuthority() const;

protected:

	/** Temps écoulé dans l'animation de coupe (préparation au début) ; par défaut depuis l'activation */
	virtual float GetSwingElapsed() const { return GetTimeSinceActivation(); }

	/** Hitbox active : uniquement pendant la phase de coupe (ni la préparation, ni le retour) */
	bool IsInSwingWindow(float SwingElapsed) const { return SwingElapsed >= GetWindupTime() && SwingElapsed < GetWindupTime() + GetSwingTime(); }

	/** Pendant l'animation (0 <= SwingElapsed < durée de l'attaque) */
	bool IsInAttack(float SwingElapsed) const { return SwingElapsed >= 0.0f && SwingElapsed < GetAttackDuration(); }

	/** La hitbox frappe : coupe, toute l'animation (Hitbox During Whole Animation) ou en permanence (Yoloooooo) ; jamais arme cachée */
	bool IsHitboxActive(float SwingElapsed) const { return IsHitboxActive(SwingElapsed, IsInSwingWindow(SwingElapsed)); }
	bool IsHitboxActive(float SwingElapsed, bool bSwingWindow) const;

	/** Applique les dégâts aux cibles dans la hitbox de l'arme (serveur) ; bMainHit : coupe (sinon dégâts secondaires) */
	void ApplySwingHits(float SwingElapsed, bool bMainHit);

	/** Temps de l'animation au dernier passage des coups (serveur) : une coupe plus courte qu'une image compte quand même */
	float LastHitCheckElapsed = -1.0f;

	/** Dégâts d'un coup sur une cible (serveur) ; ShotDirection = du porteur vers la cible (pushback). Ex. Bumping Slash : coup qui bumpe */
	virtual void DealSwingDamage(AActor* Target, float FinalDamage, const FVector& ShotDirection, AController* InstigatorController, bool bCritical);

	/** Cibles déjà touchées par la coupe en cours */
	TSet<TWeakObjectPtr<AActor>> HitThisSwing;

	/** Cibles déjà touchées par des dégâts secondaires pendant l'attaque en cours (préparation, retour) : la coupe peut encore les toucher */
	TSet<TWeakObjectPtr<AActor>> SecondaryHitThisSwing;

	/** Nouvelle attaque : plus aucune cible touchée (coupe et dégâts secondaires) */
	void ResetSwingHits() { HitThisSwing.Reset(); SecondaryHitThisSwing.Reset(); }

	/** Heure du dernier coup sur chaque cible (hitbox permanente hors attaque : Idle Rehit Interval) */
	TMap<TWeakObjectPtr<AActor>, float> LastHitTimes;

	/** Coupe en cours critique (Golden bloodlust au maximum de stacks au début de l'attaque ; serveur) */
	bool bCriticalSwing = false;

	/** Attaque en cours comptée au 1er coup de la coupe (Atk critique) : seulement les attaques auto Swing (serveur) */
	bool bCountsForCriticalSwing = false;

	/** Attaque en cours critique grâce à la rage (BloodRage) : stacks consommées au 1er coup de la coupe (serveur) */
	bool bBloodRageCritical = false;

	/** L'attaque en cours a déjà touché pendant la coupe (comptage Atk critique, rage, BL stack : une fois par attaque ; serveur) */
	bool bSwingHitCounted = false;

	/** Début d'une attaque (serveur) : coupe critique si la rage est au maximum (consommée seulement si la coupe touche) */
	void StartCriticalTracking(bool bCritical, bool bCountsForCritical);

	/** NME : attaques auto déjà lancées par cette arme (alternance des swings ; serveur). Joueur : compteur du personnage (épées akimbo comptées ensemble) */
	int32 AutoSwingCount = 0;

	/** Animation de l'attaque en cours (swing principal, second swing, estoc critique), connue de toutes les machines */
	EYSSwingAnim CurrentAnim = EYSSwingAnim::Main;

	/** Début d'une attaque auto sur toutes les machines, avec son animation */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSwingStarted(EYSSwingAnim Anim);

	/** Poses de l'animation en cours et courbes */
	const FYSSwingPoses& GetCurrentPoses(FYSSwingPoses& MainPoses) const;
	virtual void GetCurrentEases(float& OutWindup, float& OutSwing, float& OutRecover) const;

private:

	/** Vrai en akimbo si cette épée est dans l'autre main que la principale (elle suit la principale) */
	bool IsOffHandOfAkimbo() const;

	/** En akimbo, l'attaque de l'autre main quand celle-ci est la principale */
	UYSAutoSwingAbility* GetAkimboPartner() const;
};
