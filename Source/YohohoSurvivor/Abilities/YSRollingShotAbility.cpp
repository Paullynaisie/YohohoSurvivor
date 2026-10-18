#include "Abilities/YSRollingShotAbility.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/YSPlayerCharacter.h"
#include "TimerManager.h"
#include "Weapons/YSWeapon.h"

namespace YSRollingShot
{
	/** Distance du point visé devant le personnage au moment du tir */
	constexpr float AimDistance = 1000.0f;
}

UYSRollingShotAbility::UYSRollingShotAbility()
{
	DisplayName = NSLOCTEXT("YS", "RollingShot", "Rolling shot");
}

void UYSRollingShotAbility::OnActivationPredicted(const FVector& TargetLocation)
{
	StartRoll();
}

void UYSRollingShotAbility::ExecuteAbility(const FVector& TargetLocation)
{
	StartRoll();
	MulticastRollStarted();
	UE_LOG(LogTemp, Verbose, TEXT("[YSRollingShot] %s roulade à %.2f depuis (%.0f, %.0f)"), *GetOwner()->GetName(), GetWorld()->GetTimeSeconds(), GetHolderPawn() ? GetHolderPawn()->GetActorLocation().X : 0.0f, GetHolderPawn() ? GetHolderPawn()->GetActorLocation().Y : 0.0f);

	// visée puis tir
	GetWorld()->GetTimerManager().SetTimer(ShootTimer, this, &UYSRollingShotAbility::Shoot, FMath::Max(RollDuration + AimDuration, KINDA_SMALL_NUMBER), false);
}

void UYSRollingShotAbility::StartRoll()
{
	ACharacter* Character = Cast<ACharacter>(GetHolderPawn());
	if (!Character)
	{
		return;
	}
	LastRollTime = GetWorld()->GetTimeSeconds();

	// vitesse x RollSpeedMultiplier tout de suite (dans la direction du mouvement) ; elle retombe ensuite avec MaxWalkSpeed
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	// direction : celle de l'input, sinon à l'opposé du curseur (= du regard)
	const FVector Direction = Movement->GetCurrentAcceleration().IsNearlyZero() ? -Character->GetActorForwardVector().GetSafeNormal2D() : Movement->GetCurrentAcceleration().GetSafeNormal2D();
	ForcedRollDirection = -Character->GetActorForwardVector().GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		const float RollSpeed = Movement->MaxWalkSpeed * RollSpeedMultiplier;
		Movement->Velocity = Direction * RollSpeed + FVector(0.0f, 0.0f, Movement->Velocity.Z);
		Movement->MaxWalkSpeed = RollSpeed;
	}

	// esquive : traverse les NME (et les autres joueurs) pendant la roulade, puis collisions normales
	if (bRollThroughFoes)
	{
		UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		TWeakObjectPtr<UCapsuleComponent> WeakCapsule = Capsule;
		GetWorld()->GetTimerManager().SetTimer(RollCollisionTimer, FTimerDelegate::CreateWeakLambda(this, [WeakCapsule]()
		{
			if (WeakCapsule.IsValid())
			{
				WeakCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			}
		}), FMath::Max(RollDuration, KINDA_SMALL_NUMBER), false);
	}
}

void UYSRollingShotAbility::MulticastRollStarted_Implementation()
{
	// animation : le serveur et le client qui a prédit la roulade l'ont déjà notée
	if (!GetOwner()->HasAuthority() && !IsLocallyPredicted())
	{
		LastRollTime = GetWorld()->GetTimeSeconds();
	}
}

