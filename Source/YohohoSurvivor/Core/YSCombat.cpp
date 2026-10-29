#include "Core/YSCombat.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "LandscapeProxy.h"
#include "Map/YSOcean.h"

namespace YSCombat
{
	/** Recherche du sol : au-dessus et en dessous de la hauteur du personnage */
	constexpr float GroundSearchUp = 2000.0f;
	constexpr float GroundSearchDown = 3000.0f;

	/** Props traversés au plus pour trouver l'île sous un point */
	constexpr int32 MaxIgnoredActors = 8;

	bool FindIslandGround(const UWorld* World, const FVector& Point, float ReferenceZ, const AActor* IgnoredActor, FVector& OutGround)
	{
		if (!World)
		{
			return false;
		}
		// seul le Landscape (l'île) compte : un prop touché (ponton, bateau, palmier) est traversé
		FCollisionQueryParams Params(SCENE_QUERY_STAT(YSIslandGround), false, IgnoredActor);
		const FVector Start(Point.X, Point.Y, ReferenceZ + GroundSearchUp);
		const FVector End(Point.X, Point.Y, ReferenceZ - GroundSearchDown);
		for (int32 Attempt = 0; Attempt < MaxIgnoredActors; ++Attempt)
		{
			FHitResult Hit;
			if (!World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
			{
				return false;
			}
			if (Hit.GetActor() && Hit.GetActor()->IsA<ALandscapeProxy>())
			{
				// pas dans l'eau
				OutGround = Hit.ImpactPoint;
				return !AYSOcean::IsUnderwater(World, OutGround);
			}
			Params.AddIgnoredActor(Hit.GetActor());
		}
		return false;
	}

	FVector FindLandingPoint(const ACharacter* Character, const FVector& TargetLocation, float MaxDistance, float RetreatStep)
	{
		const UWorld* World = Character ? Character->GetWorld() : nullptr;
		if (!World)
		{
			return TargetLocation;
		}
		const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

		// curseur ramené à la portée max, puis on recule vers le personnage jusqu'à un sol de l'île hors de l'eau
		FVector Offset = TargetLocation - Feet;
		Offset.Z = 0.0f;
		const float Distance = FMath::Min(Offset.Size(), MaxDistance);
		const FVector Direction = Offset.GetSafeNormal();
		FVector Ground;
		for (float Step = Distance; Step > 0.0f; Step -= FMath::Max(RetreatStep, 10.0f))
		{
			if (FindIslandGround(World, Feet + Direction * Step, Feet.Z, Character, Ground))
			{
				return Ground;
			}
		}

		// aucun sol valide : sur place
		if (FindIslandGround(World, Feet, Feet.Z, Character, Ground))
		{
			return Ground;
		}
		FHitResult Hit;
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(YSLandingFeet), false, Character);
		return World->LineTraceSingleByObjectType(Hit, Feet + FVector(0.0f, 0.0f, 50.0f), Feet - FVector(0.0f, 0.0f, 500.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params)
			? Hit.ImpactPoint : Feet;
	}
}

namespace YSCombat
{
	void SetPassThroughCharacters(UPrimitiveComponent* Capsule, bool bPassThrough)
	{
		if (Capsule)
		{
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, bPassThrough ? ECR_Ignore : ECR_Block);
			Capsule->SetCollisionResponseToChannel(ECC_YSFoe, bPassThrough ? ECR_Ignore : ECR_Block);
		}
	}

	void ConfigureHurtbox(UPrimitiveComponent* Component)
	{
		if (!Component)
		{
			return;
		}

		Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Component->SetCollisionObjectType(ECC_Pawn);
		Component->SetCollisionResponseToAllChannels(ECR_Ignore);
		Component->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		Component->SetGenerateOverlapEvents(true);
	}

	void ConfigureHitbox(UPrimitiveComponent* Component)
	{
		if (!Component)
		{
			return;
		}

		Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Component->SetCollisionObjectType(ECC_WorldDynamic);
		Component->SetCollisionResponseToAllChannels(ECR_Ignore);
		Component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Component->SetGenerateOverlapEvents(true);
	}
}

namespace YSCombat
{
	float ComputePushbackForce(float Damage, float TargetMaxHealth, float MinRatio, float MaxRatio, float MinForce, float MaxForce)
	{
		if (Damage <= 0.0f || TargetMaxHealth <= 0.0f)
		{
			return 0.0f;
		}
		// part de la vie de la cible retirée par le coup : rien sous le premier palier, force max à partir du second
		const float Ratio = Damage / TargetMaxHealth;
		if (Ratio < MinRatio)
		{
			return 0.0f;
		}
		if (Ratio >= MaxRatio || MaxRatio <= MinRatio)
		{
			return MaxForce;
		}
		return FMath::Lerp(MinForce, MaxForce, (Ratio - MinRatio) / (MaxRatio - MinRatio));
	}

	FVector GetBellLocation(const FVector& Start, const FVector& Target, float Duration, float ApexHeight, float Time)
	{
		const float SafeDuration = FMath::Max(Duration, 0.05f);
		const float Clamped = FMath::Clamp(Time, 0.0f, SafeDuration);

		// montée sqrt(2 Up / g) + descente sqrt(2 Down / g) = Duration  =>  g = ((sqrt(2 Up) + sqrt(2 Down)) / Duration)²
		const float ApexZ = FMath::Max(Start.Z, Target.Z) + FMath::Max(ApexHeight, 1.0f);
		const float Rise = FMath::Sqrt(2.0f * (ApexZ - Start.Z)) + FMath::Sqrt(2.0f * (ApexZ - Target.Z));
		const float Gravity = FMath::Square(Rise / SafeDuration);
		const float VerticalSpeed = FMath::Sqrt(2.0f * Gravity * (ApexZ - Start.Z));

		FVector Location = FMath::Lerp(Start, Target, Clamped / SafeDuration);
		Location.Z = Start.Z + VerticalSpeed * Clamped - 0.5f * Gravity * Clamped * Clamped;
		return Location;
	}
}
