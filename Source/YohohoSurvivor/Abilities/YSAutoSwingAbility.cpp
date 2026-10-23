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

	// estoc : main reculée contre la hanche, lame pointée devant (axe Z de l'arme vers l'avant), puis piquée droit devant
	CriticalThrust.WindupHandLocation = FVector(-30.0f, 35.0f, 0.0f);
	CriticalThrust.WindupHandRotation = FRotator(-90.0f, 0.0f, 90.0f);
	CriticalThrust.SwingEndHandLocation = FVector(85.0f, 15.0f, 0.0f);
	CriticalThrust.SwingEndHandRotation = FRotator(-90.0f, 0.0f, 90.0f);
	CriticalThrust.bCircularSwing = false;
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

void UYSAutoSwingAbility::PlayAttackLocally(EYSSwingAnim Anim)
{
	// NME (arme locale, non répliquée) : l'animation part sur cette machine à la réception du NetMulticast du NME
	CurrentAnim = Anim;
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

	// Golden bloodlust : l'attaque est critique si toutes les BL stacks sont là au moment où elle part ;
	// BloodRage : rage au maximum -> attaque critique (stacks consommées)
	AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	bCriticalSwing = Character && (Character->IsBloodlustCritical() || Character->ConsumeBloodRageCritical());
	EYSSwingAnim Anim = EYSSwingAnim::Main;
	if (IsAutomatic() && MeleeSystem == EYSMeleeSystem::Swing)
	{
		// une attaque auto sur deux de cette arme joue le second swing
		++AutoSwingCount;
		if (bAlternateSecondSwing && AutoSwingCount % 2 == 0)
		{
			Anim = EYSSwingAnim::Second;
		}
		// Atk critique : le personnage compte ses attaques auto (épées akimbo comptées ensemble), une sur N est toujours critique (estoc)
		if (Character && Character->GetCriticalSwingInterval() > 0)
		{
			const int32 Count = Character->RegisterAutoSwing();
			if (Count % Character->GetCriticalSwingInterval() == 0)
			{
				bCriticalSwing = true;
				if (bCriticalThrust)
				{
					Anim = EYSSwingAnim::Critical;
				}
				UE_LOG(LogTemp, Verbose, TEXT("[YSCritical] %s.%s : %de attaque auto du personnage, critique"), *GetOwner()->GetName(), *GetName(), Count);
			}
		}
	}
	// la durée de l'animation (donc la recharge) dépend de l'animation choisie
	CurrentAnim = Anim;
	ReadyServerTime = GetServerTime() + GetActivationLockDuration();
	UE_LOG(LogTemp, Verbose, TEXT("[YSAbility] %s.%s attaque à %.2f, prête à %.2f, anim %s%s"), *GetOwner()->GetName(), *GetName(), GetServerTime(), ReadyServerTime,
		*StaticEnum<EYSSwingAnim>()->GetNameStringByValue(static_cast<int64>(Anim)), bCriticalSwing ? TEXT(" (critique)") : TEXT(""));
	HitThisSwing.Reset();
	MulticastSwingStarted(Anim);
	return true;
}

void UYSAutoSwingAbility::MulticastSwingStarted_Implementation(EYSSwingAnim Anim)
{
	CurrentAnim = Anim;
	MulticastActivated_Implementation();
}

const FYSSwingPoses& UYSAutoSwingAbility::GetCurrentPoses(FYSSwingPoses& MainPoses) const
{
	switch (CurrentAnim)
	{
	case EYSSwingAnim::Second:
		return SecondSwing;
	case EYSSwingAnim::Critical:
		return CriticalThrust;
	default:
		MainPoses.WindupHandLocation = WindupHandLocation;
		MainPoses.WindupHandRotation = WindupHandRotation;
		MainPoses.SwingEndHandLocation = SwingEndHandLocation;
		MainPoses.SwingEndHandRotation = SwingEndHandRotation;
		MainPoses.bCircularSwing = bCircularSwing;
		return MainPoses;
	}
}

