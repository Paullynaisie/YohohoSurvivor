#include "Abilities/YSShoulderBumpAbility.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/YSFoeCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Player/YSPlayerCharacter.h"
#include "Core/YSDamageTypes.h"
#include "Weapons/YSWeapon.h"

UYSShoulderBumpAbility::UYSShoulderBumpAbility()
{
	DisplayName = NSLOCTEXT("YS", "ShoulderBump", "Shoulder bump");
	StaminaCost = 75.0f;
	// intangible (traverse les NME : dash fluide) mais pas invulnérable (sauf Diamond shoulder)
	bRollThroughFoes = true;
	ExtraInvulnerability = 0.0f;
	AimDuration = 0.0f;
	PrimaryComponentTick.bCanEverTick = true;
}

void UYSShoulderBumpAbility::BeginPlay()
{
	Super::BeginPlay();
	// les Blueprints gardent la valeur copiée de l'ancien Shoulder bump (faux) : forcé ici, sur chaque machine (prédiction du client)
	bRollThroughFoes = true;
}

void UYSShoulderBumpAbility::ExecuteAbility(const FVector& TargetLocation)
{
	HitFoes.Reset();
	DashRestarts = 0;
	Super::ExecuteAbility(TargetLocation);
	UpdateHitbox();
}

bool UYSShoulderBumpAbility::IsGrantingInvulnerability() const
{
	// Diamond shoulder : invulnérable pendant le dash
	const float Elapsed = GetTimeSinceRoll();
	return GetMoveActionGolden() == GoldenDiamond && Elapsed >= 0.0f && Elapsed < GetRollDuration();
}

void UYSShoulderBumpAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float Elapsed = GetTimeSinceRoll();
	if (GetOwner()->HasAuthority() && Elapsed >= 0.0f && Elapsed < GetRollDuration() + HitboxExtraDuration)
	{
		UpdateHitbox();
	}
}

void UYSShoulderBumpAbility::UpdateHitbox()
{
	APawn* Holder = GetHolderPawn();
	const FVector DashDirection = LastRollDirection.GetSafeNormal2D();
	if (!Holder || DashDirection.IsNearlyZero())
	{
		return;
	}

	const int32 Golden = GetMoveActionGolden();
	const FVector Center = Holder->GetActorLocation() + DashDirection * HitboxForwardOffset;
	// perpendiculaire au dash (à plat) : le NME part du côté où il se trouve
	const FVector Side = FVector(-DashDirection.Y, DashDirection.X, 0.0f);
	bool bRestartDash = false;
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
		// Buldozer : chaque NME touché relance le dash
		bRestartDash = bRestartDash || Golden == GoldenBuldozer;

		// direction et distance du bump : perpendiculaire (normal) ; Compacteur : vers un point devant le personnage en fin de dash
		FVector BumpDirection = FVector::DotProduct(Foe->GetActorLocation() - Holder->GetActorLocation(), Side) >= 0.0f ? Side : -Side;
		float Distance = BumpDistance;
		if (Golden == GoldenCompactor)
		{
			const FVector Landing = Holder->GetActorLocation() + DashDirection * (CompactorFrontDistance + EstimateRemainingRollDistance());
			const FVector ToLanding = (Landing - Foe->GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
			Distance = ToLanding.Size();
			BumpDirection = Distance > KINDA_SMALL_NUMBER ? ToLanding / Distance : DashDirection;
		}

		// Brutass : la hitbox inflige les dégâts de l'attaque auto (le coup bumpe, comme le Bumping Slash)
		const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(Holder);
		const AYSWeapon* Weapon = Character ? Character->GetMainHandWeapon() : nullptr;
		const UYSAbilityComponent* AutoAction = Weapon ? Weapon->GetAutoAction() : nullptr;
		if (Golden == GoldenBrutass && AutoAction && AutoAction->GetBaseDamage() > 0.0f)
		{
			const float BrutassDamage = AutoAction->GetBaseDamage() * AutoAction->GetDamageMultiplier();
			FYSBumpDamageEvent BumpEvent(BrutassDamage, BumpDirection, nullptr);
			BumpEvent.Distance = Distance;
			BumpEvent.ApexHeight = BumpApexHeight;
			BumpEvent.Duration = BumpDuration;
			BumpEvent.SpinTurns = BumpSpinTurns;
			BumpEvent.bBumpElites = bBumpElites;
			Foe->TakeDamage(BrutassDamage, BumpEvent, Holder->GetController(), Holder);
			UE_LOG(LogTemp, Verbose, TEXT("[YSShoulderBump] %s : Brutass, %s prend %.0f"), *Holder->GetName(), *Foe->GetName(), BrutassDamage);
			continue;
		}

		if (!Foe->CanBeBumped(bBumpElites))
		{
			UE_LOG(LogTemp, Verbose, TEXT("[YSShoulderBump] %s touche %s : pas bumpable"), *Holder->GetName(), *Foe->GetName());
			continue;
		}
		Foe->StartBump(BumpDirection, Distance, BumpApexHeight, BumpDuration, BumpSpinTurns);
		UE_LOG(LogTemp, Verbose, TEXT("[YSShoulderBump] %s bumpe %s de %.0f cm vers (%.2f, %.2f)"), *Holder->GetName(), *Foe->GetName(), Distance, BumpDirection.X, BumpDirection.Y);
	}

	if (bRestartDash && DashRestarts < BuldozerMaxRestarts && GetTimeSinceRoll() < GetRollDuration())
	{
		++DashRestarts;
		RestartRollTimer();
		UE_LOG(LogTemp, Verbose, TEXT("[YSShoulderBump] %s : Buldozer, dash relancé (%d)"), *Holder->GetName(), DashRestarts);
	}
}

bool UYSShoulderBumpAbility::GetBodyRoll(float& OutAngle, float& OutHeightOffset) const
{
	const float Elapsed = GetTimeSinceRoll();
	if (LeanAngle == 0.0f || Elapsed < 0.0f || Elapsed >= GetRollDuration())
	{
		return false;
	}
	OutAngle = LeanAngle * FMath::Sin(PI * Elapsed / GetRollDuration());
	OutHeightOffset = 0.0f;
	return true;
}

bool UYSShoulderBumpAbility::GetLockedAimDirection(FVector& OutDirection) const
{
	const float Elapsed = GetTimeSinceRoll();
	if (LastRollDirection.IsNearlyZero() || Elapsed < 0.0f || Elapsed >= GetRollDuration())
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
	// pas de roulade (corps penché), pas d'invulnérabilité ni de visée ; toujours intangible
	OutNames.Append({
		GET_MEMBER_NAME_CHECKED(UYSShoulderBumpAbility, bRollThroughFoes),
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
