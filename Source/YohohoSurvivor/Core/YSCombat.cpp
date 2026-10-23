#include "Core/YSCombat.h"
#include "Components/PrimitiveComponent.h"

namespace YSCombat
{
	void ConfigureHurtbox(UPrimitiveComponent* Component)
	{
		if (!Component)
		{
			return;
		}

		Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Component->SetCollisionObjectType(ECC_Pawn);
		Component->SetCollisionResponseToAllChannels(ECR_Ignore);
		Component->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		Component->SetGenerateOverlapEvents(true);
	}

	void ConfigureHitbox(UPrimitiveComponent* Component)
	{
		if (!Component)
		{
			return;
		}

		Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Component->SetCollisionObjectType(ECC_WorldDynamic);
		Component->SetCollisionResponseToAllChannels(ECR_Ignore);
		Component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Component->SetGenerateOverlapEvents(true);
	}
}

namespace YSCombat
{
	float ComputePushbackForce(float Damage, float TargetMaxHealth, float MinRatio, float MaxRatio, float MinForce, float MaxForce)
	{
		if (Damage <= 0.0f || TargetMaxHealth <= 0.0f)
		{
			return 0.0f;
		}
		// part de la vie de la cible retirée par le coup : rien sous le premier palier, force max à partir du second
		const float Ratio = Damage / TargetMaxHealth;
		if (Ratio < MinRatio)
		{
			return 0.0f;
		}
		if (Ratio >= MaxRatio || MaxRatio <= MinRatio)
		{
			return MaxForce;
		}
		return FMath::Lerp(MinForce, MaxForce, (Ratio - MinRatio) / (MaxRatio - MinRatio));
	}

	FVector GetBellLocation(const FVector& Start, const FVector& Target, float Duration, float ApexHeight, float Time)
	{
		const float SafeDuration = FMath::Max(Duration, 0.05f);
		const float Clamped = FMath::Clamp(Time, 0.0f, SafeDuration);

		// montée sqrt(2 Up / g) + descente sqrt(2 Down / g) = Duration  =>  g = ((sqrt(2 Up) + sqrt(2 Down)) / Duration)²
		const float ApexZ = FMath::Max(Start.Z, Target.Z) + FMath::Max(ApexHeight, 1.0f);
		const float Rise = FMath::Sqrt(2.0f * (ApexZ - Start.Z)) + FMath::Sqrt(2.0f * (ApexZ - Target.Z));
		const float Gravity = FMath::Square(Rise / SafeDuration);
		const float VerticalSpeed = FMath::Sqrt(2.0f * Gravity * (ApexZ - Start.Z));

		FVector Location = FMath::Lerp(Start, Target, Clamped / SafeDuration);
		Location.Z = Start.Z + VerticalSpeed * Clamped - 0.5f * Gravity * Clamped * Clamped;
		return Location;
	}
}
