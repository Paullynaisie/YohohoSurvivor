#include "Abilities/YSTourbilolAbility.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "Upgrades/YSUpgrade.h"
#include "Weapons/YSPlayerBomb.h"

namespace YSTourbilol
{
	/** Golden : temps en l'air minimum avant de chercher l'atterrissage, et maximum (impact quand même, ex. tombé dans la mer) */
	constexpr double MinAirTime = 0.15;
	constexpr double MaxAirTime = 4.0;
}

UYSTourbilolAbility::UYSTourbilolAbility()
{
	DisplayName = NSLOCTEXT("YS", "Tourbilol", "TOURBILOL !");
	Damage = 50.0f;
	Cooldown = 10.0f;
}

void UYSTourbilolAbility::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UYSTourbilolAbility, State);
}

bool UYSTourbilolAbility::IsSpinning() const
{
	const float Elapsed = GetSwingElapsed();
	return Elapsed >= 0.0f && Elapsed < GetSpinDuration();
}

void UYSTourbilolAbility::PrepareActivation(const FVector& TargetLocation)
{
	// départ dans l'orientation actuelle ; durée de base (le serveur ajoute les tours achetés)
	if (const APawn* Holder = GetHolderPawn())
	{
		StartYaw = Holder->GetActorRotation().Yaw;
	}
	State.TotalTurns = BaseTurns;
}

void UYSTourbilolAbility::OnActivationPredicted(const FVector& TargetLocation)
{
	CurrentTurn = -1;
	bWasSpinning = true;
}

void UYSTourbilolAbility::ExecuteAbility(const FVector& TargetLocation)
{
	ResetSwingHits();
	StartCriticalTracking(false, false);
	State.TotalTurns = BaseTurns;
	State.bGolden = GetWeaponStat(UYSInfiniteTourbillonUpgrade::GoldenStat, 0.0f) > 0.0f;
	++State.Counter;
	ExtensionCheckedTurns = 0;
	StaminaSpent = 0.0f;
	CurrentTurn = -1;
	bWasSpinning = true;
	// recharge calculée avant l'exécution avec l'état du tourbilol précédent (tours achetés) : recalculée
	ReadyServerTime = GetServerTime() + GetActivationLockDuration();
	GetOwner()->ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("[YSTourbilol] %s : TOURBILOL ! à %.2f, %d tours de %.2f s%s"), *GetOwner()->GetName(), GetWorld()->GetTimeSeconds(), State.TotalTurns, TurnDuration,
		State.bGolden ? TEXT(" (Golden)") : TEXT(""));
}

void UYSTourbilolAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// chaque NME peut être touché une fois par tour : nouveau tour = plus aucune cible touchée (avant les coups de cette image)
	const float Elapsed = GetSwingElapsed();
	if (IsSpinning())
	{
		const int32 Turn = FMath::FloorToInt(Elapsed / FMath::Max(TurnDuration, KINDA_SMALL_NUMBER));
		if (Turn != CurrentTurn)
		{
			CurrentTurn = Turn;
			ResetSwingHits();
		}
		if (HasHolderAuthority())
		{
			TryExtend();
		}
	}
	else
	{
		CurrentTurn = -1;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const bool bSpinning = IsSpinning();
	if (bWasSpinning && !bSpinning)
	{
		OnSpinEnded();
	}
	bWasSpinning = bSpinning;

	// Golden : impact à l'atterrissage (serveur)
	if (bGoldenAirborne && GetOwner()->HasAuthority())
	{
		const ACharacter* Character = Cast<ACharacter>(GetHolderPawn());
		const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
		const double AirTime = GetWorld()->GetTimeSeconds() - GoldenLaunchTime;
		if (!Movement || (AirTime >= YSTourbilol::MinAirTime && (Movement->IsMovingOnGround() || Movement->IsSwimming())) || AirTime >= YSTourbilol::MaxAirTime)
		{
			GoldenImpact();
		}
	}
}

void UYSTourbilolAbility::TryExtend()
{
	const int32 Turns = FMath::Max(State.TotalTurns, BaseTurns);
	// décision en entrant dans le dernier tour (le tour acheté arrive chez les clients avant la fin)
	if (ExtensionCheckedTurns >= Turns || GetSwingElapsed() < (Turns - 1) * TurnDuration)
	{
		return;
	}
	ExtensionCheckedTurns = Turns;

	AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (!Character || GetWeaponStat(UYSInfiniteTourbillonUpgrade::InfiniteStat, 0.0f) <= 0.0f || !Character->IsAbilityHeld())
	{
		return;
	}
	const float Cost = FMath::Max(MinInfiniteTurnStaminaCost, InfiniteTurnStaminaCost - GetWeaponStat(UYSInfiniteTourbillonUpgrade::CostReductionStat, 0.0f));
	if (Character->GetStamina() < Cost)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[YSTourbilol] %s : plus assez de stamina pour un tour de plus (%.1f < %.1f)"), *GetOwner()->GetName(), Character->GetStamina(), Cost);
		return;
	}
	Character->ConsumeStamina(Cost);
	StaminaSpent += Cost;
	State.TotalTurns = Turns + 1;
	ReadyServerTime += TurnDuration;
	GetOwner()->ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("[YSTourbilol] %s : tour de plus (%d) pour %.1f stamina, %.1f consommée"), *GetOwner()->GetName(), State.TotalTurns, Cost, StaminaSpent);
}

