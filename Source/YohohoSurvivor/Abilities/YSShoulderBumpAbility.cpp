#include "Abilities/YSShoulderBumpAbility.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/YSFoeCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Player/YSPlayerCharacter.h"

UYSShoulderBumpAbility::UYSShoulderBumpAbility()
{
	DisplayName = NSLOCTEXT("YS", "ShoulderBump", "Shoulder bump");
	StaminaCost = 75.0f;
	// ni intangible ni invulnérable
	bRollThroughFoes = false;
	ExtraInvulnerability = 0.0f;
	AimDuration = 0.0f;
	PrimaryComponentTick.bCanEverTick = true;
}

void UYSShoulderBumpAbility::ExecuteAbility(const FVector& TargetLocation)
{
	HitFoes.Reset();
	Super::ExecuteAbility(TargetLocation);
	UpdateHitbox();
}

void UYSShoulderBumpAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float Elapsed = GetTimeSinceRoll();
	if (GetOwner()->HasAuthority() && Elapsed >= 0.0f && Elapsed < RollDuration + HitboxExtraDuration)
	{
		UpdateHitbox();
	}
}

void UYSShoulderBumpAbility::UpdateHitbox()
{
	const APawn* Holder = GetHolderPawn();
	const FVector DashDirection = LastRollDirection.GetSafeNormal2D();
	if (!Holder || DashDirection.IsNearlyZero())
	{
		return;
	}

	const FVector Center = Holder->GetActorLocation() + DashDirection * HitboxForwardOffset;
	// perpendiculaire au dash (à plat) : le NME part du côté où il se trouve
	const FVector Side = FVector(-DashDirection.Y, DashDirection.X, 0.0f);
	for (TActorIterator<AYSFoeCharacter> It(GetWorld()); It; ++It)
	{
		AYSFoeCharacter* Foe = *It;
		if (!IsValid(Foe) || Foe->IsDying() || Foe->IsBumped() || HitFoes.Contains(Foe))
		{
			continue;
		}
		const UCapsuleComponent* Capsule = Foe->GetCapsuleComponent();
		const FVector ToFoe = Foe->GetActorLocation() - Center;
		const float Reach = HitboxRadius + Capsule->GetScaledCapsuleRadius();
		if (ToFoe.SizeSquared2D() > FMath::Square(Reach) || FMath::Abs(ToFoe.Z) > HitboxHalfHeight + Capsule->GetScaledCapsuleHalfHeight())
		{
			continue;
		}

		HitFoes.Add(Foe);
		if (!Foe->CanBeBumped(bBumpElites))
		{
			UE_LOG(LogTemp, Verbose, TEXT("[YSShoulderBump] %s touche %s : pas bumpable"), *Holder->GetName(), *Foe->GetName());
			continue;
		}
		const FVector ToFoeFromBody = Foe->GetActorLocation() - Holder->GetActorLocation();
		const FVector BumpDirection = FVector::DotProduct(ToFoeFromBody, Side) >= 0.0f ? Side : -Side;
		Foe->StartBump(BumpDirection, BumpDistance, BumpApexHeight, BumpDuration, BumpSpinTurns);
		UE_LOG(LogTemp, Verbose, TEXT("[YSShoulderBump] %s bumpe %s de %.0f cm vers (%.2f, %.2f)"), *Holder->GetName(), *Foe->GetName(), BumpDistance, BumpDirection.X, BumpDirection.Y);
	}
}

bool UYSShoulderBumpAbility::GetBodyRoll(float& OutAngle, float& OutHeightOffset) const
{
	const float Elapsed = GetTimeSinceRoll();
	if (LeanAngle == 0.0f || Elapsed < 0.0f || Elapsed >= RollDuration)
	{
		return false;
	}
	OutAngle = LeanAngle * FMath::Sin(PI * Elapsed / RollDuration);
	OutHeightOffset = 0.0f;
	return true;
}

bool UYSShoulderBumpAbility::GetLockedAimDirection(FVector& OutDirection) const
{
	const float Elapsed = GetTimeSinceRoll();
	if (LastRollDirection.IsNearlyZero() || Elapsed < 0.0f || Elapsed >= RollDuration)
	{
		return false;
	}
	OutDirection = LastRollDirection.GetSafeNormal2D();
	return true;
}

#if WITH_EDITOR
void UYSShoulderBumpAbility::GetUnusedProperties(TArray<FName>& OutNames) const
{
	Super::GetUnusedProperties(OutNames);
	// pas de roulade (corps penché), pas d'invulnérabilité ni de visée
	OutNames.Append({
		GET_MEMBER_NAME_CHECKED(UYSShoulderBumpAbility, ExtraInvulnerability),
		GET_MEMBER_NAME_CHECKED(UYSShoulderBumpAbility, AimDuration),
		GET_MEMBER_NAME_CHECKED(UYSShoulderBumpAbility, AimMoveSpeedMultiplier),
		GET_MEMBER_NAME_CHECKED(UYSShoulderBumpAbility, RollAnimTurns),
		GET_MEMBER_NAME_CHECKED(UYSShoulderBumpAbility, RollAnimDuration),
		GET_MEMBER_NAME_CHECKED(UYSShoulderBumpAbility, RollAnimEaseExponent),
		GET_MEMBER_NAME_CHECKED(UYSShoulderBumpAbility, RollAnimTuckHeight),
	});
}
#endif
