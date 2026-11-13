#include "Abilities/YSMoraleBoostAbility.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Player/YSPlayerCharacter.h"

UYSMoraleBoostAbility::UYSMoraleBoostAbility()
{
	DisplayName = NSLOCTEXT("YS", "MoraleBoost", "Booster de moral");
	StaminaCost = 65.0f;
}

float UYSMoraleBoostAbility::GetMoveSpeedMultiplier() const
{
	const float Elapsed = GetTimeSinceActivation();
	return Elapsed >= 0.0f && Elapsed < RaiseHandsDuration ? 0.0f : 1.0f;
}

bool UYSMoraleBoostAbility::EvaluateHandPose(float& OutIdleWeight, FVector& OutLocation, FQuat& OutRotation) const
{
	const float Elapsed = GetTimeSinceActivation();
	if (RaiseHandsDuration <= 0.0f || Elapsed < 0.0f || Elapsed >= RaiseHandsDuration)
	{
		return false;
	}
	// monte, reste en haut, redescend
	const float Alpha = Elapsed / RaiseHandsDuration;
	const float Blend = FMath::Clamp(FMath::Min(Alpha, 1.0f - Alpha) / FMath::Max(RaiseBlendFraction, 0.01f), 0.0f, 1.0f);
	OutIdleWeight = 1.0f - FMath::InterpEaseInOut(0.0f, 1.0f, Blend, 2.0f);
	OutLocation = RaisedHandLocation;
	OutRotation = RaisedHandRotation.Quaternion();
	return true;
}

void UYSMoraleBoostAbility::ExecuteMoveAction(const FVector& TargetLocation)
{
	const AYSPlayerCharacter* Booster = GetCharacter();
	if (!Booster)
	{
		return;
	}

	const int32 Golden = GetMoveActionGolden();
	const float BoostDuration = GetBoostDuration();
	// joueurs, minions et HiredMouss (tous des AYSPlayerCharacter) vivants dans le rayon
	int32 Count = 0;
	const float RadiusSq = FMath::Square(Radius);
	for (TActorIterator<AYSPlayerCharacter> It(GetWorld()); It; ++It)
	{
		AYSPlayerCharacter* Target = *It;
		if (!IsValid(Target) || !Target->IsAlive() || Target->IsDying() || (Target == Booster && !bIncludeSelf))
		{
			continue;
		}
		if (FVector::DistSquared2D(Target->GetActorLocation(), Booster->GetActorLocation()) > RadiusSq)
		{
			continue;
		}
		Target->ApplyMoraleBoost(SpeedMultiplier, BoostDuration, Golden, GoldenDamageBonus, GoldenHealPerSecond, GoldenCooldownSpeed, Golden == GoldenCriticalIndex);
		++Count;
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSMoraleBoost] %s : %d personnages x%.2f pendant %.1f s (Golden %d)"), *Booster->GetName(), Count, SpeedMultiplier, BoostDuration, Golden);
}
