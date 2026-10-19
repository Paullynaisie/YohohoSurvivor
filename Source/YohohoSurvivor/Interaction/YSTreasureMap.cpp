#include "Interaction/YSTreasureMap.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Core/YSGameMode.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"

namespace YSTreasureMap
{
	/** Recherche du sol : depuis loin au-dessus (dessus des obstacles compris) jusqu'à loin en dessous */
	constexpr float GroundTraceRange = 5000.0f;

	/** Entre deux points, le sol est vérifié tous les GroundCheckStep cm (pas de vide sous le chemin) */
	constexpr float GroundCheckStep = 200.0f;

	/** Un nouveau point reste à au moins cette part de MinSegmentLength des points plus anciens (pas de boucle) */
	constexpr float MinDistanceToOlderPoints = 0.75f;

	/** Chemins complets essayés depuis un même départ */
	constexpr int32 PathAttempts = 10;

	/** Le cube de base mesure 100 cm */
	constexpr float CubeSize = 100.0f;

	/** Épaisseur d'un pointillé */
	constexpr float DashThickness = 1.0f;
}

AYSTreasureMap::AYSTreasureMap()
{
	PrimaryActorTick.bCanEverTick = true;

	// coordonnées monde : le chemin reste en place quand la carte est portée
	PathSpline = CreateDefaultSubobject<USplineComponent>(TEXT("PathSpline"));
	PathSpline->SetupAttachment(GetRootComponent());
	PathSpline->SetUsingAbsoluteLocation(true);
	PathSpline->SetUsingAbsoluteRotation(true);
	PathSpline->SetUsingAbsoluteScale(true);
	PathSpline->ClearSplinePoints();

	PathDashes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PathDashes"));
	PathDashes->SetupAttachment(GetRootComponent());
	PathDashes->SetUsingAbsoluteLocation(true);
	PathDashes->SetUsingAbsoluteRotation(true);
	PathDashes->SetUsingAbsoluteScale(true);
	PathDashes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PathDashes->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		DashMesh = CubeMesh.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterial.Succeeded())
	{
		DashMaterial = BasicMaterial.Object;
	}

	DisplayName = NSLOCTEXT("YS", "TreasureMap", "Carte au trésor");
	DoubloonValue = 0;
}

void AYSTreasureMap::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSTreasureMap, PathPoints);
	DOREPLIFETIME(AYSTreasureMap, ReachedPointIndex);
}

void AYSTreasureMap::BeginPlay()
{
	Super::BeginPlay();

	PathSpline->SetWorldTransform(FTransform::Identity);
	PathDashes->SetWorldTransform(FTransform::Identity);
	PathDashes->SetStaticMesh(DashMesh);
	if (DashMaterial)
	{
		DashMaterialInstance = UMaterialInstanceDynamic::Create(DashMaterial, this);
		DashMaterialInstance->SetVectorParameterValue(DashColorParameterName, DashColor);
		PathDashes->SetMaterial(0, DashMaterialInstance);
	}
	RebuildSpline();
}

void AYSTreasureMap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		UpdateProgress();
	}
	UpdateDashes();
}

bool AYSTreasureMap::FindPathGround(const UWorld* World, const FVector& Location, float ReferenceZ, float MaxHeight, FVector& OutGround)
{
	if (!World)
	{
		return false;
	}

	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(YSTreasurePathGround), false);
	const FVector Start(Location.X, Location.Y, ReferenceZ + YSTreasureMap::GroundTraceRange);
	const FVector End(Location.X, Location.Y, ReferenceZ - YSTreasureMap::GroundTraceRange);
	if (!World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params)
		|| FMath::Abs(Hit.ImpactPoint.Z - ReferenceZ) > MaxHeight)
	{
		return false;
	}
	OutGround = Hit.ImpactPoint;
	return true;
}

bool AYSTreasureMap::BuildPath(const UWorld* World, const FVector& Start, TArray<FVector>& OutPoints) const
{
	for (int32 PathAttempt = 0; PathAttempt < YSTreasureMap::PathAttempts; ++PathAttempt)
	{
		OutPoints.Reset();
		OutPoints.Add(Start);
		float PreviousYaw = FMath::FRandRange(0.0f, 360.0f);

		for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
		{
			const FVector Previous = OutPoints.Last();
			bool bFound = false;
			for (int32 Attempt = 0; Attempt < PointAttempts && !bFound; ++Attempt)
			{
				// premier segment dans n'importe quelle direction, ensuite sans demi-tour
				const float Yaw = Segment == 0 ? FMath::FRandRange(0.0f, 360.0f) : PreviousYaw + FMath::FRandRange(-MaxTurnAngle, MaxTurnAngle);
				const FVector Direction = FRotator(0.0f, Yaw, 0.0f).Vector();
				const float Length = FMath::FRandRange(MinSegmentLength, FMath::Max(MinSegmentLength, MaxSegmentLength));

				FVector Point;
				if (!FindPathGround(World, Previous + Direction * Length, Previous.Z, MaxHeightDifference, Point))
				{
					continue;
				}

				// pas de boucle : loin des points plus anciens que le précédent
				bool bTooClose = false;
				for (int32 Index = 0; Index < OutPoints.Num() - 1 && !bTooClose; ++Index)
				{
					bTooClose = FVector::Dist2D(Point, OutPoints[Index]) < MinSegmentLength * YSTreasureMap::MinDistanceToOlderPoints;
				}
				if (bTooClose)
				{
					continue;
				}

				// du sol tout le long du segment (pas de vide, pas de dessus d'obstacle)
				bool bGrounded = true;
				const int32 Checks = FMath::CeilToInt(Length / YSTreasureMap::GroundCheckStep);
				for (int32 Check = 1; Check < Checks && bGrounded; ++Check)
				{
					const FVector Sample = FMath::Lerp(Previous, Point, static_cast<float>(Check) / Checks);
					FVector SampleGround;
					bGrounded = FindPathGround(World, Sample, Sample.Z, MaxHeightDifference, SampleGround);
				}
				if (!bGrounded)
				{
					continue;
				}

				OutPoints.Add(Point);
				PreviousYaw = Yaw;
				bFound = true;
			}

			if (!bFound)
			{
				break;
			}
		}

		if (OutPoints.Num() == SegmentCount + 1)
		{
			return true;
		}
	}
	return false;
}

