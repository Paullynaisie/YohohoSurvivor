#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSBumpingSlashAbility.h"
#include "YSMeteorJumpAbility.generated.h"

/**
 *  « MeteorJump » : Ability de la Great Anchor (set Anchor Bearer du Quartermaster ; sous-classe du Bumping Slash : même pas en
 *  arrière, même animation de coupe, même bump des NME non élites touchés).
 *  Après le pas en arrière, un SAUT en 3 temps (chute du CharacterMovement à gravité réglée ; serveur et client qui contrôle) :
 *  1. Montée (Ascent Duration) jusqu'à Jump Apex Height au-dessus du départ : préparation de la coupe, pas de hitbox,
 *     déplacement ZQSD à Ascent Move Speed Multiplier.
 *  2. Apex (Apex Duration) : reste à la hauteur du sommet, dernière pose de la préparation, pas de hitbox, déplacement ZQSD
 *     de Apex Start à Apex End Move Speed Multiplier.
 *  3. Chute (Fall Duration) droit vers le sol, sans déplacement : coupe (hitbox active).
 *  Puis dernière pose gardée Land Hold Duration (immobile, pas de hitbox), et retour à la pose normale en Land Recover Duration
 *  (vitesse qui remonte de 0 à 100 %).
 *  Du pas en arrière à l'atterrissage : invulnérable, traverse les NME.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="MeteorJump"))
class UYSMeteorJumpAbility : public UYSBumpingSlashAbility
{
	GENERATED_BODY()

public:

	/** Hauteur du sommet du saut au-dessus du point de départ */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump", meta=(ClampMin="1", Units="Centimeters"))
	float JumpApexHeight = 300.0f;

	/** Montée : durée jusqu'au sommet (préparation de la coupe pendant la montée) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump", meta=(ClampMin="0.05", Units="Seconds"))
	float AscentDuration = 0.35f;

	/** Montée : vitesse de déplacement (ZQSD) x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump", meta=(ClampMin="0"))
	float AscentMoveSpeedMultiplier = 0.5f;

	/** Apex : durée à la hauteur du sommet (dernière pose de la préparation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump", meta=(ClampMin="0", Units="Seconds"))
	float ApexDuration = 0.5f;

	/** Apex : vitesse de déplacement x ce multiplicateur au début de l'apex... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump", meta=(ClampMin="0"))
	float ApexStartMoveSpeedMultiplier = 0.5f;

	/** ... jusqu'à celui-ci à la fin (linéaire) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump", meta=(ClampMin="0"))
	float ApexEndMoveSpeedMultiplier = 1.0f;

	/** Chute : durée pour retomber au sol (coupe pendant la chute, sans déplacement) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump", meta=(ClampMin="0.05", Units="Seconds"))
	float FallDuration = 0.15f;

	/** En l'air (montée, apex) : contrôle du déplacement (1 = comme au sol) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump", meta=(ClampMin="0", ClampMax="1"))
	float JumpAirControl = 1.0f;

	/** Montée et apex : le personnage regarde le curseur (décoché = direction figée au clic) ; figé pendant la chute */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump")
	bool bAimDuringJump = true;

	/** Courbe de la coupe pendant la chute (EaseOut ; 1 = la lame balaye régulièrement toute la chute) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump", meta=(ClampMin="1"))
	float JumpSwingEaseExponent = 1.0f;

	/** Après la chute : dernière pose gardée, immobile, sans hitbox */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Landing", meta=(ClampMin="0", Units="Seconds"))
	float LandHoldDuration = 0.1f;

	/** Puis retour à la pose normale, la vitesse de déplacement remontant de 0 à 100 % (remplace Recover Duration) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Landing", meta=(ClampMin="0.01", Units="Seconds"))
	float LandRecoverDuration = 0.25f;

	/** Du pas en arrière à l'atterrissage, le personnage traverse les NME et les autres joueurs (remplace Dash Through Foes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump")
	bool bJumpThroughFoes = true;

	/** Le personnage traverse encore les NME pendant ce délai après l'atterrissage (pas propulsé s'il atterrit sur un NME) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump", meta=(ClampMin="0", Units="Seconds", EditCondition="bJumpThroughFoes"))
	float PassThroughAfterLanding = 0.2f;

	/** Pendant le MeteorJump (pas en arrière + saut), la Hitbox de l'arme est agrandie : extent x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump", meta=(ClampMin="0.1"))
	float HitboxExtentMultiplier = 2.0f;

	UYSMeteorJumpAbility();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Invulnérable pendant le pas en arrière et le saut (+ Extra Invulnerability) */
	virtual bool IsGrantingInvulnerability() const override;

	/** Montée / apex : déplacement réduit ; chute et pose gardée : immobile ; retour : remonte à 100 % */
	virtual float GetMoveSpeedMultiplier() const override;

	/** Déplacement forcé seulement pendant le pas en arrière */
	virtual bool GetForcedMoveDirection(FVector& OutDirection) const override;

	/** ZQSD ignoré pendant le pas en arrière, la chute et la pose gardée ; libre pendant la montée, l'apex et le retour */
	virtual bool OverridesMoveInput() const override;

	virtual bool GetLockedAimDirection(FVector& OutDirection) const override;

	/** Pas en arrière + saut + pose gardée + retour */
	virtual float GetActionDuration() const override;

	/** Animation : préparation pendant la montée, coupe pendant la chute, retour après la pose gardée */
	virtual float GetWindupTime() const override { return AscentDuration; }
	virtual float GetSwingTime() const override { return FallDuration; }
	virtual float GetRecoverTime() const override { return LandRecoverDuration; }

