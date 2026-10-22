#include "Abilities/YSCoupDPetoirAbility.h"
#include "Upgrades/YSUpgrade.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Weapons/YSWeapon.h"
#include "Player/YSPlayerCharacter.h"
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
	StartBurst(TargetLocation);
}

void UYSCoupDPetoirAbility::OnActivationPredicted(const FVector& TargetLocation)
{
	Super::OnActivationPredicted(TargetLocation);
	if (PredictsFirstBullet())
	{
		RecoilTimeScale = ComputeRecoilTimeScale(GetTimeToNextShot(0, GetBulletsPerBurst()));
	}
}

void UYSCoupDPetoirAbility::StartBurst(const FVector& TargetLocation)
{
	// PewPew : rafale de BulletsPerBurst balles réparties sur BurstDuration (Shotgun / Piercing : à venir)
	GetWorld()->GetTimerManager().ClearTimer(BurstTimer);
	BurstTargetLocation = TargetLocation;
	BulletsFired = 0;
	BurstShooters.Reset();
	BurstBulletCount = GetBulletsPerBurst();
	FireNextBullet();
}

void UYSCoupDPetoirAbility::FireNextBullet()
{
	const int32 BulletCount = FMath::Max(1, BurstBulletCount);
	if (UYSCoupDPetoirAbility* Shooter = GetBulletShooter(BulletsFired))
	{
		Shooter->FireBullet(GetBurstTargetLocation(BulletsFired), BulletsFired, GetTimeToNextShot(BulletsFired, BulletCount));
		BurstShooters.AddUnique(Shooter);
	}
	++BulletsFired;

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

float UYSCoupDPetoirAbility::GetTimeToNextShot(int32 BulletIndex, int32 BulletCount) const
{
	if (BulletCount <= 1)
	{
		return 0.0f;
	}

	// en akimbo (Auto shot), les balles alternent : la suivante de la même arme est 2 balles plus loin
	const float Interval = BurstDuration / (BulletCount - 1);
	const UYSCoupDPetoirAbility* Shooter = GetBulletShooter(BulletIndex);
	for (int32 NextIndex = BulletIndex + 1; NextIndex < BulletCount; ++NextIndex)
	{
		if (GetBulletShooter(NextIndex) == Shooter)
		{
			return (NextIndex - BulletIndex) * Interval;
		}
	}
	return 0.0f;
}

float UYSCoupDPetoirAbility::ComputeRecoilTimeScale(float TimeToNextShot) const
{
	const float RecoilDuration = RecoilKickDuration + RecoilRecoverDuration;
	if (!bFitRecoilToFireRate || TimeToNextShot <= 0.0f || TimeToNextShot >= RecoilDuration)
	{
		return 1.0f;
	}
	return FMath::Max(TimeToNextShot / RecoilDuration, 0.01f);
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

void UYSCoupDPetoirAbility::FireBullet(const FVector& TargetLocation, int32 BulletIndex, float TimeToNextShot)
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
		// Atk critique : les balles d'attaque auto PewPew comptent sur chaque NME, une sur N est critique
		const AYSPlayerCharacter* HolderCharacter = Cast<AYSPlayerCharacter>(Shooter);
		if (IsAutomatic() && ShootSystem == EYSShootSystem::PewPew && HolderCharacter)
		{
			Projectile->SetCriticalBulletInterval(HolderCharacter->GetCriticalBulletInterval());
		}
		// upgrades d'arme (Ricochet) : seulement pour l'Ability, pas pour l'Auto shot
		if (!IsAutomatic())
		{
			Projectile->SetRicochets(FMath::RoundToInt(GetWeaponStat(UYSRicochetUpgrade::RicochetCountStat, 0.0f)), 1.0f + GetWeaponStat(UYSRicochetUpgrade::RicochetDamageBonusStat, 0.0f));
		}
		Projectile->FinishSpawning(SpawnTransform);
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSGun] %s.%s balle %d à %.2f"), *Owner->GetName(), *GetName(), BulletIndex + 1, GetWorld()->GetTimeSeconds());
	MulticastBulletFired(BulletIndex, TimeToNextShot);
}

void UYSCoupDPetoirAbility::MulticastBulletFired_Implementation(int32 BulletIndex, float TimeToNextShot)
{
	// recul de chaque balle ; le tireur a déjà joué celui de la première par prédiction (Ability tirée dès l'activation)
	if (BulletIndex > 0 || IsAutomatic() || !PredictsFirstBullet() || !IsLocallyPredicted())
	{
		NotifyActivated();
		LastBulletTime = GetWorld()->GetTimeSeconds();
		RecoilTimeScale = ComputeRecoilTimeScale(TimeToNextShot);
		UE_LOG(LogTemp, Verbose, TEXT("[YSGun] %s balle %d : recul %.3f s (x%.2f, balle suivante de l'arme dans %.3f s)"), *GetOwner()->GetName(), BulletIndex + 1,
			(RecoilKickDuration + RecoilRecoverDuration) * RecoilTimeScale, RecoilTimeScale, TimeToNextShot);
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
	// recul : depuis la dernière balle (et depuis l'activation quand la première balle part tout de suite)
	const float SinceBullet = static_cast<float>(GetWorld()->GetTimeSeconds() - LastBulletTime);
	const float Elapsed = PredictsFirstBullet() ? FMath::Min(SinceBullet, GetTimeSinceActivation()) : SinceBullet;
	// rafale rapide : recul accéléré pour finir avant la balle suivante de cette arme
	const float KickDuration = RecoilKickDuration * RecoilTimeScale;
	const float RecoverDuration = RecoilRecoverDuration * RecoilTimeScale;
	if (Elapsed < 0.0f || Elapsed >= KickDuration + RecoverDuration)
	{
		return 0.0f;
	}

	// coup sec puis retour plus doux
	if (Elapsed < KickDuration)
	{
		return FMath::InterpEaseOut(0.0f, 1.0f, Elapsed / KickDuration, 2.0f);
	}
	return FMath::InterpEaseInOut(1.0f, 0.0f, (Elapsed - KickDuration) / RecoverDuration, 2.0f);
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

int32 UYSCoupDPetoirAbility::GetBulletsPerBurst() const
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	const float Bonus = Character ? Character->GetWeaponShootSystemBonus(Cast<AYSWeapon>(GetOwner()), GetActionSlot()) : 0.0f;
	return FMath::Max(1, BulletsPerBurst + FMath::RoundToInt(Bonus));
}
