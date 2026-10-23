#include "Abilities/YSDashingSlashAbility.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/YSPlayerCharacter.h"
#include "Weapons/YSWeapon.h"

UYSDashingSlashAbility::UYSDashingSlashAbility()
{
	DisplayName = NSLOCTEXT("YS", "DashingSlash", "Dashing Slash");
	Cooldown = 5.0f;
	Damage = 75.0f;
	// les deux épées frappent en même temps
	AkimboFireDelay = 0.0f;
	IconTint = FLinearColor(1.0f, 0.45f, 0.2f);
}

bool UYSDashingSlashAbility::IsMovementDriver() const
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	return Character && (!Character->IsAkimbo() || Character->GetMainHandWeapon() == GetOwner());
}

int32 UYSDashingSlashAbility::GetPhase(float& OutAlpha) const
{
	OutAlpha = 0.0f;
	const float Elapsed = GetTimeSinceActivation();
	if (!IsMovementDriver() || Elapsed < 0.0f)
	{
		return -1;
	}
	if (Elapsed < StepBackDuration)
	{
		OutAlpha = Elapsed / StepBackDuration;
		return 0;
	}
	if (Elapsed < StepBackDuration + DashDuration)
	{
		OutAlpha = (Elapsed - StepBackDuration) / DashDuration;
		return 1;
	}
	return -1;
}

void UYSDashingSlashAbility::LockDirection(const FVector& TargetLocation)
{
	const APawn* Holder = GetHolderPawn();
	if (!Holder)
	{
		return;
	}
	// direction du curseur au clic ; curseur sur le personnage : droit devant
	const FVector ToTarget = (TargetLocation - Holder->GetActorLocation()).GetSafeNormal2D();
	LockedDirection = ToTarget.IsNearlyZero() ? Holder->GetActorForwardVector().GetSafeNormal2D() : ToTarget;
}

void UYSDashingSlashAbility::PrepareActivation(const FVector& TargetLocation)
{
	LockDirection(TargetLocation);
}

void UYSDashingSlashAbility::OnActivationPredicted(const FVector& TargetLocation)
{
	bDashKickApplied = false;
	ApplyPhaseKick(0);

	// l'autre épée joue sa coupe en même temps, sans attendre le serveur
	if (UYSDashingSlashAbility* Partner = GetDashPartner())
	{
		Partner->ReadyServerTime = ReadyServerTime;
		Partner->NotifyActivated();
	}
}

void UYSDashingSlashAbility::ExecuteAbility(const FVector& TargetLocation)
{
	LockDirection(TargetLocation);
	HitThisSwing.Reset();
	bDashKickApplied = false;
	ApplyPhaseKick(0);
	StartPartner();

	const APawn* Holder = GetHolderPawn();
	UE_LOG(LogTemp, Verbose, TEXT("[YSDashingSlash] %s lancé à %.2f depuis (%.0f, %.0f) vers (%.2f, %.2f)"), *GetOwner()->GetName(), GetWorld()->GetTimeSeconds(),
		Holder ? Holder->GetActorLocation().X : 0.0f, Holder ? Holder->GetActorLocation().Y : 0.0f, LockedDirection.X, LockedDirection.Y);
}

void UYSDashingSlashAbility::StartPartner()
{
	UYSDashingSlashAbility* Partner = GetDashPartner();
	if (!Partner)
	{
		return;
	}
	// même recharge, coupe jouée sur toutes les machines (le client qui a activé l'a déjà prédite)
	Partner->ReadyServerTime = ReadyServerTime;
	Partner->HitThisSwing.Reset();
	Partner->MulticastActivated();
}

void UYSDashingSlashAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// coupe : hitbox et dégâts (serveur), hitbox de debug
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// mouvement : serveur et client qui contrôle le personnage (prédiction)
	const APawn* Holder = GetHolderPawn();
	if (!Holder || (!GetOwner()->HasAuthority() && !Holder->IsLocallyControlled()))
	{
		return;
	}

	float Alpha;
	const int32 Phase = GetPhase(Alpha);
	if (Phase != LastLoggedPhase)
	{
		static const TCHAR* PhaseNames[] = { TEXT("fin"), TEXT("pas en arrière"), TEXT("dash") };
		UE_LOG(LogTemp, Verbose, TEXT("[YSDashingSlash] %s (%s) : %s à %.2f, position (%.0f, %.0f), vitesse %.0f"), *GetOwner()->GetName(),
			GetOwner()->HasAuthority() ? TEXT("serveur") : TEXT("client"), PhaseNames[Phase + 1], GetWorld()->GetTimeSeconds(),
			Holder->GetActorLocation().X, Holder->GetActorLocation().Y, Holder->GetVelocity().Size2D());
		LastLoggedPhase = Phase;
	}
	if (Phase == 1 && !bDashKickApplied)
	{
		bDashKickApplied = true;
		ApplyPhaseKick(1);
		SetPassThroughPawns(bDashThroughFoes);
	}
	else if (Phase != 1 && bPassingThroughPawns)
	{
		SetPassThroughPawns(false);
	}
}

void UYSDashingSlashAbility::ApplyPhaseKick(int32 Phase)
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (!Character || !IsMovementDriver() || (Phase == 0 && StepBackDuration <= 0.0f))
	{
		return;
	}

	// vitesse x multiplicateur tout de suite dans la direction figée ; elle retombe ensuite avec MaxWalkSpeed
	const float Speed = Character->GetUnmodifiedWalkSpeed() * (Phase == 0 ? StepBackSpeedMultiplier : DashSpeedMultiplier);
	const FVector Direction = Phase == 0 ? -LockedDirection : LockedDirection;
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Movement->Velocity = Direction * Speed + FVector(0.0f, 0.0f, Movement->Velocity.Z);
	Movement->MaxWalkSpeed = Speed;
}

void UYSDashingSlashAbility::SetPassThroughPawns(bool bPassThrough)
{
	const ACharacter* Character = Cast<ACharacter>(GetHolderPawn());
	if (!Character || bPassThrough == bPassingThroughPawns)
	{
		return;
	}
	bPassingThroughPawns = bPassThrough;
	Character->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, bPassThrough ? ECR_Ignore : ECR_Block);
}

void UYSDashingSlashAbility::OnActionCancelled()
{
	Super::OnActionCancelled();
	SetPassThroughPawns(false);
}

bool UYSDashingSlashAbility::IsGrantingInvulnerability() const
{
	const float Elapsed = GetTimeSinceActivation();
	return bDashInvulnerable && IsMovementDriver() && Elapsed >= StepBackDuration && Elapsed < StepBackDuration + DashDuration + ExtraInvulnerability;
}

float UYSDashingSlashAbility::GetMoveSpeedMultiplier() const
{
	float Alpha;
	switch (GetPhase(Alpha))
	{
	case 0:
		return FMath::Lerp(StepBackSpeedMultiplier, StepBackEndSpeedMultiplier, FMath::InterpEaseOut(0.0f, 1.0f, Alpha, StepBackDecayExponent));
	case 1:
		return FMath::Lerp(DashSpeedMultiplier, DashEndSpeedMultiplier, FMath::InterpEaseOut(0.0f, 1.0f, Alpha, DashDecayExponent));
	default:
		return 1.0f;
	}
}

bool UYSDashingSlashAbility::GetForcedMoveDirection(FVector& OutDirection) const
{
	float Alpha;
	const int32 Phase = GetPhase(Alpha);
	if (Phase < 0)
	{
		return false;
	}
	OutDirection = Phase == 0 ? -LockedDirection : LockedDirection;
	return true;
}

bool UYSDashingSlashAbility::OverridesMoveInput() const
{
	float Alpha;
	return GetPhase(Alpha) >= 0;
}

bool UYSDashingSlashAbility::GetLockedAimDirection(FVector& OutDirection) const
{
	float Alpha;
	if (GetPhase(Alpha) < 0)
	{
		return false;
	}
	OutDirection = LockedDirection;
	return true;
}

UYSDashingSlashAbility* UYSDashingSlashAbility::GetDashPartner() const
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (!Character || !Character->IsAkimbo() || Character->GetMainHandWeapon() != GetOwner())
	{
		return nullptr;
	}
	const AYSWeapon* OffWeapon = Character->GetOffHandWeapon();
	return OffWeapon ? Cast<UYSDashingSlashAbility>(OffWeapon->GetActiveAction()) : nullptr;
}
