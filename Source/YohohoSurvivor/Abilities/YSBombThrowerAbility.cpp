#include "Abilities/YSBombThrowerAbility.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "LandscapeProxy.h"
#include "Map/YSOcean.h"
#include "Player/YSPlayerCharacter.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/YSPlayerBomb.h"
#include "Weapons/YSWeapon.h"

namespace YSBombThrower
{
	/** Recherche du sol : au-dessus et en dessous de la hauteur du joueur */
	constexpr float GroundSearchUp = 2000.0f;
	constexpr float GroundSearchDown = 3000.0f;

	/** Props traversés au plus pour trouver l'île sous un point */
	constexpr int32 MaxIgnoredActors = 8;
}

UYSBombThrowerAbility::UYSBombThrowerAbility()
{
	DisplayName = NSLOCTEXT("YS", "BombThrower", "BombThrower");
	WeaponType = EYSWeaponType::Gun;
	Cooldown = 5.0f;
	IconTint = FLinearColor(0.85f, 0.35f, 0.1f);

	// bombe dans la main pendant le lancer (visuel local)
	PrimaryComponentTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	HeldBombMesh = SphereMesh.Object;
}

USceneComponent* UYSBombThrowerAbility::GetThrowingHand() const
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	const AYSWeapon* Weapon = Cast<AYSWeapon>(GetOwner());
	if (!Character || !Weapon)
	{
		return nullptr;
	}
	const bool bWeaponInLeft = Character->GetLeftHandWeapon() == Weapon;
	const bool bLeft = AnimatesOffHand() ? !bWeaponInLeft : bWeaponInLeft;
	return bLeft ? Character->GetLeftHand() : Character->GetRightHand();
}

bool UYSBombThrowerAbility::AnimatesOffHand() const
{
	// l'autre main n'est animée que si elle est posée sur l'arme (arme à deux mains)
	const AYSWeapon* Weapon = Cast<AYSWeapon>(GetOwner());
	return bPlayThrowAnim && bThrowWithOffHand && Weapon && Weapon->IsTwoHanded();
}

bool UYSBombThrowerAbility::EvaluateHandPose(float& OutIdleWeight, FVector& OutLocation, FQuat& OutRotation) const
{
	// même animation que le NME bomb throw (UYSFoeBombAction), depuis l'activation (prédite chez le lanceur, multicast chez les autres)
	const float Elapsed = GetTimeSinceActivation();
	if (!bPlayThrowAnim || Elapsed < 0.0f || Elapsed >= ThrowWindupDuration + ThrowDuration + ThrowRecoverDuration)
	{
		return false;
	}

	if (Elapsed < ThrowWindupDuration)
	{
		// préparation : de la pose normale vers l'arrière de l'épaule
		const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, Elapsed / ThrowWindupDuration, 2.0f);
		OutIdleWeight = 1.0f - Alpha;
		OutLocation = WindupHandLocation;
		OutRotation = WindupHandRotation.Quaternion();
	}
	else if (Elapsed < ThrowWindupDuration + ThrowDuration)
	{
		// lancer : vers l'avant, rapide
		const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, (Elapsed - ThrowWindupDuration) / ThrowDuration, ThrowEaseExponent);
		OutIdleWeight = 0.0f;
		OutLocation = FMath::Lerp(WindupHandLocation, ReleaseHandLocation, Alpha);
		OutRotation = FQuat::Slerp(WindupHandRotation.Quaternion(), ReleaseHandRotation.Quaternion(), Alpha);
	}
	else
	{
		// retour à la pose normale
		OutIdleWeight = FMath::InterpEaseInOut(0.0f, 1.0f, (Elapsed - ThrowWindupDuration - ThrowDuration) / ThrowRecoverDuration, 2.0f);
		OutLocation = ReleaseHandLocation;
		OutRotation = ReleaseHandRotation.Quaternion();
	}
	return true;
}

void UYSBombThrowerAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// bombe dans la main jusqu'au lâcher
	const bool bHolding = bPlayThrowAnim && bShowHeldBomb && HeldBombMesh && GetTimeSinceActivation() >= 0.0f && GetTimeSinceActivation() < ThrowWindupDuration + ThrowDuration;
	USceneComponent* Hand = bHolding ? GetThrowingHand() : nullptr;
	if (Hand && !HeldBomb)
	{
		HeldBomb = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("HeldBomb"), RF_Transient);
		HeldBomb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HeldBomb->SetCastShadow(false);
		HeldBomb->RegisterComponent();
	}
	if (!HeldBomb)
	{
		return;
	}

	if (Hand)
	{
		if (HeldBomb->GetAttachParent() != Hand)
		{
			HeldBomb->AttachToComponent(Hand, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
		HeldBomb->SetStaticMesh(HeldBombMesh);
		HeldBomb->SetMaterial(0, HeldBombMaterial);
		HeldBomb->SetRelativeLocation(HeldBombOffset);
		HeldBomb->SetWorldScale3D(HeldBombScale);
	}
	HeldBomb->SetVisibility(Hand != nullptr);
}

void UYSBombThrowerAbility::OnActionCancelled()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReleaseTimer);
	}
}

