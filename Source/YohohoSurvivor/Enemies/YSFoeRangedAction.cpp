#include "Enemies/YSFoeRangedAction.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Enemies/YSFoeBomb.h"
#include "Enemies/YSFoeCharacter.h"
#include "Interaction/YSInteractiveObject.h"
#include "Map/YSOcean.h"
#include "Weapons/YSProjectile.h"

UYSFoeRangedAction::UYSFoeRangedAction()
{
	PrimaryComponentTick.bCanEverTick = false;
}

float UYSFoeRangedAction::GetDamage(const AYSFoeCharacter* Foe) const
{
	return Foe ? (BaseDamage + DamagePerLevel * Foe->GetFoeLevel()) * Foe->GetDamageMultiplier() : BaseDamage;
}

void UYSFoeRangedAction::PlayAttackLocally()
{
	if (const UWorld* World = GetWorld())
	{
		LastAttackTime = World->GetTimeSeconds();
	}
}

float UYSFoeRangedAction::GetTimeSinceAttack() const
{
	const UWorld* World = GetWorld();
	return World ? static_cast<float>(World->GetTimeSeconds() - LastAttackTime) : TNumericLimits<float>::Max();
}

UYSFoeBombAction::UYSFoeBombAction()
{
	BombClass = AYSFoeBomb::StaticClass();
}

bool UYSFoeBombAction::StartAttack(AYSFoeCharacter* Foe, const APawn* Target, const FVector& Origin)
{
	UWorld* World = Foe ? Foe->GetWorld() : nullptr;
	if (!World || !BombClass || !Target || IsAnimating())
	{
		return false;
	}

	// point visé au hasard autour de la cible (répartition uniforme dans le disque), choisi au début du lancer
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Distance = TargetScatterRadius * FMath::Sqrt(FMath::FRand());
	const FVector Aim = Target->GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance;
	// cible qui nage : la bombe tombe à la surface de la mer, pas au fond
	FVector Point = AYSInteractiveObject::FindGround(World, Aim, Target);
	Point.Z = AYSOcean::ClampAboveSea(World, Point.Z);

	// animation de lancer sur toutes les machines ; la bombe part au lâcher
	Foe->NotifyRangedAttack(this);
	TWeakObjectPtr<AYSFoeCharacter> WeakFoe = Foe;
	World->GetTimerManager().SetTimer(ReleaseTimer, FTimerDelegate::CreateWeakLambda(this, [this, WeakFoe, Point]()
	{
		ReleaseBomb(WeakFoe, Point);
	}), FMath::Max(ThrowWindupDuration + ThrowDuration, KINDA_SMALL_NUMBER), false);
	UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s prépare une bombe sur %s (point à %.0f cm de la cible)"), *Foe->GetName(), *Target->GetName(), Distance);
	return true;
}

void UYSFoeBombAction::ReleaseBomb(TWeakObjectPtr<AYSFoeCharacter> Foe, FVector Target)
{
	AYSFoeCharacter* Thrower = Foe.Get();
	UWorld* World = Thrower ? Thrower->GetWorld() : nullptr;
	if (!World || !IsValid(Thrower) || Thrower->GetHealth() <= 0.0f)
	{
		return;
	}

	FYSBombThrow Throw;
	Throw.Start = Thrower->GetAttackOrigin(this);
	Throw.Target = Target;
	Throw.ApexHeight = BombApexHeight;
	Throw.FlightDuration = BombFlightDuration;
	Throw.FuseDuration = BombFuseDuration;
	Throw.BombColor = BombColor;
	Throw.FuseColor = FuseColor;
	Throw.FuseGlow = FuseGlow;
	Throw.BombMaterial = BombMaterial;
	Throw.Damage = GetDamage(Thrower);
	Throw.ExplosionRadius = ExplosionRadius;
	Throw.ExplosionGrowDuration = ExplosionGrowDuration;
	Throw.ExplosionFadeDuration = ExplosionFadeDuration;
	Throw.ExplosionColor = ExplosionColor;
	Throw.LandingZoneColor = PreviewColor;
	Throw.LandingZoneFillOpacity = PreviewFillOpacity;
	Throw.LandingZoneEdgeOpacity = PreviewEdgeOpacity;
	Throw.LandingZoneMaterial = PreviewMaterial;
	Throw.ExplosionMaterial = ExplosionMaterial;
	// visuel de la bombe lancée = celui de l'arme (copie locale sur chaque machine)
	if (const AActor* Weapon = GetOwner())
	{
		Throw.VisualClass = Weapon->GetClass();
		Throw.VisualScale = Weapon->GetActorScale3D();
	}

	const FTransform SpawnTransform(Throw.Start);
	AYSFoeBomb* Bomb = World->SpawnActorDeferred<AYSFoeBomb>(BombClass, SpawnTransform, Thrower, Thrower, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Bomb)
	{
		return;
	}
	Bomb->SetThrow(Throw);
	Bomb->FinishSpawning(SpawnTransform);
	UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s lance une bombe (%.0f cm, %.0f dégâts)"), *Thrower->GetName(), FVector::Dist2D(Throw.Start, Throw.Target), Throw.Damage);
}