void UYSRollingShotAbility::Shoot()
{
	if (const APawn* Holder = GetHolderPawn())
	{
		UE_LOG(LogTemp, Verbose, TEXT("[YSRollingShot] %s tir à %.2f depuis (%.0f, %.0f)"), *GetOwner()->GetName(), GetWorld()->GetTimeSeconds(), Holder->GetActorLocation().X, Holder->GetActorLocation().Y);
	}
	StartBurst(GetBurstTargetLocation(0));

	// akimbo : l'autre pistolet tire après le délai akimbo (une seule roulade)
	if (UYSRollingShotAbility* Partner = GetAkimboPartner())
	{
		TWeakObjectPtr<UYSRollingShotAbility> WeakPartner = Partner;
		GetWorld()->GetTimerManager().SetTimer(PartnerTimer, FTimerDelegate::CreateWeakLambda(this, [WeakPartner]()
		{
			if (WeakPartner.IsValid())
			{
				WeakPartner->StartBurst(WeakPartner->GetBurstTargetLocation(0));
			}
		}), FMath::Max(AkimboFireDelay, KINDA_SMALL_NUMBER), false);
	}
}

FVector UYSRollingShotAbility::GetBurstTargetLocation(int32 BulletIndex) const
{
	// le personnage regarde le curseur : on tire droit devant (la visée assistée prend un NME proche de cette direction)
	const APawn* Holder = GetHolderPawn();
	return Holder ? Holder->GetActorLocation() + Holder->GetActorForwardVector() * YSRollingShot::AimDistance : Super::GetBurstTargetLocation(BulletIndex);
}

void UYSRollingShotAbility::OnActionCancelled()
{
	Super::OnActionCancelled();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShootTimer);
		World->GetTimerManager().ClearTimer(PartnerTimer);
	}
}

UYSRollingShotAbility* UYSRollingShotAbility::GetAkimboPartner() const
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (!Character || !Character->IsAkimbo() || Character->GetMainHandWeapon() != GetOwner())
	{
		return nullptr;
	}
	const AYSWeapon* OffWeapon = Character->GetOffHandWeapon();
	return OffWeapon ? Cast<UYSRollingShotAbility>(OffWeapon->GetActiveAction()) : nullptr;
}

float UYSRollingShotAbility::GetTimeSinceRoll() const
{
	const UWorld* World = GetWorld();
	return World ? static_cast<float>(World->GetTimeSeconds() - LastRollTime) : TNumericLimits<float>::Max();
}

bool UYSRollingShotAbility::IsGrantingInvulnerability() const
{
	const float Elapsed = GetTimeSinceRoll();
	return Elapsed >= 0.0f && Elapsed < RollDuration + ExtraInvulnerability;
}

float UYSRollingShotAbility::GetMoveSpeedMultiplier() const
{
	const float Elapsed = GetTimeSinceRoll();
	// roulade : bonus de vitesse qui retombe jusqu'à la vitesse de la visée
	if (Elapsed >= 0.0f && Elapsed < RollDuration)
	{
		const float Decay = FMath::InterpEaseOut(0.0f, 1.0f, Elapsed / RollDuration, RollSpeedDecayExponent);
		return FMath::Lerp(RollSpeedMultiplier, AimMoveSpeedMultiplier, Decay);
	}
	return Elapsed >= RollDuration && Elapsed < RollDuration + AimDuration ? AimMoveSpeedMultiplier : 1.0f;
}

bool UYSRollingShotAbility::GetBodyRoll(float& OutAngle, float& OutHeightOffset) const
{
	const float Duration = RollAnimDuration > 0.0f ? RollAnimDuration : RollDuration;
	const float Elapsed = GetTimeSinceRoll();
	if (Elapsed < 0.0f || Elapsed >= Duration)
	{
		return false;
	}

	const float Alpha = Elapsed / Duration;
	OutAngle = 360.0f * RollAnimTurns * FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, RollAnimEaseExponent);
	OutHeightOffset = -RollAnimTuckHeight * FMath::Sin(PI * Alpha);
	return true;
}

bool UYSRollingShotAbility::GetForcedMoveDirection(FVector& OutDirection) const
{
	const float Elapsed = GetTimeSinceRoll();
	if (!bForceMoveWithoutInput || Elapsed < 0.0f || Elapsed >= RollDuration || ForcedRollDirection.IsNearlyZero())
	{
		return false;
	}
	OutDirection = ForcedRollDirection;
	return true;
}
