#include "Weapons/YSBeamShot.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AYSBeamShot::AYSBeamShot()
{
	// immobile, 0.1 s, traverse tous les NME, ni relief ni mur
	Speed = 0.0f;
	LifeTime = 0.1f;
	bFollowGround = false;
	bDestroyOnWorldHit = false;
	bPierceAllTargets = true;

	if (UStaticMeshComponent* BeamMesh = GetMeshComponent())
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (Cylinder.Succeeded())
		{
			BeamMesh->SetStaticMesh(Cylinder.Object);
		}
	}
}

void AYSBeamShot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AYSBeamShot, SizeMultiplier, COND_InitialOnly);
}

void AYSBeamShot::BeginPlay()
{
	Super::BeginPlay();

	// cylindre de base (axe Z, 1 m) couché le long du tir, du canon au bout du rayon
	if (UStaticMeshComponent* BeamMesh = GetMeshComponent())
	{
		const float Length = GetScaledLength();
		const float Width = 2.0f * GetScaledRadius();
		BeamMesh->SetRelativeLocation(FVector(0.5f * Length, 0.0f, 0.0f));
		BeamMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
		BeamMesh->SetRelativeScale3D(FVector(Width / 100.0f, Width / 100.0f, Length / 100.0f));
	}

	// les NME déjà dans le rayon sont touchés tout de suite (Solar Shot : le compte est prêt pour le tir de l'autre main)
	SweepBeam();
	SetActorTickEnabled(HasAuthority());
	UE_LOG(LogTemp, Verbose, TEXT("[YSBeamShot] %s : rayon de %.0f cm, épaisseur %.0f cm (x%.2f) en (%.0f, %.0f, %.0f)"), *GetName(), GetScaledLength(), 2.0f * GetScaledRadius(),
		SizeMultiplier, GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
}

void AYSBeamShot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// NME entrés dans le rayon pendant sa durée de vie
	if (!IsActorBeingDestroyed())
	{
		SweepBeam();
	}
}

void AYSBeamShot::SweepBeam()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World)
	{
		return;
	}
	const FVector Start = GetActorLocation();
	const FVector End = Start + GetActorForwardVector().GetSafeNormal2D() * GetScaledLength();
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSBeamShot), false, this);
	Params.AddIgnoredActor(GetInstigator());
	World->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(GetScaledRadius()), Params);
	// du plus proche au plus loin (perte de dégâts multi-cibles), un seul coup par acteur (HitTarget s'en charge)
	auto HitLocation = [](const FHitResult& Hit) -> FVector { return Hit.GetActor() ? Hit.GetActor()->GetActorLocation() : FVector(Hit.ImpactPoint); };
	Hits.Sort([&Start, &HitLocation](const FHitResult& A, const FHitResult& B)
	{
		return FVector::DistSquared2D(Start, HitLocation(A)) < FVector::DistSquared2D(Start, HitLocation(B));
	});
	for (const FHitResult& Hit : Hits)
	{
		if (IsActorBeingDestroyed())
		{
			return;
		}
		HitTarget(Hit.GetActor(), Hit);
	}
}

FVector AYSBeamShot::GetImpactLocation(const AActor* Target) const
{
	// point de l'axe du rayon le plus proche du NME, à la hauteur du rayon
	if (!Target)
	{
		return GetActorLocation();
	}
	const FVector Start = GetActorLocation();
	const FVector End = Start + GetActorForwardVector().GetSafeNormal2D() * GetScaledLength();
	return FMath::ClosestPointOnSegment(FVector(Target->GetActorLocation().X, Target->GetActorLocation().Y, Start.Z), Start, End);
}

FVector AYSBeamShot::GetDisappearLocation() const
{
	return GetActorLocation() + GetActorForwardVector().GetSafeNormal2D() * GetScaledLength();
}
