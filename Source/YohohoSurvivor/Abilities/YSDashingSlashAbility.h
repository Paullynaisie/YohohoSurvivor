#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSCoupDSabreAbility.h"
#include "YSDashingSlashAbility.generated.h"

/**
 *  « Dashing Slash » : Ability de la BattleSword (Melee ; sous-classe du Coup d'sabre : même animation, mêmes timings,
 *  même hitbox, et les upgrades d'Ability s'y appliquent : Damage bonus, CD reduction).
 *  Pendant de Rolling shot pour les épées. À l'activation, interrompt les actions moins prioritaires des DEUX armes
 *  (BlocksAllWeapons). La direction est prise sur le curseur au clic, puis ZQSD et la souris sont ignorés jusqu'à la fin :
 *  1. Petit pas en arrière (Step Back Duration), à l'opposé du curseur : vitesse x Step Back Speed Multiplier
 *     qui décroît jusqu'à Step Back End Speed Multiplier.
 *  2. Énorme dash vers le curseur (Dash Duration) : vitesse x Dash Speed Multiplier qui décroît jusqu'à Dash End Speed Multiplier ;
 *     invulnérable ; l'animation du Coup d'sabre démarre avec le dash. En akimbo, les deux épées frappent en même temps
 *     (Akimbo Fire Delay 0) et chacune inflige Damage.
 *  La recharge (Cooldown, réduite par CD reduction) commence à la fin du dash.
 *  Le mouvement est piloté par l'épée de la main principale (une seule fois en akimbo).
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Dashing Slash"))
class UYSDashingSlashAbility : public UYSCoupDSabreAbility
{
	GENERATED_BODY()

public:

	/** Pas en arrière : durée */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dashing Slash|Step Back", meta=(ClampMin="0", Units="Seconds"))
	float StepBackDuration = 0.25f;

	/** Pas en arrière : vitesse de déplacement x ce multiplicateur au départ */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dashing Slash|Step Back", meta=(ClampMin="0"))
	float StepBackSpeedMultiplier = 2.0f;

	/** Pas en arrière : multiplicateur atteint à la fin (décroissance depuis Step Back Speed Multiplier) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dashing Slash|Step Back", meta=(ClampMin="0"))
	float StepBackEndSpeedMultiplier = 0.5f;

	/** Pas en arrière : courbe de la décroissance (1 = linéaire ; plus grand = retombe plus vite au début) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dashing Slash|Step Back", meta=(ClampMin="1"))
	float StepBackDecayExponent = 1.0f;

	/** Dash : durée (l'attaque démarre au début du dash) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dashing Slash|Dash", meta=(ClampMin="0.01", Units="Seconds"))
	float DashDuration = 0.4f;

	/** Dash : vitesse de déplacement x ce multiplicateur au départ */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dashing Slash|Dash", meta=(ClampMin="0"))
	float DashSpeedMultiplier = 8.0f;

	/** Dash : multiplicateur atteint à la fin (décroissance depuis Dash Speed Multiplier) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dashing Slash|Dash", meta=(ClampMin="0"))
	float DashEndSpeedMultiplier = 0.5f;

	/** Dash : courbe de la décroissance (1 = linéaire ; plus grand = retombe plus vite au début) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dashing Slash|Dash", meta=(ClampMin="1"))
	float DashDecayExponent = 1.0f;

	/** Pendant le dash, le personnage traverse les NME et les autres joueurs (sinon le premier NME l'arrête net) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dashing Slash|Dash")
	bool bDashThroughFoes = true;

	/** Invulnérable pendant le dash, plus ce délai */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dashing Slash|Dash", meta=(ClampMin="0", Units="Seconds"))
	float ExtraInvulnerability = 0.0f;

public:

	UYSDashingSlashAbility();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Ability (clic gauche), pas une attaque auto */
	virtual bool IsAutomatic() const override { return false; }

	/** Pas en arrière + dash, puis la recharge (CD reduction : seulement la recharge) */
	virtual float GetCooldownDuration() const override { return StepBackDuration + DashDuration + Cooldown * GetCooldownMultiplier(); }

	virtual float GetBaseCooldown() const override { return Cooldown; }

	/** Priorités : en cours pendant le pas en arrière, puis le dash et l'animation de coupe */
	virtual float GetActionDuration() const override { return FMath::Max(StepBackDuration + DashDuration, GetAttackDuration()); }

	/** Animation calée sur le mouvement : préparation pendant le pas en arrière, coupe pendant le dash (Windup / Swing Duration ignorés) */
	virtual float GetWindupTime() const override { return StepBackDuration; }
	virtual float GetSwingTime() const override { return DashDuration; }

	virtual bool BlocksAllWeapons() const override { return true; }

	virtual bool IsGrantingInvulnerability() const override;

	virtual float GetMoveSpeedMultiplier() const override;

	virtual bool GetForcedMoveDirection(FVector& OutDirection) const override;

	virtual bool OverridesMoveInput() const override;

	virtual bool GetLockedAimDirection(FVector& OutDirection) const override;

protected:

	/** Machine qui active (client local ou hôte) : direction figée sur le curseur */
	virtual void PrepareActivation(const FVector& TargetLocation) override;

	/** Client : pas en arrière prédit, l'autre épée aussi */
	virtual void OnActivationPredicted(const FVector& TargetLocation) override;

	/** Serveur : pas en arrière, et l'autre épée démarre en même temps */
	virtual void ExecuteAbility(const FVector& TargetLocation) override;

	virtual void OnActionCancelled() override;

private:

	/** Cette épée pilote le mouvement (une seule fois en akimbo : la main principale) */
	bool IsMovementDriver() const;

	/** En cours : 0 pas en arrière, 1 dash, -1 hors enchaînement (machine qui pilote le mouvement seulement) */
	int32 GetPhase(float& OutAlpha) const;

	/** Direction du curseur au moment du clic, à plat */
	void LockDirection(const FVector& TargetLocation);

	/** Élan immédiat dans la direction de la phase (vitesse de base x multiplicateur de départ) */
	void ApplyPhaseKick(int32 Phase);

	/** Collisions avec les pawns ignorées pendant le dash, puis rétablies */
	void SetPassThroughPawns(bool bPassThrough);

	/** Démarre l'autre épée en même temps (serveur) */
	void StartPartner();

	/** En akimbo : le Dashing Slash de l'autre main si celle-ci est la principale */
	UYSDashingSlashAbility* GetDashPartner() const;

	/** Direction figée (vers le curseur au clic) */
	FVector LockedDirection = FVector::ForwardVector;

	/** Élan du dash déjà donné / traversée des pawns active (machine qui pilote le mouvement) */
	bool bDashKickApplied = false;
	bool bPassingThroughPawns = false;

	/** Dernière phase écrite dans le log Verbose [YSDashingSlash] */
	int32 LastLoggedPhase = -1;
};
