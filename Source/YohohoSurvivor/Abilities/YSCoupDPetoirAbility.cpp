#include "Abilities/YSCoupDPetoirAbility.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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

AActor* UYSCoupDPetoirAbility::FindAssistTarget(const FVector& Origin, const FVector& AimDirection) const
{
	const FVector Direction = FVector(AimDirection.X, AimDirection.Y, 0.0f).GetSafeNormal();
	const AYSProjectile* Projectile = ProjectileClass ? ProjectileClass->GetDefaultObject<AYSProjectile>() : nullptr;
	if (!Projectile || AssistAimDistance <= 0.0f || AssistAimAngle <= 0.0f || Direction.IsNearlyZero() || !GetWorld())
	{
		return nullptr;
	}

	const AYSWeapon* Weapon = Cast<AYSWeapon>(GetOwner());
	const AActor* Holder = Weapon ? Weapon->GetHolder() : GetOwner();
	const float MinCos = FMath::Cos(FMath::DegreesToRadians(AssistAimAngle));

	AActor* BestTarget = nullptr;
	float BestDistanceSq = FMath::Square(AssistAimDistance);
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Candidate = *It;
		if (Candidate == Holder || !Projectile->CanHit(Candidate))
		{
			continue;
		}

		const FVector ToCandidate = FVector(Candidate->GetActorLocation() - Origin) * FVector(1.0f, 1.0f, 0.0f);
		const float DistanceSq = ToCandidate.SizeSquared();
		if (DistanceSq > BestDistanceSq || DistanceSq < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		if (FVector::DotProduct(ToCandidate.GetSafeNormal(), Direction) >= MinCos)
		{
			BestTarget = Candidate;
			BestDistanceSq = DistanceSq;
		}
	}
	return BestTarget;
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

	// visée assistée : un NME proche de la direction de la souris remplace le point visé
	FVector AimPoint = TargetLocation;
	if (const AActor* AssistTarget = FindAssistTarget(Aimer->GetActorLocation(), TargetLocation - Aimer->GetActorLocation()))
	{
		AimPoint = AssistTarget->GetActorLocation();
	}

	// trajectoire parallèle au sol
	FVector Direction = AimPoint - Muzzle;
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
