#include "Ship/YSShipCannon.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "Ship/YSPirateShip.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/YSProjectile.h"

namespace YSShipCannon
{
	/** Hauteur de vol des CanonAMMO au-dessus du sol / de la mer (comme une balle tirée à hauteur de main) */
	constexpr float ShotHeightAboveGround = 100.0f;
}

AYSShipCannon::AYSShipCannon()
{
	DisplayName = NSLOCTEXT("YS", "ShipCannonName", "Cannonerie");
	MaxHealth = 1000.0f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WoodFinder(TEXT("/Game/Art/Materials/PH_materials/m_PH_monochrome_Brown.m_PH_monochrome_Brown"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MetalFinder(TEXT("/Game/Art/Materials/PH_materials/m_PH_monochrome_Black.m_PH_monochrome_Black"));
	static ConstructorHelpers::FClassFinder<AYSProjectile> AmmoFinder(TEXT("/Game/Placeable/AMMO/PetoirAMMO"));

	// affût sur le pont (origine = pont), fût au-dessus du bastingage (100 cm)
	CannonBase = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CannonBase"));
	CannonBase->SetupAttachment(GetRootComponent());
	CannonBase->SetRelativeLocation(FVector(0.0f, 0.0f, 45.0f));
	CannonBase->SetRelativeScale3D(FVector(0.9f, 0.9f, 0.9f));
	CannonBase->SetCollisionProfileName(TEXT("BlockAll"));
	CannonBase->SetMobility(EComponentMobility::Movable);

	CannonPivot = CreateDefaultSubobject<USceneComponent>(TEXT("CannonPivot"));
	CannonPivot->SetupAttachment(GetRootComponent());
	CannonPivot->SetRelativeLocation(FVector(0.0f, 0.0f, 115.0f));

	CannonBarrel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CannonBarrel"));
	CannonBarrel->SetupAttachment(CannonPivot);
	CannonBarrel->SetRelativeLocation(FVector(40.0f, 0.0f, 0.0f));
	CannonBarrel->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	CannonBarrel->SetRelativeScale3D(FVector(0.45f, 0.45f, 1.7f));
	CannonBarrel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CannonBarrel->SetMobility(EComponentMobility::Movable);

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(CannonPivot);
	Muzzle->SetRelativeLocation(FVector(135.0f, 0.0f, 0.0f));

	if (CubeFinder.Succeeded())
	{
		CannonBase->SetStaticMesh(CubeFinder.Object);
	}
	if (CylinderFinder.Succeeded())
	{
		CannonBarrel->SetStaticMesh(CylinderFinder.Object);
	}
	if (WoodFinder.Succeeded())
	{
		CannonBase->SetMaterial(0, WoodFinder.Object);
	}
	if (MetalFinder.Succeeded())
	{
		CannonBarrel->SetMaterial(0, MetalFinder.Object);
	}
	if (AmmoFinder.Succeeded())
	{
		ProjectileClass = AmmoFinder.Class;
	}

	// joueur derrière le canon, mains sur la culasse (suivent le canon)
	GetOccupantPoint()->SetRelativeLocation(FVector(-110.0f, 0.0f, 0.0f));
	GetHandPoint(true)->SetupAttachment(CannonPivot);
	GetHandPoint(false)->SetupAttachment(CannonPivot);
	GetHandPoint(true)->SetRelativeLocation(FVector(-55.0f, -22.0f, 5.0f));
	GetHandPoint(false)->SetRelativeLocation(FVector(-55.0f, 22.0f, 5.0f));

	// le joueur vise (regard = curseur) ; caméra dézoomée comme à la barre, décalée au maximum vers la visée du canon
	bLockAim = false;
	CameraArmLength = 3600.0f;
	bCameraOnShip = false;
	CameraOffsetMode = EYSStationCameraOffset::Aim;
	CameraOffsetDistance = 1000.0f;
}

void AYSShipCannon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSShipCannon, ReadyServerTime);
}

float AYSShipCannon::ComputeAimYaw(const FVector& WorldTarget) const
{
	const FVector ToTarget = (WorldTarget - CannonPivot->GetComponentLocation()).GetSafeNormal2D();
	if (ToTarget.IsNearlyZero())
	{
		return CurrentAimYaw;
	}
	const float Relative = FRotator::NormalizeAxis(ToTarget.Rotation().Yaw - GetActorRotation().Yaw);
	return FMath::Clamp(Relative, -MaxAimAngle, MaxAimAngle);
}

