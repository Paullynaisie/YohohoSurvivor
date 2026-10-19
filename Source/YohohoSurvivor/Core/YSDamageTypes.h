#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "YSDamageTypes.generated.h"

struct FDamageEvent;

/**
 *  Coup critique : type de dégât passé dans le FDamageEvent par l'attaquant (dégâts déjà multipliés par lui).
 *  La victime le transmet à AYSGameState::ReportDamage : chiffre de dégâts plus gros (AYSDamageNumber::CriticalScale).
 */
UCLASS()
class UYSCriticalDamageType : public UDamageType
{
	GENERATED_BODY()

public:

	/** Le coup est-il critique ? */
	static bool IsCritical(const FDamageEvent& DamageEvent);
};
