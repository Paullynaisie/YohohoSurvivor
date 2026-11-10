#include "Ship/YSBoatCrew.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/YSGameState.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSFaceDecalComponent.h"
#include "Player/YSPlayerCharacter.h"
#include "Ship/YSPirateShip.h"
#include "Ship/YSShipStation.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/YSWeapon.h"

namespace YSBoatCrew
{
	UStaticMeshComponent* CreateShape(AYSBoatCrew* Owner, const TCHAR* Name, USceneComponent* Parent, UStaticMesh* Mesh, UMaterialInterface* Material,
		const FVector& Location, const FRotator& Rotation, const FVector& Scale)
	{
		UStaticMeshComponent* Shape = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Shape->SetupAttachment(Parent);
		Shape->SetStaticMesh(Mesh);
		if (Material)
		{
			Shape->SetMaterial(0, Material);
		}
		Shape->SetRelativeLocationAndRotation(Location, Rotation);
		Shape->SetRelativeScale3D(Scale);
		Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shape->SetGenerateOverlapEvents(false);
		Shape->SetCanEverAffectNavigation(false);
		return Shape;
	}
}

AYSBoatCrew::AYSBoatCrew()
{
	// bonus du Charpentier et du Cuisinier : serveur, à chaque image (rien à faire avant d'avoir rejoint l'équipage)
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WhiteFinder(TEXT("/Game/Art/Materials/PH_materials/m_PH_monochrome_white.m_PH_monochrome_white"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RedFinder(TEXT("/Game/Art/Materials/PH_materials/m_PH_monochrome_Red.m_PH_monochrome_Red"));
	UStaticMesh* Cube = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	UStaticMesh* Cylinder = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;
	UStaticMesh* Sphere = SphereFinder.Succeeded() ? SphereFinder.Object : nullptr;
	UMaterialInterface* White = WhiteFinder.Succeeded() ? WhiteFinder.Object : nullptr;
	UMaterialInterface* Red = RedFinder.Succeeded() ? RedFinder.Object : nullptr;

	// origine = pieds ; corps comme celui des joueurs (BodyPivot à la hauteur du centre de leur capsule), mais un cube
	BodyPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BodyPivot"));
	BodyPivot->SetupAttachment(GetRootComponent());
	BodyPivot->SetRelativeLocation(FVector(0.0f, 0.0f, 96.0f));

	BodyMesh = YSBoatCrew::CreateShape(this, TEXT("BodyMesh"), BodyPivot, Cube, White, FVector(0.0f, 0.0f, -20.0f), FRotator::ZeroRotator, FVector(0.8f, 0.8f, 1.4f));

	FaceDecal = CreateDefaultSubobject<UYSFaceDecalComponent>(TEXT("FaceDecal"));
	FaceDecal->SetupAttachment(BodyPivot);
	FaceDecal->SetRelativeLocationAndRotation(FVector(42.0f, 0.0f, 10.0f), FRotator(0.0f, 180.0f, 90.0f));
	FaceDecal->DecalSize = FVector(12.0f, 28.0f, 28.0f);

	// chapeau de JohnnyPlaytest : bord (Head Feature) et haut
	HeadFeature = YSBoatCrew::CreateShape(this, TEXT("HeadFeature"), BodyPivot, Cylinder, Red, FVector(-5.4f, 0.0f, 50.0f), FRotator(10.0f, 0.0f, 0.0f), FVector(1.1f, 1.1f, 0.2f));
	HatCrown = YSBoatCrew::CreateShape(this, TEXT("HatCrown"), HeadFeature, Cylinder, Red, FVector(0.0f, 0.0f, 133.6f), FRotator::ZeroRotator, FVector(0.7f, 0.7f, 2.7f));

	// mains devant le corps (droite +Y) ; l'arme est dans la main droite
	LeftHand = CreateDefaultSubobject<USceneComponent>(TEXT("LeftHand"));
	LeftHand->SetupAttachment(BodyPivot);
	LeftHand->SetRelativeLocation(FVector(45.0f, -50.0f, -15.0f));
	RightHand = CreateDefaultSubobject<USceneComponent>(TEXT("RightHand"));
	RightHand->SetupAttachment(BodyPivot);
	RightHand->SetRelativeLocation(FVector(45.0f, 50.0f, -15.0f));
	LeftHandMesh = YSBoatCrew::CreateShape(this, TEXT("LeftHandMesh"), LeftHand, Sphere, White, FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.2f));
	RightHandMesh = YSBoatCrew::CreateShape(this, TEXT("RightHandMesh"), RightHand, Sphere, White, FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.2f));

	Weapon = CreateDefaultSubobject<UChildActorComponent>(TEXT("Weapon"));
	Weapon->SetupAttachment(RightHand);

	// porté couché dans les bras, comme l'ancien Charpentier
	CarryLocation = FVector(70.0f, 90.0f, 20.0f);
	CarryRotation = FRotator(0.0f, -90.0f, 90.0f);
	bShowIndicator = true;
}

void AYSBoatCrew::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSBoatCrew, FreeReviveReadyServerTime);
}