#if WITH_EDITOR
	virtual void GetUnusedProperties(TArray<FName>& OutNames) const override;
#endif

protected:

	virtual float GetDashPhaseDuration() const override { return AscentDuration + ApexDuration + FallDuration; }

	/** Temps de l'animation : immobile pendant le pas en arrière, l'apex (fin de la préparation) et la pose gardée (fin de la coupe) */
	virtual float GetSwingElapsed() const override;

	/** Coupe : courbe propre au saut (Jump Swing Ease Exponent) */
	virtual void GetCurrentEases(float& OutWindup, float& OutSwing, float& OutRecover) const override;

	/** Phase 1 : départ du saut (montée) */
	virtual void ApplyPhaseKick(int32 Phase) override;

	/** Pas en arrière, saut, et Pass Through After Landing après l'atterrissage */
	virtual bool PassesThroughPawns(int32 Phase) const override;

	virtual void OnActionCancelled() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Étapes du MeteorJump (machine qui pilote le mouvement) */
	enum class EJumpStage : uint8 { None, StepBack, Ascent, Apex, Fall, LandHold, LandRecover };

	/** Étape en cours et avancement dans l'étape (0..1) */
	EJumpStage GetJumpStage(float& OutAlpha) const;

	/** Apex : plus de gravité ni de vitesse verticale */
	void StartApex();

	/** Chute : droit vers le sol sous le personnage en Fall Duration, sans vitesse horizontale */
	void StartFall();

	/** Fin du saut : gravité et contrôle en l'air d'origine */
	void EndJump();

	/** Hitbox de l'arme x Hitbox Extent Multiplier pendant le MeteorJump, taille du Blueprint sinon (toutes les machines) */
	void UpdateHitboxExtent();

	/** Extent de la Hitbox du Blueprint (notée au premier passage) */
	FVector BaseHitboxExtent = FVector::ZeroVector;
	bool bHitboxEnlarged = false;

	/** Saut en cours (réglages du CharacterMovement modifiés), étape du saut appliquée et réglages d'origine */
	bool bJumping = false;
	EJumpStage AppliedStage = EJumpStage::None;
	float SavedGravityScale = 1.0f;
	float SavedAirControl = 0.05f;
	float SavedBrakingDecelerationFalling = 0.0f;
	float SavedFallingLateralFriction = 0.0f;

	/** Saut lancé, atterrissage pas encore vu ; traversée des NME jusqu'à cet instant (temps local) */
	bool bAwaitingLanding = false;
	double PassThroughUntil = -1.0;
};
