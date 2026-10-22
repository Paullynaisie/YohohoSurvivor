#include "Abilities/YSAutoSwingAbility.h"
#include "Components/BoxComponent.h"
#include "Core/YSDamageTypes.h"
#include "Core/YSGameplayTags.h"
#include "DrawDebugHelpers.h"
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

UYSAutoSwingAbility::UYSAutoSwingAbility()
{
	PrimaryComponentTick.bCanEverTick = true;

	DisplayName = NSLOCTEXT("YS", "AutoSwing", "Auto swing");
	Cooldown = AutoAtkCooldown;
	IconTint = FLinearColor(0.75f, 0.75f, 0.8f);
	HitTags.AddTag(YSTags::Team_Foe);
}

void UYSAutoSwingAbility::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UYSAutoSwingAbility, Damage);
	DOREPLIFETIME(UYSAutoSwingAbility, AutoAtkCooldown);
}

void UYSAutoSwingAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if ENABLE_DRAW_DEBUG
	// TEMPORAIRE : hitbox visible (rouge transparent) pendant la fenêtre de coupe, sur toutes les machines
	const float SwingElapsed = GetSwingElapsed();
	if (bShowHitbox && IsInSwingWindow(SwingElapsed))
	{
		const AYSWeapon* Weapon = Cast<AYSWeapon>(GetOwner());
		if (const UBoxComponent* Hitbox = Weapon ? Weapon->FindComponentByClass<UBoxComponent>() : nullptr)
		{
			DrawDebugSolidBox(GetWorld(), Hitbox->GetComponentLocation(), Hitbox->GetScaledBoxExtent(), Hitbox->GetComponentQuat(), HitboxColor, false, -1.0f, 0);
		}
	}
#endif

	// logique de combat : serveur uniquement (les clients ne font que jouer l'animation). Autorité du PORTEUR :
	// une arme de NME n'est pas répliquée, chaque machine a la sienne et en est « autorité »
	if (!HasHolderAuthority())
	{
		return;
	}

	const float Elapsed = GetSwingElapsed();
	if (IsInSwingWindow(Elapsed))
	{
		ApplySwingHits();
	}

	// déclenchement automatique : seulement pour l'attaque auto (une sous-classe en Ability est lancée par le clic)
	// ; en akimbo, la main gauche est déclenchée par la droite ; NME : déclenchée par sa hitbox (AYSFoeCharacter)
	if (!IsAutomatic() || IsTriggeredByHolder() || IsOffHandOfAkimbo() || !IsReady() || !CanStartByPriority() || !FindAutoTarget())
	{
		return;
	}

	StartAttack();

	if (UYSAutoSwingAbility* Partner = GetAkimboPartner())
	{
		TWeakObjectPtr<UYSAutoSwingAbility> WeakPartner = Partner;
		GetWorld()->GetTimerManager().SetTimer(AkimboTimer, FTimerDelegate::CreateWeakLambda(this, [WeakPartner]()
		{
			if (WeakPartner.IsValid())
			{
				WeakPartner->StartAttack();
			}
		}), FMath::Max(AkimboFireDelay, KINDA_SMALL_NUMBER), false);
	}
}

AActor* UYSAutoSwingAbility::FindAutoTarget() const
{
	// la direction du curseur est celle du regard du personnage
	const APawn* Holder = GetHolderPawn();
	if (!Holder)
	{
		return nullptr;
	}
	return FindTargetInCone(GetWorld(), Holder->GetActorLocation(), Holder->GetActorForwardVector(), DetectionDistance, DetectionAngle, HitTags, Holder);
}

bool UYSAutoSwingAbility::HasHolderAuthority() const
{
	const APawn* Holder = GetHolderPawn();
	return Holder ? Holder->HasAuthority() : GetOwner() && GetOwner()->HasAuthority();
}

bool UYSAutoSwingAbility::IsTriggeredByHolder() const
{
	// le joueur déclenche ses attaques auto par la détection en cône ; un NME par sa hitbox de contact
	return GetHolderPawn() && !Cast<AYSPlayerCharacter>(GetHolderPawn());
}

void UYSAutoSwingAbility::PlayAttackLocally()
{
	// NME (arme locale, non répliquée) : l'animation part sur cette machine à la réception du NetMulticast du NME
	HitThisSwing.Reset();
	NotifyActivated();
}

bool UYSAutoSwingAbility::StartAttack()
{
	if (!HasHolderAuthority() || !IsReady() || !CanStartByPriority())
	{
		return false;
	}
	CancelLowerPriorityActions();

	ReadyServerTime = GetServerTime() + GetActivationLockDuration();
	// Golden bloodlust : l'attaque est critique si toutes les BL stacks sont là au moment où elle part
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	bCriticalSwing = Character && Character->IsBloodlustCritical();
	// Atk critique : chaque arme compte ses attaques auto, une sur N est toujours critique
	if (Character && IsAutomatic() && MeleeSystem == EYSMeleeSystem::Swing && Character->GetCriticalSwingInterval() > 0)
	{
		++AutoSwingCount;
		if (AutoSwingCount % Character->GetCriticalSwingInterval() == 0)
		{
			bCriticalSwing = true;
			UE_LOG(LogTemp, Verbose, TEXT("[YSCritical] %s.%s : %de attaque auto, critique"), *GetOwner()->GetName(), *GetName(), AutoSwingCount);
		}
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSAbility] %s.%s attaque à %.2f, prête à %.2f%s"), *GetOwner()->GetName(), *GetName(), GetServerTime(), ReadyServerTime, bCriticalSwing ? TEXT(" (critique)") : TEXT(""));
	HitThisSwing.Reset();
	MulticastActivated();
	return true;
}

