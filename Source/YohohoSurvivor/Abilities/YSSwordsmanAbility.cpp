#include "Abilities/YSSwordsmanAbility.h"
#include "Map/YSGroundZoneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "Upgrades/YSUpgrade.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/YSLesserSpinningBlade.h"
#include "Weapons/YSSpinningSword.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UYSSwordsmanAbility::UYSSwordsmanAbility()
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultPreview(TEXT("/Game/Art/Materials/M_BrookerZone.M_BrookerZone"));
	if (DefaultPreview.Succeeded())
	{
		PreviewMaterial = DefaultPreview.Object;
	}
}

void UYSSwordsmanAbility::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UYSSwordsmanAbility, Mode);
}

bool UYSSwordsmanAbility::IsInThrowStepBack(float& OutAlpha) const
{
	OutAlpha = 0.0f;
	const float Elapsed = GetTimeSinceActivation();
	const float Duration = GetThrowStepBackDuration();
	if (Mode != EYSSwordsmanMode::BladeThrow || !IsMovementDriver() || Elapsed < 0.0f || Elapsed >= Duration || Duration <= 0.0f)
	{
		return false;
	}
	OutAlpha = Elapsed / Duration;
	return true;
}

float UYSSwordsmanAbility::GetCooldownDuration() const
{
	switch (Mode)
	{
	case EYSSwordsmanMode::BladeThrow:		return GetThrowStepBackDuration() + BladeThrowCooldown * GetCooldownMultiplier();
	case EYSSwordsmanMode::BladeMastery:	return BladeMasteryCooldown * GetCooldownMultiplier();
	default:								return Super::GetCooldownDuration();
	}
}

float UYSSwordsmanAbility::GetBaseCooldown() const
{
	switch (Mode)
	{
	case EYSSwordsmanMode::BladeThrow:		return BladeThrowCooldown;
	case EYSSwordsmanMode::BladeMastery:	return BladeMasteryCooldown;
	default:								return Super::GetBaseCooldown();
	}
}

float UYSSwordsmanAbility::GetActionDuration() const
{
	switch (Mode)
	{
	case EYSSwordsmanMode::BladeThrow:		return GetThrowStepBackDuration();
	case EYSSwordsmanMode::BladeMastery:	return 0.0f;
	default:								return Super::GetActionDuration();
	}
}

float UYSSwordsmanAbility::GetSwingElapsed() const
{
	// pas de coupe de l'épée pour Blade Throw / Blade Mastery
	return Mode == EYSSwordsmanMode::Dash ? Super::GetSwingElapsed() : -1.0f;
}

bool UYSSwordsmanAbility::IsGrantingInvulnerability() const
{
	// Blade Throw : invulnérable pendant le pas en arrière
	float Alpha;
	return Mode == EYSSwordsmanMode::Dash ? Super::IsGrantingInvulnerability() : IsInThrowStepBack(Alpha);
}

float UYSSwordsmanAbility::GetMoveSpeedMultiplier() const
{
	if (Mode == EYSSwordsmanMode::Dash)
	{
		return Super::GetMoveSpeedMultiplier();
	}
	float Alpha;
	return IsInThrowStepBack(Alpha) ? BladeThrowStepBackSpeedScale * FMath::Lerp(StepBackSpeedMultiplier, StepBackEndSpeedMultiplier, FMath::InterpEaseOut(0.0f, 1.0f, Alpha, StepBackDecayExponent)) : 1.0f;
}

bool UYSSwordsmanAbility::GetForcedMoveDirection(FVector& OutDirection) const
{
	if (Mode == EYSSwordsmanMode::Dash)
	{
		return Super::GetForcedMoveDirection(OutDirection);
	}
	float Alpha;
	if (!IsInThrowStepBack(Alpha))
	{
		return false;
	}
	OutDirection = -LockedDirection;
	return true;
}

