#include "Enemies/YSFoeRangedAction.h"
#include "Engine/World.h"
#include "Enemies/YSFoeBomb.h"
#include "Enemies/YSFoeCharacter.h"
#include "Interaction/YSInteractiveObject.h"
#include "Weapons/YSProjectile.h"

UYSFoeRangedAction::UYSFoeRangedAction()
{
	PrimaryComponentTick.bCanEverTick = false;
}

float UYSFoeRangedAction::GetDamage(const AYSFoeCharacter* Foe) const
{
	return Foe ? (BaseDamage + DamagePerLevel * Foe->GetFoeLevel()) * Foe->GetDamageMultiplier() : BaseDamage;
}

UYSFoeBombAction::UYSFoeBombAction()
{
	BombClass = AYSFoeBomb::StaticClass();
}

bool UYSFoeBombAction::StartAttack(AYSFoeCharacter* Foe, const APawn* Target, const FVector& Origin)
{
	UWorld* World = Foe ? Foe->GetWorld() : nullptr;
	if (!World || !BombClass || !Target)
	{
		return false;
	}

	// vise le sol sous la cible au moment du lancer
	FYSBombThrow Throw;
	Throw.Start = Origin;
	Throw.Target = AYSInteractiveObject::FindGround(World, Target->GetActorLocation(), Target);
	Throw.ApexHeight = BombApexHeight;
	Throw.FlightDuration = BombFlightDuration;
	Throw.FuseDuration = BombFuseDuration;
	Throw.BombColor = BombColor;
	Throw.FuseColor = FuseColor;
	Throw.FuseGlow = FuseGlow;
	Throw.BombMaterial = BombMaterial;
	Throw.Damage = GetDamage(Foe);
	Throw.ExplosionRadius = ExplosionRadius;
	Throw.ExplosionGrowDuration = ExplosionGrowDuration;
	Throw.ExplosionFadeDuration = ExplosionFadeDuration;
	Throw.ExplosionColor = ExplosionColor;
	Throw.LandingZoneColor = PreviewColor;
	Throw.LandingZoneFillOpacity = PreviewFillOpacity;
	Throw.LandingZoneEdgeOpacity = PreviewEdgeOpacity;
	Throw.LandingZoneMaterial = PreviewMaterial;
	Throw.ExplosionMaterial = ExplosionMaterial;

	const FTransform SpawnTransform(Throw.Start);
	AYSFoeBomb* Bomb = World->SpawnActorDeferred<AYSFoeBomb>(BombClass, SpawnTransform, Foe, Foe, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Bomb)
	{
		return false;
	}
	Bomb->SetThrow(Throw);
	Bomb->FinishSpawning(SpawnTransform);
	UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s lance une bombe sur %s (%.0f cm, %.0f dégâts)"), *Foe->GetName(), *Target->GetName(), FVector::Dist2D(Throw.Start, Throw.Target), Throw.Damage);
	return true;
}

bool UYSFoeShotAction::StartAttack(AYSFoeCharacter* Foe, const APawn* Target, const FVector& Origin)
{
	return Foe && Target && Foe->StartAim(Target, AimDuration);
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