float UYSAutoSwingAbility::GetCooldownMultiplier() const
{
	// BL stacks : vitesse d'atk en plus, appliquée après l'Atk speed (attaque auto seulement, pas le Dashing Slash)
	const AYSPlayerCharacter* Character = IsAutomatic() ? Cast<AYSPlayerCharacter>(GetHolderPawn()) : nullptr;
	return Super::GetCooldownMultiplier() * (Character ? Character->GetBloodlustCooldownFactor() : 1.0f);
}

bool UYSAutoSwingAbility::EvaluateHandPose(float& OutIdleWeight, FVector& OutLocation, FQuat& OutRotation) const
{
	const float Elapsed = GetSwingElapsed();
	if (Elapsed < 0.0f || Elapsed >= GetAttackDuration())
	{
		return false;
	}

	const FQuat WindupQuat = WindupHandRotation.Quaternion();
	const FQuat SwingEndQuat = SwingEndHandRotation.Quaternion();

	if (Elapsed < GetWindupTime())
	{
		// préparation : de la pose normale vers le flanc
		const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, Elapsed / GetWindupTime(), WindupEaseExponent);
		OutIdleWeight = 1.0f - Alpha;
		OutLocation = WindupHandLocation;
		OutRotation = WindupQuat;
	}
	else if (Elapsed < GetWindupTime() + GetSwingTime())
	{
		// coupe : départ immédiat, très rapide
		const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, (Elapsed - GetWindupTime()) / GetSwingTime(), SwingEaseExponent);
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
		OutIdleWeight = FMath::InterpEaseInOut(0.0f, 1.0f, (Elapsed - GetWindupTime() - GetSwingTime()) / GetRecoverTime(), RecoverEaseExponent);
		OutLocation = SwingEndHandLocation;
		OutRotation = SwingEndQuat;
	}
	return true;
}

void UYSAutoSwingAbility::ApplySwingHits()
{
	const AYSWeapon* Weapon = Cast<AYSWeapon>(GetOwner());
	const UBoxComponent* Hitbox = Weapon ? Weapon->FindComponentByClass<UBoxComponent>() : nullptr;
	APawn* Holder = GetHolderPawn();
	if (!Hitbox || !GetWorld())
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AutoSwing), false, Holder);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Hitbox->GetComponentLocation(), Hitbox->GetComponentQuat(),
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeBox(Hitbox->GetScaledBoxExtent()), Params);

	AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(Holder);
	const float CriticalMultiplier = bCriticalSwing && Character ? Character->GetCriticalDamageMultiplier() : 1.0f;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		const IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Target);
		if (!Target || HitThisSwing.Contains(Target) || !TagInterface || !TagInterface->HasAnyMatchingGameplayTags(HitTags))
		{
			continue;
		}

		// Bloodlust : une seule BL stack par attaque auto, même si elle touche plusieurs NME
		if (HitThisSwing.Num() == 0 && IsAutomatic() && Character)
		{
			Character->AddBloodlustStack();
		}

		HitThisSwing.Add(Target);
		const float FinalDamage = Damage * GetDamageMultiplier() * CriticalMultiplier;
		UE_LOG(LogTemp, Verbose, TEXT("[YSAutoSwing] %s.%s touche %s : %.1f dégâts (base %.1f x %.3f%s)"), *GetOwner()->GetName(), *GetName(), *Target->GetName(),
			FinalDamage, Damage, GetDamageMultiplier(), bCriticalSwing ? TEXT(", critique") : TEXT(""));
		// pas de DamageCauser : le chiffre de dégâts s'affiche sur la cible ; critique : type de dégât qui grossit le chiffre ;
		// direction du coup (pushback) = du porteur vers la cible : le NME recule, il n'est pas balayé sur le côté par la lame
		const FVector ShotDirection = Holder ? (Target->GetActorLocation() - Holder->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
		const FPointDamageEvent DamageEvent(FinalDamage, FHitResult(), ShotDirection, bCriticalSwing ? UYSCriticalDamageType::StaticClass() : nullptr);
		Target->TakeDamage(FinalDamage, DamageEvent, Holder ? Holder->GetController() : nullptr, nullptr);
	}
}

bool UYSAutoSwingAbility::IsOffHandOfAkimbo() const
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	return Character && Character->IsAkimbo() && Character->GetOffHandWeapon() == GetOwner();
}

UYSAutoSwingAbility* UYSAutoSwingAbility::GetAkimboPartner() const
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (!Character || !Character->IsAkimbo() || Character->GetMainHandWeapon() != GetOwner())
	{
		return nullptr;
	}
	const AYSWeapon* OffWeapon = Character->GetOffHandWeapon();
	return OffWeapon ? Cast<UYSAutoSwingAbility>(OffWeapon->GetAutoAction()) : nullptr;
}
