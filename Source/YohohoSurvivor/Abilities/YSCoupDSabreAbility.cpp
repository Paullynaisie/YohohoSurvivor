#include "Abilities/YSCoupDSabreAbility.h"
#include "Components/BoxComponent.h"
#include "Core/YSGameplayTags.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagAssetInterface.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "TimerManager.h"
#include "Weapons/YSWeapon.h"

namespace
{
	/** Marge après la coupe pendant laquelle la hitbox reste active (évite de rater la dernière image) */
	constexpr float HitWindowMargin = 0.05f;
}

UYSCoupDSabreAbility::UYSCoupDSabreAbility()
{
	PrimaryComponentTick.bCanEverTick = true;

	DisplayName = NSLOCTEXT("YS", "CoupDSabre", "Coup d'sabre");
	Cooldown = AutoAtkCooldown;
	IconTint = FLinearColor(0.75f, 0.75f, 0.8f);
	HitTags.AddTag(YSTags::Team_Foe);
}

void UYSCoupDSabreAbility::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UYSCoupDSabreAbility, Damage);
	DOREPLIFETIME(UYSCoupDSabreAbility, AutoAtkCooldown);
}

void UYSCoupDSabreAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// logique de combat : serveur uniquement (les clients ne font que jouer l'animation)
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const float Elapsed = GetTimeSinceActivation();
	if (Elapsed >= WindupDuration && Elapsed <= WindupDuration + SwingDuration + HitWindowMargin)
	{
		ApplySwingHits();
	}

	// en akimbo, la main gauche est déclenchée par la droite
	if (IsOffHandOfAkimbo() || !IsReady() || !FindAutoTarget())
	{
		return;
	}

	StartAttack();

	if (UYSCoupDSabreAbility* Partner = GetAkimboPartner())
	{
		const AYSWeapon* Weapon = Cast<AYSWeapon>(GetOwner());
		TWeakObjectPtr<UYSCoupDSabreAbility> WeakPartner = Partner;
		GetWorld()->GetTimerManager().SetTimer(AkimboTimer, FTimerDelegate::CreateWeakLambda(this, [WeakPartner]()
		{
			if (WeakPartner.IsValid())
			{
				WeakPartner->StartAttack();
			}
		}), FMath::Max(Weapon ? Weapon->GetAkimboFireDelay() : 0.0f, KINDA_SMALL_NUMBER), false);
	}
}

AActor* UYSCoupDSabreAbility::FindAutoTarget() const
{
	// la direction du curseur est celle du regard du personnage
	const APawn* Holder = GetHolderPawn();
	if (!Holder)
	{
		return nullptr;
	}
	return FindTargetInCone(GetWorld(), Holder->GetActorLocation(), Holder->GetActorForwardVector(), DetectionDistance, DetectionAngle, HitTags, Holder);
}

void UYSCoupDSabreAbility::StartAttack()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsReady())
	{
		return;
	}

	ReadyServerTime = GetServerTime() + GetCooldownDuration();
	HitThisSwing.Reset();
	MulticastActivated();
}

