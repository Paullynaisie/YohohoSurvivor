#include "Abilities/YSMeteorJumpAbility.h"
#include "Components/CapsuleComponent.h"
#include "Core/YSCombat.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UYSMeteorJumpAbility::UYSMeteorJumpAbility()
{
	DisplayName = NSLOCTEXT("YS", "MeteorJump", "MeteorJump");
}

void UYSMeteorJumpAbility::LockDirection(const FVector& TargetLocation)
{
	Super::LockDirection(TargetLocation);
	JumpLanding = YSCombat::FindLandingPoint(Cast<ACharacter>(GetHolderPawn()), TargetLocation, MaxJumpDistance, RetreatStep);
}

void UYSMeteorJumpAbility::ApplyPhaseKick(int32 Phase)
{
	if (Phase != 1)
	{
		Super::ApplyPhaseKick(Phase);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetHolderPawn());
	if (!Character || !IsMovementDriver())
	{
		return;
	}

	// cloche de la position actuelle (centre de la capsule) au sol visé : sommet au-dessus du plus haut des deux points,
	// montée + descente = Jump Duration  =>  gravité g = 2 (sqrt(Montée) + sqrt(Descente))² / T², vitesse verticale sqrt(2 g Montée)
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const FVector Start = Character->GetActorLocation();
	const FVector Target = JumpLanding + FVector(0.0f, 0.0f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const float ApexZ = FMath::Max(Start.Z, Target.Z) + FMath::Max(JumpApexHeight, 1.0f);
	const float Rise = ApexZ - Start.Z;
	const float Fall = ApexZ - Target.Z;
	const float Duration = FMath::Max(JumpDuration, 0.05f);
	const float Gravity = 2.0f * FMath::Square(FMath::Sqrt(Rise) + FMath::Sqrt(Fall)) / FMath::Square(Duration);
	const float WorldGravity = FMath::Abs(GetWorld()->GetGravityZ());

	if (!bJumping)
	{
		SavedGravityScale = Movement->GravityScale;
	}
	bJumping = true;
	Movement->GravityScale = WorldGravity > KINDA_SMALL_NUMBER ? Gravity / WorldGravity : 1.0f;
	Movement->SetMovementMode(MOVE_Falling);
	const FVector Horizontal = (Target - Start) * FVector(1.0f, 1.0f, 0.0f) / Duration;
	Movement->Velocity = Horizontal + FVector(0.0f, 0.0f, FMath::Sqrt(2.0f * Gravity * Rise));

	UE_LOG(LogTemp, Verbose, TEXT("[YSMeteorJump] %s (%s) saut à %.2f de (%.0f, %.0f, %.0f) vers (%.0f, %.0f, %.0f) : %.2f s, sommet %.0f, gravité x %.2f%s"), *GetOwner()->GetName(),
		GetOwner()->HasAuthority() ? TEXT("serveur") : TEXT("client"), GetWorld()->GetTimeSeconds(), Start.X, Start.Y, Start.Z, Target.X, Target.Y, Target.Z,
		Duration, ApexZ, Movement->GravityScale, bJumpThroughFoes ? TEXT(", traverse les NME") : TEXT(", ne traverse pas les NME"));
}

void UYSMeteorJumpAbility::GetCurrentEases(float& OutWindup, float& OutSwing, float& OutRecover) const
{
	Super::GetCurrentEases(OutWindup, OutSwing, OutRecover);
	OutSwing = JumpSwingEaseExponent;
}

void UYSMeteorJumpAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// en l'air (saut, ou chute qui le prolonge) : la traversée des NME dure jusqu'à Pass Through After Landing après l'atterrissage
	float Alpha;
	const int32 Phase = GetPhase(Alpha);
	const ACharacter* Character = Cast<ACharacter>(GetHolderPawn());
	if (Phase == 1)
	{
		bAwaitingLanding = true;
	}
	if (bAwaitingLanding)
	{
		if (Phase == 1 || (Character && Character->GetCharacterMovement()->IsFalling()))
		{
			PassThroughUntil = GetWorld()->GetTimeSeconds() + PassThroughAfterLanding;
		}
		else
		{
			bAwaitingLanding = false;
			UE_LOG(LogTemp, Verbose, TEXT("[YSMeteorJump] %s (%s) atterrit à %.2f, traverse les NME jusqu'à %.2f"), *GetOwner()->GetName(),
				GetOwner()->HasAuthority() ? TEXT("serveur") : TEXT("client"), GetWorld()->GetTimeSeconds(), PassThroughUntil);
		}
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// fin du saut : gravité normale (atterrissage géré par le CharacterMovement)
	if (bJumping && Phase != 1)
	{
		EndJump();
	}
}

bool UYSMeteorJumpAbility::PassesThroughPawns(int32 Phase) const
{
	return bJumpThroughFoes && (Phase >= 0 || GetWorld()->GetTimeSeconds() < PassThroughUntil);
}

void UYSMeteorJumpAbility::EndJump()
{
	if (!bJumping)
	{
		return;
	}
	bJumping = false;
	if (const ACharacter* Character = Cast<ACharacter>(GetHolderPawn()))
	{
		Character->GetCharacterMovement()->GravityScale = SavedGravityScale;
		UE_LOG(LogTemp, Verbose, TEXT("[YSMeteorJump] %s (%s) fin du saut à %.2f en (%.0f, %.0f, %.0f)"), *GetOwner()->GetName(), GetOwner()->HasAuthority() ? TEXT("serveur") : TEXT("client"),
			GetWorld()->GetTimeSeconds(), Character->GetActorLocation().X, Character->GetActorLocation().Y, Character->GetActorLocation().Z);
	}
}

void UYSMeteorJumpAbility::OnActionCancelled()
{
	Super::OnActionCancelled();
	EndJump();
}

void UYSMeteorJumpAbility::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndJump();
	Super::EndPlay(EndPlayReason);
}

bool UYSMeteorJumpAbility::IsGrantingInvulnerability() const
{
	const float Elapsed = GetTimeSinceActivation();
	return IsMovementDriver() && Elapsed >= 0.0f && Elapsed < StepBackDuration + JumpDuration + ExtraInvulnerability;
}

float UYSMeteorJumpAbility::GetMoveSpeedMultiplier() const
{
	float Alpha;
	return GetPhase(Alpha) == 1 ? 1.0f : Super::GetMoveSpeedMultiplier();
}

bool UYSMeteorJumpAbility::GetForcedMoveDirection(FVector& OutDirection) const
{
	float Alpha;
	return GetPhase(Alpha) != 1 && Super::GetForcedMoveDirection(OutDirection);
}
