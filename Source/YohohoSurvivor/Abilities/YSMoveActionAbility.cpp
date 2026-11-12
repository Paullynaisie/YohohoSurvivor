#include "Abilities/YSMoveActionAbility.h"
#include "Abilities/YSRollAbility.h"
#include "Player/YSPlayerCharacter.h"

UYSMoveActionAbility::UYSMoveActionAbility()
{
	Cooldown = 0.0f;
	AkimboFireDelay = 0.0f;
}

AYSPlayerCharacter* UYSMoveActionAbility::GetCharacter() const
{
	return Cast<AYSPlayerCharacter>(GetHolderPawn());
}

bool UYSMoveActionAbility::CanActivateNow() const
{
	return UYSRollAbility::CanCharacterUseMoveAction(GetCharacter(), StaminaCost);
}

void UYSMoveActionAbility::OnActivationPredicted(const FVector& TargetLocation)
{
	if (AYSPlayerCharacter* Character = GetCharacter())
	{
		Character->PredictStaminaCost(StaminaCost);
	}
}

void UYSMoveActionAbility::ExecuteAbility(const FVector& TargetLocation)
{
	if (AYSPlayerCharacter* Character = GetCharacter())
	{
		Character->ConsumeStamina(StaminaCost);
	}
	ExecuteMoveAction(TargetLocation);
}

void UYSMoveActionAbility::OnActionCancelled()
{
	Super::OnActionCancelled();
	// activation prédite refusée par le serveur : la stamina prédite est rendue
	AYSPlayerCharacter* Character = GetCharacter();
	if (Character && !Character->HasAuthority())
	{
		Character->ClearPredictedStamina();
	}
}

#if WITH_EDITOR
void UYSMoveActionAbility::GetUnusedProperties(TArray<FName>& OutNames) const
{
	Super::GetUnusedProperties(OutNames);
	// pas d'arme : ni akimbo, ni attaque auto à rendre, ni type d'arme
	OutNames.Append({
		GET_MEMBER_NAME_CHECKED(UYSMoveActionAbility, WeaponType),
		GET_MEMBER_NAME_CHECKED(UYSMoveActionAbility, AkimboFireDelay),
		GET_MEMBER_NAME_CHECKED(UYSMoveActionAbility, bResetAutoAttackOnUse),
		GET_MEMBER_NAME_CHECKED(UYSMoveActionAbility, AutoAttackResetDelay),
	});
}
#endif
