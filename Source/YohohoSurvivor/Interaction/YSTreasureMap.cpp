#include "Interaction/YSTreasureMap.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Core/YSGameMode.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "LandscapeProxy.h"
#include "Map/YSOcean.h"
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
		|| FMath::Abs(Hit.ImpactPoint.Z - ReferenceZ) > MaxHeight
		|| AYSOcean::IsUnderwater(World, Hit.ImpactPoint))
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

		// chemin arrondi : la courbe affichée doit aussi rester sur le sol (elle déborde des segments dans les virages)
		if (OutPoints.Num() == SegmentCount + 1 && (!bSmoothPath || IsSmoothPathGrounded(World, OutPoints)))
		{
			return true;
		}
	}
	return false;
}

bool AYSTreasureMap::IsSmoothPathGrounded(const UWorld* World, const TArray<FVector>& Points) const
{
	// même courbe que la spline en points Curve (tangentes automatiques), vérifiée tous les GroundCheckStep / 4
	FInterpCurveVector Curve;
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		const int32 Added = Curve.AddPoint(static_cast<float>(Index), Points[Index]);
		Curve.Points[Added].InterpMode = CIM_CurveAuto;
	}
	Curve.AutoSetTangents(0.0f, false);

	for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
	{
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Dist(Points[Index], Points[Index + 1]) / (YSTreasureMap::GroundCheckStep * 0.25f)));
		for (int32 Step = 1; Step < Steps; ++Step)
		{
			const FVector Sample = Curve.Eval(Index + static_cast<float>(Step) / Steps);
			FVector Ground;
			if (!FindPathGround(World, Sample, Sample.Z, MaxHeightDifference, Ground))
			{
				return false;
			}
		}
	}
	return true;
}

void AYSTreasureMap::SetPath(const TArray<FVector>& Points)
{
	PathPoints = Points;
	ReachedPointIndex = 0;
	RebuildSpline();
	ForceNetUpdate();
	LogDisplayedPathCheck();
}

void AYSTreasureMap::LogDisplayedPathCheck()
{
	// le chemin affiché (spline) doit rester sur le sol vérifié par BuildPath : écart aux segments droits et pointillés sans sol ;
	// mesuré pour le chemin arrondi et pour les segments droits
	auto Measure = [this](float& OutMaxDeviation, int32& OutUngrounded) -> int32
	{
		int32 Samples = 0;
		OutMaxDeviation = 0.0f;
		OutUngrounded = 0;
		const FVector Flat(1.0f, 1.0f, 0.0f);
		for (float Distance = 0.0f; Distance <= PathSpline->GetSplineLength(); Distance += DashSpacing, ++Samples)
		{
			const FVector Location = PathSpline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
			float Deviation = TNumericLimits<float>::Max();
			for (int32 Index = 0; Index + 1 < PathPoints.Num(); ++Index)
			{
				const FVector Closest = FMath::ClosestPointOnSegment(Location * Flat, PathPoints[Index] * Flat, PathPoints[Index + 1] * Flat);
				Deviation = FMath::Min(Deviation, static_cast<float>(FVector::Dist2D(Location, Closest)));
			}
			OutMaxDeviation = FMath::Max(OutMaxDeviation, Deviation);
			FVector Ground;
			OutUngrounded += FindPathGround(GetWorld(), Location, Location.Z, MaxHeightDifference, Ground) ? 0 : 1;

			// pointillé posé sur autre chose que l'île (ponton, bateau...)
			FHitResult Hit;
			const FVector Up(0.0f, 0.0f, YSTreasureMap::GroundTraceRange);
			if (GetWorld()->LineTraceSingleByObjectType(Hit, Location + Up, Location - Up, FCollisionObjectQueryParams(ECC_WorldStatic)) && !(Hit.GetActor() && Hit.GetActor()->IsA<ALandscapeProxy>()))
			{
				UE_LOG(LogTemp, Verbose, TEXT("[YSTreasureMap] %s : pointillé à %.0f cm sur %s"), *GetName(), Distance, Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("?"));
			}
		}
		return Samples;
	};

	const bool bWasSmooth = bSmoothPath;
	float Deviation[2];
	int32 Ungrounded[2];
	int32 Samples[2];
	for (int32 Smooth = 0; Smooth < 2; ++Smooth)
	{
		bSmoothPath = Smooth == 1;
		RebuildSpline();
		Samples[Smooth] = Measure(Deviation[Smooth], Ungrounded[Smooth]);
	}
	bSmoothPath = bWasSmooth;
	RebuildSpline();

	UE_LOG(LogTemp, Verbose, TEXT("[YSTreasureMap] %s : chemin de %.0f cm (%s) ; segments droits : écart %.0f cm, %d / %d pointillés sans sol ; arrondi : écart %.0f cm, %d / %d sans sol"),
		*GetName(), PathSpline->GetSplineLength(), bSmoothPath ? TEXT("arrondi") : TEXT("segments droits"), Deviation[0], Ungrounded[0], Samples[0], Deviation[1], Ungrounded[1], Samples[1]);
}

void AYSTreasureMap::OnRep_PathPoints()
{
	RebuildSpline();
}

