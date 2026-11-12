#include "Ship/YSShipBoarding.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/YSPlayerCharacter.h"
#include "Ship/YSPirateShip.h"

AYSShipBoarding::AYSShipBoarding()
{
	bCarriable = false;
	DisplayName = NSLOCTEXT("YS", "ShipBoardingName", "Monter à bord");

	// pont du grand bateau : 200 vers l'intérieur depuis le flanc (+X de l'acteur = le large), dessus du pont à +210
	BoardPoint = CreateDefaultSubobject<USceneComponent>(TEXT("BoardPoint"));
	BoardPoint->SetupAttachment(GetRootComponent());
	BoardPoint->SetRelativeLocationAndRotation(FVector(-200.0f, 0.0f, 210.0f), FRotator(0.0f, 180.0f, 0.0f));

	// invite « E » au-dessus de la planche, côté large
	if (UWidgetComponent* Prompt = FindComponentByClass<UWidgetComponent>())
	{
		Prompt->SetRelativeLocation(FVector(250.0f, 0.0f, 200.0f));
	}

	// échelle de corde (visuel, sans collision), reconstruite d'après les réglages Boarding|Ladder
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RopeFinder(TEXT("/Game/Art/Materials/PH_materials/m_PH_monochrome_Sand.m_PH_monochrome_Sand"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RungFinder(TEXT("/Game/Art/Materials/PH_materials/m_PH_monochrome_Brown.m_PH_monochrome_Brown"));
	RopeMaterial = RopeFinder.Succeeded() ? RopeFinder.Object : nullptr;
	RungMaterial = RungFinder.Succeeded() ? RungFinder.Object : nullptr;
	for (TObjectPtr<UInstancedStaticMeshComponent>* Ladder : { &LadderRopes, &LadderRungs })
	{
		const bool bRopes = Ladder == &LadderRopes;
		*Ladder = CreateDefaultSubobject<UInstancedStaticMeshComponent>(bRopes ? TEXT("LadderRopes") : TEXT("LadderRungs"));
		(*Ladder)->SetupAttachment(GetRootComponent());
		(*Ladder)->SetMobility(EComponentMobility::Movable);
		(*Ladder)->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		(*Ladder)->SetGenerateOverlapEvents(false);
		(*Ladder)->SetCanEverAffectNavigation(false);
		if (bRopes ? CylinderFinder.Succeeded() : CubeFinder.Succeeded())
		{
			(*Ladder)->SetStaticMesh(bRopes ? CylinderFinder.Object : CubeFinder.Object);
		}
	}
}

void AYSShipBoarding::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildLadder();
}

void AYSShipBoarding::BeginPlay()
{
	Super::BeginPlay();
	BuildLadder();
}

void AYSShipBoarding::BuildLadder()
{
	LadderRopes->ClearInstances();
	LadderRungs->ClearInstances();
	// (ou le bateau le demande pour tous ses accès)
	const AYSPirateShip* Ship = GetShip();
	const bool bLadder = bShowLadder || (Ship && Ship->ShowsBoardingLadders());
	LadderRopes->SetVisibility(bLadder);
	LadderRungs->SetVisibility(bLadder);
	const float Height = LadderTop - LadderBottom;
	if (!bLadder || Height <= 0.0f)
	{
		return;
	}
	LadderRopes->SetMaterial(0, RopeMaterial);
	LadderRungs->SetMaterial(0, RungMaterial);

	// formes de base de 100 cm : cordes verticales (cylindres) de chaque côté, barreaux (cubes) le long du flanc (Y de l'acteur)
	for (const float Side : { -1.0f, 1.0f })
	{
		LadderRopes->AddInstance(FTransform(FRotator::ZeroRotator, FVector(LadderOffset, Side * LadderHalfWidth, LadderBottom + Height * 0.5f),
			FVector(RopeDiameter / 100.0f, RopeDiameter / 100.0f, Height / 100.0f)));
	}
	const int32 RungCount = FMath::Max(1, FMath::FloorToInt(Height / RungSpacing));
	for (int32 Index = 1; Index <= RungCount; ++Index)
	{
		const float Z = LadderTop - Index * RungSpacing + RungSpacing * 0.5f;
		if (Z < LadderBottom)
		{
			break;
		}
		LadderRungs->AddInstance(FTransform(FRotator::ZeroRotator, FVector(LadderOffset, 0.0f, Z),
			FVector(RungThickness / 100.0f, LadderHalfWidth * 2.0f / 100.0f, RungThickness / 100.0f)));
	}
}

AYSPirateShip* AYSShipBoarding::GetShip() const
{
	return AYSPirateShip::GetShipOf(GetParentActor());
}

bool AYSShipBoarding::CanInteract(const AYSPlayerCharacter* Character) const
{
	const AYSPirateShip* Ship = GetShip();
	// en portant un objet aussi : il monte à bord avec (demande de Paul)
	if (!Ship || !Ship->IsPresent() || !Character || !Character->IsAlive() || Character->IsAtStation())
	{
		return false;
	}
	if (Character->IsSwimming() ? !bAllowSwimmers : !bAllowWalkers)
	{
		return false;
	}
	// anti-abordage : bateau de NME accessible seulement une fois un poste détruit ou son core en négatif
	FVector BoardingPoint;
	if (Ship->IsFoeShip() && !Ship->FindBoardingPoint(GetActorLocation(), BoardingPoint))
	{
		return false;
	}
	// déjà sur ce bateau (pont, planche d'embarquement) : rien à faire
	const UPrimitiveComponent* Base = Character->GetCharacterMovement() ? Cast<UPrimitiveComponent>(Character->GetCharacterMovement()->GetMovementBaseObject()) : nullptr;
	return !Base || AYSPirateShip::GetShipOf(Base->GetOwner()) != Ship;
}

FVector AYSShipBoarding::GetInteractLocation(const FVector& From) const
{
	// point du rectangle d'accès (repère de l'acteur, à plat) le plus proche
	const FVector Local = GetActorTransform().InverseTransformPosition(From);
	// (zone réduite par Access Range Multiplier, comme la portée du personnage : GetInteractRangeMultiplier)
	const float Reach = AccessReach * AccessRangeMultiplier;
	const float HalfWidth = AccessHalfWidth * AccessRangeMultiplier;
	const FVector Clamped(FMath::Clamp(Local.X, 0.0f, Reach), FMath::Clamp(Local.Y, -HalfWidth, HalfWidth), 0.0f);
	return GetActorTransform().TransformPosition(Clamped);
}

FVector AYSShipBoarding::GetInteractPriorityLocation(const FVector& From) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(From);
	const float HalfWidth = AccessHalfWidth * AccessRangeMultiplier;
	return GetActorTransform().TransformPosition(FVector(0.0f, FMath::Clamp(Local.Y, -HalfWidth, HalfWidth), 0.0f));
}

