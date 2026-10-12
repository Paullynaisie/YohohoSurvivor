#include "Abilities/YSCoupDPetoirAbility.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "Weapons/YSProjectile.h"

namespace
{
	/** En dessous de cette distance au point visé, on tire droit devant (évite les directions instables) */
	constexpr float MinAimDistance = 50.0f;
}

UYSCoupDPetoirAbility::UYSCoupDPetoirAbility()
{
	DisplayName = NSLOCTEXT("YS", "CoupDPetoir", "Coup d'Petoir");
	Cooldown = 5.0f;
	IconTint = FLinearColor(0.85f, 0.55f, 0.15f);
}

void UYSCoupDPetoirAbility::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UYSCoupDPetoirAbility, Damage);
}

void UYSCoupDPetoirAbility::ExecuteAbility(const FVector& TargetLocation)
{
	AActor* Owner = GetOwner();
	if (!ProjectileClass || !Owner)
	{
		return;
	}

	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(Owner);
	const FVector Muzzle = Character ? Character->GetMuzzleLocation() : Owner->GetActorLocation();

	// trajectoire parallèle au sol
	FVector Direction = TargetLocation - Muzzle;
	Direction.Z = 0.0f;
	if (Direction.SizeSquared() < FMath::Square(MinAimDistance))
	{
		Direction = Owner->GetActorForwardVector();
		Direction.Z = 0.0f;
	}

	const FTransform SpawnTransform(Direction.GetSafeNormal().Rotation(), Muzzle);
	AYSProjectile* Projectile = GetWorld()->SpawnActorDeferred<AYSProjectile>(ProjectileClass, SpawnTransform, Owner, Cast<APawn>(Owner), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Projectile)
	{
		Projectile->SetDamage(Damage);
		Projectile->FinishSpawning(SpawnTransform);
	}
}