void UYSBombThrowerAbility::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReleaseTimer);
	}
	if (HeldBomb)
	{
		HeldBomb->DestroyComponent();
		HeldBomb = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

bool UYSBombThrowerAbility::FindIslandGround(const FVector& Point, float ReferenceZ, FVector& OutGround) const
{
	// seul le Landscape (l'île) compte : un prop touché (ponton, bateau, palmier) est traversé
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSBombThrowerGround), false, GetHolderPawn());
	const FVector Start(Point.X, Point.Y, ReferenceZ + YSBombThrower::GroundSearchUp);
	const FVector End(Point.X, Point.Y, ReferenceZ - YSBombThrower::GroundSearchDown);
	for (int32 Attempt = 0; Attempt < YSBombThrower::MaxIgnoredActors; ++Attempt)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			return false;
		}
		if (Hit.GetActor() && Hit.GetActor()->IsA<ALandscapeProxy>())
		{
			// pas dans l'eau
			OutGround = Hit.ImpactPoint;
			return !AYSOcean::IsUnderwater(GetWorld(), OutGround);
		}
		Params.AddIgnoredActor(Hit.GetActor());
	}
	return false;
}

FVector UYSBombThrowerAbility::FindLandingPoint(const FVector& TargetLocation) const
{
	const ACharacter* Character = Cast<ACharacter>(GetHolderPawn());
	if (!Character)
	{
		return TargetLocation;
	}
	const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	// curseur ramené à la portée max, puis on recule vers le joueur jusqu'à un sol de l'île hors de l'eau
	FVector Offset = TargetLocation - Feet;
	Offset.Z = 0.0f;
	const float Distance = FMath::Min(Offset.Size(), MaxThrowDistance);
	const FVector Direction = Offset.GetSafeNormal();
	FVector Ground;
	for (float Step = Distance; Step > 0.0f; Step -= RetreatStep)
	{
		if (FindIslandGround(Feet + Direction * Step, Feet.Z, Ground))
		{
			return Ground;
		}
	}

	// aucun sol valide : sur place
	if (FindIslandGround(Feet, Feet.Z, Ground))
	{
		return Ground;
	}
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(YSBombThrowerFeet), false, Character);
	return GetWorld()->LineTraceSingleByObjectType(Hit, Feet + FVector(0.0f, 0.0f, 50.0f), Feet - FVector(0.0f, 0.0f, 500.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params)
		? Hit.ImpactPoint : Feet;
}

void UYSBombThrowerAbility::ExecuteAbility(const FVector& TargetLocation)
{
	if (!BombClass || !GetHolderPawn())
	{
		return;
	}

	// point d'impact choisi au clic ; la bombe part au lâcher de l'animation
	const FVector Landing = FindLandingPoint(TargetLocation);
	const float ReleaseDelay = bPlayThrowAnim ? ThrowWindupDuration + ThrowDuration : 0.0f;
	if (ReleaseDelay <= 0.0f)
	{
		ReleaseBomb(Landing);
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(ReleaseTimer, FTimerDelegate::CreateWeakLambda(this, [this, Landing]()
	{
		ReleaseBomb(Landing);
	}), ReleaseDelay, false);
}

void UYSBombThrowerAbility::ReleaseBomb(FVector Landing)
{
	APawn* Holder = GetHolderPawn();
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(Holder);
	if (!BombClass || !IsValid(Holder) || (Character && Character->IsCarrying()))
	{
		return;
	}

	// départ : la main qui lance (sinon la bouche de l'arme, sinon le personnage), arrivée : le sol visé
	const AYSWeapon* Weapon = Cast<AYSWeapon>(GetOwner());
	const USceneComponent* Hand = bPlayThrowAnim ? GetThrowingHand() : nullptr;
	const FVector Start = Hand ? Hand->GetComponentTransform().TransformPosition(HeldBombOffset) : (Weapon ? Weapon->GetMuzzleLocation() : Holder->GetActorLocation());
	const FVector TargetLocation = Landing;

	const FTransform SpawnTransform(FRotator(0.0f, Holder->GetActorRotation().Yaw, 0.0f), Start);
	AYSPlayerBomb* Bomb = GetWorld()->SpawnActorDeferred<AYSPlayerBomb>(BombClass, SpawnTransform, Holder, Holder, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Bomb)
	{
		Bomb->SetDamage(BombDamage * GetDamageMultiplier());
		Bomb->SetThrow(Start, Landing, FlightDuration, ApexHeight);
		Bomb->FinishSpawning(SpawnTransform);
		UE_LOG(LogTemp, Verbose, TEXT("[YSBombThrower] %s lance %s vers (%.0f, %.0f, %.0f) (curseur (%.0f, %.0f), %.0f cm du joueur) : %.0f dégâts"), *GetOwner()->GetName(),
			*Bomb->GetName(), Landing.X, Landing.Y, Landing.Z, TargetLocation.X, TargetLocation.Y, FVector::Dist2D(Landing, Holder->GetActorLocation()), BombDamage * GetDamageMultiplier());
	}
}
