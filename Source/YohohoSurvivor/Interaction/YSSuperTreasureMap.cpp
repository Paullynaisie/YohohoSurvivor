#include "Interaction/YSSuperTreasureMap.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Enemies/YSDavyJonesArena.h"
#include "Interaction/YSTreasureMap.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Map/YSOcean.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/YSPlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"

AYSSuperTreasureMap::AYSSuperTreasureMap()
{
	PrimaryActorTick.bCanEverTick = true;

	GuideDashes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GuideDashes"));
	GuideDashes->SetupAttachment(GetRootComponent());
	GuideDashes->SetUsingAbsoluteLocation(true);
	GuideDashes->SetUsingAbsoluteRotation(true);
	GuideDashes->SetUsingAbsoluteScale(true);
	GuideDashes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GuideDashes->SetCastShadow(false);

	GuideArrows = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GuideArrows"));
	GuideArrows->SetupAttachment(GetRootComponent());
	GuideArrows->SetUsingAbsoluteLocation(true);
	GuideArrows->SetUsingAbsoluteRotation(true);
	GuideArrows->SetUsingAbsoluteScale(true);
	GuideArrows->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GuideArrows->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		DashMesh = CubeMesh.Object;
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded())
	{
		ArrowMesh = ConeMesh.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterial.Succeeded())
	{
		DashMaterial = BasicMaterial.Object;
	}

	DisplayName = NSLOCTEXT("YS", "SuperTreasureMap", "Super carte au trésor");
}

bool AYSSuperTreasureMap::CanInteract(const AYSPlayerCharacter* Character) const
{
	// comme un objet portable, livré ou non
	return IsCarriable() && !GetCarrier() && Character && Character->IsAlive() && !Character->IsCarrying() && !IsActorBeingDestroyed();
}

void AYSSuperTreasureMap::BeginPlay()
{
	Super::BeginPlay();

	GuideDashes->SetWorldTransform(FTransform::Identity);
	GuideDashes->SetStaticMesh(DashMesh);
	GuideArrows->SetWorldTransform(FTransform::Identity);
	GuideArrows->SetStaticMesh(ArrowMesh ? ArrowMesh.Get() : DashMesh.Get());
	if (DashMaterial)
	{
		DashMaterialInstance = UMaterialInstanceDynamic::Create(DashMaterial, this);
		DashMaterialInstance->SetVectorParameterValue(DashColorParameterName, DashColor);
		GuideDashes->SetMaterial(0, DashMaterialInstance);
		GuideArrows->SetMaterial(0, DashMaterialInstance);
	}
}

void AYSSuperTreasureMap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshTimer -= DeltaSeconds;
	if (RefreshTimer <= 0.0f)
	{
		RefreshTimer = RefreshInterval;
		UpdateDashes();
	}
}

void AYSSuperTreasureMap::UpdateDashes()
{
	// seul le joueur local qui tient la carte voit la ligne, tant que le combat contre Davy Jones n'a pas commencé
	const AYSPlayerCharacter* Character = GetCarrier();
	const AYSDavyJonesArena* Arena = AYSDavyJonesArena::Find(GetWorld());
	const bool bShow = Character && Character->IsLocallyControlled() && Arena && !Arena->IsFightStarted();
	if (!bShow)
	{
		if (bDashesVisible)
		{
			bDashesVisible = false;
			GuideDashes->ClearInstances();
			GuideArrows->ClearInstances();
			UE_LOG(LogTemp, Log, TEXT("[YSSuperTreasureMap] %s : pointillés vers Davy Jones retirés"), *GetName());
		}
		return;
	}
	if (!bDashesVisible)
	{
		bDashesVisible = true;
		UE_LOG(LogTemp, Log, TEXT("[YSSuperTreasureMap] %s : pointillés vers Davy Jones (%s, %.0f m)"), *GetName(), *Arena->GetActorLocation().ToCompactString(),
			FVector::Dist2D(Character->GetActorLocation(), Arena->GetActorLocation()) / 100.0f);
	}

	const AYSOcean* Ocean = AYSOcean::Find(GetWorld());
	const float Sea = Ocean ? Ocean->GetSeaLevel() : Arena->GetActorLocation().Z;
	const FVector Start = Character->GetActorLocation();
	const FVector End = Arena->GetActorLocation();
	const FVector Direction = (End - Start).GetSafeNormal2D();
	const float Length = FVector::Dist2D(Start, End);
	const float Yaw = Direction.Rotation().Yaw;
	const FVector Scale(DashLength / 100.0f, DashWidth / 100.0f, DashThickness / 100.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSSuperTreasureMapDash), false, Character);
	Params.AddIgnoredActor(this);

	// près du joueur : posé sur ce qu'il y a dessous (île, pont du bateau), jamais sous la mer
	auto PlaceOnSurface = [&](FVector& Location)
	{
		Location.Z = Sea;
		if (FVector::Dist2D(Location, Start) <= GroundFollowRadius)
		{
			FHitResult Hit;
			const FVector Top(Location.X, Location.Y, Start.Z + 1500.0f);
			const FVector Bottom(Location.X, Location.Y, Sea - 500.0f);
			if (GetWorld()->LineTraceSingleByObjectType(Hit, Top, Bottom, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
			{
				Location.Z = FMath::Max(Hit.ImpactPoint.Z, Sea);
			}
		}
		Location.Z += DashHeightOffset;
	};

	// cercle de la taille du déclencheur de la zone (le bateau qui y entre lance le combat) ; la ligne s'arrête à son bord
	const float TriggerRadius = Arena->GetTriggerRadius();
	const bool bCircle = bShowEndCircle && TriggerRadius > 0.0f;
	const float LineEnd = bCircle && Length > TriggerRadius ? Length - TriggerRadius : Length;

	TArray<FTransform> Transforms;
	TArray<FTransform> ArrowTransforms;
	// grille fixe depuis la zone du world boss : les pointillés ne glissent pas quand le joueur avance
	for (float FromEnd = FMath::Fmod(Length - DashStartOffset, DashSpacing); FromEnd <= Length - DashStartOffset && Transforms.Num() + ArrowTransforms.Num() < MaxDashes; FromEnd += DashSpacing)
	{
		if (Length - FromEnd > LineEnd)
		{
			continue;
		}
		FVector Location = End - Direction * FromEnd;
		PlaceOnSurface(Location);
		// un pointillé sur Arrow Every (grille fixe) : flèche vers la zone
		if (ArrowEvery > 0 && FMath::RoundToInt(FromEnd / DashSpacing) % ArrowEvery == 0)
		{
			ArrowTransforms.Add(AYSTreasureMap::MakeArrowTransform(Location, Yaw, ArrowLength, ArrowWidth, DashThickness));
			continue;
		}
		Transforms.Emplace(FRotator(0.0f, Yaw, 0.0f), Location, Scale);
	}

	if (bCircle)
	{
		const float Spacing = EndCircleDashSpacing > 0.0f ? EndCircleDashSpacing : DashSpacing;
		const int32 Count = FMath::Max(8, FMath::CeilToInt(2.0f * PI * TriggerRadius / Spacing));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Angle = 2.0f * PI * Index / Count;
			FVector Location = End + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * TriggerRadius;
			PlaceOnSurface(Location);
			Transforms.Emplace(FRotator(0.0f, FMath::RadiansToDegrees(Angle) + 90.0f, 0.0f), Location, Scale);
		}
	}

	GuideDashes->ClearInstances();
	GuideDashes->AddInstances(Transforms, false, true);
	GuideArrows->ClearInstances();
	GuideArrows->AddInstances(ArrowTransforms, false, true);
}
