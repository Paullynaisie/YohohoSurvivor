#include "Abilities/YSAbilityComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "GameplayTagAssetInterface.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "Weapons/YSWeapon.h"

namespace
{
	/** Marge acceptée côté serveur : l'horloge du client n'est synchronisée qu'approximativement */
	constexpr double CooldownTolerance = 0.2;
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
	if (!IsReady())
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
	ReadyServerTime = GetServerTime() + GetCooldownDuration();
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

	ReadyServerTime = Now + GetCooldownDuration();
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
	return Character ? Character->GetWeaponDamageMultiplier(Cast<AYSWeapon>(GetOwner())) : 1.0f;
}
