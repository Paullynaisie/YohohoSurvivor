#include "Ship/YSShipCore.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Interaction/YSPlayerCorpse.h"
#include "Materials/MaterialInterface.h"
#include "Ship/YSPirateShip.h"
#include "UObject/ConstructorHelpers.h"

AYSShipCore::AYSShipCore()
{
	BrookerName = NSLOCTEXT("YS", "ShipCoreName", "Core du bateau");

	// zone Brooker + Medic au milieu du pont (doré et vert : les deux rôles)
	AcceptRange = 450.0f;
	ZoneColor = FLinearColor(0.55f, 0.85f, 0.2f);
	IndicatorColor = FLinearColor(0.55f, 0.85f, 0.2f);
	IndicatorHeight = 220.0f;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ZoneMaterialFinder(TEXT("/Game/Art/Materials/M_BrookerZone.M_BrookerZone"));
	if (ZoneMaterialFinder.Succeeded())
	{
		ZoneMaterial = ZoneMaterialFinder.Object;
	}

	// visuel provisoire : socle en cylindre, orbe lumineuse
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMaterialFinder(TEXT("/Game/Art/Materials/PH_materials/m_PH_monochrome_Brown.m_PH_monochrome_Brown"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OrbMaterialFinder(TEXT("/Game/Art/Materials/PH_materials/m_PH_emissive_turboGreen.m_PH_emissive_turboGreen"));

	CoreBase = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreBase"));
	CoreBase->SetupAttachment(GetRootComponent());
	CoreBase->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	CoreBase->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.8f));
	CoreBase->SetCollisionProfileName(TEXT("BlockAll"));
	CoreBase->SetMobility(EComponentMobility::Movable);

	CoreOrb = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreOrb"));
	CoreOrb->SetupAttachment(GetRootComponent());
	CoreOrb->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	CoreOrb->SetRelativeScale3D(FVector(0.7f));
	CoreOrb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoreOrb->SetMobility(EComponentMobility::Movable);

	if (CylinderFinder.Succeeded())
	{
		CoreBase->SetStaticMesh(CylinderFinder.Object);
	}
	if (SphereFinder.Succeeded())
	{
		CoreOrb->SetStaticMesh(SphereFinder.Object);
	}
	if (BaseMaterialFinder.Succeeded())
	{
		CoreBase->SetMaterial(0, BaseMaterialFinder.Object);
	}
	if (OrbMaterialFinder.Succeeded())
	{
		CoreOrb->SetMaterial(0, OrbMaterialFinder.Object);
	}
}

bool AYSShipCore::AcceptsObject(const AYSInteractiveObject* Object) const
{
	// Medic (cadavre) ou Brooker (objet qui vaut des doublons)
	return Super::AcceptsObject(Object) || AYSBrookerNPC::AcceptsObject(Object);
}

bool AYSShipCore::IsAvailable() const
{
	const AYSPirateShip* Ship = AYSPirateShip::GetShipOf(this);
	return !Ship || Ship->IsPresent();
}

void AYSShipCore::ReceiveObject(AYSInteractiveObject* Object)
{
	if (Object && Object->IsA<AYSPlayerCorpse>())
	{
		UE_LOG(LogTemp, Log, TEXT("[YSShipCore] %s : cadavre %s -> résurrection"), *GetName(), *Object->GetName());
		Super::ReceiveObject(Object);
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSShipCore] %s : %s -> doublons"), *GetName(), Object ? *Object->GetName() : TEXT("?"));
	AYSBrookerNPC::ReceiveObject(Object);
}
