#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSRollAbility.h"
#include "YSShoulderBumpAbility.generated.h"

class AYSFoeCharacter;

/**
 *  « Shoulder bump » : MoveAction du Quartermaster. Dash (réglages de la roulade : Rolling Shot|Roll, durée, vitesse) dans la
 *  direction de l'input, sinon vers le curseur, coût Stamina Cost (75) ; NI intangible NI invulnérable (Roll Through Foes faux).
 *  Pendant le dash, une hitbox devant le personnage (Hitbox Radius / Forward Offset / Half Height) ne fait AUCUN dégât mais
 *  bumpe chaque NME touché (une fois par dash) PERPENDICULAIREMENT au dash, du côté où il se trouve, à Bump Distance (400).
 *  Corps penché vers l'avant (Lean Angle) au lieu de la roulade, regard figé dans la direction du dash.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Shoulder bump"))
class UYSShoulderBumpAbility : public UYSRollAbility
{
	GENERATED_BODY()

public:

	/** Distance du bump (perpendiculaire au dash, à plat) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoulder Bump|Bump", meta=(ClampMin="0", Units="Centimeters"))
	float BumpDistance = 400.0f;

	/** Hauteur du sommet du vol */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoulder Bump|Bump", meta=(ClampMin="0", Units="Centimeters"))
	float BumpApexHeight = 200.0f;

	/** Durée du vol */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoulder Bump|Bump", meta=(ClampMin="0.05", Units="Seconds"))
	float BumpDuration = 0.7f;

	/** Tours sur lui-même pendant le vol */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoulder Bump|Bump")
	float BumpSpinTurns = 1.0f;

	/** Les élites sont bumpés aussi (comme pour un coup qui bumpe, un élite tombe sur place) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoulder Bump|Bump")
	bool bBumpElites = true;

	/** Hitbox : rayon (à plat, ajouté au rayon de la capsule du NME) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoulder Bump|Hitbox", meta=(ClampMin="0", Units="Centimeters"))
	float HitboxRadius = 80.0f;

	/** Hitbox : centre à cette distance devant le personnage, dans la direction du dash */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoulder Bump|Hitbox", meta=(Units="Centimeters"))
	float HitboxForwardOffset = 60.0f;

	/** Hitbox : demi-hauteur (ajoutée à celle de la capsule du NME) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoulder Bump|Hitbox", meta=(ClampMin="0", Units="Centimeters"))
	float HitboxHalfHeight = 100.0f;

	/** La hitbox reste active ce temps après la fin du dash */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoulder Bump|Hitbox", meta=(ClampMin="0", Units="Seconds"))
	float HitboxExtraDuration = 0.05f;

	/** Corps penché vers l'avant pendant le dash (degrés ; monte puis redescend) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shoulder Bump|Anim")
	float LeanAngle = 25.0f;

	UYSShoulderBumpAbility();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Jamais invulnérable */
	virtual bool IsGrantingInvulnerability() const override { return false; }

	/** Penché vers l'avant au lieu de la roulade */
	virtual bool GetBodyRoll(float& OutAngle, float& OutHeightOffset) const override;

	/** Regard figé dans la direction du dash */
	virtual bool GetLockedAimDirection(FVector& OutDirection) const override;

#if WITH_EDITOR
	virtual void GetUnusedProperties(TArray<FName>& OutNames) const override;
#endif

protected:

	virtual bool RollsTowardAim() const override { return true; }

	virtual void ExecuteAbility(const FVector& TargetLocation) override;

private:

	/** NME déjà bumpés pendant ce dash (serveur) */
	TSet<TWeakObjectPtr<AYSFoeCharacter>> HitFoes;

	/** Serveur : bumpe les NME dans la hitbox */
	void UpdateHitbox();
};
