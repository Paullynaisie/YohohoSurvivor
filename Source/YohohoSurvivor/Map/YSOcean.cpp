#include "Map/YSOcean.h"
#include "Core/YSCombat.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AYSOcean::AYSOcean()
{
	// écume autour de la caméra locale
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
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
	// NME (canal d'objet Foe) : ils nagent aussi (leur capsule ne bloque le canal Water que tant qu'ils peuvent nager)
	SwimFloor->SetCollisionResponseToChannel(ECC_YSFoe, ECR_Block);
	SwimFloor->SetGenerateOverlapEvents(false);
	SwimFloor->SetCanEverAffectNavigation(false);
	SwimFloor->SetHiddenInGame(true);

	FoamMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FoamMesh"));
	FoamMesh->SetupAttachment(Surface);
	FoamMesh->SetUsingAbsoluteLocation(true);
	FoamMesh->SetUsingAbsoluteRotation(true);
	FoamMesh->SetUsingAbsoluteScale(true);
	FoamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FoamMesh->SetCastShadow(false);
	FoamMesh->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FoamMaterialFinder(TEXT("/Game/Core/Ship/M_ShipPreview.M_ShipPreview"));
	if (FoamMaterialFinder.Succeeded())
	{
		FoamMaterial = FoamMaterialFinder.Object;
	}
}

void AYSOcean::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bFoam || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// centre = ce que suit la caméra du joueur local (son personnage)
	const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	const AActor* ViewTarget = Controller ? Controller->GetViewTarget() : nullptr;
	if (!ViewTarget)
	{
		return;
	}
	const FVector Location = ViewTarget->GetActorLocation();
	const float Cell = FMath::Max(FoamCellSize, 100.0f);
	const FIntPoint Center(FMath::FloorToInt(Location.X / Cell), FMath::FloorToInt(Location.Y / Cell));
	if (Center != FoamCenterCell)
	{
		FoamCenterCell = Center;
		RebuildFoam(Center);
	}
}

void AYSOcean::RebuildFoam(const FIntPoint& Center)
{
	constexpr int32 Sides = 10;
	const float Cell = FMath::Max(FoamCellSize, 100.0f);
	const float Z = GetSeaLevel() + FoamHeightAboveSea;
	FLinearColor Edge = FoamColor;
	Edge.A = 0.0f;
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FLinearColor> Colors;
	for (int32 X = Center.X - FoamGridRadius; X <= Center.X + FoamGridRadius; ++X)
	{
		for (int32 Y = Center.Y - FoamGridRadius; Y <= Center.Y + FoamGridRadius; ++Y)
		{
			// tirage propre à la case : mêmes taches à chaque reconstruction
			FRandomStream Stream(static_cast<int32>(HashCombine(GetTypeHash(X * 73856093), GetTypeHash(Y * 19349663))));
			const int32 Count = Stream.RandRange(FMath::Min(FoamPerCell.X, FoamPerCell.Y), FMath::Max(FoamPerCell.X, FoamPerCell.Y));
			for (int32 Patch = 0; Patch < Count; ++Patch)
			{
				const FVector PatchCenter((X + Stream.FRand()) * Cell, (Y + Stream.FRand()) * Cell, Z);
				const float Radius = Stream.FRandRange(FoamRadius.X, FMath::Max(FoamRadius.X, FoamRadius.Y));
				const float Stretch = Stream.FRandRange(FoamStretch.X, FMath::Max(FoamStretch.X, FoamStretch.Y));
				const float Yaw = Stream.FRandRange(0.0f, 2.0f * PI);
				const FVector Long(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0f);
				const FVector Short(-Long.Y, Long.X, 0.0f);
				// tache irrégulière : centre opaque, bord transparent, rayon qui varie autour
				const int32 Base = Vertices.Num();
				Vertices.Add(PatchCenter);
				Colors.Add(FoamColor);
				for (int32 K = 0; K < Sides; ++K)
				{
					const float Angle = 2.0f * PI * K / Sides;
					const float Jitter = Stream.FRandRange(0.65f, 1.0f);
					Vertices.Add(PatchCenter + (Long * (FMath::Cos(Angle) * Stretch) + Short * FMath::Sin(Angle)) * (Radius * Jitter));
					Colors.Add(Edge);
				}
				for (int32 K = 0; K < Sides; ++K)
				{
					Triangles.Append({ Base, Base + 1 + (K + 1) % Sides, Base + 1 + K });
				}
			}
		}
	}
	FoamMesh->ClearAllMeshSections();
	if (Vertices.Num() > 0)
	{
		FoamMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, TArray<FVector>(), TArray<FVector2D>(), Colors, TArray<FProcMeshTangent>(), false);
		FoamMesh->SetMaterial(0, FoamMaterial);
	}
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
	// réponses imposées ici aussi (océan sauvegardé dans les maps avant que les NME nagent)
	SwimFloor->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	SwimFloor->SetCollisionResponseToChannel(ECC_YSFoe, ECR_Block);
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
