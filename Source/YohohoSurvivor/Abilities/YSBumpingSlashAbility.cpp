#include "Abilities/YSBumpingSlashAbility.h"
#include "Core/YSDamageTypes.h"

UYSBumpingSlashAbility::UYSBumpingSlashAbility()
{
	bDashInvulnerable = false;
}

void UYSBumpingSlashAbility::DealSwingDamage(AActor* Target, float FinalDamage, const FVector& ShotDirection, AController* InstigatorController, bool bCritical)
{
	FYSBumpDamageEvent DamageEvent(FinalDamage, ShotDirection, bCritical ? UYSCriticalDamageType::StaticClass() : nullptr);
	DamageEvent.Distance = BumpDistance;
	DamageEvent.ApexHeight = BumpApexHeight;
	DamageEvent.Duration = BumpDuration;
	DamageEvent.SpinTurns = BumpSpinTurns;
	Target->TakeDamage(FinalDamage, DamageEvent, InstigatorController, nullptr);
}
