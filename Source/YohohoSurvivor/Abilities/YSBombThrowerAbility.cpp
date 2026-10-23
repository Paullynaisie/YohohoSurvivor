#include "Abilities/YSBombThrowerAbility.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "LandscapeProxy.h"
#include "Map/YSOcean.h"
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
	APawn* Holder = GetHolderPawn();
	if (!BombClass || !Holder)
	{
		return;
	}

	// départ : la bouche de l'arme (sinon le personnage), arrivée : le sol visé
	const AYSWeapon* Weapon = Cast<AYSWeapon>(GetOwner());
	const FVector Start = Weapon ? Weapon->GetMuzzleLocation() : Holder->GetActorLocation();
	const FVector Landing = FindLandingPoint(TargetLocation);

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
