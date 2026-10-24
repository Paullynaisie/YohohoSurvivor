#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSBumpingSlashAbility.h"
#include "YSMeteorJumpAbility.generated.h"

/**
 *  « MeteorJump » : Ability de la Great Anchor (set Anchor Bearer du Quartermaster ; sous-classe du Bumping Slash : même pas en
 *  arrière, même animation de coupe, même bump des NME non élites touchés).
 *  Au lieu du dash, un vrai SAUT en cloche vers le curseur, comme le lancer de bombe du Sniper : point d'arrivée ramené à
 *  Max Jump Distance et reculé par pas de Retreat Step jusqu'à un sol de l'île hors de l'eau (YSCombat::FindLandingPoint),
 *  sommet Jump Apex Height au-dessus du plus haut des deux points, durée Jump Duration. Le saut est une chute du
 *  CharacterMovement (vitesse de départ et gravité calculées pour suivre la cloche ; serveur et client qui contrôle : prédiction).
 *  Pendant le pas en arrière ET le saut : invulnérable, traverse les NME. Préparation de la coupe pendant le pas en arrière,
 *  coupe (hitbox active) pendant le saut.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="MeteorJump"))
class UYSMeteorJumpAbility : public UYSBumpingSlashAbility
{
	GENERATED_BODY()

public:

	/** Durée du saut jusqu'au point d'arrivée */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump", meta=(ClampMin="0.05", Units="Seconds"))
	float JumpDuration = 1.0f;

	/** Hauteur du sommet de la cloche au-dessus du plus haut des deux points */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump", meta=(ClampMin="1", Units="Centimeters"))
	float JumpApexHeight = 300.0f;

	/** Distance max du point d'arrivée au personnage (le curseur plus loin est ramené à cette distance) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump", meta=(ClampMin="0", Units="Centimeters"))
	float MaxJumpDistance = 1000.0f;

	/** Point d'arrivée invalide (eau, hors de l'île) : on recule vers le personnage de ce pas jusqu'à un sol valide */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump", meta=(ClampMin="10", Units="Centimeters"))
	float RetreatStep = 50.0f;

	/** Pendant le pas en arrière et le saut, le personnage traverse les NME et les autres joueurs (remplace Dash Through Foes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump")
	bool bJumpThroughFoes = true;

	/** Le personnage traverse encore les NME pendant ce délai après l'atterrissage (pas propulsé s'il atterrit sur un NME) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump", meta=(ClampMin="0", Units="Seconds", EditCondition="bJumpThroughFoes"))
	float PassThroughAfterLanding = 0.2f;

	/** Pendant le MeteorJump (pas en arrière + saut), la Hitbox de l'arme est agrandie : extent x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump", meta=(ClampMin="0.1"))
	float HitboxExtentMultiplier = 2.0f;

	/** Courbe de la coupe pendant le saut (EaseOut ; 1 = la lame balaye régulièrement tout le saut, jusqu'à l'atterrissage) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meteor Jump|Jump", meta=(ClampMin="1"))
	float JumpSwingEaseExponent = 1.0f;

	UYSMeteorJumpAbility();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Invulnérable pendant le pas en arrière et le saut (+ Extra Invulnerability) */
	virtual bool IsGrantingInvulnerability() const override;

	/** Saut : vitesse au sol inchangée (le saut est une chute à vitesse fixée au départ) */
	virtual float GetMoveSpeedMultiplier() const override;

	/** Pas de déplacement forcé pendant le saut (la trajectoire est celle de la chute) */
	virtual bool GetForcedMoveDirection(FVector& OutDirection) const override;

protected:

	virtual float GetDashPhaseDuration() const override { return JumpDuration; }

	/** Coupe : courbe propre au saut (Jump Swing Ease Exponent) */
	virtual void GetCurrentEases(float& OutWindup, float& OutSwing, float& OutRecover) const override;

	/** Direction et point d'arrivée du saut (curseur au clic) */
	virtual void LockDirection(const FVector& TargetLocation) override;

	/** Phase 1 : départ du saut */
	virtual void ApplyPhaseKick(int32 Phase) override;

	/** Pas en arrière, saut, et Pass Through After Landing après l'atterrissage */
	virtual bool PassesThroughPawns(int32 Phase) const override;

	virtual void OnActionCancelled() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Fin du saut : gravité normale */
	void EndJump();

	/** Hitbox de l'arme x Hitbox Extent Multiplier pendant le MeteorJump, taille du Blueprint sinon (toutes les machines) */
	void UpdateHitboxExtent();

	/** Extent de la Hitbox du Blueprint (notée au premier passage) */
	FVector BaseHitboxExtent = FVector::ZeroVector;
	bool bHitboxEnlarged = false;

	/** Point d'arrivée du saut (sol), choisi au clic */
	FVector JumpLanding = FVector::ZeroVector;

	/** Saut en cours (gravité modifiée) et gravité d'origine du personnage */
	bool bJumping = false;
	float SavedGravityScale = 1.0f;

	/** Saut lancé, atterrissage pas encore vu ; traversée des NME jusqu'à cet instant (temps local) */
	bool bAwaitingLanding = false;
	double PassThroughUntil = -1.0;
};
