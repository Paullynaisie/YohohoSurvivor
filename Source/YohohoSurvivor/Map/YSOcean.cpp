#include "Map/YSOcean.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

AYSOcean::AYSOcean()
{
	PrimaryActorTick.bCanEverTick = false;
	// placé dans la map : présent sur toutes les machines, rien à répliquer
	bReplicates = false;

	// surface : aucune collision, les joueurs passent au travers
	Surface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Surface"));
	RootComponent = Surface;
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Surface->SetCollisionResponseToAllChannels(ECR_Ignore);
	Surface->SetGenerateOverlapEvents(false);
	Surface->SetCanEverAffectNavigation(false);
	Surface->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane.Succeeded())
	{
		Surface->SetStaticMesh(Plane.Object);
	}

	// sol de nage : seuls les Pawns qui bloquent le canal Water (joueurs, minions) marchent dessus ; invisible, rien d'autre ne le touche
	SwimFloor = CreateDefaultSubobject<UBoxComponent>(TEXT("SwimFloor"));
	SwimFloor->SetupAttachment(Surface);
	SwimFloor->SetUsingAbsoluteLocation(true);
	SwimFloor->SetUsingAbsoluteRotation(true);
	SwimFloor->SetUsingAbsoluteScale(true);
	SwimFloor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SwimFloor->SetCollisionObjectType(ECC_YSWater);
	SwimFloor->SetCollisionResponseToAllChannels(ECR_Ignore);
	SwimFloor->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	SwimFloor->SetGenerateOverlapEvents(false);
	SwimFloor->SetCanEverAffectNavigation(false);
	SwimFloor->SetHiddenInGame(true);
}

void AYSOcean::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplySwimFloor();
}

void AYSOcean::BeginPlay()
{
	Super::BeginPlay();
	// OnConstruction ne tourne pas pour un acteur chargé avec la map
	ApplySwimFloor();
}

void AYSOcean::ApplySwimFloor()
{
	const FBoxSphereBounds Bounds = Surface->CalcBounds(Surface->GetComponentTransform());
	const float Thickness = 20.0f;
	SwimFloor->SetBoxExtent(FVector(FMath::Max(Bounds.BoxExtent.X, 100.0f), FMath::Max(Bounds.BoxExtent.Y, 100.0f), Thickness * 0.5f));
	SwimFloor->SetWorldLocationAndRotation(FVector(Bounds.Origin.X, Bounds.Origin.Y, GetSeaLevel() - SwimFloorDepth - Thickness * 0.5f), FRotator::ZeroRotator);
}

float AYSOcean::GetWaterDepth(const UWorld* World, const FVector& Point)
{
	const AYSOcean* Ocean = Find(World);
	return Ocean ? FMath::Max(Ocean->GetSeaLevel() - Point.Z, 0.0f) : 0.0f;
}

const AYSOcean* AYSOcean::Find(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AYSOcean> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

bool AYSOcean::IsUnderwater(const UWorld* World, const FVector& GroundPoint)
{
	const AYSOcean* Ocean = Find(World);
	return Ocean && GroundPoint.Z < Ocean->GetSeaLevel() + Ocean->ShoreMargin;
}

float AYSOcean::ClampAboveSea(const UWorld* World, float Z)
{
	const AYSOcean* Ocean = Find(World);
	return Ocean ? FMath::Max(Z, Ocean->GetSeaLevel()) : Z;
}
