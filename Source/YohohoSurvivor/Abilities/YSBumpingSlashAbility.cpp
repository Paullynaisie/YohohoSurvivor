#include "Abilities/YSBumpingSlashAbility.h"
#include "Core/YSDamageTypes.h"

UYSBumpingSlashAbility::UYSBumpingSlashAbility()
{
	DisplayName = NSLOCTEXT("YS", "BumpingSlash", "Bumping Slash");
	bDashInvulnerable = false;
	// la recharge de l'attaque auto est rendue (Quartermaster, Pull du Harquebusier)
	bResetAutoAttackOnUse = true;
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
