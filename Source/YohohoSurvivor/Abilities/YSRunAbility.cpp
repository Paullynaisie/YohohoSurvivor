#include "Abilities/YSRunAbility.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"

UYSRunAbility::UYSRunAbility()
{
	DisplayName = NSLOCTEXT("YS", "Run", "Run");
	StaminaCost = 20.0f;
	PrimaryComponentTick.bCanEverTick = true;
}

void UYSRunAbility::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// le client qui contrôle a sa propre prédiction ; une fin imposée par le serveur lui arrive par ClientStopRunning
	DOREPLIFETIME_CONDITION(UYSRunAbility, bRunning, COND_SkipOwner);
}

bool UYSRunAbility::CanActivateNow() const
{
	return !bRunning && Super::CanActivateNow();
}

float UYSRunAbility::GetMoveSpeedMultiplier() const
{
	return bRunning ? RunSpeedMultiplier : 1.0f;
}

void UYSRunAbility::OnInputPressed(const FVector& TargetLocation)
{
	// en bascule : un 2e appui arrête la course
	if (bRunning)
	{
		if (!bHoldToRun)
		{
			RequestStop();
		}
		return;
	}
	TryActivate(TargetLocation);
}

void UYSRunAbility::OnInputReleased()
{
	if (bHoldToRun && bRunning)
	{
		RequestStop();
	}
}

void UYSRunAbility::OnActivationPredicted(const FVector& TargetLocation)
{
	Super::OnActivationPredicted(TargetLocation);
	bRunning = true;
}

void UYSRunAbility::ExecuteMoveAction(const FVector& TargetLocation)
{
	bRunning = true;
	GetOwner()->ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("[YSRun] %s court à %.2f"), *GetOwner()->GetName(), GetWorld()->GetTimeSeconds());
}

void UYSRunAbility::OnActivationRejected()
{
	bRunning = false;
}

void UYSRunAbility::RequestStop()
{
	if (GetOwner()->HasAuthority())
	{
		StopRunning(false, TEXT("touche"));
		return;
	}
	bRunning = false;
	ServerStopRunning();
}

void UYSRunAbility::ServerStopRunning_Implementation()
{
	StopRunning(false, TEXT("touche"));
}

void UYSRunAbility::ClientStopRunning_Implementation()
{
	bRunning = false;
}

void UYSRunAbility::StopRunning(bool bNotifyOwner, const TCHAR* Reason)
{
	if (!bRunning)
	{
		return;
	}
	bRunning = false;
	GetOwner()->ForceNetUpdate();
	const APawn* Holder = GetHolderPawn();
	if (bNotifyOwner && Holder && !Holder->IsLocallyControlled())
	{
		ClientStopRunning();
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSRun] %s arrête de courir (%s) à %.2f"), *GetOwner()->GetName(), Reason, GetWorld()->GetTimeSeconds());
}

void UYSRunAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bRunning || !GetOwner()->HasAuthority())
	{
		return;
	}

	// serveur : fin imposée (plus de stamina, objet porté, nage, poste, mort), sinon consommation continue
	AYSPlayerCharacter* Character = GetCharacter();
	if (!Character || !Character->IsAlive() || Character->IsDying() || Character->IsCarrying() || Character->IsSwimming() || Character->IsAtStation())
	{
		StopRunning(true, TEXT("mains prises / eau / poste / mort"));
		return;
	}
	if (Character->GetStamina() <= 0.0f)
	{
		StopRunning(true, TEXT("plus de stamina"));
		return;
	}
	Character->ConsumeStamina(StaminaPerSecond * DeltaTime, false);
}
