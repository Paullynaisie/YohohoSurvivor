#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "YSBodyAnimTypes.generated.h"

/** Sortie du sol à l'apparition (NME, minions du Captain) : répliquée, position recalculée à l'identique sur chaque machine */
USTRUCT()
struct FYSFoeRise
{
	GENERATED_BODY()

	UPROPERTY()
	bool bActive = false;

	/** Position finale de l'acteur (au sol) */
	UPROPERTY()
	FVector_NetQuantize Ground = FVector::ZeroVector;

	/** Profondeur de départ sous Ground */
	UPROPERTY()
	float Depth = 0.0f;

	UPROPERTY()
	float Duration = 2.0f;

	UPROPERTY()
	double StartServerTime = 0.0;
};

/** Mort (NME, joueurs, minions) : ragdoll local sur chaque machine, lancé à la vitesse Impulse */
USTRUCT()
struct FYSFoeDeath
{
	GENERATED_BODY()

	UPROPERTY()
	bool bDead = false;

	UPROPERTY()
	FVector_NetQuantize Impulse = FVector::ZeroVector;
};
