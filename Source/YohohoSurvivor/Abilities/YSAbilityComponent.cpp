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

	/** Rangs de priorité des actions : action forcée (« Allez-y mes minions ! »), puis type d'action */
	enum class EYSActionPriority : uint8
	{
		Forced,
		Ability,
		AutoAtk,
	};

	/** Priorités des actions d'une arme, de la plus prioritaire à la moins prioritaire (liste donnée par Paul) */
	constexpr EYSActionPriority ActionPriority[] = { EYSActionPriority::Forced, EYSActionPriority::Ability, EYSActionPriority::AutoAtk };

	/** Délai pour utiliser une activation forcée accordée (aller-retour réseau compris) */
	constexpr double ForcedGrantLifetime = 2.0;
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
	if (!IsReady() || IsForcedActionInProgress() || !CanStartByPriority() || !CanActivateNow())
	{
		return;
	}

	PrepareActivation(TargetLocation);

	if (GetOwner()->HasAuthority())
	{
		// joueur hôte (listen server) : exécution directe
		ActivateOnServer(TargetLocation);
		return;
	}

	// prédiction locale de la recharge et des effets pour un retour immédiat, le serveur fait foi
	ReadyServerTime = GetServerTime() + GetActivationLockDuration();
	bLastActivationForced = false;
	NotifyActivated();
	OnActivationPredicted(TargetLocation);
	ServerActivate(TargetLocation);
}

float UYSAbilityComponent::GetTimeSinceActivation() const
{
	const UWorld* World = GetWorld();
	return World ? static_cast<float>(World->GetTimeSeconds() - LastActivationTime) : TNumericLimits<float>::Max();
}

void UYSAbilityComponent::MulticastActivated_Implementation()
{
	bLastActivationForced = false;
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

bool UYSAbilityComponent::IsAutoAttackAllowedByHolder() const
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	return !Character || Character->WantsAutoAttack();
}

bool UYSAbilityComponent::IsAutoAttackTargetOptional() const
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	return Character && Character->UsesHeldAutoAttack() && Character->WantsAutoAttack();
}

AActor* UYSAbilityComponent::FindTargetInCone(const UWorld* World, const FVector& Origin, const FVector& Direction, float MaxDistance, float MaxAngle, const FGameplayTagContainer& Tags, const AActor* IgnoredActor,
	float AngleTieTolerance)
{
	const FVector FlatDirection = FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal();
	if (!World || MaxDistance <= 0.0f || MaxAngle <= 0.0f || FlatDirection.IsNearlyZero() || Tags.IsEmpty())
	{
		return nullptr;
	}

	const float MaxDistanceSq = FMath::Square(MaxDistance);
	AActor* BestTarget = nullptr;
	float BestAngle = 0.0f;
	float BestDistanceSq = 0.0f;

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
		if (DistanceSq > MaxDistanceSq || DistanceSq < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ToCandidate.GetSafeNormal(), FlatDirection), -1.0f, 1.0f)));
		if (Angle > MaxAngle)
		{
			continue;
		}
		// le plus au centre du cône ; à angles égaux (à la tolérance près), le plus proche
		const bool bTie = BestTarget && FMath::Abs(Angle - BestAngle) <= AngleTieTolerance;
		if (!BestTarget || (bTie ? DistanceSq < BestDistanceSq : Angle < BestAngle))
		{
			BestTarget = Candidate;
			BestAngle = Angle;
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
	if (!ActivateOnServer(TargetLocation))
	{
		ClientActivationRejected(ReadyServerTime);
	}
}

bool UYSAbilityComponent::ActivateOnServer(const FVector& TargetLocation)
{
	const double Now = GetServerTime();
	if (Now + CooldownTolerance < ReadyServerTime)
	{
		return false;
	}
	if (IsForcedActionInProgress() || !CanStartByPriority() || !CanActivateNow())
	{
		return false;
	}
	CancelLowerPriorityActions();

	ReadyServerTime = Now + GetActivationLockDuration();
	bLastActivationForced = false;
	UE_LOG(LogTemp, Verbose, TEXT("[YSAbility] %s.%s activée à %.2f, prête à %.2f"), *GetOwner()->GetName(), *GetName(), Now, ReadyServerTime);
	ExecuteAbility(TargetLocation);
	ScheduleAutoAttackReset();
	MulticastActivated();
	return true;
}

