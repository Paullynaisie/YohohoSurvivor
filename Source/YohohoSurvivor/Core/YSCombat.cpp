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
