#include "Abilities/YSCamouflageAbility.h"
#include "Components/CapsuleComponent.h"
#include "Core/YSCombat.h"
#include "Enemies/YSFoeCharacter.h"
#include "Engine/World.h"
#include "Player/YSPlayerCharacter.h"

UYSCamouflageAbility::UYSCamouflageAbility()
{
	DisplayName = NSLOCTEXT("YS", "Camouflage", "Camouflage");
	StaminaCost = 60.0f;
	StaminaPerSecond = 10.0f;
	MinDuration = 1.0f;
}

float UYSCamouflageAbility::GetActivationStaminaCost() const
{
	// Camouflage ++ : activation moins chère ; Like a Shadow : x2
	return FMath::Max(0.0f, StaminaCost - GetMoveActionBonus()) * GetSustainCostMultiplier();
}

void UYSCamouflageAbility::OnSustainStarted()
{
	if (GetMoveActionGolden() == GoldenLikeAShadow)
	{
		if (ACharacter* Character = Cast<ACharacter>(GetHolderPawn()))
		{
			YSCombat::SetPassThroughCharacters(Character->GetCapsuleComponent(), true);
			bPassThroughApplied = true;
		}
	}
}

void UYSCamouflageAbility::OnSustainStopped()
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

void UYSCamouflageAbility::OnCancelledByAttack(UYSAbilityComponent* Action, float Consumed)
{
	if (!Action)
	{
		return;
	}
	const float AttackDuration = FMath::Max(Action->GetActionDuration(), ShadowStrikeMinDuration);
	switch (GetMoveActionGolden())
	{
	case GoldenSuckerPunch:
		SuckerPunchEndTime = GetWorld()->GetTimeSeconds() + AttackDuration + SuckerPunchExtraWindow;
		UE_LOG(LogTemp, Verbose, TEXT("[YSCamouflage] %s : Sucker Punch pendant %.2f s (%s)"), *GetOwner()->GetName(), AttackDuration + SuckerPunchExtraWindow, *Action->GetName());
		break;
	case GoldenShadowStrike:
		if (AYSPlayerCharacter* Character = GetCharacter())
		{
			Character->StartShadowStrike(Action, ShadowStrikeShardLevel, AttackDuration + 0.1f);
		}
		break;
	default:
		break;
	}
}

bool UYSCamouflageAbility::OnHolderDealtDamage(AYSFoeCharacter* Foe)
{
	// Sucker Punch : les NME touchés par l'attaque qui a mis fin au camouflage sont bumpés sur place
	if (GetMoveActionGolden() != GoldenSuckerPunch || !Foe || GetWorld()->GetTimeSeconds() > SuckerPunchEndTime || Foe->IsBumped() || !Foe->CanBeBumped(bSuckerPunchElites))
	{
		return false;
	}
	const APawn* Holder = GetHolderPawn();
	FVector Direction = Holder ? (Foe->GetActorLocation() - Holder->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
	if (Direction.IsNearlyZero())
	{
		Direction = Foe->GetActorForwardVector();
	}
	Foe->StartBump(Direction, 0.0f, SuckerPunchApexHeight, SuckerPunchBumpDuration, SuckerPunchSpinTurns);
	UE_LOG(LogTemp, Verbose, TEXT("[YSCamouflage] %s : Sucker Punch bumpe %s sur place"), *GetOwner()->GetName(), *Foe->GetName());
	return true;
}