bool UYSSwordsmanAbility::OverridesMoveInput() const
{
	float Alpha;
	return Mode == EYSSwordsmanMode::Dash ? Super::OverridesMoveInput() : IsInThrowStepBack(Alpha);
}

bool UYSSwordsmanAbility::GetLockedAimDirection(FVector& OutDirection) const
{
	if (Mode == EYSSwordsmanMode::Dash)
	{
		return Super::GetLockedAimDirection(OutDirection);
	}
	float Alpha;
	if (!IsInThrowStepBack(Alpha))
	{
		return false;
	}
	OutDirection = LockedDirection;
	return true;
}

bool UYSSwordsmanAbility::GetBodyRoll(float& OutAngle, float& OutHeightOffset) const
{
	return Mode == EYSSwordsmanMode::Dash && Super::GetBodyRoll(OutAngle, OutHeightOffset);
}

FVector UYSSwordsmanAbility::ComputeThrowTarget(const FVector& TargetLocation) const
{
	const APawn* Holder = GetHolderPawn();
	if (!Holder)
	{
		return TargetLocation;
	}
	const FVector Origin = Holder->GetActorLocation();
	FVector ToTarget = TargetLocation - Origin;
	ToTarget.Z = 0.0f;
	const FVector Direction = ToTarget.IsNearlyZero() ? Holder->GetActorForwardVector().GetSafeNormal2D() : ToTarget.GetSafeNormal();
	const float Distance = FMath::Clamp(ToTarget.Size(), BladeThrowMinRange, BladeThrowMaxRange);
	return Origin + Direction * Distance;
}

void UYSSwordsmanAbility::OnActivationPredicted(const FVector& TargetLocation)
{
	switch (Mode)
	{
	case EYSSwordsmanMode::BladeThrow:
		// pas en arrière prédit (direction figée par PrepareActivation) et prévisu du point de croisement
		KickThrowStepBack();
		ShowPreview(ComputeThrowTarget(TargetLocation));
		break;
	case EYSSwordsmanMode::BladeMastery:
		break;
	default:
		Super::OnActivationPredicted(TargetLocation);
		break;
	}
}

void UYSSwordsmanAbility::ExecuteAbility(const FVector& TargetLocation)
{
	switch (Mode)
	{
	case EYSSwordsmanMode::BladeThrow:
	{
		LockDirection(TargetLocation);
		ThrowTarget = ComputeThrowTarget(TargetLocation);
		bThrowPending = true;
		KickThrowStepBack();
		const APawn* Holder = GetHolderPawn();
		if (Holder && Holder->IsLocallyControlled())
		{
			ShowPreview(ThrowTarget);
		}
		UE_LOG(LogTemp, Verbose, TEXT("[YSBladeThrow] %s : pas en arrière (%.2f s), croisement en (%.0f, %.0f)"), *GetOwner()->GetName(), GetThrowStepBackDuration(), ThrowTarget.X, ThrowTarget.Y);
		break;
	}
	case EYSSwordsmanMode::BladeMastery:
		LockDirection(TargetLocation);
		DoBladeMastery();
		break;
	default:
		Super::ExecuteAbility(TargetLocation);
		break;
	}
}

void UYSSwordsmanAbility::OnActionCancelled()
{
	Super::OnActionCancelled();
	bThrowPending = false;
	HidePreview();
}

