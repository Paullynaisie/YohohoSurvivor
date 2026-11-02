#include "Abilities/YSRollAbility.h"
#include "Engine/World.h"
#include "Player/YSPlayerCharacter.h"

UYSRollAbility::UYSRollAbility()
{
	DisplayName = NSLOCTEXT("YS", "Roll", "Roll");
	Cooldown = 0.0f;
	AkimboFireDelay = 0.0f;
}

bool UYSRollAbility::CanActivateNow() const
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	return Character && !Character->IsCarrying() && !Character->IsSwimming() && !Character->IsAtStation() && Character->GetStamina() >= StaminaCost;
}

void UYSRollAbility::OnActivationPredicted(const FVector& TargetLocation)
{
	Super::OnActivationPredicted(TargetLocation);
	if (AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn()))
	{
		Character->PredictStaminaCost(StaminaCost);
	}
}

void UYSRollAbility::ExecuteAbility(const FVector& TargetLocation)
{
	if (AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn()))
	{
		Character->ConsumeStamina(StaminaCost);
	}
	StartRoll();
	MulticastRollStarted();
	UE_LOG(LogTemp, Verbose, TEXT("[YSRoll] %s roulade à %.2f"), *GetOwner()->GetName(), GetWorld()->GetTimeSeconds());
}

void UYSRollAbility::OnActionCancelled()
{
	Super::OnActionCancelled();
	// activation prédite refusée par le serveur : la stamina prédite est rendue
	AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (Character && !Character->HasAuthority())
	{
		Character->ClearPredictedStamina();
	}
}

#if WITH_EDITOR
void UYSRollAbility::GetUnusedProperties(TArray<FName>& OutNames) const
{
	Super::GetUnusedProperties(OutNames);
	// pas de bombe ni d'arme
	OutNames.Append({
		GET_MEMBER_NAME_CHECKED(UYSRollAbility, BombClass),
		GET_MEMBER_NAME_CHECKED(UYSRollAbility, BombDamage),
		GET_MEMBER_NAME_CHECKED(UYSRollAbility, MineClass),
		GET_MEMBER_NAME_CHECKED(UYSRollAbility, SecondMineRollFraction),
		GET_MEMBER_NAME_CHECKED(UYSRollAbility, bAlwaysDropSecondBomb),
		GET_MEMBER_NAME_CHECKED(UYSRollAbility, AkimboFireDelay),
		GET_MEMBER_NAME_CHECKED(UYSRollAbility, bResetAutoAttackOnUse),
		GET_MEMBER_NAME_CHECKED(UYSRollAbility, AutoAttackResetDelay),
	});
}
#endif