void UYSAutoSwingAbility::GetCurrentEases(float& OutWindup, float& OutSwing, float& OutRecover) const
{
	// estoc : mêmes courbes que le swing principal
	const bool bSecond = CurrentAnim == EYSSwingAnim::Second;
	OutWindup = bSecond ? SecondSwing.WindupEaseExponent : WindupEaseExponent;
	OutSwing = bSecond ? SecondSwing.SwingEaseExponent : SwingEaseExponent;
	OutRecover = bSecond ? SecondSwing.RecoverEaseExponent : RecoverEaseExponent;
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

	// poses et courbes de l'animation en cours (swing principal, second swing ou estoc)
	FYSSwingPoses MainPoses;
	const FYSSwingPoses& Poses = GetCurrentPoses(MainPoses);
	float WindupEase, SwingEase, RecoverEase;
	GetCurrentEases(WindupEase, SwingEase, RecoverEase);
	const FVector& StartLocation = Poses.WindupHandLocation;
	const FVector& EndLocation = Poses.SwingEndHandLocation;
	const FRotator& StartRotation = Poses.WindupHandRotation;
	const FRotator& EndRotation = Poses.SwingEndHandRotation;

	if (Elapsed < GetWindupTime())
	{
		// préparation : de la pose normale vers le flanc
		const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, Elapsed / GetWindupTime(), WindupEase);
		OutIdleWeight = 1.0f - Alpha;
		OutLocation = StartLocation;
		OutRotation = StartRotation.Quaternion();
	}
	else if (Elapsed < GetWindupTime() + GetSwingTime())
	{
		// coupe : départ immédiat, très rapide
		const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, (Elapsed - GetWindupTime()) / GetSwingTime(), SwingEase);
		OutIdleWeight = 0.0f;

		if (Poses.bCircularSwing)
		{
			// arc autour du corps : angle et rayon (plan horizontal) interpolés, hauteur linéaire
			const float StartAngle = FMath::Atan2(StartLocation.Y, StartLocation.X);
			const float EndAngle = FMath::Atan2(EndLocation.Y, EndLocation.X);
			const float Angle = StartAngle + FMath::FindDeltaAngleRadians(StartAngle, EndAngle) * Alpha;
			const float Radius = FMath::Lerp(FVector2D(StartLocation).Size(), FVector2D(EndLocation).Size(), Alpha);
			OutLocation = FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, FMath::Lerp(StartLocation.Z, EndLocation.Z, Alpha));
		}
		else
		{
			OutLocation = FMath::Lerp(StartLocation, EndLocation, Alpha);
		}

		// valeur par valeur (pas le plus court chemin) pour respecter les tours complets demandés
		const FRotator Rotation(
			FMath::Lerp(StartRotation.Pitch, EndRotation.Pitch, Alpha),
			FMath::Lerp(StartRotation.Yaw, EndRotation.Yaw, Alpha),
			FMath::Lerp(StartRotation.Roll, EndRotation.Roll, Alpha));
		OutRotation = Rotation.Quaternion();
	}
	else
	{
		// retour : de la fin de coupe vers la pose normale
		OutIdleWeight = FMath::InterpEaseInOut(0.0f, 1.0f, (Elapsed - GetWindupTime() - GetSwingTime()) / GetRecoverTime(), RecoverEase);
		OutLocation = EndLocation;
		OutRotation = EndRotation.Quaternion();
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
		DealSwingDamage(Target, FinalDamage, ShotDirection, Holder ? Holder->GetController() : nullptr);
	}
}

void UYSAutoSwingAbility::DealSwingDamage(AActor* Target, float FinalDamage, const FVector& ShotDirection, AController* InstigatorController)
{
	const FPointDamageEvent DamageEvent(FinalDamage, FHitResult(), ShotDirection, bCriticalSwing ? UYSCriticalDamageType::StaticClass() : nullptr);
	Target->TakeDamage(FinalDamage, DamageEvent, InstigatorController, nullptr);
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
