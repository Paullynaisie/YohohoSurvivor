#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSDashingSlashAbility.h"
#include "YSBumpingSlashAbility.generated.h"

/**
 *  « Bumping Slash » : Ability de la GreatAxe du Quartermaster. Variante du Dashing Slash (même pas en arrière, même dash,
 *  même animation de coupe) mais le dash NE rend PAS invulnérable (Dash Invulnerable décoché par défaut).
 *  Chaque NME non élite touché qui survit au coup est « bumpé » (AYSFoeCharacter::StartBump) : plus de comportement, lancé en
 *  cloche à Bump Distance dans la direction du coup (du Quartermaster vers le NME, comme le pushback), en tournant sur lui-même.
 *  Un élite touché reçoit le pushback normal.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Bumping Slash"))
class UYSBumpingSlashAbility : public UYSDashingSlashAbility
{
	GENERATED_BODY()

public:

	/** Distance du bump (à plat, depuis la position du NME) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bumping Slash|Bump", meta=(ClampMin="0", Units="Centimeters"))
	float BumpDistance = 800.0f;

	/** Sommet de la cloche : hauteur au-dessus du plus haut des deux points (départ / arrivée), comme le lancer de bombe */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bumping Slash|Bump", meta=(ClampMin="1", Units="Centimeters"))
	float BumpApexHeight = 300.0f;

	/** Durée du vol */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bumping Slash|Bump", meta=(ClampMin="0.05", Units="Seconds"))
	float BumpDuration = 1.0f;

	/** Tours sur lui-même pendant le vol (1 = 360°), basculé dans le sens du bump comme un ragdoll */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bumping Slash|Bump", meta=(ClampMin="0"))
	float BumpSpinTurns = 1.0f;

	UYSBumpingSlashAbility();

protected:

	/** Coup qui bumpe (FYSBumpDamageEvent) */
	virtual void DealSwingDamage(AActor* Target, float FinalDamage, const FVector& ShotDirection, AController* InstigatorController, bool bCritical) override;
};
