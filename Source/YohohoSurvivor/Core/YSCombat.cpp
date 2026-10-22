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
}
