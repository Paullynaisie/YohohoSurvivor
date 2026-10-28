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
	// PewPew / Sniper : rafale de BulletsPerBurst balles réparties sur BurstDuration ; Shotgun : toutes les AMMO d'un coup
	GetWorld()->GetTimerManager().ClearTimer(BurstTimer);
	BurstTargetLocation = TargetLocation;
	BulletsFired = 0;
	BurstShooters.Reset();
	BurstBulletCount = GetBulletsPerBurst();
	if (bSpreadFire)
	{
		FireShotgun(TargetLocation);
		return;
	}
	FireNextBullet();
}

void UYSCoupDPetoirAbility::FireShotgun(const FVector& TargetLocation)
{
	FVector Muzzle;
	FVector Direction;
	if (!ComputeShot(TargetLocation, Muzzle, Direction))
	{
		return;
	}

	// Gros sel (attaque auto) : les AMMO de ce tir partagent le compte des touches par cible
	CurrentVolley.Reset();
	if (IsAutomatic() && GetWeaponStat(UYSGrosSelUpgrade::GrosSelStat, 0.0f) > 0.0f)
	{
		CurrentVolley = MakeShared<FYSShotVolley>();
		CurrentVolley->DamageBonusPerHit = GetWeaponStat(UYSGrosSelUpgrade::GrosSelStat, 0.0f);
		CurrentVolley->CriticalHitCount = FMath::RoundToInt(GetWeaponStat(UYSGrosSelUpgrade::GrosSelCriticalStat, 0.0f));
	}

	// éventail régulier : de -Spread à +Spread, à angles égaux (une seule AMMO : droit vers la cible)
	const float BaseYaw = Direction.Rotation().Yaw;
	for (int32 Index = 0; Index < BurstBulletCount; ++Index)
	{
		const float Offset = BurstBulletCount > 1 ? FMath::Lerp(-ShotgunSpread, ShotgunSpread, static_cast<float>(Index) / (BurstBulletCount - 1)) : 0.0f;
		SpawnProjectile(Muzzle, FRotator(0.0f, BaseYaw + Offset, 0.0f).Vector());
	}
	CurrentVolley.Reset();
	UE_LOG(LogTemp, Verbose, TEXT("[YSGun] %s.%s éventail : %d AMMO (+/- %.0f°) à %.2f"), *GetOwner()->GetName(), *GetName(),
		BurstBulletCount, ShotgunSpread, GetWorld()->GetTimeSeconds());

	// recul sur toutes les machines, puis l'animation de rechargement
	MulticastBulletFired(0, 0.0f, 0.0f);
	BurstShooters.AddUnique(this);
	PlayReloadAnims();
}

