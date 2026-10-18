#include "Abilities/YSAbilityComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "GameplayTagAssetInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Player/YSPlayerCharacter.h"
#include "Weapons/YSWeapon.h"

namespace
{
	/** Marge acceptée côté serveur : l'horloge du client n'est synchronisée qu'approximativement */
	constexpr double CooldownTolerance = 0.2;

	/** Priorités des actions d'une arme, de la plus prioritaire à la moins prioritaire (liste donnée par Paul) */
	constexpr EYSActionSlot ActionPriority[] = { EYSActionSlot::Ability, EYSActionSlot::AutoAtk };
}

UYSAbilityComponent::UYSAbilityComponent()
{
	SetIsReplicatedByDefault(true);
}

void UYSAbilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UYSAbilityComponent, Cooldown);
	DOREPLIFETIME(UYSAbilityComponent, ReadyServerTime);
}

void UYSAbilityComponent::TryActivate(const FVector& TargetLocation)
{
	if (!IsReady() || !CanStartByPriority())
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		// joueur hôte (listen server) : exécution directe
		ActivateOnServer(TargetLocation);
		return;
	}

	// prédiction locale de la recharge et des effets pour un retour immédiat, le serveur fait foi
	ReadyServerTime = GetServerTime() + GetActivationLockDuration();
	NotifyActivated();
	ServerActivate(TargetLocation);
}

float UYSAbilityComponent::GetTimeSinceActivation() const
{
	const UWorld* World = GetWorld();
	return World ? static_cast<float>(World->GetTimeSeconds() - LastActivationTime) : TNumericLimits<float>::Max();
}

void UYSAbilityComponent::MulticastActivated_Implementation()
{
	// une attaque automatique n'est jamais prédite : tout le monde la joue à réception
	if (IsAutomatic() || !IsLocallyPredicted())
	{
		NotifyActivated();
	}
}

void UYSAbilityComponent::NotifyActivated()
{
	if (const UWorld* World = GetWorld())
	{
		LastActivationTime = World->GetTimeSeconds();
	}
}

bool UYSAbilityComponent::IsLocallyPredicted() const
{
	const AActor* Owner = GetOwner();
	const APawn* Shooter = GetHolderPawn();
	return Owner && !Owner->HasAuthority() && Shooter && Shooter->IsLocallyControlled();
}

APawn* UYSAbilityComponent::GetHolderPawn() const
{
	AActor* Owner = GetOwner();
	APawn* Holder = Cast<APawn>(Owner);
	if (!Holder && Owner)
	{
		Holder = Cast<APawn>(Owner->GetOwner());
	}
	return Holder;
}

AActor* UYSAbilityComponent::FindTargetInCone(const UWorld* World, const FVector& Origin, const FVector& Direction, float MaxDistance, float MaxAngle, const FGameplayTagContainer& Tags, const AActor* IgnoredActor)
{
	const FVector FlatDirection = FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal();
	if (!World || MaxDistance <= 0.0f || MaxAngle <= 0.0f || FlatDirection.IsNearlyZero() || Tags.IsEmpty())
	{
		return nullptr;
	}

	const float MinCos = FMath::Cos(FMath::DegreesToRadians(MaxAngle));
	AActor* BestTarget = nullptr;
	float BestDistanceSq = FMath::Square(MaxDistance);

	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Candidate = *It;
		const IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Candidate);
		if (Candidate == IgnoredActor || !TagInterface || !TagInterface->HasAnyMatchingGameplayTags(Tags))
		{
			continue;
		}

		const FVector ToCandidate = FVector(Candidate->GetActorLocation() - Origin) * FVector(1.0f, 1.0f, 0.0f);
		const float DistanceSq = ToCandidate.SizeSquared();
		if (DistanceSq > BestDistanceSq || DistanceSq < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		if (FVector::DotProduct(ToCandidate.GetSafeNormal(), FlatDirection) >= MinCos)
		{
			BestTarget = Candidate;
			BestDistanceSq = DistanceSq;
		}
	}
	return BestTarget;
}

float UYSAbilityComponent::GetCooldownRemaining() const
{
	return FMath::Max(0.0f, static_cast<float>(ReadyServerTime - GetServerTime()));
}

void UYSAbilityComponent::ServerActivate_Implementation(FVector_NetQuantize TargetLocation)
{
	ActivateOnServer(TargetLocation);
}

void UYSAbilityComponent::ActivateOnServer(const FVector& TargetLocation)
{
	const double Now = GetServerTime();
	if (Now + CooldownTolerance < ReadyServerTime)
	{
		return;
	}
	if (!CanStartByPriority())
	{
		return;
	}
	CancelLowerPriorityActions();

	ReadyServerTime = Now + GetActivationLockDuration();
	UE_LOG(LogTemp, Verbose, TEXT("[YSAbility] %s.%s activée à %.2f, prête à %.2f"), *GetOwner()->GetName(), *GetName(), Now, ReadyServerTime);
	ExecuteAbility(TargetLocation);
	MulticastActivated();
}

