#include "Abilities/YSMoveActionAbility.h"
#include "Abilities/YSRollAbility.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
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
	// conditions du Roll ; objet porté permis si la MoveAction le permet (Fenwick, Thief)
	const AYSPlayerCharacter* Character = GetCharacter();
	if (Character && Character->IsCarrying() && AllowsCarrying())
	{
		return !Character->IsSwimming() && !Character->IsAtStation() && Character->GetStamina() >= GetActivationStaminaCost();
	}
	return UYSRollAbility::CanCharacterUseMoveAction(Character, GetActivationStaminaCost());
}

void UYSMoveActionAbility::OnActivationPredicted(const FVector& TargetLocation)
{
	if (AYSPlayerCharacter* Character = GetCharacter())
	{
		Character->PredictStaminaCost(GetActivationStaminaCost());
	}
}

void UYSMoveActionAbility::ExecuteAbility(const FVector& TargetLocation)
{
	if (AYSPlayerCharacter* Character = GetCharacter())
	{
		Character->ConsumeStamina(GetActivationStaminaCost());
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

UYSSustainedMoveActionAbility::UYSSustainedMoveActionAbility()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UYSSustainedMoveActionAbility::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// le client qui contrôle a sa propre prédiction ; une fin imposée par le serveur lui arrive par ClientStopSustain
	DOREPLIFETIME_CONDITION(UYSSustainedMoveActionAbility, bSustained, COND_SkipOwner);
}

bool UYSSustainedMoveActionAbility::CanActivateNow() const
{
	return !bSustained && Super::CanActivateNow();
}

float UYSSustainedMoveActionAbility::GetSustainElapsed() const
{
	const UWorld* World = GetWorld();
	return bSustained && World ? static_cast<float>(World->GetTimeSeconds() - SustainStartTime) : TNumericLimits<float>::Max();
}

void UYSSustainedMoveActionAbility::SetSustainedLocal(bool bNewSustained)
{
	if (bNewSustained)
	{
		SustainStartTime = GetWorld()->GetTimeSeconds();
		bStopRequested = false;
	}
	if (bSustained == bNewSustained)
	{
		return;
	}
	bSustained = bNewSustained;
	if (bSustained)
	{
		OnSustainStarted();
	}
	else
	{
		OnSustainStopped();
	}
}

void UYSSustainedMoveActionAbility::OnRep_Sustained()
{
	// autres machines : bSustained vient d'être changé par la réplication, effets locaux
	if (bSustained)
	{
		SustainStartTime = GetWorld()->GetTimeSeconds();
		OnSustainStarted();
	}
	else
	{
		OnSustainStopped();
	}
}

void UYSSustainedMoveActionAbility::OnInputPressed(const FVector& TargetLocation)
{
	// en bascule : un 2e appui l'arrête (après Min Duration)
	if (bSustained)
	{
		if (!bHoldToSustain)
		{
			bStopRequested = true;
		}
		return;
	}
	TryActivate(TargetLocation);
}

void UYSSustainedMoveActionAbility::OnInputReleased()
{
	if (bHoldToSustain && bSustained)
	{
		bStopRequested = true;
	}
}

void UYSSustainedMoveActionAbility::OnActivationPredicted(const FVector& TargetLocation)
{
	Super::OnActivationPredicted(TargetLocation);
	SetSustainedLocal(true);
}

void UYSSustainedMoveActionAbility::ExecuteMoveAction(const FVector& TargetLocation)
{
	ConsumedStamina = GetActivationStaminaCost();
	SetSustainedLocal(true);
	GetOwner()->ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("[YSMoveAction] %s %s à %.2f (%.0f de stamina)"), *GetOwner()->GetName(), GetLogVerb(), GetWorld()->GetTimeSeconds(), ConsumedStamina);
}

void UYSSustainedMoveActionAbility::OnActivationRejected()
{
	SetSustainedLocal(false);
}

void UYSSustainedMoveActionAbility::RequestStop()
{
	if (GetOwner()->HasAuthority())
	{
		StopSustain(false, TEXT("touche"));
		return;
	}
	SetSustainedLocal(false);
	ServerStopSustain();
}

void UYSSustainedMoveActionAbility::ServerStopSustain_Implementation()
{
	StopSustain(false, TEXT("touche"));
}

void UYSSustainedMoveActionAbility::ClientStopSustain_Implementation()
{
	SetSustainedLocal(false);
}

void UYSSustainedMoveActionAbility::StopSustain(bool bNotifyOwner, const TCHAR* Reason)
{
	if (!bSustained)
	{
		return;
	}
	SetSustainedLocal(false);
	GetOwner()->ForceNetUpdate();
	const APawn* Holder = GetHolderPawn();
	if (bNotifyOwner && Holder && !Holder->IsLocallyControlled())
	{
		ClientStopSustain();
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSMoveAction] %s : fin (%s) à %.2f, %.0f de stamina dépensée"), *GetOwner()->GetName(), Reason, GetWorld()->GetTimeSeconds(), ConsumedStamina);
}

void UYSSustainedMoveActionAbility::OnHolderActionStarted(UYSAbilityComponent* Action)
{
	if (!bSustained || !GetOwner()->HasAuthority())
	{
		return;
	}
	const float Consumed = ConsumedStamina;
	StopSustain(true, TEXT("attaque"));
	OnCancelledByAttack(Action, Consumed);
}

void UYSSustainedMoveActionAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bSustained)
	{
		return;
	}

	// machine qui contrôle : touche relâchée, arrêt une fois Min Duration passée
	const APawn* Holder = GetHolderPawn();
	if (bStopRequested && Holder && Holder->IsLocallyControlled() && GetSustainElapsed() >= GetMinDuration())
	{
		bStopRequested = false;
		RequestStop();
		return;
	}

	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	// serveur : fin imposée (plus de stamina, objet porté, nage, poste, mort), sinon consommation continue après Min Duration
	AYSPlayerCharacter* Character = GetCharacter();
	if (!Character || !Character->IsAlive() || Character->IsDying() || (Character->IsCarrying() && !AllowsCarrying()) || Character->IsSwimming() || Character->IsAtStation())
	{
		StopSustain(true, TEXT("mains prises / eau / poste / mort"));
		return;
	}
	if (GetSustainElapsed() < GetMinDuration())
	{
		return;
	}
	if (Character->GetStamina() <= 0.0f)
	{
		StopSustain(true, TEXT("plus de stamina"));
		return;
	}
	const float Cost = FMath::Min(Character->GetStamina(), StaminaPerSecond * GetSustainCostMultiplier() * DeltaTime);
	ConsumedStamina += Cost;
	Character->ConsumeStamina(Cost, false);
}
