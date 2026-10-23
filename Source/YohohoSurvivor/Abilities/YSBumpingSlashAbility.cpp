#include "Abilities/YSBumpingSlashAbility.h"
#include "Core/YSDamageTypes.h"

UYSBumpingSlashAbility::UYSBumpingSlashAbility()
{
	bDashInvulnerable = false;
}

void UYSBumpingSlashAbility::DealSwingDamage(AActor* Target, float FinalDamage, const FVector& ShotDirection, AController* InstigatorController)
{
	FYSBumpDamageEvent DamageEvent(FinalDamage, ShotDirection, bCriticalSwing ? UYSCriticalDamageType::StaticClass() : nullptr);
	DamageEvent.Distance = BumpDistance;
	DamageEvent.ApexHeight = BumpApexHeight;
	DamageEvent.Duration = BumpDuration;
	DamageEvent.SpinTurns = BumpSpinTurns;
	Target->TakeDamage(FinalDamage, DamageEvent, InstigatorController, nullptr);
}