void UYSCoupDPetoirAbility::FireNextBullet()
{
	const int32 BulletCount = FMath::Max(1, BurstBulletCount);
	if (UYSCoupDPetoirAbility* Shooter = GetBulletShooter(BulletsFired))
	{
		const float BurstRemaining = BulletCount > 1 ? BurstDuration * (BulletCount - 1 - BulletsFired) / (BulletCount - 1) : 0.0f;
		Shooter->FireBullet(GetBurstTargetLocation(BulletsFired), BulletsFired, GetTimeToNextShot(BulletsFired, BulletCount), BurstRemaining);
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

	PlayReloadAnims();
}

void UYSCoupDPetoirAbility::PlayReloadAnims()
{
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
	if (BulletCount <= 1 || bSpreadFire)
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
	BurstEndTime = -1.0e6;
}

bool UYSCoupDPetoirAbility::IsActionInProgress() const
{
	const UWorld* World = GetWorld();
	return Super::IsActionInProgress() || (World && World->GetTimerManager().IsTimerActive(BurstTimer));
}

bool UYSCoupDPetoirAbility::IsBlockingLowerPriority() const
{
	const UWorld* World = GetWorld();
	return Super::IsBlockingLowerPriority() || (World && World->GetTimerManager().IsTimerActive(BurstTimer));
}

bool UYSCoupDPetoirAbility::ComputeShot(const FVector& TargetLocation, FVector& OutMuzzle, FVector& OutDirection) const
{
	const AActor* Owner = GetOwner();
	if (!ProjectileClass || !Owner)
	{
		return false;
	}

	// l'action vit sur une arme : on tire depuis sa bouche, au nom du pawn qui la tient
	const AYSWeapon* Weapon = Cast<AYSWeapon>(Owner);
	const APawn* Shooter = Weapon ? Weapon->GetHolder() : Cast<APawn>(Owner);
	OutMuzzle = Weapon ? Weapon->GetMuzzleLocation() : Owner->GetActorLocation();
	const AActor* Aimer = Shooter ? static_cast<const AActor*>(Shooter) : Owner;

	// visée assistée : un NME proche de la direction visée remplace le point visé
	FVector AimPoint = TargetLocation;
	if (const AActor* AssistTarget = FindAssistTarget(Aimer->GetActorLocation(), TargetLocation - Aimer->GetActorLocation()))
	{
		AimPoint = AssistTarget->GetActorLocation();
	}

	// trajectoire parallèle au sol
	OutDirection = AimPoint - OutMuzzle;
	OutDirection.Z = 0.0f;
	if (OutDirection.SizeSquared() < FMath::Square(YSGun::MinAimDistance))
	{
		OutDirection = Aimer->GetActorForwardVector();
		OutDirection.Z = 0.0f;
	}
	OutDirection = OutDirection.GetSafeNormal();
	return true;
}

void UYSCoupDPetoirAbility::SpawnProjectile(const FVector& Muzzle, const FVector& Direction)
{
	const AYSWeapon* Weapon = Cast<AYSWeapon>(GetOwner());
	APawn* Shooter = Weapon ? Weapon->GetHolder() : Cast<APawn>(GetOwner());
	const FTransform SpawnTransform(Direction.Rotation(), Muzzle);
	AYSProjectile* Projectile = GetWorld()->SpawnActorDeferred<AYSProjectile>(ProjectileClass, SpawnTransform, Shooter, Shooter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		return;
	}

	const float DamageMultiplier = GetDamageMultiplier();
	Projectile->SetDamage(Damage * DamageMultiplier);
	// critiques des attaques auto : PewPew / Explosif = les balles comptent sur chaque NME, une sur N est critique ;
	// Pierce = critique sur un NME en train d'être bumpé
	const AYSPlayerCharacter* HolderCharacter = Cast<AYSPlayerCharacter>(Shooter);
	if (IsAutomatic() && HolderCharacter)
	{
		switch (ShootSystem)
		{
		case EYSShootSystem::Pierce:	Projectile->SetCriticalOnBumped(true); break;
		default:						Projectile->SetCriticalBulletInterval(HolderCharacter->GetCriticalBulletInterval()); break;
		}
	}
	// Pierce : la balle traverse les NME
	Projectile->SetMaxHits(GetMaxHitsPerBullet());
	// Aiming (Harquebusier immobile assez longtemps) : toutes ses balles critiques ; Golden aiming : elles traversent tous les NME
	if (HolderCharacter && HolderCharacter->IsAimingCritical())
	{
		Projectile->SetAlwaysCritical(true);
		if (HolderCharacter->HasGoldenAiming())
		{
			Projectile->SetPierceAllTargets(true);
		}
	}
	// Gros sel : AMMO de l'éventail en cours
	if (CurrentVolley.IsValid())
	{
		Projectile->SetVolley(CurrentVolley);
	}
	// Explosif : explosion à l'impact (dégâts et rayon : Damage bonus, Shoot system) ; MineShooter : mine collée au NME
	if (ShootSystem == EYSShootSystem::Explosive && (ExplosionClass || StickyMineClass))
	{
		FYSProjectileExplosion ProjectileExplosion;
		ProjectileExplosion.ExplosionClass = ExplosionClass;
		ProjectileExplosion.Damage = ExplosionDamage * DamageMultiplier;
		ProjectileExplosion.RadiusMultiplier = GetExplosiveRadiusMultiplier();
		if (IsAutomatic() && GetWeaponStat(UYSMineShooterUpgrade::MineShooterStat, 0.0f) > 0.0f)
		{
			ProjectileExplosion.StickyMineClass = StickyMineClass;
			ProjectileExplosion.StickyFuse = StickyMineFuse;
			ProjectileExplosion.StickyBonusPerMine = GetWeaponStat(UYSMineShooterUpgrade::MineShooterBonusStat, 0.0f);
			ProjectileExplosion.RadiusMultiplier *= GetWeaponStat(UYSMineShooterUpgrade::MineShooterRadiusStat, 1.0f);
		}
		Projectile->SetExplosion(ProjectileExplosion);
	}
	// upgrades d'arme (Ricochet) : seulement pour l'Ability, pas pour l'Auto shot
	if (!IsAutomatic())
	{
		Projectile->SetRicochets(FMath::RoundToInt(GetWeaponStat(UYSRicochetUpgrade::RicochetCountStat, 0.0f)), 1.0f + GetWeaponStat(UYSRicochetUpgrade::RicochetDamageBonusStat, 0.0f));
	}
	ConfigureProjectile(Projectile);
	Projectile->FinishSpawning(SpawnTransform);
}

void UYSCoupDPetoirAbility::FireBullet(const FVector& TargetLocation, int32 BulletIndex, float TimeToNextShot, float BurstRemaining)
{
	FVector Muzzle;
	FVector Direction;
	if (!ComputeShot(TargetLocation, Muzzle, Direction))
	{
		return;
	}

	SpawnProjectile(Muzzle, Direction);
	UE_LOG(LogTemp, Verbose, TEXT("[YSGun] %s.%s balle %d à %.2f"), *GetOwner()->GetName(), *GetName(), BulletIndex + 1, GetWorld()->GetTimeSeconds());
	MulticastBulletFired(BulletIndex, TimeToNextShot, BurstRemaining);
}

bool UYSCoupDPetoirAbility::IsBurstInProgressLocally() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < BurstEndTime;
}

