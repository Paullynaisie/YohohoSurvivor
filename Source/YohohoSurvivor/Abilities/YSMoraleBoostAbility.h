#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSMoveActionAbility.h"
#include "YSMoraleBoostAbility.generated.h"

/**
 *  « Booster de moral » : MoveAction du Captain. Coûte Stamina Cost (65, cran de la barre) ; tous les joueurs ET minions vivants
 *  (HiredMouss compris, le Captain aussi si Include Self) à moins de Radius (15 m, à plat) gagnent x Speed Multiplier de vitesse
 *  de déplacement pendant Duration (AYSPlayerCharacter::ApplyMoveSpeedBuff : un bonus déjà actif garde le plus grand et le plus long).
 *  Instantané : n'interrompt et ne bloque aucune action.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Booster de moral"))
class UYSMoraleBoostAbility : public UYSMoveActionAbility
{
	GENERATED_BODY()

public:

	/** Portée (à plat) autour du Captain */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morale Boost", meta=(ClampMin="0", Units="Centimeters"))
	float Radius = 1500.0f;

	/** Vitesse de déplacement x ce multiplicateur (1.35 = +35 %) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morale Boost", meta=(ClampMin="0"))
	float SpeedMultiplier = 1.35f;

	/** Durée du bonus */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morale Boost", meta=(ClampMin="0", Units="Seconds"))
	float Duration = 5.0f;

	/** Le Captain profite aussi du bonus */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morale Boost")
	bool bIncludeSelf = true;

	UYSMoraleBoostAbility();

protected:

	virtual void ExecuteMoveAction(const FVector& TargetLocation) override;
};
