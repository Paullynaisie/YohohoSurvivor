#pragma once

#include "CoreMinimal.h"
#include "Engine/DamageEvents.h"
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

/**
 *  Coup ponctuel (direction du pushback = ShotDirection) qui ajoute ExtraPushback à la force naturelle du pushback
 *  (ex. mine avec l'upgrade « Mine repulsante » ; AYSFoeCharacter::ApplyPushback).
 */
USTRUCT()
struct FYSPushbackDamageEvent : public FPointDamageEvent
{
	GENERATED_BODY()

	UPROPERTY()
	float ExtraPushback = 0.0f;

	FYSPushbackDamageEvent() = default;

	FYSPushbackDamageEvent(float InDamage, const FHitResult& InHitInfo, const FVector& InShotDirection, float InExtraPushback)
		: FPointDamageEvent(InDamage, InHitInfo, InShotDirection, nullptr), ExtraPushback(InExtraPushback)
	{
	}

	static const int32 ClassID = 101;

	virtual int32 GetTypeID() const override { return FYSPushbackDamageEvent::ClassID; }
	virtual bool IsOfType(int32 InID) const override { return FYSPushbackDamageEvent::ClassID == InID || FPointDamageEvent::IsOfType(InID); }
};
