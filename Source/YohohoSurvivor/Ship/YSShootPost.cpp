#include "Ship/YSShootPost.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInterface.h"
#include "Player/YSPlayerCharacter.h"
#include "Player/YSPlayerController.h"
#include "ProceduralMeshComponent.h"
#include "Ship/YSPirateShip.h"
#include "UObject/ConstructorHelpers.h"

AYSShootPost::AYSShootPost()
{
	DisplayName = NSLOCTEXT("YS", "ShootPostName", "Poste de tir");

	// origine = pied de la place, sur le pont (+X vers l'extérieur du mât) ; joueur sur la plateforme à +220 (on passe encore dessous ; pas trop haut : les balles
	// redescendent vite à hauteur des NME), reposé sur le pont hors de la plateforme en partant
	GetOccupantPoint()->SetRelativeLocation(FVector(0.0f, 0.0f, 220.0f));
	GetExitPoint()->SetRelativeLocation(FVector(150.0f, 0.0f, 0.0f));

	// pas de mains sur le poste, regard libre (il tire vers le curseur) ; caméra de la barre, vers le curseur
	bTeleportOnLeave = true;
	bHandsOnStation = false;
	bLockAim = false;
	bCameraOnShip = false;
	CameraOffsetMode = EYSStationCameraOffset::Cursor;
	CameraOffsetDistance = 2500.0f;

	PreviewMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PreviewMesh"));
	PreviewMesh->SetupAttachment(GetRootComponent());
	PreviewMesh->SetUsingAbsoluteLocation(true);
	PreviewMesh->SetUsingAbsoluteRotation(true);
	PreviewMesh->SetUsingAbsoluteScale(true);
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetCastShadow(false);
	PreviewMesh->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PreviewMaterialFinder(TEXT("/Game/Core/Ship/M_ShipPreview.M_ShipPreview"));
	if (PreviewMaterialFinder.Succeeded())
	{
		PreviewMaterial = PreviewMaterialFinder.Object;
	}
}

float AYSShootPost::GetCameraArmLength() const
{
	const AYSPirateShip* Ship = GetShip();
	return Ship && Ship->GetHelmCameraArmLength() > 0.0f ? Ship->GetHelmCameraArmLength() : CameraArmLength;
}

void AYSShootPost::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdatePreview();
}

void AYSShootPost::UpdatePreview()
{
	// joueur local au poste seulement
	const AYSPlayerCharacter* Character = GetOccupant();
	const AYSPlayerController* Controller = Character && Character->IsLocallyControlled() ? Cast<AYSPlayerController>(Character->GetController()) : nullptr;
	if (!Controller || !Character->IsAlive())
	{
		if (bPreviewVisible)
		{
			bPreviewVisible = false;
			PreviewMesh->SetVisibility(false);
		}
		return;
	}

	const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FVector Direction = Controller->GetAimLocation() - Feet;
	Direction.Z = 0.0f;
	Direction = Direction.IsNearlyZero() ? Character->GetActorForwardVector().GetSafeNormal2D() : Direction.GetSafeNormal();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Direction) * (PreviewWidth * 0.5f);
	const FVector Start = Feet + FVector(0.0f, 0.0f, PreviewHeight);
	const FVector End = Start + Direction * PreviewLength;

	// une bande, opaque au départ, estompée au bout (découpée pour que l'estompe suive la longueur)
	constexpr int32 Segments = 8;
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FLinearColor> Colors;
	for (int32 K = 0; K <= Segments; ++K)
	{
		const float T = static_cast<float>(K) / Segments;
		const FVector Center = FMath::Lerp(Start, End, T);
		FLinearColor Color = PreviewColor;
		Color.A = FMath::Lerp(PreviewColor.A, PreviewEndAlpha, T);
		Vertices.Add(Center - Side);
		Vertices.Add(Center + Side);
		Colors.Add(Color);
		Colors.Add(Color);
		if (K < Segments)
		{
			const int32 A = K * 2;
			Triangles.Append({ A, A + 1, A + 2, A + 1, A + 3, A + 2 });
		}
	}
	PreviewMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, TArray<FVector>(), TArray<FVector2D>(), Colors, TArray<FProcMeshTangent>(), false);
	PreviewMesh->SetMaterial(0, PreviewMaterial);
	if (!bPreviewVisible)
	{
		bPreviewVisible = true;
		PreviewMesh->SetVisibility(true);
	}
}
