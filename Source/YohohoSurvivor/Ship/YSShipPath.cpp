#include "Ship/YSShipPath.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "LandscapeProxy.h"
#include "Map/YSOcean.h"

namespace YSShipPath
{
	/** Rayon du cercle par défaut (acteur tout juste posé dans une map, avant Build Around Island) */
	constexpr float DefaultRadius = 12000.0f;
	constexpr int32 DefaultPointCount = 8;

	/** Recherche du fond : depuis haut au-dessus de la mer (collines) jusqu'à loin dessous */
	constexpr float TraceUp = 2000.0f;
	constexpr float TraceDown = 3000.0f;

	/** Essais en traversant ce qui n'est pas le Landscape (bateau, ponton, palmiers) */
	constexpr int32 MaxTraceAttempts = 8;
}

AYSShipPath::AYSShipPath()
{
	PrimaryActorTick.bCanEverTick = false;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	RootComponent = Spline;
	Spline->SetMobility(EComponentMobility::Static);
	Spline->ClearSplinePoints(false);
	for (int32 Index = 0; Index < YSShipPath::DefaultPointCount; ++Index)
	{
		const float Angle = 2.0f * PI * Index / YSShipPath::DefaultPointCount;
		Spline->AddSplinePoint(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * YSShipPath::DefaultRadius, ESplineCoordinateSpace::Local, false);
	}
	Spline->SetClosedLoop(true, true);
}

void AYSShipPath::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("[YSShipPath] %s : %d points, %.0f cm"), *GetName(), Spline->GetNumberOfSplinePoints(), Spline->GetSplineLength());
}

void AYSShipPath::BuildAroundIsland()
{
	UWorld* World = GetWorld();
	const AYSOcean* Ocean = AYSOcean::Find(World);
	if (!World || !Ocean)
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSShipPath] %s : pas d'océan dans la map"), *GetName());
		return;
	}
	const float Sea = Ocean->GetSeaLevel();

	Modify();
	Spline->Modify();
	Spline->ClearSplinePoints(false);
	for (int32 Index = 0; Index < PointCount; ++Index)
	{
		// depuis le large vers le centre : premier fond (Landscape) à moins de Min Shore Depth sous la mer
		const float Angle = 2.0f * PI * Index / PointCount;
		const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		float Radius = ShoreSearchRadius;
		for (float Distance = ShoreSearchRadius; Distance > 0.0f; Distance -= ShoreSearchStep)
		{
			const FVector Point = IslandCenter + Direction * Distance;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(YSShipPathShore), false, this);
			float GroundZ = -TNumericLimits<float>::Max();
			for (int32 Attempt = 0; Attempt < YSShipPath::MaxTraceAttempts; ++Attempt)
			{
				FHitResult Hit;
				if (!World->LineTraceSingleByObjectType(Hit, FVector(Point.X, Point.Y, Sea + YSShipPath::TraceUp), FVector(Point.X, Point.Y, Sea - YSShipPath::TraceDown),
					FCollisionObjectQueryParams(ECC_WorldStatic), Params))
				{
					break;
				}
				if (Hit.GetActor() && Hit.GetActor()->IsA<ALandscapeProxy>())
				{
					GroundZ = Hit.ImpactPoint.Z;
					break;
				}
				Params.AddIgnoredActor(Hit.GetActor());
			}
			if (GroundZ > Sea - MinShoreDepth)
			{
				Radius = Distance + ShoreClearance;
				break;
			}
		}
		Spline->AddSplinePoint(FVector(IslandCenter.X, IslandCenter.Y, Sea) + Direction * Radius, ESplineCoordinateSpace::World, false);
	}
	Spline->SetClosedLoop(true, true);
	UE_LOG(LogTemp, Log, TEXT("[YSShipPath] %s : tour de l'île, %d points, %.0f cm"), *GetName(), Spline->GetNumberOfSplinePoints(), Spline->GetSplineLength());
}

bool AYSShipPath::GetPointAhead(const FVector& From, float LookAhead, FVector& OutPoint) const
{
	const float Length = Spline->GetSplineLength();
	if (Spline->GetNumberOfSplinePoints() < 2 || Length <= 0.0f)
	{
		return false;
	}
	const float Key = Spline->FindInputKeyClosestToWorldLocation(From);
	const float Distance = FMath::Fmod(Spline->GetDistanceAlongSplineAtSplineInputKey(Key) + LookAhead, Length);
	OutPoint = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
	return true;
}
