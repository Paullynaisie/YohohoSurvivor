#include "Abilities/YSCoupDPetoirAbility.h"
#include "Upgrades/YSUpgrade.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Weapons/YSWeapon.h"
#include "Weapons/YSProjectile.h"

namespace YSGun
{
	/** En dessous de cette distance au point visé, on tire droit devant (évite les directions instables) */
	constexpr float MinAimDistance = 50.0f;
}

UYSCoupDPetoirAbility::UYSCoupDPetoirAbility()
{
	DisplayName = NSLOCTEXT("YS", "CoupDPetoir", "Coup d'Petoir");
	WeaponType = EYSWeaponType::Gun;
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
	// cibles : ce que le projectile peut toucher (ses HitTags)
	const AYSProjectile* Projectile = ProjectileClass ? ProjectileClass->GetDefaultObject<AYSProjectile>() : nullptr;
	if (!Projectile)
	{
		return nullptr;
	}
	return FindTargetInCone(GetWorld(), Origin, AimDirection, AssistAimDistance, AssistAimAngle, Projectile->GetHitTags(), GetHolderPawn());
}

void UYSCoupDPetoirAbility::ExecuteAbility(const FVector& TargetLocation)
{
	// PewPew : rafale de BulletsPerBurst balles réparties sur BurstDuration (Shotgun / Piercing : à venir)
	GetWorld()->GetTimerManager().ClearTimer(BurstTimer);
	BurstTargetLocation = TargetLocation;
	BulletsFired = 0;
	BurstShooters.Reset();
	FireNextBullet();
}

void UYSCoupDPetoirAbility::FireNextBullet()
{
	if (UYSCoupDPetoirAbility* Shooter = GetBulletShooter(BulletsFired))
	{
		Shooter->FireBullet(GetBurstTargetLocation(BulletsFired), BulletsFired);
		BurstShooters.AddUnique(Shooter);
	}
	++BulletsFired;

	const int32 BulletCount = FMath::Max(1, BulletsPerBurst);
	if (BulletsFired < BulletCount)
	{
		// première balle tout de suite, dernière à la fin de la rafale
		const float Interval = BurstDuration / (BulletCount - 1);
		GetWorld()->GetTimerManager().SetTimer(BurstTimer, this, &UYSCoupDPetoirAbility::FireNextBullet, FMath::Max(Interval, KINDA_SMALL_NUMBER), false);
		return;
	}

	// fin de rafale : chaque arme qui a tiré joue l'animation de rechargement
	for (const TWeakObjectPtr<UYSCoupDPetoirAbility>& Shooter : BurstShooters)
	{
		if (Shooter.IsValid() && Shooter->bPlayReloadAnim)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[YSGun] %s rechargement à %.2f"), *Shooter->GetOwner()->GetName(), GetWorld()->GetTimeSeconds());
			Shooter->MulticastReloadStarted();
		}
	}
	BurstShooters.Reset();
}

void UYSCoupDPetoirAbility::OnActionCancelled()
{
	// action interrompue (priorité, objet porté) : la rafale s'arrête
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BurstTimer);
	}
	BurstShooters.Reset();
}

bool UYSCoupDPetoirAbility::IsActionInProgress() const
{
	const UWorld* World = GetWorld();
	return Super::IsActionInProgress() || (World && World->GetTimerManager().IsTimerActive(BurstTimer));
}

void UYSCoupDPetoirAbility::FireBullet(const FVector& TargetLocation, int32 BulletIndex)
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

	// visée assistée : un NME proche de la direction visée remplace le point visé
	FVector AimPoint = TargetLocation;
	if (const AActor* AssistTarget = FindAssistTarget(Aimer->GetActorLocation(), TargetLocation - Aimer->GetActorLocation()))
	{
		AimPoint = AssistTarget->GetActorLocation();
	}

	// trajectoire parallèle au sol
	FVector Direction = AimPoint - Muzzle;
	Direction.Z = 0.0f;
	if (Direction.SizeSquared() < FMath::Square(YSGun::MinAimDistance))
	{
		Direction = Aimer->GetActorForwardVector();
		Direction.Z = 0.0f;
	}

	const FTransform SpawnTransform(Direction.GetSafeNormal().Rotation(), Muzzle);
	AYSProjectile* Projectile = GetWorld()->SpawnActorDeferred<AYSProjectile>(ProjectileClass, SpawnTransform, Shooter, Shooter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Projectile)
	{
		Projectile->SetDamage(Damage * GetDamageMultiplier());
		// upgrades d'arme (Ricochet) : seulement pour l'Ability, pas pour l'Auto shot
		if (!IsAutomatic())
		{
			Projectile->SetRicochets(FMath::RoundToInt(GetWeaponStat(UYSRicochetUpgrade::RicochetCountStat, 0.0f)), 1.0f + GetWeaponStat(UYSRicochetUpgrade::RicochetDamageBonusStat, 0.0f));
		}
		Projectile->FinishSpawning(SpawnTransform);
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSGun] %s.%s balle %d à %.2f"), *Owner->GetName(), *GetName(), BulletIndex + 1, GetWorld()->GetTimeSeconds());
	MulticastBulletFired(BulletIndex);
}

void UYSCoupDPetoirAbility::MulticastBulletFired_Implementation(int32 BulletIndex)
{
	// recul de chaque balle ; le tireur a déjà joué celui de la première par prédiction (Ability)
	if (BulletIndex > 0 || IsAutomatic() || !IsLocallyPredicted())
	{
		NotifyActivated();
	}
}

void UYSCoupDPetoirAbility::MulticastReloadStarted_Implementation()
{
	if (const UWorld* World = GetWorld())
	{
		LastReloadTime = World->GetTimeSeconds();
	}
}

float UYSCoupDPetoirAbility::GetRecoilAlpha() const
{
	const float Elapsed = GetTimeSinceActivation();
	if (Elapsed < 0.0f || Elapsed >= RecoilKickDuration + RecoilRecoverDuration)
	{
		return 0.0f;
	}

	// coup sec puis retour plus doux
	if (Elapsed < RecoilKickDuration)
	{
		return FMath::InterpEaseOut(0.0f, 1.0f, Elapsed / RecoilKickDuration, 2.0f);
	}
	return FMath::InterpEaseInOut(1.0f, 0.0f, (Elapsed - RecoilKickDuration) / RecoilRecoverDuration, 2.0f);
}

float UYSCoupDPetoirAbility::GetTimeSinceReload() const
{
	const UWorld* World = GetWorld();
	return World ? static_cast<float>(World->GetTimeSeconds() - LastReloadTime) : TNumericLimits<float>::Max();
}

FRotator UYSCoupDPetoirAbility::GetReloadSpin() const
{
	const float Elapsed = GetTimeSinceReload();
	if (Elapsed < 0.0f || Elapsed >= ReloadSpinDuration)
	{
		return FRotator::ZeroRotator;
	}

	const float Angle = 360.0f * ReloadSpinTurns * FMath::InterpEaseInOut(0.0f, 1.0f, Elapsed / ReloadSpinDuration, ReloadSpinEaseExponent);
	switch (ReloadSpinAxis)
	{
	case EYSReloadSpinAxis::Yaw:	return FRotator(0.0f, Angle, 0.0f);
	case EYSReloadSpinAxis::Roll:	return FRotator(0.0f, 0.0f, Angle);
	default:						return FRotator(Angle, 0.0f, 0.0f);
	}
}
