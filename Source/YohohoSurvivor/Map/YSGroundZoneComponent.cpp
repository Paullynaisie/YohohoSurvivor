#include "Map/YSGroundZoneComponent.h"
#include "Engine/World.h"
#include "Map/YSOcean.h"

namespace YSGroundZone
{
	/** Nombre de sommets sur le tour (bornes) */
	constexpr int32 MinSegments = 16;
	constexpr int32 MaxSegments = 256;
}

UYSGroundZoneComponent::UYSGroundZoneComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// sommets calculés en coordonnées monde autour du composant : ni rotation ni échelle héritées
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetCastShadow(false);
	bUseAsyncCooking = false;
}

float UYSGroundZoneComponent::FindGroundZ(float X, float Y, float ReferenceZ) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return ReferenceZ;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSGroundZone), false, GetOwner());
	const FVector Start(X, Y, ReferenceZ + MaxStepHeight);
	const FVector End(X, Y, ReferenceZ - MaxStepDown);
	return World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params) ? Hit.ImpactPoint.Z : ReferenceZ;
}

void UYSGroundZoneComponent::BuildZone(float Radius, float HeightAboveGround)
{
	ClearAllMeshSections();
	if (Radius <= 0.0f)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const FVector Center = GetComponentLocation();
	const float Spacing = FMath::Max(VertexSpacing, 10.0f);
	const int32 Rings = FMath::Max(1, FMath::CeilToInt(Radius / Spacing));
	const int32 Segments = FMath::Clamp(FMath::CeilToInt(2.0f * PI * Radius / Spacing), YSGroundZone::MinSegments, YSGroundZone::MaxSegments);

	auto SurfaceZ = [this, World](float GroundZ)
	{
		return bClampAboveSea ? AYSOcean::ClampAboveSea(World, GroundZ) : GroundZ;
	};

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	Vertices.Reserve(1 + Rings * Segments);

	// centre, puis anneau par anneau ; chaque rayon suit le sol depuis le centre (un obstacle trop haut est ignoré)
	const float CenterGroundZ = FindGroundZ(Center.X, Center.Y, Center.Z);
	Vertices.Add(FVector(0.0f, 0.0f, SurfaceZ(CenterGroundZ) + HeightAboveGround - Center.Z));
	TArray<float> PreviousGroundZ;
	PreviousGroundZ.Init(CenterGroundZ, Segments);
	TArray<FVector> RingPoints;
	RingPoints.SetNum(Rings * Segments);
	for (int32 Segment = 0; Segment < Segments; ++Segment)
	{
		const float Angle = 2.0f * PI * Segment / Segments;
		const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
		for (int32 Ring = 1; Ring <= Rings; ++Ring)
		{
			const FVector2D Offset = Direction * (Radius * Ring / Rings);
			const float GroundZ = FindGroundZ(Center.X + Offset.X, Center.Y + Offset.Y, PreviousGroundZ[Segment]);
			PreviousGroundZ[Segment] = GroundZ;
			RingPoints[(Ring - 1) * Segments + Segment] = FVector(Offset.X, Offset.Y, SurfaceZ(GroundZ) + HeightAboveGround - Center.Z);
		}
	}
	Vertices.Append(RingPoints);

	for (const FVector& Vertex : Vertices)
	{
		Normals.Add(FVector::UpVector);
		UVs.Add(FVector2D(0.5f + 0.5f * Vertex.X / Radius, 0.5f + 0.5f * Vertex.Y / Radius));
	}

	// éventail au centre puis bandes entre anneaux (matériau double face : l'ordre des sommets importe peu)
	TArray<int32> Triangles;
	Triangles.Reserve(Segments * 3 + (Rings - 1) * Segments * 6);
	auto VertexIndex = [Segments](int32 Ring, int32 Segment)
	{
		return 1 + (Ring - 1) * Segments + (Segment % Segments);
	};
	for (int32 Segment = 0; Segment < Segments; ++Segment)
	{
		Triangles.Append({ 0, VertexIndex(1, Segment + 1), VertexIndex(1, Segment) });
	}
	for (int32 Ring = 1; Ring < Rings; ++Ring)
	{
		for (int32 Segment = 0; Segment < Segments; ++Segment)
		{
			const int32 A = VertexIndex(Ring, Segment);
			const int32 B = VertexIndex(Ring, Segment + 1);
			const int32 C = VertexIndex(Ring + 1, Segment);
			const int32 D = VertexIndex(Ring + 1, Segment + 1);
			Triangles.Append({ A, B, C, B, D, C });
		}
	}

	CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
}

void UYSGroundZoneComponent::BuildStrip(const FVector& Direction, float Length, float Width, float HeightAboveGround)
{
	ClearAllMeshSections();
	const FVector Forward = FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal();
	if (Length <= 0.0f || Width <= 0.0f || Forward.IsNearlyZero())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const FVector Center = GetComponentLocation();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const int32 Rows = FMath::Max(1, FMath::CeilToInt(Length / FMath::Max(VertexSpacing, 10.0f))) + 1;
	constexpr int32 Columns = 3;

	// chaque colonne (bord gauche, milieu, bord droit) suit le sol de proche en proche depuis le départ
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	float PreviousGroundZ[Columns];
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		const float Along = Length * Row / (Rows - 1);
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			const float Across = Width * (static_cast<float>(Column) / (Columns - 1) - 0.5f);
			const FVector Point = Center + Forward * Along + Right * Across;
			const float GroundZ = FindGroundZ(Point.X, Point.Y, Row == 0 ? Center.Z : PreviousGroundZ[Column]);
			PreviousGroundZ[Column] = GroundZ;
			const float SurfaceZ = bClampAboveSea ? AYSOcean::ClampAboveSea(World, GroundZ) : GroundZ;
			Vertices.Add(FVector(Point.X - Center.X, Point.Y - Center.Y, SurfaceZ + HeightAboveGround - Center.Z));
			Normals.Add(FVector::UpVector);
			UVs.Add(FVector2D(Along / Length, static_cast<float>(Column) / (Columns - 1)));
		}
	}

	TArray<int32> Triangles;
	for (int32 Row = 0; Row + 1 < Rows; ++Row)
	{
		for (int32 Column = 0; Column + 1 < Columns; ++Column)
		{
			const int32 A = Row * Columns + Column;
			const int32 B = A + 1;
			const int32 C = A + Columns;
			const int32 D = C + 1;
			Triangles.Append({ A, C, B, B, C, D });
		}
	}

	CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
}