void AYSShipBoarding::Interact(AYSPlayerCharacter* Character)
{
	if (!HasAuthority() || !CanInteract(Character))
	{
		return;
	}
	const bool bWasSwimming = Character->IsSwimming();
	const FVector Feet = FindLandingPoint(Character);
	Character->TeleportToFeetLocation(Feet, BoardPoint->GetForwardVector());
	UE_LOG(LogTemp, Log, TEXT("[YSBoarding] %s monte à bord de %s%s (pieds en %s, %.0f cm du point prévu)"), *Character->GetName(), *GetNameSafe(GetShip()),
		bWasSwimming ? TEXT(" depuis l'eau") : TEXT(""), *Feet.ToCompactString(), FVector::Dist(Feet, BoardPoint->GetComponentLocation()));
}

FVector AYSShipBoarding::GetBoardPointLocation() const
{
	return BoardPoint->GetComponentLocation();
}

bool AYSShipBoarding::IsBoardPointFree(const AYSPlayerCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSBoardingFree), false, Character);
	const FVector Center = BoardPoint->GetComponentLocation() + FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight() + 5.0f);
	return !GetWorld()->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Params);
}

FVector AYSShipBoarding::FindLandingPoint(const AYSPlayerCharacter* Character) const
{
	// le point prévu peut tomber dans une cabine ou sous le gaillard (échelles du fond du galion / brigantin) : première place libre sur le
	// dessus du bateau (sol tracé par le dessus, capsule du personnage qui y tient), en avançant vers l'intérieur et le long de la coque
	const AYSPirateShip* Ship = GetShip();
	const FVector Nominal = BoardPoint->GetComponentLocation();
	if (!Ship || !Character)
	{
		return Nominal;
	}
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Inward = BoardPoint->GetForwardVector().GetSafeNormal2D();
	const FVector Along(-Inward.Y, Inward.X, 0.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSBoardingLanding), false, Character);
	const float Top = Ship->GetActorLocation().Z + LandingSearchHeight;
	for (float Depth = 0.0f; Depth <= LandingSearchDepth; Depth += LandingSearchStep)
	{
		for (const float Side : { 0.0f, 1.0f, -1.0f, 2.0f, -2.0f, 3.0f, -3.0f })
		{
			const FVector Candidate = Nominal + Inward * Depth + Along * (Side * LandingSearchStep);
			FHitResult Hit;
			if (!GetWorld()->LineTraceSingleByChannel(Hit, FVector(Candidate.X, Candidate.Y, Top), FVector(Candidate.X, Candidate.Y, Ship->GetActorLocation().Z - 200.0f), ECC_WorldStatic, Params)
				|| Hit.ImpactNormal.Z < 0.7f || AYSPirateShip::GetShipOf(Hit.GetActor()) != Ship)
			{
				continue;
			}
			// la capsule tient au-dessus de ce sol (ni mât, ni bastingage, ni toit)
			const FVector Center = Hit.ImpactPoint + FVector(0.0f, 0.0f, HalfHeight + 5.0f);
			if (!GetWorld()->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params))
			{
				return Hit.ImpactPoint;
			}
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[YSBoarding] %s : pas de place libre trouvée sur %s, point prévu gardé"), *GetName(), *Ship->GetName());
	return Nominal;
}