double UYSAbilityComponent::GetServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}

	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

float UYSAbilityComponent::GetDamageMultiplier() const
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	return Character ? Character->GetWeaponDamageMultiplier(Cast<AYSWeapon>(GetOwner()), GetActionSlot()) : 1.0f;
}

float UYSAbilityComponent::GetCooldownMultiplier() const
{
	// l'Auto atk des Guns n'a pas de CD reduction (elle a l'upgrade Shoot system) ; l'Ability des Guns si
	if (WeaponType == EYSWeaponType::Gun && IsAutomatic())
	{
		return 1.0f;
	}
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	return Character ? Character->GetWeaponCooldownMultiplier(Cast<AYSWeapon>(GetOwner()), GetActionSlot()) : 1.0f;
}

float UYSAbilityComponent::GetWeaponStat(FName Stat, float Default) const
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	return Character ? Character->GetWeaponStat(Cast<AYSWeapon>(GetOwner()), Stat, Default) : Default;
}

bool UYSAbilityComponent::IsAkimboMainHand() const
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	return Character && Character->IsAkimbo() && Character->GetMainHandWeapon() == GetOwner();
}

float UYSAbilityComponent::GetActivationLockDuration() const
{
	return GetCooldownDuration() + (IsAkimboMainHand() ? AkimboFireDelay : 0.0f);
}

bool UYSAbilityComponent::IsActionInProgress() const
{
	const float Elapsed = GetTimeSinceActivation();
	const float Duration = GetActionDuration() + (IsAkimboMainHand() ? AkimboFireDelay : 0.0f);
	return Elapsed >= 0.0f && Elapsed < Duration;
}

int32 UYSAbilityComponent::GetPriorityRank() const
{
	const int32 Rank = UE_ARRAY_COUNT(ActionPriority);
	for (int32 Index = 0; Index < Rank; ++Index)
	{
		if (ActionPriority[Index] == GetActionSlot())
		{
			return Index;
		}
	}
	// type absent de la liste : après tous les autres
	return Rank;
}

void UYSAbilityComponent::GetPriorityGroup(TArray<UYSAbilityComponent*>& OutActions) const
{
	const AYSWeapon* Weapon = Cast<AYSWeapon>(GetOwner());
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	const AYSWeapon* Partner = Character && Character->IsAkimbo()
		? (Character->GetMainHandWeapon() == Weapon ? Character->GetOffHandWeapon() : Character->GetMainHandWeapon())
		: nullptr;

	for (const AActor* Actor : { static_cast<const AActor*>(Weapon ? Weapon : GetOwner()), static_cast<const AActor*>(Partner) })
	{
		if (Actor)
		{
			TInlineComponentArray<UYSAbilityComponent*> Actions(Actor);
			OutActions.Append(Actions);
		}
	}
}

bool UYSAbilityComponent::CanStartByPriority() const
{
	// objet porté à deux mains (coffre) : aucune action d'arme
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (Character && Character->IsCarrying())
	{
		return false;
	}

	const int32 Rank = GetPriorityRank();
	TArray<UYSAbilityComponent*> Group;
	GetPriorityGroup(Group);
	for (const UYSAbilityComponent* Other : Group)
	{
		if (Other != this && Other->GetPriorityRank() < Rank && Other->IsActionInProgress())
		{
			return false;
		}
	}
	return true;
}

void UYSAbilityComponent::CancelLowerPriorityActions()
{
	const int32 Rank = GetPriorityRank();
	TArray<UYSAbilityComponent*> Group;
	GetPriorityGroup(Group);
	for (UYSAbilityComponent* Other : Group)
	{
		if (Other != this && Other->GetPriorityRank() > Rank)
		{
			Other->CancelAction();
		}
	}
}

void UYSAbilityComponent::CancelAction()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AkimboTimer);
	}
	OnActionCancelled();
	if (!IsActionInProgress())
	{
		return;
	}

	// recharge rendue : l'action reprendra dès que l'action prioritaire sera finie
	UE_LOG(LogTemp, Verbose, TEXT("[YSAbility] %s.%s interrompue à %.2f"), *GetOwner()->GetName(), *GetName(), GetServerTime());
	ReadyServerTime = FMath::Min(ReadyServerTime, GetServerTime());
	MulticastCancelled();
}

void UYSAbilityComponent::MulticastCancelled_Implementation()
{
	LastActivationTime = -1.0e6;
}
