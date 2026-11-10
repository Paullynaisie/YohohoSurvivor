#include "Ship/YSShipBoarding.h"
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
	const FVector Clamped(FMath::Clamp(Local.X, 0.0f, AccessReach), FMath::Clamp(Local.Y, -AccessHalfWidth, AccessHalfWidth), 0.0f);
	return GetActorTransform().TransformPosition(Clamped);
}

FVector AYSShipBoarding::GetInteractPriorityLocation(const FVector& From) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(From);
	return GetActorTransform().TransformPosition(FVector(0.0f, FMath::Clamp(Local.Y, -AccessHalfWidth, AccessHalfWidth), 0.0f));
}

void AYSShipBoarding::Interact(AYSPlayerCharacter* Character)
{
	if (!HasAuthority() || !CanInteract(Character))
	{
		return;
	}
	const bool bWasSwimming = Character->IsSwimming();
	Character->TeleportToFeetLocation(BoardPoint->GetComponentLocation(), BoardPoint->GetForwardVector());
	UE_LOG(LogTemp, Log, TEXT("[YSBoarding] %s monte à bord de %s%s (pieds en %s)"), *Character->GetName(), *GetNameSafe(GetShip()),
		bWasSwimming ? TEXT(" depuis l'eau") : TEXT(""), *BoardPoint->GetComponentLocation().ToCompactString());
}
