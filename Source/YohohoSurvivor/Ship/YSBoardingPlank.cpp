#include "Ship/YSBoardingPlank.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AYSBoardingPlank::AYSBoardingPlank()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	Plank = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plank"));
	RootComponent = Plank;
	Plank->SetMobility(EComponentMobility::Movable);
	// sol des joueurs et des NME (comme le pont)
	Plank->SetCollisionProfileName(TEXT("BlockAll"));
	Plank->SetCollisionObjectType(ECC_WorldStatic);
	Plank->SetGenerateOverlapEvents(false);
	Plank->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Plank->SetStaticMesh(Cube.Object);
	}
}

void AYSBoardingPlank::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AYSBoardingPlank, Span, COND_InitialOnly);
}

void AYSBoardingPlank::InitPlank(const FYSBoardingPlankSpan& InSpan)
{
	Span = InSpan;
}

void AYSBoardingPlank::BeginPlay()
{
	Super::BeginPlay();
	ApplySpan();
}

void AYSBoardingPlank::OnRep_Span()
{
	ApplySpan();
}

void AYSBoardingPlank::ApplySpan()
{
	const FVector Start = Span.Start;
	const FVector End = Span.End;
	const FVector Delta = End - Start;
	const float Length = Delta.Size();
	if (Length < 1.0f)
	{
		return;
	}
	// le dessus de la planche va de Start à End (cube de 100 cm)
	const FRotator Rotation = FRotationMatrix::MakeFromXZ(Delta / Length, FVector::UpVector).Rotator();
	const FVector Up = Rotation.RotateVector(FVector::UpVector);
	SetActorLocationAndRotation((Start + End) * 0.5f - Up * (Span.Thickness * 0.5f), Rotation);
	SetActorScale3D(FVector(Length / 100.0f, Span.Width / 100.0f, Span.Thickness / 100.0f));
	if (Span.Material)
	{
		Plank->SetMaterial(0, Span.Material);
	}
	UE_LOG(LogTemp, Log, TEXT("[YSGalion] Planche d'abordage %s posée, %.0f cm (%s)"), *GetName(), Length, HasAuthority() ? TEXT("serveur") : TEXT("client"));
}
