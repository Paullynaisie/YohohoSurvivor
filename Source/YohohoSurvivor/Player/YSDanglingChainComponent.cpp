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

	// bouts : composants placés en coordonnées absolues (la simulation les place, ils ne suivent plus leur parent)
	EndAnchor = FindSiblingComponent(EndAnchorComponentName);
	EndAttached.Reset();
	for (const FName Name : EndAttachedComponentNames)
	{
		if (USceneComponent* Component = FindSiblingComponent(Name))
		{
			Component->SetUsingAbsoluteLocation(true);
			Component->SetUsingAbsoluteRotation(true);
			EndAttached.Add(Component);
		}
	}

	// l'acteur est maintenant à sa position de départ : on repart d'une chaîne au repos
	if (Segments.Num() > 0)
	{
		ResetPoints();
		UpdateSegmentTransforms();
	}
}

USceneComponent* UYSDanglingChainComponent::FindSiblingComponent(FName Name) const
{
	if (Name.IsNone() || !GetOwner())
	{
		return nullptr;
	}
	const FString Wanted = Name.ToString().Replace(TEXT(" "), TEXT(""));
	TInlineComponentArray<USceneComponent*> Components(GetOwner());
	for (USceneComponent* Component : Components)
	{
		if (Component != this && Component->GetName().Replace(TEXT(" "), TEXT("")) == Wanted)
		{
			return Component;
		}
	}
	return nullptr;
}

float UYSDanglingChainComponent::GetInverseMass(int32 Index) const
{
	const bool bLast = Index == Points.Num() - 1;
	if (Index == 0 || (bLast && EndAnchor.IsValid()))
	{
		return 0.0f;
	}
	return bLast ? 1.0f / FMath::Max(EndMass, 0.1f) : 1.0f;
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
	// repos : vers l'arrière et vers le bas ; sans poussée vers l'arrière, droit vers le bas (ex. mèche devant le visage)
	const FVector RestDirection = TrailAcceleration > 0.0f ? (GetTrailDirection() - FVector::UpVector).GetSafeNormal() : -FVector::UpVector;

	Points.SetNum(SegmentCount + 1);
	const USceneComponent* EndComponent = EndAnchor.IsValid() ? EndAnchor.Get() : FindSiblingComponent(EndAnchorComponentName);
	if (EndComponent)
	{
		// accrochée aux deux bouts : ligne entre les deux, creusée vers le bas par la longueur en trop
		const FVector End = EndComponent->GetComponentLocation();
		const float Span = FVector::Dist(Anchor, End);
		const float Length = SegmentLength * SegmentCount;
		const float Sag = Length > Span ? 0.5f * FMath::Sqrt(FMath::Square(Length) - FMath::Square(Span)) : 0.0f;
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			const float Alpha = static_cast<float>(Index) / SegmentCount;
			Points[Index] = FMath::Lerp(Anchor, End, Alpha) - FVector::UpVector * Sag * FMath::Sin(PI * Alpha);
		}
	}
	else
	{
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			Points[Index] = Anchor + RestDirection * SegmentLength * Index;
		}
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
	// dernier point accroché (corde entre deux points)
	if (const USceneComponent* EndComponent = EndAnchor.Get())
	{
		Points.Last() = EndComponent->GetComponentLocation();
		PreviousPoints.Last() = Points.Last();
	}

	// contraintes : chaque segment garde sa longueur, les points accrochés ne bougent pas, un bout lourd bouge moins
	for (int32 Iteration = 0; Iteration < SolverIterations; ++Iteration)
	{
		for (int32 Index = 1; Index < Points.Num(); ++Index)
		{
			const FVector Delta = Points[Index] - Points[Index - 1];
			const float Length = Delta.Size();
			const float PreviousWeight = GetInverseMass(Index - 1);
			const float Weight = GetInverseMass(Index);
			if (Length <= KINDA_SMALL_NUMBER || PreviousWeight + Weight <= 0.0f)
			{
				continue;
			}

			const FVector Correction = Delta * ((Length - SegmentLength) / Length);
			Points[Index - 1] += Correction * (PreviousWeight / (PreviousWeight + Weight));
			Points[Index] -= Correction * (Weight / (PreviousWeight + Weight));
		}
	}

	// bout libre posé sur le sol au lieu de passer dessous (boulet), avec un peu de frottement
	if (EndGroundRadius > 0.0f && !EndAnchor.IsValid())
	{
		FVector& End = Points.Last();
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(YSChainGround), false, GetOwner());
		Params.AddIgnoredActor(GetOwner()->GetOwner());
		if (GetWorld()->LineTraceSingleByObjectType(Hit, End + FVector(0.0f, 0.0f, 200.0f), End - FVector(0.0f, 0.0f, EndGroundRadius), FCollisionObjectQueryParams(ECC_WorldStatic), Params)
			&& End.Z < Hit.ImpactPoint.Z + EndGroundRadius)
		{
			End.Z = Hit.ImpactPoint.Z + EndGroundRadius;
			FVector& Previous = PreviousPoints.Last();
			Previous.Z = End.Z;
			const float Keep = FMath::Pow(1.0f - EndGroundFriction, Step * 60.0f);
			Previous = End - (End - Previous) * Keep;
		}
	}

	UpdateSegmentTransforms();

	// composants au bout (boulet, hitbox), orientés selon le dernier segment
	if (EndAttached.Num() > 0)
	{
		const FVector Axis = (Points.Last() - Points[Points.Num() - 2]).GetSafeNormal();
		const FQuat Rotation = Axis.IsNearlyZero() ? FQuat::Identity : FRotationMatrix::MakeFromZ(Axis).ToQuat();
		for (const TWeakObjectPtr<USceneComponent>& Component : EndAttached)
		{
			if (Component.IsValid())
			{
				Component->SetWorldLocationAndRotation(Points.Last(), Rotation);
			}
		}
		UE_LOG(LogTemp, VeryVerbose, TEXT("[YSChain] %s.%s : bout à %.0f cm de l'accroche (%.0f à plat), %.0f cm au-dessus, vitesse %.0f"), *GetOwner()->GetName(), *GetName(),
			FVector::Dist(Points.Last(), Anchor), FVector::Dist2D(Points.Last(), Anchor), Points.Last().Z - Anchor.Z, FVector::Dist(Points.Last(), PreviousPoints.Last()) / Step);
	}
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
