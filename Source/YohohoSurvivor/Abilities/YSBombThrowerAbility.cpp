#include "Abilities/YSBombThrowerAbility.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Core/YSCombat.h"
#include "Player/YSPlayerCharacter.h"
#include "TimerManager.h"
#include "Upgrades/YSUpgrade.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/YSPlayerBomb.h"
#include "Weapons/YSWeapon.h"

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

FVector UYSBombThrowerAbility::FindLandingPoint(const FVector& TargetLocation) const
{
	return YSCombat::FindLandingPoint(Cast<ACharacter>(GetHolderPawn()), TargetLocation, MaxThrowDistance, RetreatStep);
}

void UYSBombThrowerAbility::ExecuteAbility(const FVector& TargetLocation)
{
	if (!BombClass || !GetHolderPawn())
	{
		return;
	}

	// point d'impact choisi au clic ; la bombe part au lâcher de l'animation
	const FVector Landing = FindLandingPoint(TargetLocation);

	// Bomb Count > 1 : les autres bombes de part et d'autre de la première (+angle, -angle, +2 angles...), à la même distance du joueur ;
	// trop proches (Min Bomb Spacing) : l'angle s'ouvre (au plus : bombes réparties sur tout le cercle), puis elles s'éloignent du joueur
	TArray<FVector> ExtraLandings;
	const FVector HolderLocation = GetHolderPawn()->GetActorLocation();
	const FVector ToLanding(Landing.X - HolderLocation.X, Landing.Y - HolderLocation.Y, 0.0f);
	const float Radius = ToLanding.Size();
	const FVector Direction = Radius > KINDA_SMALL_NUMBER ? ToLanding / Radius : FVector(GetHolderPawn()->GetActorForwardVector().GetSafeNormal2D());
	float SpreadAngle = BombSpreadAngle;
	float ArcRadius = Radius;
	if (BombCount > 1 && MinBombSpacing > 0.0f)
	{
		const float NeededAngle = Radius > MinBombSpacing * 0.5f ? FMath::RadiansToDegrees(2.0f * FMath::Asin(MinBombSpacing / (2.0f * Radius))) : 180.0f;
		SpreadAngle = FMath::Min(FMath::Max(SpreadAngle, NeededAngle), 360.0f / BombCount);
		const float SpreadRad = FMath::DegreesToRadians(SpreadAngle);
		// distance à la première bombe (rayon Radius) : rayon de l'arc agrandi si besoin (plus grande racine de la loi des cosinus)
		const float SinSpread = FMath::Sin(SpreadRad);
		if (FMath::Sqrt(2.0f * Radius * Radius * (1.0f - FMath::Cos(SpreadRad))) < MinBombSpacing)
		{
			ArcRadius = Radius * FMath::Cos(SpreadRad) + FMath::Sqrt(FMath::Max(0.0f, FMath::Square(MinBombSpacing) - FMath::Square(Radius * SinSpread)));
		}
		// entre deux bombes voisines de l'arc (+angle et +2 angles...)
		if (BombCount > 3 && SpreadRad > KINDA_SMALL_NUMBER)
		{
			ArcRadius = FMath::Max(ArcRadius, MinBombSpacing / (2.0f * FMath::Sin(SpreadRad * 0.5f)));
		}
	}
	for (int32 Index = 1; Index < BombCount; ++Index)
	{
		const float Yaw = SpreadAngle * ((Index + 1) / 2) * (Index % 2 == 1 ? 1.0f : -1.0f);
		ExtraLandings.Add(FindLandingPoint(HolderLocation + FRotator(0.0f, Yaw, 0.0f).RotateVector(Direction * ArcRadius)));
	}

	const float ReleaseDelay = bPlayThrowAnim ? ThrowWindupDuration + ThrowDuration : 0.0f;
	if (ReleaseDelay <= 0.0f)
	{
		ReleaseBomb(Landing, ExtraLandings);
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(ReleaseTimer, FTimerDelegate::CreateWeakLambda(this, [this, Landing, ExtraLandings]()
	{
		ReleaseBomb(Landing, ExtraLandings);
	}), ReleaseDelay, false);
}

void UYSBombThrowerAbility::ReleaseBomb(FVector Landing, TArray<FVector> ExtraLandings)
{
	// upgrade « Une Mine ! » : une mine à la place de la bombe
	const bool bMine = MineClass && GetWeaponStat(UYSMineUpgrade::MineStat, 0.0f) > 0.0f;
	const TSubclassOf<AYSPlayerBomb> SpawnClass = bMine ? MineClass : BombClass;
	APawn* Holder = GetHolderPawn();
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(Holder);
	if (!SpawnClass || !IsValid(Holder) || (Character && Character->AreWeaponsStowed()))
	{
		return;
	}

	// départ : la main qui lance (sinon la bouche de l'arme, sinon le personnage), arrivée : le sol visé
	const AYSWeapon* Weapon = Cast<AYSWeapon>(GetOwner());
	const USceneComponent* Hand = bPlayThrowAnim ? GetThrowingHand() : nullptr;
	const FVector Start = Hand ? Hand->GetComponentTransform().TransformPosition(HeldBombOffset) : (Weapon ? Weapon->GetMuzzleLocation() : Holder->GetActorLocation());

	// Golden Mine : mines en plus sur la ligne joueur -> première mine, une plus près (le point entre le joueur et la première) puis
	// une plus loin du même écart ; écart d'au moins Min Bomb Spacing (la plus proche peut passer derrière le joueur) ; sol valide
	TArray<FVector> Landings = { Landing };
	if (bMine && GetWeaponStat(UYSMineUpgrade::MineDoubleStat, 0.0f) > 0.0f)
	{
		const FVector HolderLocation = Holder->GetActorLocation();
		const FVector ToLanding(Landing.X - HolderLocation.X, Landing.Y - HolderLocation.Y, 0.0f);
		const float Radius = ToLanding.Size();
		const FVector Direction = Radius > KINDA_SMALL_NUMBER ? ToLanding / Radius : FVector(Holder->GetActorForwardVector().GetSafeNormal2D());
		const float Gap = FMath::Max(Radius * 0.5f, MinBombSpacing);
		const ACharacter* HolderCharacter = Cast<ACharacter>(Holder);
		for (int32 Index = 0; Index < GoldenMineCount; ++Index)
		{
			const float Distance = Index % 2 == 0 ? Radius - Gap * (Index / 2 + 1) : Radius + Gap * (Index / 2 + 1);
			// plus loin que la première : la distance max de lancer est repoussée d'autant
			Landings.Add(YSCombat::FindLandingPoint(HolderCharacter, HolderLocation + Direction * Distance, FMath::Max(MaxThrowDistance, FMath::Abs(Distance)), RetreatStep));
		}
	}
	// bombes de l'arc (Bomb Count)
	Landings.Append(ExtraLandings);

	for (const FVector& Target : Landings)
	{
		const FTransform SpawnTransform(FRotator(0.0f, Holder->GetActorRotation().Yaw, 0.0f), Start);
		AYSPlayerBomb* Bomb = GetWorld()->SpawnActorDeferred<AYSPlayerBomb>(SpawnClass, SpawnTransform, Holder, Holder, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Bomb)
		{
			// mine : x Mine Damage Multiplier de l'upgrade
			const float BombHitDamage = BombDamage * GetDamageMultiplier() * (bMine ? GetWeaponStat(UYSMineUpgrade::MineDamageStat, 1.0f) : 1.0f);
			Bomb->SetDamage(BombHitDamage);
			Bomb->SetExtraPushback(bMine ? GetWeaponStat(UYSMineUpgrade::MinePushbackStat, 0.0f) : 0.0f);
			Bomb->SetRadiusMultiplier(bMine ? 1.0f + GetWeaponStat(UYSMineUpgrade::MineRadiusStat, 0.0f) : 1.0f);
			Bomb->SetThrow(Start, Target, FlightDuration, ApexHeight);
			Bomb->FinishSpawning(SpawnTransform);
			UE_LOG(LogTemp, Verbose, TEXT("[YSBombThrower] %s lance %s vers (%.0f, %.0f, %.0f), %.0f cm du joueur : %.0f dégâts"), *GetOwner()->GetName(),
				*Bomb->GetName(), Target.X, Target.Y, Target.Z, FVector::Dist2D(Target, Holder->GetActorLocation()), BombHitDamage);
		}
	}
}

float UYSBombThrowerAbility::GetCooldownMultiplier() const
{
	// « Une Mine ! » : recharge x Mine Cooldown Multiplier
	const bool bMine = MineClass && GetWeaponStat(UYSMineUpgrade::MineStat, 0.0f) > 0.0f;
	return Super::GetCooldownMultiplier() * (bMine ? GetWeaponStat(UYSMineUpgrade::MineCooldownStat, 1.0f) : 1.0f);
}