void UYSCoupDPetoirAbility::MulticastBulletFired_Implementation(int32 BulletIndex, float TimeToNextShot, float BurstRemaining)
{
	// fin de la rafale (ralentissement de l'attaque auto, toutes les machines)
	BurstEndTime = GetWorld()->GetTimeSeconds() + BurstRemaining;

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
		return FMath::InterpEaseOut(0.0f, 1.0f, Elapsed / KickDuration, RecoilKickEaseExponent);
	}
	return FMath::InterpEaseInOut(1.0f, 0.0f, (Elapsed - KickDuration) / RecoverDuration, RecoilRecoverEaseExponent);
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
	// éventail : Spread Ammo Count + Gros sel ++ (attaque auto)
	if (bSpreadFire)
	{
		const int32 ExtraAmmo = IsAutomatic() ? FMath::RoundToInt(GetWeaponStat(UYSGrosSelUpgrade::GrosSelAmmoStat, 0.0f)) : 0;
		return FMath::Max(1, ShotgunAmmoPerShot + ExtraAmmo);
	}
	// rafale : le Shoot system n'ajoute des balles qu'en PewPew (Pierce : NME traversés ; Explosif : rayon)
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	const float Bonus = Character && ShootSystem == EYSShootSystem::PewPew ? Character->GetWeaponShootSystemBonus(Cast<AYSWeapon>(GetOwner()), GetActionSlot()) : 0.0f;
	return FMath::Max(1, BulletsPerBurst + FMath::RoundToInt(Bonus));
}

int32 UYSCoupDPetoirAbility::GetMaxHitsPerBullet() const
{
	if (ShootSystem != EYSShootSystem::Pierce)
	{
		return 1;
	}
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	const float Bonus = Character ? Character->GetWeaponShootSystemBonus(Cast<AYSWeapon>(GetOwner()), GetActionSlot()) : 0.0f;
	return FMath::Max(1, PierceMaxHits + FMath::RoundToInt(Bonus));
}

bool UYSCoupDPetoirAbility::GetShootSystem(EYSShootSystem& OutSystem) const
{
	if (WeaponType != EYSWeaponType::Gun || !IsAutomatic())
	{
		return false;
	}
	OutSystem = ShootSystem;
	return true;
}
