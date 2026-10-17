#include "Player/YSDanglingChainComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Au-delà de ce saut d'ancre en une image (téléportation, respawn), la chaîne est réinitialisée */
	constexpr float TeleportDistance = 500.0f;

	/** Pas de temps max simulé, pour rester stable lors d'une chute de framerate */
	constexpr float MaxSimulationStep = 1.0f / 30.0f;
}

UYSDanglingChainComponent::UYSDanglingChainComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// après le mouvement du personnage et l'inclinaison du corps : l'ancre est à sa position finale
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	// dans l'éditeur, la chaîne suit le composant en pose de repos (viewport du Blueprint, niveau)
	bTickInEditor = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultSegment(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (DefaultSegment.Succeeded())
	{
		SegmentMesh = DefaultSegment.Object;
	}
}

void UYSDanglingChainComponent::OnRegister()
{
	Super::OnRegister();

	// rien à afficher sur un serveur dédié ni sur les modèles (CDO / archétypes)
	if (IsTemplate() || !GetOwner() || IsNetMode(NM_DedicatedServer))
	{
		return;
	}

	BuildSegments();
}

void UYSDanglingChainComponent::OnUnregister()
{
	DestroySegments();

	Super::OnUnregister();
}

void UYSDanglingChainComponent::BeginPlay()
{
	Super::BeginPlay();

	// l'acteur est maintenant à sa position de départ : on repart d'une chaîne au repos
	if (Segments.Num() > 0)
	{
		ResetPoints();
		UpdateSegmentTransforms();
	}
}

void UYSDanglingChainComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);

	const UWorld* World = GetWorld();
	if (Segments.Num() > 0 && (!World || !World->IsGameWorld()))
	{
		ResetPoints();
		UpdateSegmentTransforms();
	}
}

void UYSDanglingChainComponent::BuildSegments()
{
	DestroySegments();

	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		// composants transient, en coordonnées absolues : ils ne suivent pas la hiérarchie, la simulation les place
		UStaticMeshComponent* Segment = NewObject<UStaticMeshComponent>(GetOwner(), NAME_None, RF_Transient);
		Segment->SetStaticMesh(SegmentMesh);
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetUsingAbsoluteLocation(true);
		Segment->SetUsingAbsoluteRotation(true);
		Segment->SetUsingAbsoluteScale(true);
		Segment->SetupAttachment(this);
		Segment->RegisterComponent();

		if (SegmentMaterial)
		{
			Segment->SetMaterial(0, SegmentMaterial);
		}
		else if (UMaterialInstanceDynamic* Material = Segment->CreateAndSetMaterialInstanceDynamic(0))
		{
			Material->SetVectorParameterValue(ColorParameterName, Color);
		}

		Segments.Add(Segment);
	}

	ResetPoints();
	UpdateSegmentTransforms();
}

void UYSDanglingChainComponent::DestroySegments()
{
	for (UStaticMeshComponent* Segment : Segments)
	{
		if (Segment)
		{
			Segment->DestroyComponent();
		}
	}
	Segments.Reset();
}

FVector UYSDanglingChainComponent::GetTrailDirection() const
{
	const FVector Back = GetOwner() ? -GetOwner()->GetActorForwardVector() : -GetForwardVector();
	return FVector(Back.X, Back.Y, 0.0f).GetSafeNormal();
}

void UYSDanglingChainComponent::ResetPoints()
{
	const FVector Anchor = GetComponentLocation();
	const FVector RestDirection = (GetTrailDirection() - FVector::UpVector).GetSafeNormal();

	Points.SetNum(SegmentCount + 1);
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		Points[Index] = Anchor + RestDirection * SegmentLength * Index;
	}
	PreviousPoints = Points;
}

void UYSDanglingChainComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Segments.Num() == 0 || Points.Num() != SegmentCount + 1)
	{
		return;
	}

	// hors jeu : pas de simulation, pose de repos
	if (!GetWorld()->IsGameWorld())
	{
		ResetPoints();
		UpdateSegmentTransforms();
		return;
	}

	const float Step = FMath::Min(DeltaTime, MaxSimulationStep);
	if (Step <= 0.0f)
	{
		return;
	}

	const FVector Anchor = GetComponentLocation();
	if (FVector::DistSquared(Anchor, Points[0]) > FMath::Square(TeleportDistance))
	{
		ResetPoints();
	}

	// intégration de Verlet : la vitesse est implicite (position - position précédente)
	const FVector Acceleration = Gravity + GetTrailDirection() * TrailAcceleration;
	const float VelocityKeep = FMath::Pow(1.0f - Damping, Step * 60.0f);
	Points[0] = Anchor;
	PreviousPoints[0] = Anchor;
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		const FVector Velocity = (Points[Index] - PreviousPoints[Index]) * VelocityKeep;
		PreviousPoints[Index] = Points[Index];
		Points[Index] += Velocity + Acceleration * Step * Step;
	}

	// contraintes : chaque segment garde sa longueur, le premier point reste accroché
	for (int32 Iteration = 0; Iteration < SolverIterations; ++Iteration)
	{
		for (int32 Index = 1; Index < Points.Num(); ++Index)
		{
			const FVector Delta = Points[Index] - Points[Index - 1];
			const float Length = Delta.Size();
			if (Length <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const FVector Correction = Delta * ((Length - SegmentLength) / Length);
			if (Index == 1)
			{
				Points[Index] -= Correction;
			}
			else
			{
				Points[Index - 1] += Correction * 0.5f;
				Points[Index] -= Correction * 0.5f;
			}
		}
	}

	UpdateSegmentTransforms();
}

void UYSDanglingChainComponent::UpdateSegmentTransforms()
{
	const FVector Scale(SegmentRadius / 50.0f, SegmentRadius / 50.0f, SegmentLength / 100.0f);

	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		const FVector Start = Points[Index];
		const FVector End = Points[Index + 1];
		const FVector Axis = (End - Start).GetSafeNormal();
		if (Axis.IsNearlyZero())
		{
			continue;
		}

		// le mesh est centré sur son origine et aligné sur Z : on le place au milieu du segment
		Segments[Index]->SetWorldLocationAndRotation((Start + End) * 0.5f, FRotationMatrix::MakeFromZ(Axis).ToQuat());
		Segments[Index]->SetWorldScale3D(Scale);
	}
}

void UYSDanglingChainComponent::SetSegmentMaterial(UMaterialInterface* Material)
{
	SegmentMaterial = Material;
	for (UStaticMeshComponent* Segment : Segments)
	{
		if (Segment && Material)
		{
			Segment->SetMaterial(0, Material);
		}
	}
}
