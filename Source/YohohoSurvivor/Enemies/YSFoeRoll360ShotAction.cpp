#include "Enemies/YSFoeRoll360ShotAction.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Enemies/YSFoeCharacter.h"
#include "Enemies/YSFoeRangedAction.h"
#include "Map/YSOcean.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/YSProjectile.h"

namespace YSRoll360Shot
{
	double GetServerTime(const UWorld* World)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
	}
}

UYSFoeRoll360ShotAction::UYSFoeRoll360ShotAction()
{
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultPreview(TEXT("/Game/Art/Materials/M_BrookerZone.M_BrookerZone"));
	if (DefaultPreview.Succeeded())
	{
		PreviewMaterial = DefaultPreview.Object;
	}
}

bool UYSFoeRoll360ShotAction::IsReady() const
{
	return YSRoll360Shot::GetServerTime(GetWorld()) >= ReadyTime;
}

void UYSFoeRoll360ShotAction::NotifyStarted()
{
	ReadyTime = YSRoll360Shot::GetServerTime(GetWorld()) + Cooldown;
}

UYSFoeShotAction* UYSFoeRoll360ShotAction::GetShotAction() const
{
	const AActor* Weapon = GetOwner();
	return Weapon ? Weapon->FindComponentByClass<UYSFoeShotAction>() : nullptr;
}

TSubclassOf<AYSProjectile> UYSFoeRoll360ShotAction::GetProjectileClass() const
{
	if (ProjectileClass)
	{
		return ProjectileClass;
	}
	const UYSFoeShotAction* Shot = GetShotAction();
	return Shot ? Shot->ProjectileClass : nullptr;
}

float UYSFoeRoll360ShotAction::GetPreviewWidth() const
{
	const TSubclassOf<AYSProjectile> Class = GetProjectileClass();
	const float Radius = Class ? Class->GetDefaultObject<AYSProjectile>()->GetCollisionRadius() : 45.0f;
	return FMath::Max(1.0f, 2.0f * Radius * BulletScale * PreviewWidthMultiplier);
}

float UYSFoeRoll360ShotAction::GetBulletDamage(const AYSFoeCharacter* Foe) const
{
	const UYSFoeShotAction* Shot = GetShotAction();
	return (Shot ? Shot->GetDamage(Foe) : 0.0f) * DamageMultiplier;
}

float UYSFoeRoll360ShotAction::GetRollFraction(float Alpha) const
{
	return FMath::InterpEaseOut(0.0f, 1.0f, FMath::Clamp(Alpha, 0.0f, 1.0f), RollEaseExponent);
}

FVector UYSFoeRoll360ShotAction::FindRollEnd(const FVector& Start, const FVector& Direction, float HalfHeight, const AActor* IgnoredActor) const
{
	const UWorld* World = GetWorld();
	const FVector FlatDirection = Direction.GetSafeNormal2D();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSRoll360ShotEnd), false, IgnoredActor);
	for (float Distance = RollDistance; Distance > 0.0f; Distance -= WaterRetreatStep)
	{
		const FVector Point = Start + FlatDirection * Distance;
		FHitResult Hit;
		if (World && World->LineTraceSingleByObjectType(Hit, Point + FVector(0.0f, 0.0f, 1000.0f), Point - FVector(0.0f, 0.0f, 5000.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params)
			&& !AYSOcean::IsUnderwater(World, Hit.ImpactPoint))
		{
			return FVector(Point.X, Point.Y, Hit.ImpactPoint.Z + HalfHeight);
		}
	}
	// aucun sol hors de l'eau dans cette direction : il roule sur place
	return Start;
}

void UYSFoeRoll360ShotAction::FireBullet(AYSFoeCharacter* Foe, const FVector& Origin, const FVector& Direction) const
{
	UWorld* World = Foe ? Foe->GetWorld() : nullptr;
	const TSubclassOf<AYSProjectile> Class = GetProjectileClass();
	if (!World || !Class)
	{
		return;
	}
	const FTransform SpawnTransform(Direction.GetSafeNormal2D().Rotation(), Origin, FVector(BulletScale));
	AYSProjectile* Projectile = World->SpawnActorDeferred<AYSProjectile>(Class, SpawnTransform, Foe, Foe, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Projectile)
	{
		Projectile->SetDamage(GetBulletDamage(Foe));
		Projectile->MultiplyLifeTime(Foe->GetAmmoLifeTimeMultiplier());
		Projectile->FinishSpawning(SpawnTransform);
	}
}