void UYSAbilityComponent::ScheduleAutoAttackReset()
{
	if (!bResetAutoAttackOnUse || !GetWorld())
	{
		return;
	}

	auto ResetAutoAttacks = [this]()
	{
		const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
		for (const AYSWeapon* Weapon : Character ? Character->GetHeldWeapons() : TArray<AYSWeapon*>())
		{
			if (UYSAbilityComponent* AutoAction = Weapon ? Weapon->GetAutoAction() : nullptr)
			{
				AutoAction->ResetCooldown();
			}
		}
	};
	if (AutoAttackResetDelay <= 0.0f)
	{
		ResetAutoAttacks();
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(AutoAttackResetTimer, FTimerDelegate::CreateWeakLambda(this, ResetAutoAttacks), AutoAttackResetDelay, false);
}

float UYSAbilityComponent::GetExplosiveRadiusMultiplier() const
{
	EYSShootSystem System;
	if (!GetShootSystem(System) || System != EYSShootSystem::Explosive)
	{
		return 1.0f;
	}
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	return 1.0f + (Character ? FMath::Max(0.0f, Character->GetWeaponShootSystemBonus(Cast<AYSWeapon>(GetOwner()), GetActionSlot())) : 0.0f);
}

void UYSAbilityComponent::ClientActivationRejected_Implementation(double ServerReadyTime)
{
	UE_LOG(LogTemp, Log, TEXT("[YSAbility] %s.%s refusée par le serveur : prête à %.2f (prédiction %.2f)"), *GetOwner()->GetName(), *GetName(), ServerReadyTime, ReadyServerTime);
	ReadyServerTime = ServerReadyTime;
	bLastActivationForced = false;
	StopActionLocally();
}

void UYSAbilityComponent::GrantForcedActivation(const FYSForcedActivation& Forced)
{
	bForcedGranted = true;
	PendingForced = Forced;
	ForcedGrantTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UYSAbilityComponent::TryForcedActivate(const FVector& TargetLocation)
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (!bForcedGranted || !CanActivateNow() || (Character && (Character->AreWeaponsStowed() || Character->IsRising() || Character->IsDying())))
	{
		bForcedGranted = false;
		return;
	}

	PrepareActivation(TargetLocation);

	if (GetOwner()->HasAuthority())
	{
		ActivateForcedOnServer(TargetLocation);
		return;
	}

	// prédiction : effets immédiats ; la recharge ne change pas (rendue en Golden), le serveur fait foi
	bForcedGranted = false;
	if (PendingForced.bResetCooldown)
	{
		ReadyServerTime = FMath::Min(ReadyServerTime, GetServerTime());
	}
	bLastActivationForced = true;
	NotifyActivated();
	OnActivationPredicted(TargetLocation);
	ServerForcedActivate(TargetLocation);
}

void UYSAbilityComponent::ServerForcedActivate_Implementation(FVector_NetQuantize TargetLocation)
{
	if (!ActivateForcedOnServer(TargetLocation))
	{
		ClientActivationRejected(ReadyServerTime);
	}
}

bool UYSAbilityComponent::ActivateForcedOnServer(const FVector& TargetLocation)
{
	// accord du serveur obligatoire (une seule fois, pendant un court délai)
	const bool bGranted = bForcedGranted && GetWorld() && GetWorld()->GetTimeSeconds() - ForcedGrantTime <= ForcedGrantLifetime;
	bForcedGranted = false;
	if (!bGranted || !CanActivateNow())
	{
		return false;
	}

	// priorité Forced : l'action en cours de cette capacité s'arrête (sans effacer son animation chez le client qui a prédit),
	// toutes les autres actions du groupe sont interrompues
	OnActionCancelled();
	TArray<UYSAbilityComponent*> Group;
	GetPriorityGroup(Group);
	for (UYSAbilityComponent* Other : Group)
	{
		if (Other != this)
		{
			Other->CancelAction();
		}
	}

	// la recharge n'est pas déclenchée ; Golden : elle est rendue
	const double Now = GetServerTime();
	const double ReadyTime = PendingForced.bResetCooldown ? FMath::Min(ReadyServerTime, Now) : ReadyServerTime;
	bLastActivationForced = true;
	ForcedDamageMultiplier = PendingForced.DamageMultiplier;
	UE_LOG(LogTemp, Verbose, TEXT("[YSAbility] %s.%s FORCÉE à %.2f (dégâts x %.2f%s), prête à %.2f"), *GetOwner()->GetName(), *GetName(), Now,
		ForcedDamageMultiplier, PendingForced.bResetCooldown ? TEXT(", recharge rendue") : TEXT(""), ReadyTime);
	ExecuteAbility(TargetLocation);
	ScheduleAutoAttackReset();
	ReadyServerTime = ReadyTime;
	MulticastForcedActivated();
	return true;
}

void UYSAbilityComponent::MulticastForcedActivated_Implementation()
{
	bLastActivationForced = true;
	if (!IsLocallyPredicted())
	{
		NotifyActivated();
	}
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
	// action forcée : You're all my minions ++
	const float ForcedMultiplier = bLastActivationForced ? ForcedDamageMultiplier : 1.0f;
	return (Character ? Character->GetWeaponDamageMultiplier(Cast<AYSWeapon>(GetOwner()), GetActionSlot()) : 1.0f) * ForcedMultiplier;
}

float UYSAbilityComponent::GetCooldownMultiplier() const
{
	// Atk speed (shards) : aussi l'Auto atk des Guns, sur la seule recharge (Cooldown), pas la durée de la rafale
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

bool UYSAbilityComponent::IsBlockingLowerPriority() const
{
	// action forcée : bloque pendant toute sa durée (le clic ne la relance pas tant qu'elle est en cours)
	if (IsForcedActionInProgress())
	{
		return true;
	}
	const float Elapsed = GetTimeSinceActivation();
	const float Duration = GetPriorityBlockDuration() + (IsAkimboMainHand() ? AkimboFireDelay : 0.0f);
	return Elapsed >= 0.0f && Elapsed < Duration;
}

int32 UYSAbilityComponent::GetPriorityRank() const
{
	const EYSActionPriority Priority = IsForcedActionInProgress() ? EYSActionPriority::Forced
		: GetActionSlot() == EYSActionSlot::Ability ? EYSActionPriority::Ability : EYSActionPriority::AutoAtk;
	const int32 Rank = UE_ARRAY_COUNT(ActionPriority);
	for (int32 Index = 0; Index < Rank; ++Index)
	{
		if (ActionPriority[Index] == Priority)
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

	// autre arme (pas akimbo) : dans le groupe si une de ses actions ou celle-ci bloque toutes les armes (ex. Rolling shot)
	for (const AYSWeapon* Other : Character ? Character->GetHeldWeapons() : TArray<AYSWeapon*>())
	{
		if (Other == Weapon || Other == Partner)
		{
			continue;
		}
		TInlineComponentArray<UYSAbilityComponent*> Actions(Other);
		for (UYSAbilityComponent* Action : Actions)
		{
			if (BlocksAllWeapons() || Action->BlocksAllWeapons())
			{
				OutActions.Add(Action);
			}
		}
	}
}

bool UYSAbilityComponent::CanStartByPriority() const
{
	// objet porté à deux mains (coffre), nage, sortie du sol, mort : aucune action d'arme
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (Character && (Character->AreWeaponsStowed() || Character->IsRising() || Character->IsDying()))
	{
		return false;
	}

	const int32 Rank = GetPriorityRank();
	TArray<UYSAbilityComponent*> Group;
	GetPriorityGroup(Group);
	for (const UYSAbilityComponent* Other : Group)
	{
		if (Other != this && Other->GetPriorityRank() < Rank && Other->IsBlockingLowerPriority())
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

void UYSAbilityComponent::StopActionLocally()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AkimboTimer);
	}
	OnActionCancelled();
	LastActivationTime = -1.0e6;
}

bool UYSAbilityComponent::IsActivationBlocked() const
{
	return IsForcedActionInProgress() || !CanStartByPriority() || !CanActivateNow();
}

void UYSAbilityComponent::ResetCooldown()
{
	ReadyServerTime = FMath::Min(ReadyServerTime, GetServerTime());
	// souvent pendant la pause du level up : réplication forcée
	GetOwner()->ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("[YSAbility] %s.%s recharge rendue à %.2f"), *GetOwner()->GetName(), *GetName(), GetServerTime());
}

void UYSAbilityComponent::ReduceCooldown(float Seconds)
{
	const float Now = GetServerTime();
	if (Seconds <= 0.0f || ReadyServerTime <= Now)
	{
		return;
	}
	ReadyServerTime = FMath::Max(Now, ReadyServerTime - Seconds);
	GetOwner()->ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("[YSAbility] %s.%s recharge -%.2f s : prête à %.2f"), *GetOwner()->GetName(), *GetName(), Seconds, ReadyServerTime);
}

void UYSAbilityComponent::MulticastCancelled_Implementation()
{
	LastActivationTime = -1.0e6;
}

#if WITH_EDITOR
void UYSAbilityComponent::GetUnusedProperties(TArray<FName>& OutNames) const
{
	// toutes les actions ont leur icône dans l'UI (Ability et attaque auto) ; une attaque auto ne rend pas sa propre recharge
	if (IsAutomatic())
	{
		OutNames.Append({
			GET_MEMBER_NAME_CHECKED(UYSAbilityComponent, bResetAutoAttackOnUse),
			GET_MEMBER_NAME_CHECKED(UYSAbilityComponent, AutoAttackResetDelay),
		});
	}
}
#endif