void UYSSwordsmanAbility::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PreviewZone)
	{
		PreviewZone->DestroyComponent();
		PreviewZone = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UYSSwordsmanAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// upgrade d'arme qui remplace l'Ability (serveur, répliqué) : Blade Mastery, sinon Blade Throw, sinon Dashing Slash
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Mode = GetWeaponStat(UYSBladeMasterUpgrade::BladeMasteryStat, 0.0f) > 0.0f ? EYSSwordsmanMode::BladeMastery
			: GetWeaponStat(UYSBladeThrowUpgrade::BladeThrowStat, 0.0f) > 0.0f ? EYSSwordsmanMode::BladeThrow
			: EYSSwordsmanMode::Dash;
	}
	if (Mode == EYSSwordsmanMode::Dash)
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
		return;
	}
	UYSAbilityComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Blade Throw : pas en arrière intangible (traverse NME et joueurs), point de croisement qui suit le curseur jusqu'au lancer
	float StepBackAlpha;
	const APawn* AimHolder = GetHolderPawn();
	SetPassThroughPawns(IsInThrowStepBack(StepBackAlpha) && AimHolder && (GetOwner()->HasAuthority() || AimHolder->IsLocallyControlled()));
	UpdateThrowAim(DeltaTime);

	// fin du pas en arrière : lancer (serveur), prévisu cachée (toutes les machines)
	const float Elapsed = GetTimeSinceActivation();
	if (Elapsed >= GetThrowStepBackDuration())
	{
		if (bThrowPending && GetOwner()->HasAuthority())
		{
			bThrowPending = false;
			Throw();
		}
		HidePreview();
	}
}

void UYSSwordsmanAbility::Throw()
{
	AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (!Character)
	{
		return;
	}
	// Swordsrang : 2x plus lentes, puis vers le joueur
	const bool bSwordsrang = Character->GetCharacterWeaponStat(UYSBladeThrowUpgrade::SwordsrangStat) > 0.0f;
	FYSBladePath Path;
	Path.Mode = EYSBladePathMode::Curve;
	Path.Start = Character->GetActorLocation();
	Path.Target = ThrowTarget;
	Path.Apex = BladeThrowApex;
	Path.OutDuration = bSwordsrang ? SwordsrangDuration : BladeThrowDuration;
	Path.ReturnDuration = Path.OutDuration;
	Path.bHomingReturn = bSwordsrang;
	Path.CriticalRadius = CriticalZoneRadius;
	for (const float Side : { 1.0f, -1.0f })
	{
		Path.Side = Side;
		AYSLesserSpinningBlade::Launch(Character, Path);
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSBladeThrow] %s : 2 lames vers (%.0f, %.0f)%s"), *GetOwner()->GetName(), ThrowTarget.X, ThrowTarget.Y, bSwordsrang ? TEXT(" (Swordsrang)") : TEXT(""));
	// Blood Swords (Golden du Bloodlust) : fin du pas en arrière
	Character->OnAbilityStepBackFinished();
}

void UYSSwordsmanAbility::DoBladeMastery()
{
	AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (!Character)
	{
		return;
	}
	AYSSpinningSword* Self = AYSSpinningSword::FindSelf(Character);
	const bool bBladeguedon = Character->GetCharacterWeaponStat(UYSBladeMasterUpgrade::BladeguedonStat) > 0.0f;
	if (!bBladeguedon || !Self || Self->GetLevel() < Self->GetMaxLevel())
	{
		const AYSSpinningSword* Sword = AYSSpinningSword::GiveLevel(Character, true, Character->GetActorLocation());
		UE_LOG(LogTemp, Verbose, TEXT("[YSBladeMastery] %s : Self Spinning Sword level %d"), *GetOwner()->GetName(), Sword ? Sword->GetLevel() : 0);
		return;
	}

	// Bladeguedon : la Self Spinning Sword au level max disparaît, des Lesser Spinning Blades partent en parallèle devant
	Self->Destroy();
	const FVector Forward = LockedDirection.IsNearlyZero() ? Character->GetActorForwardVector().GetSafeNormal2D() : LockedDirection;
	const FVector Right(-Forward.Y, Forward.X, 0.0f);
	FYSBladePath Path;
	Path.Mode = EYSBladePathMode::Straight;
	Path.OutDuration = BladeguedonDuration;
	for (int32 Index = 0; Index < BladeguedonCount; ++Index)
	{
		const FVector Start = Character->GetActorLocation() + Right * (Index - (BladeguedonCount - 1) * 0.5f) * BladeguedonSpacing;
		Path.Start = Start;
		Path.Target = Start + Forward * BladeguedonRange;
		AYSLesserSpinningBlade::Launch(Character, Path);
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSBladeMastery] %s : Bladeguedon, %d lames"), *GetOwner()->GetName(), BladeguedonCount);
}

