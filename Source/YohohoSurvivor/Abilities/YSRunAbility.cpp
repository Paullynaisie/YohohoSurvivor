#include "Abilities/YSRunAbility.h"
#include "Components/CapsuleComponent.h"
#include "Core/YSCombat.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/YSPlayerCharacter.h"

UYSRunAbility::UYSRunAbility()
{
	DisplayName = NSLOCTEXT("YS", "Run", "Run");
	StaminaCost = 20.0f;
	StaminaPerSecond = 10.0f;
	MinDuration = 1.0f;
}

float UYSRunAbility::GetActivationStaminaCost() const
{
	return GetMoveActionGolden() == GoldenImpulse ? ImpulseStaminaCost : StaminaCost;
}

float UYSRunAbility::GetMinDuration() const
{
	// Impulsion : le dash remplace la première seconde
	return GetMoveActionGolden() == GoldenImpulse ? ImpulseDuration : MinDuration;
}

bool UYSRunAbility::IsImpulseDashing() const
{
	return GetMoveActionGolden() == GoldenImpulse && IsSustained() && GetSustainElapsed() < ImpulseDuration;
}

float UYSRunAbility::GetMoveSpeedMultiplier() const
{
	if (!IsSustained())
	{
		return 1.0f;
	}
	return IsImpulseDashing() ? ImpulseSpeedMultiplier : RunSpeedMultiplier + GetMoveActionBonus();
}

bool UYSRunAbility::IsGrantingInvulnerability() const
{
	return IsImpulseDashing();
}

bool UYSRunAbility::WantsPassThrough() const
{
	return IsSustained() && (GetMoveActionGolden() == GoldenFreeRunning || IsImpulseDashing());
}

void UYSRunAbility::OnSustainStarted()
{
	// Impulsion : départ du dash tout de suite dans la direction de l'input (sinon du regard), comme une roulade
	ACharacter* Character = Cast<ACharacter>(GetHolderPawn());
	if (GetMoveActionGolden() == GoldenImpulse && Character && (Character->HasAuthority() || Character->IsLocallyControlled()))
	{
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		const FVector Direction = Movement->GetCurrentAcceleration().IsNearlyZero() ? Character->GetActorForwardVector().GetSafeNormal2D() : Movement->GetCurrentAcceleration().GetSafeNormal2D();
		const float DashSpeed = Movement->MaxWalkSpeed * ImpulseSpeedMultiplier;
		Movement->Velocity = Direction * DashSpeed + FVector(0.0f, 0.0f, Movement->Velocity.Z);
		Movement->MaxWalkSpeed = DashSpeed;
	}
}

void UYSRunAbility::OnSustainStopped()
{
	if (bPassThroughApplied)
	{
		if (ACharacter* Character = Cast<ACharacter>(GetHolderPawn()))
		{
			YSCombat::SetPassThroughCharacters(Character->GetCapsuleComponent(), false);
		}
		bPassThroughApplied = false;
	}
}

void UYSRunAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Free running (toute la course) / Impulsion (le dash) : traverse les NME et les joueurs, sur chaque machine
	const bool bWanted = WantsPassThrough();
	if (bWanted != bPassThroughApplied)
	{
		if (ACharacter* Character = Cast<ACharacter>(GetHolderPawn()))
		{
			YSCombat::SetPassThroughCharacters(Character->GetCapsuleComponent(), bWanted);
		}
		bPassThroughApplied = bWanted;
	}
}

void UYSRunAbility::OnCancelledByAttack(UYSAbilityComponent* Action, float Consumed)
{
	// Run and Hit : la stamina dépensée s'ajoute aux dégâts de base de l'attaque (avant les multiplicateurs)
	if (GetMoveActionGolden() == GoldenRunAndHit && Action && Consumed > 0.0f)
	{
		Action->SetAttackBonusBaseDamage(Consumed);
		UE_LOG(LogTemp, Verbose, TEXT("[YSRun] %s : Run and Hit, %s +%.0f dégâts de base (%.0f)"), *GetOwner()->GetName(), *Action->GetName(), Consumed, Action->GetBaseDamage());
	}
}
