#include "Abilities/YSCoupDPetoirAbility.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"
#include "Weapons/YSWeapon.h"
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

	// l'action vit sur une arme : on tire depuis sa bouche, au nom du pawn qui la tient
	const AYSWeapon* Weapon = Cast<AYSWeapon>(Owner);
	APawn* Shooter = Weapon ? Weapon->GetHolder() : Cast<APawn>(Owner);
	const FVector Muzzle = Weapon ? Weapon->GetMuzzleLocation() : Owner->GetActorLocation();
	const AActor* Aimer = Shooter ? static_cast<const AActor*>(Shooter) : Owner;

	// trajectoire parallèle au sol
	FVector Direction = TargetLocation - Muzzle;
	Direction.Z = 0.0f;
	if (Direction.SizeSquared() < FMath::Square(MinAimDistance))
	{
		Direction = Aimer->GetActorForwardVector();
		Direction.Z = 0.0f;
	}

	const FTransform SpawnTransform(Direction.GetSafeNormal().Rotation(), Muzzle);
	AYSProjectile* Projectile = GetWorld()->SpawnActorDeferred<AYSProjectile>(ProjectileClass, SpawnTransform, Shooter, Shooter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Projectile)
	{
		Projectile->SetDamage(Damage);
		Projectile->FinishSpawning(SpawnTransform);
	}
}
