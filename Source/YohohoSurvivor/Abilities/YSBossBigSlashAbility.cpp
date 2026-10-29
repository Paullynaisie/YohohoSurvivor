#include "Abilities/YSBossBigSlashAbility.h"
#include "Core/YSGameplayTags.h"
#include "Engine/World.h"
#include "Map/YSOcean.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace YSBossBigSlash
{
	/** Pas d'intégration de la vitesse du dash (part de la distance parcourue) */
	constexpr int32 DistanceSamples = 24;
}

UYSBossBigSlashAbility::UYSBossBigSlashAbility()
{
	DisplayName = NSLOCTEXT("YS", "BossBigSlash", "Boss Big Slash");
	Cooldown = 10.0f;
	// préparation de 2 s (pendant le « pas en arrière » du Dashing Slash, ici immobile)
	StepBackDuration = 2.0f;
	HitTags.Reset();
	HitTags.AddTag(YSTags::Team_Player);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultWarning(TEXT("/Game/Art/Materials/M_BrookerZone.M_BrookerZone"));
	if (DefaultWarning.Succeeded())
	{
		WarningMaterial = DefaultWarning.Object;
	}
}

float UYSBossBigSlashAbility::GetDashDistanceFraction(float Alpha) const
{
	Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	auto Speed = [this](float T)
	{
		return FMath::Lerp(DashSpeedMultiplier, DashEndSpeedMultiplier, FMath::InterpEaseOut(0.0f, 1.0f, T, DashDecayExponent));
	};
	// intégrale de la vitesse (points milieux) jusqu'à Alpha, rapportée à l'intégrale sur tout le dash
	float Done = 0.0f;
	float Total = 0.0f;
	const float Step = 1.0f / YSBossBigSlash::DistanceSamples;
	for (int32 Index = 0; Index < YSBossBigSlash::DistanceSamples; ++Index)
	{
		const float From = Index * Step;
		const float Value = Speed(From + Step * 0.5f) * Step;
		Total += Value;
		if (From < Alpha)
		{
			Done += Value * FMath::Min(1.0f, (Alpha - From) / Step);
		}
	}
	return Total > KINDA_SMALL_NUMBER ? Done / Total : Alpha;
}

FVector UYSBossBigSlashAbility::FindDashEnd(const FVector& Start, const FVector& Direction, float HalfHeight, const AActor* IgnoredActor) const
{
	const UWorld* World = GetWorld();
	const FVector FlatDirection = Direction.GetSafeNormal2D();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSBossBigSlashEnd), false, IgnoredActor);
	for (float Distance = DashDistance; Distance > 0.0f; Distance -= WaterRetreatStep)
	{
		const FVector Point = Start + FlatDirection * Distance;
		FHitResult Hit;
		if (World && World->LineTraceSingleByObjectType(Hit, Point + FVector(0.0f, 0.0f, 1000.0f), Point - FVector(0.0f, 0.0f, 5000.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params)
			&& !AYSOcean::IsUnderwater(World, Hit.ImpactPoint))
		{
			return FVector(Point.X, Point.Y, Hit.ImpactPoint.Z + HalfHeight);
		}
	}
	// aucun sol hors de l'eau devant : il reste sur place
	return Start;
}

#if WITH_EDITOR
void UYSBossBigSlashAbility::GetUnusedProperties(TArray<FName>& OutNames) const
{
	Super::GetUnusedProperties(OutNames);
	// NME : préparation immobile, pas d'invulnérabilité ni de traversée (position calculée par le NME), dégâts = contact x multiplicateur
	OutNames.Append({
		GET_MEMBER_NAME_CHECKED(UYSBossBigSlashAbility, StepBackSpeedMultiplier),
		GET_MEMBER_NAME_CHECKED(UYSBossBigSlashAbility, StepBackEndSpeedMultiplier),
		GET_MEMBER_NAME_CHECKED(UYSBossBigSlashAbility, StepBackDecayExponent),
		GET_MEMBER_NAME_CHECKED(UYSBossBigSlashAbility, bDashThroughFoes),
		GET_MEMBER_NAME_CHECKED(UYSBossBigSlashAbility, bDashInvulnerable),
		GET_MEMBER_NAME_CHECKED(UYSBossBigSlashAbility, ExtraInvulnerability),
		GET_MEMBER_NAME_CHECKED(UYSBossBigSlashAbility, Damage),
		GET_MEMBER_NAME_CHECKED(UYSBossBigSlashAbility, bResetAutoAttackOnUse),
		GET_MEMBER_NAME_CHECKED(UYSBossBigSlashAbility, AutoAttackResetDelay),
	});
}
#endif
