#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSAbilityComponent.h"
#include "YSBombThrowerAbility.generated.h"

class AYSPlayerBomb;

/**
 *  « BombThrower » : Ability du Sniper (Gun). Lance une bombe (Bomb Class : même Blueprint que la Roll and Bomb, donc même rayon
 *  d'explosion) en cloche vers le curseur ; elle met Flight Duration à arriver et explose en touchant le sol (pas de mèche).
 *  Point d'impact (serveur) : le curseur, ramené à Max Throw Distance du joueur ; il doit être sur l'île (seul le Landscape compte,
 *  les props sont traversés) et hors de l'eau. Sinon, on recule vers le joueur jusqu'à un sol valide ; aucun : lancée sur place.
 *  Dégâts : Bomb Damage x Damage bonus de l'Ability.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="BombThrower"))
class UYSBombThrowerAbility : public UYSAbilityComponent
{
	GENERATED_BODY()

public:

	/** Blueprint de la bombe lancée (/Game/Placeable/AMMO/BP_CannoneerBomb) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower")
	TSubclassOf<AYSPlayerBomb> BombClass;

	/** Dégâts de l'explosion à chaque NME touché (x Damage bonus de l'Ability) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower", meta=(ClampMin="0"))
	float BombDamage = 50.0f;

	/** Durée du vol jusqu'au point d'impact */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw", meta=(ClampMin="0.05", Units="Seconds"))
	float FlightDuration = 1.0f;

	/** Hauteur du sommet de la cloche au-dessus du plus haut des deux points */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw", meta=(ClampMin="1", Units="Centimeters"))
	float ApexHeight = 300.0f;

	/** Distance max du point d'impact au joueur (le curseur plus loin est ramené à cette distance) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw", meta=(ClampMin="0", Units="Centimeters"))
	float MaxThrowDistance = 1000.0f;

	/** Point d'impact invalide (eau, hors de l'île) : on recule vers le joueur de ce pas jusqu'à un sol valide */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw", meta=(ClampMin="10", Units="Centimeters"))
	float RetreatStep = 50.0f;

	UYSBombThrowerAbility();

	virtual float GetBaseDamage() const override { return BombDamage; }

	/** Point d'impact pour un curseur en TargetLocation (sol valide le plus proche du curseur vers le joueur, sinon sous le joueur) */
	FVector FindLandingPoint(const FVector& TargetLocation) const;

protected:

	/** Lance la bombe (serveur) */
	virtual void ExecuteAbility(const FVector& TargetLocation) override;

private:

	/** Sol de l'île (Landscape, props ignorés) sous Point, hors de l'eau ; faux sinon */
	bool FindIslandGround(const FVector& Point, float ReferenceZ, FVector& OutGround) const;
};