void AYSTreasureMap::SetPath(const TArray<FVector>& Points)
{
	PathPoints = Points;
	ReachedPointIndex = 0;
	RebuildSpline();
	ForceNetUpdate();
}

void AYSTreasureMap::OnRep_PathPoints()
{
	RebuildSpline();
}

void AYSTreasureMap::RebuildSpline()
{
	PathSpline->ClearSplinePoints(false);
	for (const FVector& Point : PathPoints)
	{
		PathSpline->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
	}
	PathSpline->UpdateSpline();
}

void AYSTreasureMap::UpdateProgress()
{
	AYSPlayerCharacter* Character = GetCarrier();
	if (!Character || PathPoints.Num() < 2 || ReachedPointIndex >= PathPoints.Num() - 1)
	{
		return;
	}

	if (FVector::Dist2D(Character->GetActorLocation(), PathPoints[ReachedPointIndex + 1]) > ReachRadius)
	{
		return;
	}

	++ReachedPointIndex;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSTreasureMap] %s : %s atteint le point %d / %d"), *GetName(), *Character->GetName(), ReachedPointIndex, PathPoints.Num() - 1);

	// milieu du chemin : grosse vague
	if (ReachedPointIndex == WaveAtPoint)
	{
		if (AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>())
		{
			GameMode->SpawnBonusWave(WaveEnemiesPerPlayer, TEXT("carte au trésor"));
		}
	}

	if (ReachedPointIndex == PathPoints.Num() - 1)
	{
		Complete();
	}
}

void AYSTreasureMap::Complete()
{
	if (AYSPlayerCharacter* Character = GetCarrier())
	{
		Character->DropCarriedObject(TEXT("trésor trouvé"));
	}

	const AYSInteractiveObject* Reward = AYSInteractiveObject::SpawnOnGround(GetWorld(), RewardClass, PathPoints.Last(), GetActorRotation().Yaw);
	UE_LOG(LogTemp, Log, TEXT("[YSTreasureMap] %s terminée : %s"), *GetName(), Reward ? *Reward->GetName() : TEXT("pas de récompense (Reward Class vide ?)"));
	Destroy();
}

void AYSTreasureMap::UpdateDashes()
{
	// seul le joueur local qui porte la carte voit le chemin
	const AYSPlayerCharacter* Character = GetCarrier();
	const float SplineLength = PathSpline->GetSplineLength();
	if (!Character || !Character->IsLocallyControlled() || PathSpline->GetNumberOfSplinePoints() < 2 || SplineLength <= 0.0f)
	{
		if (VisibleDashCount > 0)
		{
			PathDashes->ClearInstances();
			VisibleDashCount = 0;
		}
		return;
	}

	const FVector Center = Character->GetActorLocation();
	const float StartDistance = bHideTravelledPath ? PathSpline->GetDistanceAlongSplineAtSplinePoint(FMath::Clamp(ReachedPointIndex, 0, PathSpline->GetNumberOfSplinePoints() - 1)) : 0.0f;
	const float VisibleRadiusSquared = FMath::Square(VisibleRadius);

	TArray<FTransform> Transforms;
	for (float Distance = StartDistance; Distance < SplineLength; Distance += DashSpacing)
	{
		const float DashCenter = FMath::Min(Distance + DashLength * 0.5f, SplineLength);
		FVector Location = PathSpline->GetLocationAtDistanceAlongSpline(DashCenter, ESplineCoordinateSpace::World);
		const float DistanceSquared = FVector::DistSquared2D(Location, Center);
		if (DistanceSquared > VisibleRadiusSquared)
		{
			continue;
		}

		// rétrécit avant la limite d'affichage
		const float Fade = FadeDistance > 0.0f ? FMath::Clamp((VisibleRadius - FMath::Sqrt(DistanceSquared)) / FadeDistance, 0.0f, 1.0f) : 1.0f;
		if (Fade <= 0.0f)
		{
			continue;
		}

		// posé sur le sol (la spline passe entre les points, le sol peut varier)
		FVector Ground;
		if (FindPathGround(GetWorld(), Location, Location.Z, MaxHeightDifference, Ground))
		{
			Location.Z = Ground.Z;
		}
		Location.Z += DashHeightOffset + YSTreasureMap::DashThickness * 0.5f;

		const FVector Direction = PathSpline->GetDirectionAtDistanceAlongSpline(DashCenter, ESplineCoordinateSpace::World);
		const FVector Scale(DashLength * Fade / YSTreasureMap::CubeSize, DashWidth * Fade / YSTreasureMap::CubeSize, YSTreasureMap::DashThickness / YSTreasureMap::CubeSize);
		Transforms.Emplace(FRotator(0.0f, Direction.Rotation().Yaw, 0.0f), Location, Scale);
	}

	PathDashes->ClearInstances();
	PathDashes->AddInstances(Transforms, false, true);
	VisibleDashCount = Transforms.Num();
}
