#include "Island/YSIslandParts.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Island/YSIsland.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AYSIslandPropChunk::AYSIslandPropChunk()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetNetUpdateFrequency(1.0f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AYSIslandPropChunk::InitChunk(AYSIsland* InIsland, TArray<FYSIslandPropSpawn>&& InProps)
{
	Island = InIsland;
	Props = MoveTemp(InProps);
}

void AYSIslandPropChunk::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSIslandPropChunk, Island);
	DOREPLIFETIME(AYSIslandPropChunk, Props);
}

void AYSIslandPropChunk::BeginPlay()
{
	Super::BeginPlay();
	TrySpawnProps();
}

void AYSIslandPropChunk::OnRep_Chunk()
{
	TrySpawnProps();
}

void AYSIslandPropChunk::TrySpawnProps()
{
	// le serveur pose les props de l'île lui-même
	if (bSpawned || HasAuthority() || !HasActorBegunPlay() || !Island || !Island->IsBuiltLocally() || Props.Num() == 0)
	{
		return;
	}
	bSpawned = true;
	Island->SpawnPropList(Props);
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : %d props reçus et posés (client)"), *GetName(), Props.Num());
}

AYSIslandPier::AYSIslandPier()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Planks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Planks"));
	Planks->SetupAttachment(RootComponent);
	Planks->SetStaticMesh(CubeMesh.Object);
	// on marche dessus : sol des personnages, des traces de sol et des balles (comme le pont d'un bateau)
	Planks->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Planks->SetGenerateOverlapEvents(false);
	Planks->SetCanEverAffectNavigation(false);

	Posts = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Posts"));
	Posts->SetupAttachment(RootComponent);
	Posts->SetStaticMesh(CylinderMesh.Object);
	Posts->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Posts->SetGenerateOverlapEvents(false);
	Posts->SetCanEverAffectNavigation(false);
}

void AYSIslandPier::Build(const TArray<FYSIslandPierSegment>& Segments, const FYSIslandGenerationRules& Rules)
{
	if (Rules.PierMaterial)
	{
		Planks->SetMaterial(0, Rules.PierMaterial);
		Posts->SetMaterial(0, Rules.PierMaterial);
	}
	const float Width = Rules.PierWidth;
	const float Thickness = Rules.PierThickness;
	for (const FYSIslandPierSegment& Segment : Segments)
	{
		const FVector A(Segment.A);
		const FVector B(Segment.B);
		const FVector Delta = B - A;
		const float Length = Delta.Size();
		if (Length < 1.0f)
		{
			continue;
		}
		const FVector Direction = Delta / Length;
		// le dessus de la planche suit A -> B ; morceaux d'une courbe un peu plus longs pour que les joints se recouvrent
		const FRotator Rotation = FRotationMatrix::MakeFromXZ(Direction, FVector::UpVector).Rotator();
		const FVector Up = Rotation.RotateVector(FVector::UpVector);
		const float PlankLength = Length + (Segment.bOverlapEnds ? Width * 0.3f : 0.0f);
		Planks->AddInstance(FTransform(Rotation, (A + B) * 0.5f - Up * (Thickness * 0.5f), FVector(PlankLength / 100.0f, Width / 100.0f, Thickness / 100.0f)));

		if (Segment.bPosts)
		{
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal();
			for (float Along = 0.0f; Along <= Length; Along += Rules.PierPostSpacing)
			{
				const FVector Top = A + Direction * Along - FVector(0.0f, 0.0f, Thickness);
				const float Height = FMath::Max(Top.Z - Rules.SeaFloor, 10.0f);
				for (const float Side : { -1.0f, 1.0f })
				{
					const FVector Post = Top + Right * (Side * (Width * 0.5f - 20.0f)) - FVector(0.0f, 0.0f, Height * 0.5f);
					Posts->AddInstance(FTransform(FRotator::ZeroRotator, Post, FVector(0.3f, 0.3f, Height / 100.0f)));
				}
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] Pontons : %d planches, %d poteaux"), Planks->GetInstanceCount(), Posts->GetInstanceCount());
}