void AYSBoatCrew::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// arme visuelle (classe non répliquée : chaque machine crée la sienne), visible dans l'éditeur
	if (Weapon->GetChildActorClass() != WeaponClass)
	{
		Weapon->SetChildActorClass(WeaponClass);
	}
	if (bHandsUseBodyMaterial && BodyMesh->GetMaterial(0))
	{
		LeftHandMesh->SetMaterial(0, BodyMesh->GetMaterial(0));
		RightHandMesh->SetMaterial(0, BodyMesh->GetMaterial(0));
	}
}

AYSPirateShip* AYSBoatCrew::GetCrewShip() const
{
	const AYSGameState* YSGameState = HasJoined() ? GetWorld()->GetGameState<AYSGameState>() : nullptr;
	AYSPirateShip* Ship = YSGameState ? YSGameState->GetGameBoat() : nullptr;
	return IsValid(Ship) && !Ship->IsSinking() ? Ship : nullptr;
}

void AYSBoatCrew::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AYSPirateShip* Ship = HasAuthority() ? GetCrewShip() : nullptr;
	if (!Ship)
	{
		return;
	}
	if (Bonus == EYSBoatCrewBonus::AutoRepair)
	{
		TickAutoRepair(Ship, DeltaSeconds);
	}
	else if (Bonus == EYSBoatCrewBonus::ShipHeal)
	{
		TickShipHeal(Ship, DeltaSeconds);
	}
}

void AYSBoatCrew::TickAutoRepair(AYSPirateShip* Ship, float DeltaSeconds)
{
	const float Amount = RepairPerSecond * DeltaSeconds;
	if (Amount <= 0.0f)
	{
		return;
	}
	// postes de combat (canons, Gatling posés), chalouperie : tout poste qui a des PV
	for (AYSShipStation* Station : Ship->GetStations())
	{
		Station->AddRepairHealth(Amount);
	}
	if (Ship->CanRepairCore())
	{
		Ship->RepairCoreBy(Amount);
	}
}

void AYSBoatCrew::TickShipHeal(AYSPirateShip* Ship, float DeltaSeconds)
{
	const float Amount = HealPerSecond * DeltaSeconds;
	if (Amount <= 0.0f)
	{
		return;
	}
	// joueurs et minions vivants sur le pont (ou à un poste) du bateau des joueurs
	for (TActorIterator<AYSPlayerCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsAlive() && !It->IsDying() && It->GetShipUnderFeet() == Ship)
		{
			It->AddHealth(Amount);
		}
	}
}

void AYSBoatCrew::OnDelivered()
{
	Super::OnDelivered();
	if (!IsValid(this) || IsActorBeingDestroyed())
	{
		return;
	}
	// debout sur le pont (ou dans la zone), tourné comme il a été posé ; il voyage avec le bateau (accroché par la pose)
	SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw, 0.0f));
	if (Bonus == EYSBoatCrewBonus::FreeRevive)
	{
		FreeReviveReadyServerTime = 0.0;
	}
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSBoatCrew] %s rejoint l'équipage : bonus %s"), *GetName(), *StaticEnum<EYSBoatCrewBonus>()->GetNameStringByValue(static_cast<int64>(Bonus)));
}

bool AYSBoatCrew::IsFreeReviveReady() const
{
	if (Bonus != EYSBoatCrewBonus::FreeRevive || !GetCrewShip())
	{
		return false;
	}
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	return Now >= FreeReviveReadyServerTime;
}

void AYSBoatCrew::ConsumeFreeRevive()
{
	if (!HasAuthority())
	{
		return;
	}
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	FreeReviveReadyServerTime = Now + FreeReviveCooldown;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSBoatCrew] %s : réanimation gratuite utilisée, prête dans %.0f s"), *GetName(), FreeReviveCooldown);
}

AYSBoatCrew* AYSBoatCrew::FindJoined(const UWorld* World, EYSBoatCrewBonus InBonus)
{
	for (TActorIterator<AYSBoatCrew> It(World); It && InBonus != EYSBoatCrewBonus::None; ++It)
	{
		if (It->Bonus == InBonus && It->GetCrewShip() && !It->IsActorBeingDestroyed())
		{
			return *It;
		}
	}
	return nullptr;
}

AYSBoatCrew* AYSBoatCrew::FindFreeRevive(const UWorld* World)
{
	for (TActorIterator<AYSBoatCrew> It(World); It; ++It)
	{
		if (It->IsFreeReviveReady() && !It->IsActorBeingDestroyed())
		{
			return *It;
		}
	}
	return nullptr;
}

float AYSBoatCrew::GetShipSpeedMultiplier(const AYSPirateShip* Ship)
{
	const AYSBoatCrew* Navigator = Ship ? FindJoined(Ship->GetWorld(), EYSBoatCrewBonus::ShipSpeed) : nullptr;
	return Navigator && Navigator->GetCrewShip() == Ship ? 1.0f + Navigator->ShipSpeedBonus : 1.0f;
}

float AYSBoatCrew::GetCannonCooldownMultiplier(const AYSPirateShip* Ship)
{
	const AYSBoatCrew* Gunner = Ship ? FindJoined(Ship->GetWorld(), EYSBoatCrewBonus::CannonCooldown) : nullptr;
	return Gunner && Gunner->GetCrewShip() == Ship ? Gunner->CannonCooldownMultiplier : 1.0f;
}