bool UYSCoupDSabreAbility::EvaluateHandPose(float& OutIdleWeight, FVector& OutLocation, FQuat& OutRotation) const
{
	const float Elapsed = GetTimeSinceActivation();
	if (Elapsed < 0.0f || Elapsed >= GetAttackDuration())
	{
		return false;
	}

	const FQuat WindupQuat = WindupHandRotation.Quaternion();
	const FQuat SwingEndQuat = SwingEndHandRotation.Quaternion();

	if (Elapsed < WindupDuration)
	{
		// préparation : de la pose normale vers le flanc
		const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, Elapsed / WindupDuration, WindupEaseExponent);
		OutIdleWeight = 1.0f - Alpha;
		OutLocation = WindupHandLocation;
		OutRotation = WindupQuat;
	}
	else if (Elapsed < WindupDuration + SwingDuration)
	{
		// coupe : départ immédiat, très rapide
		const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, (Elapsed - WindupDuration) / SwingDuration, SwingEaseExponent);
		OutIdleWeight = 0.0f;

		if (bCircularSwing)
		{
			// arc autour du corps : angle et rayon (plan horizontal) interpolés, hauteur linéaire
			const float StartAngle = FMath::Atan2(WindupHandLocation.Y, WindupHandLocation.X);
			const float EndAngle = FMath::Atan2(SwingEndHandLocation.Y, SwingEndHandLocation.X);
			const float Angle = StartAngle + FMath::FindDeltaAngleRadians(StartAngle, EndAngle) * Alpha;
			const float Radius = FMath::Lerp(FVector2D(WindupHandLocation).Size(), FVector2D(SwingEndHandLocation).Size(), Alpha);
			OutLocation = FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, FMath::Lerp(WindupHandLocation.Z, SwingEndHandLocation.Z, Alpha));
		}
		else
		{
			OutLocation = FMath::Lerp(WindupHandLocation, SwingEndHandLocation, Alpha);
		}

		// valeur par valeur (pas le plus court chemin) pour respecter les tours complets demandés
		const FRotator Rotation(
			FMath::Lerp(WindupHandRotation.Pitch, SwingEndHandRotation.Pitch, Alpha),
			FMath::Lerp(WindupHandRotation.Yaw, SwingEndHandRotation.Yaw, Alpha),
			FMath::Lerp(WindupHandRotation.Roll, SwingEndHandRotation.Roll, Alpha));
		OutRotation = Rotation.Quaternion();
	}
	else
	{
		// retour : de la fin de coupe vers la pose normale
		OutIdleWeight = FMath::InterpEaseInOut(0.0f, 1.0f, (Elapsed - WindupDuration - SwingDuration) / RecoverDuration, RecoverEaseExponent);
		OutLocation = SwingEndHandLocation;
		OutRotation = SwingEndQuat;
	}
	return true;
}

void UYSCoupDSabreAbility::ApplySwingHits()
{
	const AYSWeapon* Weapon = Cast<AYSWeapon>(GetOwner());
	const UBoxComponent* Hitbox = Weapon ? Weapon->FindComponentByClass<UBoxComponent>() : nullptr;
	APawn* Holder = GetHolderPawn();
	if (!Hitbox || !GetWorld())
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CoupDSabre), false, Holder);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Hitbox->GetComponentLocation(), Hitbox->GetComponentQuat(),
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeBox(Hitbox->GetScaledBoxExtent()), Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		const IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Target);
		if (!Target || HitThisSwing.Contains(Target) || !TagInterface || !TagInterface->HasAnyMatchingGameplayTags(HitTags))
		{
			continue;
		}

		HitThisSwing.Add(Target);
		// pas de DamageCauser : le chiffre de dégâts s'affiche sur la cible
		Target->TakeDamage(Damage * GetDamageMultiplier(), FDamageEvent(), Holder ? Holder->GetController() : nullptr, nullptr);
	}
}

bool UYSCoupDSabreAbility::IsOffHandOfAkimbo() const
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	const AYSWeapon* LeftWeapon = Character ? Character->GetLeftHandWeapon() : nullptr;
	const AYSWeapon* RightWeapon = Character ? Character->GetRightHandWeapon() : nullptr;
	return LeftWeapon && RightWeapon && LeftWeapon == GetOwner() && RightWeapon->GetClass() == LeftWeapon->GetClass();
}

UYSCoupDSabreAbility* UYSCoupDSabreAbility::GetAkimboPartner() const
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	const AYSWeapon* LeftWeapon = Character ? Character->GetLeftHandWeapon() : nullptr;
	const AYSWeapon* RightWeapon = Character ? Character->GetRightHandWeapon() : nullptr;
	if (!LeftWeapon || !RightWeapon || RightWeapon != GetOwner() || RightWeapon->GetClass() != LeftWeapon->GetClass())
	{
		return nullptr;
	}
	return Cast<UYSCoupDSabreAbility>(LeftWeapon->GetAutoAction());
}
