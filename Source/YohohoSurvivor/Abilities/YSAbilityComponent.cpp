#include "Abilities/YSAbilityComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

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

	// prédiction locale de la recharge pour une UI réactive, le serveur fait foi
	ReadyServerTime = GetServerTime() + Cooldown;
	ServerActivate(TargetLocation);
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

	ReadyServerTime = Now + Cooldown;
	ExecuteAbility(TargetLocation);
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
