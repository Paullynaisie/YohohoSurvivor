#include "Abilities/YSMoraleBoostAbility.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Player/YSPlayerCharacter.h"

UYSMoraleBoostAbility::UYSMoraleBoostAbility()
{
	DisplayName = NSLOCTEXT("YS", "MoraleBoost", "Booster de moral");
	StaminaCost = 65.0f;
}

void UYSMoraleBoostAbility::ExecuteMoveAction(const FVector& TargetLocation)
{
	const AYSPlayerCharacter* Captain = GetCharacter();
	if (!Captain)
	{
		return;
	}

	// joueurs, minions et HiredMouss (tous des AYSPlayerCharacter) vivants dans le rayon
	int32 Count = 0;
	const float RadiusSq = FMath::Square(Radius);
	for (TActorIterator<AYSPlayerCharacter> It(GetWorld()); It; ++It)
	{
		AYSPlayerCharacter* Target = *It;
		if (!IsValid(Target) || !Target->IsAlive() || Target->IsDying() || (Target == Captain && !bIncludeSelf))
		{
			continue;
		}
		if (FVector::DistSquared2D(Target->GetActorLocation(), Captain->GetActorLocation()) > RadiusSq)
		{
			continue;
		}
		Target->ApplyMoveSpeedBuff(SpeedMultiplier, Duration);
		++Count;
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSMoraleBoost] %s : %d personnages x%.2f pendant %.1f s"), *Captain->GetName(), Count, SpeedMultiplier, Duration);
}