void UYSSwordsmanAbility::ShowPreview(const FVector& Location)
{
	if (!PreviewZone && GetOwner())
	{
		PreviewZone = NewObject<UYSGroundZoneComponent>(GetOwner(), TEXT("BladeThrowPreview"));
		PreviewZone->SetUsingAbsoluteLocation(true);
		PreviewZone->SetUsingAbsoluteRotation(true);
		PreviewZone->SetUsingAbsoluteScale(true);
		PreviewZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PreviewZone->SetCastShadow(false);
		PreviewZone->RegisterComponent();
	}
	if (!PreviewZone)
	{
		return;
	}
	PreviewZone->SetWorldLocation(Location);
	PreviewZone->BuildZone(CriticalZoneRadius, 5.0f);
	if (PreviewMaterial)
	{
		UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(PreviewZone->GetMaterial(0));
		if (!Material)
		{
			Material = PreviewZone->CreateDynamicMaterialInstance(0, PreviewMaterial);
		}
		Material->SetVectorParameterValue(TEXT("Color"), PreviewColor);
		Material->SetScalarParameterValue(TEXT("Glow"), 0.5f);
		Material->SetScalarParameterValue(TEXT("FillOpacity"), PreviewFillOpacity);
		Material->SetScalarParameterValue(TEXT("EdgeOpacity"), PreviewEdgeOpacity);
		Material->SetScalarParameterValue(TEXT("Radius"), CriticalZoneRadius);
		Material->SetScalarParameterValue(TEXT("EdgeWidth"), 15.0f);
	}
	PreviewZone->SetVisibility(true);
}

void UYSSwordsmanAbility::HidePreview()
{
	if (PreviewZone && PreviewZone->IsVisible())
	{
		PreviewZone->SetVisibility(false);
	}
}

void UYSSwordsmanAbility::KickThrowStepBack()
{
	ACharacter* Character = Cast<ACharacter>(GetHolderPawn());
	const AYSPlayerCharacter* Player = Cast<AYSPlayerCharacter>(Character);
	if (!Player || !IsMovementDriver() || GetThrowStepBackDuration() <= 0.0f)
	{
		return;
	}
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const float Speed = Player->GetUnmodifiedWalkSpeed() * StepBackSpeedMultiplier * BladeThrowStepBackSpeedScale;
	Movement->Velocity = -LockedDirection * Speed + FVector(0.0f, 0.0f, Movement->Velocity.Z);
	Movement->MaxWalkSpeed = Speed;
}

void UYSSwordsmanAbility::UpdateThrowAim(float DeltaTime)
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	float Alpha;
	if (!Character || !Character->IsLocallyControlled() || !IsInThrowStepBack(Alpha))
	{
		return;
	}
	// joueur qui lance : le point de croisement suit le curseur jusqu'au lancer
	const FVector Target = ComputeThrowTarget(Character->GetPublicAimLocation());
	ShowPreview(Target);
	if (GetOwner()->HasAuthority())
	{
		ThrowTarget = Target;
		return;
	}
	ThrowAimSendTimer -= DeltaTime;
	if (ThrowAimSendTimer <= 0.0f)
	{
		ThrowAimSendTimer = 0.05f;
		ServerSetThrowTarget(Target);
	}
}

void UYSSwordsmanAbility::ServerSetThrowTarget_Implementation(FVector_NetQuantize Target)
{
	if (Mode == EYSSwordsmanMode::BladeThrow && bThrowPending)
	{
		ThrowTarget = ComputeThrowTarget(Target);
	}
}
