#include "Abilities/YSRollingShotAbility.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Player/YSPlayerCharacter.h"
#include "TimerManager.h"
#include "Weapons/YSWeapon.h"

namespace YSRollingShot
{
	/** Points de la courbe EaseOut de la roulade */
	constexpr int32 RollCurveKeys = 16;

	/** En dessous de cette vitesse, le personnage est considéré à l'arrêt (roulade à l'opposé du curseur) */
	constexpr float MovingSpeedThreshold = 10.0f;

	/** Distance du point visé devant le personnage au moment du tir */
	constexpr float AimDistance = 1000.0f;
}

UYSRollingShotAbility::UYSRollingShotAbility()
{
	DisplayName = NSLOCTEXT("YS", "RollingShot", "Rolling shot");
}

void UYSRollingShotAbility::BeginPlay()
{
	Super::BeginPlay();

	// temps (0..1) -> distance parcourue (0..1) : EaseOut, identique sur toutes les machines (prédiction de la roulade)
	RollCurve = NewObject<UCurveFloat>(this, NAME_None, RF_Transient);
	for (int32 Index = 0; Index <= YSRollingShot::RollCurveKeys; ++Index)
	{
		const float Time = static_cast<float>(Index) / YSRollingShot::RollCurveKeys;
		const FKeyHandle Key = RollCurve->FloatCurve.AddKey(Time, 1.0f - FMath::Pow(1.0f - Time, RollEaseExponent));
		RollCurve->FloatCurve.SetKeyInterpMode(Key, RCIM_Linear);
	}
}

void UYSRollingShotAbility::PrepareActivation(const FVector& TargetLocation)
{
	// direction du mouvement (input, sinon vitesse) ; à l'arrêt : à l'opposé du curseur (= du regard)
	const ACharacter* Character = Cast<ACharacter>(GetHolderPawn());
	if (!Character)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	FVector Direction = Movement->GetCurrentAcceleration().GetSafeNormal2D();
	if (Direction.IsNearlyZero() && Character->GetVelocity().Size2D() > YSRollingShot::MovingSpeedThreshold)
	{
		Direction = Character->GetVelocity().GetSafeNormal2D();
	}
	if (Direction.IsNearlyZero())
	{
		Direction = -Character->GetActorForwardVector().GetSafeNormal2D();
	}
	RollDirection = Direction;

	if (!GetOwner()->HasAuthority())
	{
		ServerSetRollDirection(Direction);
	}
}

void UYSRollingShotAbility::ServerSetRollDirection_Implementation(FVector_NetQuantizeNormal Direction)
{
	const FVector Flat = FVector(Direction).GetSafeNormal2D();
	if (!Flat.IsNearlyZero())
	{
		RollDirection = Flat;
	}
}

void UYSRollingShotAbility::OnActivationPredicted(const FVector& TargetLocation)
{
	StartRoll();
}

void UYSRollingShotAbility::ExecuteAbility(const FVector& TargetLocation)
{
	StartRoll();
	MulticastRollStarted();
	UE_LOG(LogTemp, Verbose, TEXT("[YSRollingShot] %s roulade vers (%.2f, %.2f) à %.2f depuis (%.0f, %.0f)"), *GetOwner()->GetName(), RollDirection.X, RollDirection.Y, GetWorld()->GetTimeSeconds(), GetHolderPawn() ? GetHolderPawn()->GetActorLocation().X : 0.0f, GetHolderPawn() ? GetHolderPawn()->GetActorLocation().Y : 0.0f);

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

	// root motion : remplace le déplacement pendant RollDuration, s'arrête net à la fin
	TSharedPtr<FRootMotionSource_MoveToDynamicForce> Roll = MakeShared<FRootMotionSource_MoveToDynamicForce>();
	Roll->InstanceName = TEXT("YSRollingShot");
	Roll->AccumulateMode = ERootMotionAccumulateMode::Override;
	Roll->Priority = 500;
	Roll->StartLocation = Character->GetActorLocation();
	Roll->InitialTargetLocation = Roll->StartLocation + RollDirection * RollDistance;
	Roll->TargetLocation = Roll->InitialTargetLocation;
	Roll->Duration = RollDuration;
	Roll->bRestrictSpeedToExpected = true;
	Roll->TimeMappingCurve = RollCurve;
	Roll->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Roll->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	// une roulade précédente encore enregistrée empêcherait la nouvelle
	Movement->RemoveRootMotionSource(Roll->InstanceName);
	Movement->ApplyRootMotionSource(Roll);

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