float AYSShipCannon::GetTargetAimYaw() const
{
	// regard du joueur (tourné vers son curseur, répliqué avec ses mouvements) : même angle sur toutes les machines
	const AYSPlayerCharacter* Character = GetOccupant();
	if (!IsValid(Character))
	{
		return 0.0f;
	}
	const float Relative = FRotator::NormalizeAxis(Character->GetActorRotation().Yaw - GetActorRotation().Yaw);
	return FMath::Clamp(Relative, -MaxAimAngle, MaxAimAngle);
}

void AYSShipCannon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// visuel : le canon suit le regard du joueur (sans joueur : revient droit)
	const float Target = GetTargetAimYaw();
	CurrentAimYaw = AimFollowSpeed > 0.0f ? FMath::FInterpTo(CurrentAimYaw, Target, DeltaSeconds, AimFollowSpeed) : Target;
	CannonPivot->SetRelativeRotation(FRotator(0.0f, CurrentAimYaw, 0.0f));

	// recul du fût : en arrière d'un coup, puis retour
	if (BarrelRestLocation.IsZero())
	{
		BarrelRestLocation = CannonBarrel->GetRelativeLocation();
	}
	const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - LastFireTime);
	const float Kick = Elapsed < RecoilDuration ? FMath::InterpEaseOut(1.0f, 0.0f, Elapsed / RecoilDuration, 2.0f) : 0.0f;
	CannonBarrel->SetRelativeLocation(BarrelRestLocation - FVector(RecoilDistance * Kick, 0.0f, 0.0f));
}

void AYSShipCannon::ServerHandleAction(AYSPlayerCharacter* Character, EYSStationAction Action, const FVector& AimLocation)
{
	if (!HasAuthority() || Action != EYSStationAction::Fire || !Character || Character != GetOccupant() || !ProjectileClass)
	{
		return;
	}
	if (GetCooldownRemaining() > 0.0f)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[YSCannon] %s : pas encore rechargé (%.1f s)"), *GetName(), GetCooldownRemaining());
		return;
	}

	// direction : vers le point visé, dans les limites du canon ; le fût est tourné tout de suite pour partir de sa bouche
	const float AimYaw = ComputeAimYaw(AimLocation);
	CurrentAimYaw = AimYaw;
	CannonPivot->SetRelativeRotation(FRotator(0.0f, AimYaw, 0.0f));
	const float BaseYaw = GetActorRotation().Yaw + AimYaw;
	const FVector MuzzleLocation = Muzzle->GetComponentLocation();
	AYSPirateShip* Ship = GetShip();

	// comme le Shotgun : toutes les AMMO d'un coup, chacune au hasard dans la dispersion
	for (int32 Index = 0; Index < AmmoPerShot; ++Index)
	{
		const float Yaw = BaseYaw + FMath::FRandRange(-Spread, Spread);
		const FTransform SpawnTransform(FRotator(0.0f, Yaw, 0.0f), MuzzleLocation, FVector(ProjectileScale));
		AYSProjectile* Projectile = GetWorld()->SpawnActorDeferred<AYSProjectile>(ProjectileClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Projectile)
		{
			continue;
		}
		Projectile->SetDamage(Damage);
		Projectile->SetFlightOverrides(Ship ? static_cast<AActor*>(Ship) : this, YSShipCannon::ShotHeightAboveGround);
		Projectile->FinishSpawning(SpawnTransform);
	}

	const AGameStateBase* GameState = GetWorld()->GetGameState();
	ReadyServerTime = (GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds()) + Cooldown;
	ForceNetUpdate();
	MulticastFired();
	UE_LOG(LogTemp, Log, TEXT("[YSCannon] %s : tir de canon de %s, %d AMMO x %.0f dégâts (+/- %.0f°), angle %.0f°, recharge %.1f s"), *GetName(), *Character->GetName(),
		AmmoPerShot, Damage, Spread, AimYaw, Cooldown);
}

void AYSShipCannon::MulticastFired_Implementation()
{
	LastFireTime = GetWorld()->GetTimeSeconds();
}

bool AYSShipCannon::GetCameraAimDirection(FVector& OutDirection) const
{
	// direction visée (limitée) : la caméra bute quand le canon est à son angle max
	OutDirection = FRotator(0.0f, GetActorRotation().Yaw + GetTargetAimYaw(), 0.0f).Vector();
	return true;
}

float AYSShipCannon::GetCooldownRemaining() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	return FMath::Max(0.0f, static_cast<float>(ReadyServerTime - Now));
}

bool AYSShipCannon::GetActionIcon(UTexture2D*& OutIcon, FLinearColor& OutTint, float& OutRemaining, float& OutDuration) const
{
	OutIcon = Icon;
	OutTint = IconTint;
	OutRemaining = GetCooldownRemaining();
	OutDuration = Cooldown;
	return true;
}