void AYSTreasureMap::RebuildSpline()
{
	PathSpline->ClearSplinePoints(false);
	for (int32 Index = 0; Index < PathPoints.Num(); ++Index)
	{
		PathSpline->AddSplinePoint(PathPoints[Index], ESplineCoordinateSpace::World, false);
		PathSpline->SetSplinePointType(Index, bSmoothPath ? ESplinePointType::Curve : ESplinePointType::Linear, false);
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

	// n'importe quel point plus loin sur le chemin (le plus loin d'abord) : un virage coupé sans passer près du coin
	// ne bloque plus la progression (avant : seul le point suivant comptait, le trésor n'était alors jamais trouvé)
	int32 Reached = INDEX_NONE;
	for (int32 Index = PathPoints.Num() - 1; Index > ReachedPointIndex && Reached == INDEX_NONE; --Index)
	{
		Reached = FVector::Dist2D(Character->GetActorLocation(), PathPoints[Index]) <= ReachRadius ? Index : INDEX_NONE;
	}
	if (Reached == INDEX_NONE)
	{
		return;
	}

	const int32 PreviousReached = ReachedPointIndex;
	ReachedPointIndex = Reached;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSTreasureMap] %s : %s atteint le point %d / %d%s"), *GetName(), *Character->GetName(), ReachedPointIndex, PathPoints.Num() - 1,
		Reached > PreviousReached + 1 ? *FString::Printf(TEXT(" (%d point(s) passé(s) de loin)"), Reached - PreviousReached - 1) : TEXT(""));

	// milieu du chemin : grosse vague (aussi si le point a été passé de loin)
	if (WaveAtPoint > PreviousReached && WaveAtPoint <= ReachedPointIndex)
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

bool AYSTreasureMap::GetGuideIndicatorLocation(FVector& OutLocation) const
{
	// joueur local qui porte la carte, trop loin du chemin pour en voir un seul pointillé
	const AYSPlayerCharacter* Character = GetCarrier();
	if (!bShowGuideIndicator || !Character || !Character->IsLocallyControlled() || VisibleDashCount > 0 || PathSpline->GetNumberOfSplinePoints() < 2)
	{
		return false;
	}
	// début du chemin restant = dernier point atteint
	const int32 StartPoint = FMath::Clamp(ReachedPointIndex, 0, PathSpline->GetNumberOfSplinePoints() - 1);
	OutLocation = PathSpline->GetLocationAtSplinePoint(StartPoint, ESplineCoordinateSpace::World) + FVector(0.0f, 0.0f, GuideIndicatorHeight);
	return true;
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
		// pas de fondu à rejouer en reprenant une carte déjà avancée
		LastSeenReachedPointIndex = ReachedPointIndex;
		return;
	}

	const FVector Center = Character->GetActorLocation();
	const int32 LastPoint = PathSpline->GetNumberOfSplinePoints() - 1;
	const float StartDistance = bHideTravelledPath ? PathSpline->GetDistanceAlongSplineAtSplinePoint(FMath::Clamp(ReachedPointIndex, 0, LastPoint)) : 0.0f;
	const float VisibleRadiusSquared = FMath::Square(VisibleRadius);

	// point atteint : le segment terminé rétrécit au lieu de disparaître d'un coup
	const double Now = GetWorld()->GetTimeSeconds();
	if (ReachedPointIndex != LastSeenReachedPointIndex)
	{
		TravelledFadeFromDistance = PathSpline->GetDistanceAlongSplineAtSplinePoint(FMath::Clamp(LastSeenReachedPointIndex, 0, LastPoint));
		TravelledFadeStartTime = Now;
		LastSeenReachedPointIndex = ReachedPointIndex;
	}
	const float TravelledFade = TravelledFadeDuration > 0.0f ? 1.0f - FMath::Clamp(static_cast<float>(Now - TravelledFadeStartTime) / TravelledFadeDuration, 0.0f, 1.0f) : 0.0f;
	const float FirstDistance = TravelledFade > 0.0f ? FMath::Min(TravelledFadeFromDistance, StartDistance) : StartDistance;

	TArray<FTransform> Transforms;
	// grille fixe le long du chemin : les pointillés ne sautent pas quand le début affiché change
	for (float Distance = FMath::FloorToFloat(FirstDistance / DashSpacing) * DashSpacing; Distance < SplineLength; Distance += DashSpacing)
	{
		if (Distance < FirstDistance)
		{
			continue;
		}
		const float DashCenter = FMath::Min(Distance + DashLength * 0.5f, SplineLength);
		FVector Location = PathSpline->GetLocationAtDistanceAlongSpline(DashCenter, ESplineCoordinateSpace::World);
		const float DistanceSquared = FVector::DistSquared2D(Location, Center);
		if (DistanceSquared > VisibleRadiusSquared)
		{
			continue;
		}

		// rétrécit avant la limite d'affichage, et sur le segment qui vient d'être terminé
		float Fade = FadeDistance > 0.0f ? FMath::Clamp((VisibleRadius - FMath::Sqrt(DistanceSquared)) / FadeDistance, 0.0f, 1.0f) : 1.0f;
		if (Distance < StartDistance)
		{
			Fade *= TravelledFade;
		}
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