void UYSTourbilolAbility::OnSpinEnded()
{
	ACharacter* Character = Cast<ACharacter>(GetHolderPawn());
	UE_LOG(LogTemp, Verbose, TEXT("[YSTourbilol] %s : fin du tourbilol à %.2f (%d tours)"), *GetOwner()->GetName(), GetWorld()->GetTimeSeconds(), FMath::Max(State.TotalTurns, BaseTurns));
	if (!State.bGolden || !Character || (!Character->HasAuthority() && !Character->IsLocallyControlled()))
	{
		return;
	}

	// Golden : saut (serveur et client qui contrôle : même mouvement), impact à l'atterrissage (serveur)
	Character->LaunchCharacter(FVector(0.0f, 0.0f, GoldenJumpVelocity), false, true);
	if (Character->HasAuthority())
	{
		bGoldenAirborne = true;
		GoldenLaunchTime = GetWorld()->GetTimeSeconds();
		GoldenStaminaSpent = StaminaSpent;
		UE_LOG(LogTemp, Verbose, TEXT("[YSTourbilol] %s : saut Golden (%.1f stamina consommée)"), *GetOwner()->GetName(), GoldenStaminaSpent);
	}
}

void UYSTourbilolAbility::GoldenImpact()
{
	bGoldenAirborne = false;
	ACharacter* Character = Cast<ACharacter>(GetHolderPawn());
	const float ImpactDamage = GoldenStaminaSpent * GoldenDamagePerStamina * GetDamageMultiplier();
	if (!Character || !GoldenExplosionClass || ImpactDamage <= 0.0f)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[YSTourbilol] %s : atterrissage Golden sans impact (%.1f dégâts%s)"), *GetOwner()->GetName(), ImpactDamage, GoldenExplosionClass ? TEXT("") : TEXT(", pas de Golden Explosion Class"));
		return;
	}

	const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FTransform SpawnTransform(FRotator::ZeroRotator, Feet);
	if (AYSPlayerBomb* Bomb = GetWorld()->SpawnActorDeferred<AYSPlayerBomb>(GoldenExplosionClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Bomb->SetDamage(ImpactDamage);
		Bomb->SetInstant(false);
		Bomb->FinishSpawning(SpawnTransform);
		UE_LOG(LogTemp, Verbose, TEXT("[YSTourbilol] %s : impact Golden en (%.0f, %.0f, %.0f), %.1f dégâts (%.1f stamina)"), *GetOwner()->GetName(), Feet.X, Feet.Y, Feet.Z, ImpactDamage, GoldenStaminaSpent);
	}
}

bool UYSTourbilolAbility::GetLockedAimDirection(FVector& OutDirection) const
{
	if (!IsSpinning())
	{
		return false;
	}
	const float Turns = GetSwingElapsed() / FMath::Max(TurnDuration, KINDA_SMALL_NUMBER);
	const float Yaw = StartYaw + (bClockwise ? 360.0f : -360.0f) * Turns;
	OutDirection = FRotator(0.0f, Yaw, 0.0f).Vector();
	return true;
}

bool UYSTourbilolAbility::GetFixedMoveSpeedRatio(float& OutRatio) const
{
	if (!IsSpinning())
	{
		return false;
	}
	OutRatio = MoveSpeedRatio;
	return true;
}

bool UYSTourbilolAbility::EvaluateHandPose(float& OutIdleWeight, FVector& OutLocation, FQuat& OutRotation) const
{
	if (!bUseSpinHandPose || !IsSpinning())
	{
		return false;
	}
	const float Elapsed = GetSwingElapsed();
	const float Alpha = SpinHandBlendTime > 0.0f ? FMath::Clamp(FMath::Min(Elapsed, GetSpinDuration() - Elapsed) / SpinHandBlendTime, 0.0f, 1.0f) : 1.0f;
	OutIdleWeight = 1.0f - Alpha;
	OutLocation = SpinHandLocation;
	OutRotation = SpinHandRotation.Quaternion();
	return true;
}

#if WITH_EDITOR
void UYSTourbilolAbility::GetUnusedProperties(TArray<FName>& OutNames) const
{
	Super::GetUnusedProperties(OutNames);
	// pas d'animation de coupe ni de détection : le tourbilol remplace tout
	OutNames.Append({
		GET_MEMBER_NAME_CHECKED(UYSTourbilolAbility, AutoAtkCooldown),
		GET_MEMBER_NAME_CHECKED(UYSTourbilolAbility, AutoAtkMoveSpeedMultiplier),
		GET_MEMBER_NAME_CHECKED(UYSTourbilolAbility, DetectionDistance),
		GET_MEMBER_NAME_CHECKED(UYSTourbilolAbility, DetectionAngle),
		GET_MEMBER_NAME_CHECKED(UYSTourbilolAbility, bHitboxDuringWholeAnimation),
		GET_MEMBER_NAME_CHECKED(UYSTourbilolAbility, IdleRehitInterval),
		GET_MEMBER_NAME_CHECKED(UYSTourbilolAbility, WindupDuration),
		GET_MEMBER_NAME_CHECKED(UYSTourbilolAbility, SwingDuration),
		GET_MEMBER_NAME_CHECKED(UYSTourbilolAbility, RecoverDuration),
	});
}
#endif
