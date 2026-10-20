#include "Map/YSOcean.h"
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
