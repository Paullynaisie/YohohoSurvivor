#include "Abilities/YSRollBombShotAbility.h"
#include "Engine/World.h"
#include "Weapons/YSProjectile.h"

UYSRollBombShotAbility::UYSRollBombShotAbility()
{
	DisplayName = NSLOCTEXT("YS", "RollBombShot", "Roll, bomb and shot");
	// visée après la roulade, comme le Rolling shot
	AimDuration = 0.25f;
	AimMoveSpeedMultiplier = 0.0f;
}

void UYSRollBombShotAbility::ExecuteAbility(const FVector& TargetLocation)
{
	DropBombs();
	UYSRollingShotAbility::ExecuteAbility(TargetLocation);
	UE_LOG(LogTemp, Verbose, TEXT("[YSRollBombShot] %s : bombe, roulade puis tir à %.2f"), *GetOwner()->GetName(), GetWorld()->GetTimeSeconds());
}

void UYSRollBombShotAbility::ConfigureProjectile(AYSProjectile* Projectile) const
{
	Projectile->SetDetonatesBombs(true);
	if (bPierceAllFoes)
	{
		Projectile->SetMaxHits(MAX_int32);
	}
}

#if WITH_EDITOR
void UYSRollBombShotAbility::GetUnusedProperties(TArray<FName>& OutNames) const
{
	// tire comme le Rolling shot : tous les réglages de tir servent (pas ceux cachés par la Roll and Bomb)
	UYSRollingShotAbility::GetUnusedProperties(OutNames);
}
#endif
