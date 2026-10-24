#include "Abilities/YSBumpingSlashAbility.h"
#include "Core/YSDamageTypes.h"

UYSBumpingSlashAbility::UYSBumpingSlashAbility()
{
	DisplayName = NSLOCTEXT("YS", "BumpingSlash", "Bumping Slash");
	bDashInvulnerable = false;
}

void UYSBumpingSlashAbility::DealSwingDamage(AActor* Target, float FinalDamage, const FVector& ShotDirection, AController* InstigatorController, bool bCritical)
{
	FYSBumpDamageEvent DamageEvent(FinalDamage, ShotDirection, bCritical ? UYSCriticalDamageType::StaticClass() : nullptr);
	DamageEvent.Distance = BumpDistance;
	DamageEvent.ApexHeight = BumpApexHeight;
	DamageEvent.Duration = BumpDuration;
	DamageEvent.SpinTurns = BumpSpinTurns;
	DamageEvent.bBumpElites = bBumpElites;
	Target->TakeDamage(FinalDamage, DamageEvent, InstigatorController, nullptr);
}