bool UYSFoeBombAction::EvaluateHandPose(float& OutIdleWeight, FVector& OutLocation, FQuat& OutRotation) const
{
	const float Elapsed = GetTimeSinceAttack();
	if (Elapsed < 0.0f || Elapsed >= ThrowWindupDuration + ThrowDuration + ThrowRecoverDuration)
	{
		return false;
	}

	if (Elapsed < ThrowWindupDuration)
	{
		// préparation : de la pose normale vers l'arrière de l'épaule
		const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, Elapsed / ThrowWindupDuration, 2.0f);
		OutIdleWeight = 1.0f - Alpha;
		OutLocation = WindupHandLocation;
		OutRotation = WindupHandRotation.Quaternion();
	}
	else if (Elapsed < ThrowWindupDuration + ThrowDuration)
	{
		// lancer : vers l'avant, rapide
		const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, (Elapsed - ThrowWindupDuration) / ThrowDuration, ThrowEaseExponent);
		OutIdleWeight = 0.0f;
		OutLocation = FMath::Lerp(WindupHandLocation, ReleaseHandLocation, Alpha);
		OutRotation = FQuat::Slerp(WindupHandRotation.Quaternion(), ReleaseHandRotation.Quaternion(), Alpha);
	}
	else
	{
		// retour à la pose normale
		OutIdleWeight = FMath::InterpEaseInOut(0.0f, 1.0f, (Elapsed - ThrowWindupDuration - ThrowDuration) / ThrowRecoverDuration, 2.0f);
		OutLocation = ReleaseHandLocation;
		OutRotation = ReleaseHandRotation.Quaternion();
	}
	return true;
}

bool UYSFoeBombAction::IsWeaponHidden() const
{
	// partie au lâcher, la bombe revient dans la main BombReappearDelay plus tard
	const float SinceRelease = GetTimeSinceAttack() - (ThrowWindupDuration + ThrowDuration);
	return SinceRelease >= 0.0f && SinceRelease < BombReappearDelay;
}

bool UYSFoeShotAction::StartAttack(AYSFoeCharacter* Foe, const APawn* Target, const FVector& Origin)
{
	// imprécision : direction décalée au hasard, ligne de visée comprise
	return Foe && Target && Foe->StartAim(Target, AimDuration, FMath::FRandRange(-AimSpread, AimSpread));
}

void UYSFoeShotAction::Fire(AYSFoeCharacter* Foe, const FVector& Origin, const FVector& Direction) const
{
	UWorld* World = Foe ? Foe->GetWorld() : nullptr;
	if (!World || !ProjectileClass)
	{
		return;
	}

	const FTransform SpawnTransform(Direction.Rotation(), Origin);
	AYSProjectile* Projectile = World->SpawnActorDeferred<AYSProjectile>(ProjectileClass, SpawnTransform, Foe, Foe, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Projectile)
	{
		Projectile->SetDamage(GetDamage(Foe));
		Projectile->FinishSpawning(SpawnTransform);
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s tire (%.0f dégâts) depuis (%.0f, %.0f, %.0f)"), *Foe->GetName(), GetDamage(Foe), Origin.X, Origin.Y, Origin.Z);
}

bool UYSFoeShotAction::GetRecoil(float& OutPitch, float& OutKickBack) const
{
	// même courbe que le pistolet du joueur : coup sec (EaseOut) puis retour plus doux (EaseInOut)
	const float Elapsed = GetTimeSinceAttack();
	if (Elapsed < 0.0f || Elapsed >= RecoilKickDuration + RecoilRecoverDuration)
	{
		return false;
	}
	const float Alpha = Elapsed < RecoilKickDuration
		? FMath::InterpEaseOut(0.0f, 1.0f, Elapsed / RecoilKickDuration, 2.0f)
		: FMath::InterpEaseInOut(1.0f, 0.0f, (Elapsed - RecoilKickDuration) / RecoilRecoverDuration, 2.0f);
	OutPitch = RecoilPitch * Alpha;
	OutKickBack = RecoilKickBack * Alpha;
	return true;
}
