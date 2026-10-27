#include "NPC/YSBrookerNPC.h"
#include "Core/YSGameMode.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Interaction/YSInteractiveObject.h"
#include "Map/YSGroundZoneComponent.h"
#include "NPC/YSMedicNPC.h"
#include "Materials/MaterialInstanceDynamic.h"

AYSBrookerNPC::AYSBrookerNPC()
{
	PrimaryActorTick.bCanEverTick = false;
	// placé dans la map : présent sur toutes les machines, rien à répliquer
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// zone de rachat : disque translucide posé sur le relief, sans collision ni ombre
	GroundZone = CreateDefaultSubobject<UYSGroundZoneComponent>(TEXT("GroundZone"));
	GroundZone->SetupAttachment(Root);

	BrookerName = NSLOCTEXT("YS", "BrookerName", "Brooker");
}

void AYSBrookerNPC::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// visible dans l'éditeur, pour placer le Brooker
	ApplyZone();
}

void AYSBrookerNPC::ApplyZone()
{
	// la zone suit AcceptRange et le relief autour du Brooker
	const bool bVisible = bShowZone && AcceptRange > 0.0f;
	GroundZone->SetVisibility(bVisible);
	GroundZone->BuildZone(bVisible ? AcceptRange : 0.0f, ZoneHeight);

	if (ZoneMaterial)
	{
		UMaterialInstanceDynamic* Material = GroundZone->CreateDynamicMaterialInstance(0, ZoneMaterial);
		Material->SetVectorParameterValue(TEXT("Color"), ZoneColor);
		Material->SetScalarParameterValue(TEXT("Glow"), ZoneGlow);
		Material->SetScalarParameterValue(TEXT("FillOpacity"), ZoneFillOpacity);
		Material->SetScalarParameterValue(TEXT("EdgeOpacity"), ZoneEdgeOpacity);
		Material->SetScalarParameterValue(TEXT("Radius"), AcceptRange);
		Material->SetScalarParameterValue(TEXT("EdgeWidth"), ZoneEdgeWidth);
	}
}

void AYSBrookerNPC::BeginPlay()
{
	Super::BeginPlay();

	// posé au sol (même calcul sur toutes les machines)
	SetActorLocation(AYSInteractiveObject::FindGround(GetWorld(), GetActorLocation(), this));

	// OnConstruction ne tourne pas pour un Brooker chargé avec la map : matériau de zone recréé ici
	ApplyZone();

	const UMaterialInterface* ZoneMaterialUsed = GroundZone->GetMaterial(0);
	UE_LOG(LogTemp, Verbose, TEXT("[YSBrooker] %s : zone %.0f cm, matériau %s (parent %s), visible %d, %d sommets"), *GetName(), AcceptRange,
		ZoneMaterialUsed ? *ZoneMaterialUsed->GetName() : TEXT("aucun"), ZoneMaterialUsed && ZoneMaterialUsed->GetMaterial() ? *ZoneMaterialUsed->GetMaterial()->GetName() : TEXT("-"), GroundZone->IsVisible(),
		GroundZone->GetNumSections() > 0 ? GroundZone->GetProcMeshSection(0)->ProcVertexBuffer.Num() : 0);
}

void AYSBrookerNPC::Resettle()
{
	SetActorLocation(AYSInteractiveObject::FindGround(GetWorld(), GetActorLocation(), this));
	ApplyZone();
}

bool AYSBrookerNPC::TryBuy(AYSInteractiveObject* Object)
{
	// serveur seulement (PNJ non répliqué : il a l'autorité sur toutes les machines, le GameMode n'existe que sur le serveur)
	if (!GetWorld()->GetAuthGameMode() || !IsValid(Object) || Object->GetCarrier() || !IsAvailable())
	{
		return false;
	}

	const float Distance = FVector::Dist2D(Object->GetActorLocation(), GetActorLocation());
	if (Distance > AcceptRange)
	{
		UE_LOG(LogTemp, Log, TEXT("%s %s : %s hors de la zone (%.0f cm, zone %.0f cm)"), GetLogTag(), *GetName(), *Object->GetName(), Distance, AcceptRange);
		return false;
	}
	if (!AcceptsObject(Object))
	{
		UE_LOG(LogTemp, Log, TEXT("%s %s : %s refusé (Brooker : Doubloon Value = 0 ; Medic : pas un cadavre)"), GetLogTag(), *GetName(), *Object->GetName());
		return false;
	}

	ReceiveObject(Object);
	return true;
}

bool AYSBrookerNPC::IsMedic() const
{
	return IsA<AYSMedicNPC>();
}

bool AYSBrookerNPC::AcceptsObject(const AYSInteractiveObject* Object) const
{
	return Object && Object->GetDoubloonValue() > 0;
}

void AYSBrookerNPC::ReceiveObject(AYSInteractiveObject* Object)
{
	AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	if (!GameMode)
	{
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSBrooker] %s rachète %s : +%d doublons"), *GetName(), *Object->GetName(), Object->GetDoubloonValue());
	GameMode->AddDoubloons(Object->GetDoubloonValue());
	Object->Destroy();
}

AYSBrookerNPC* AYSBrookerNPC::FindNearest(const UWorld* World, const FVector& Location, const AYSInteractiveObject* AcceptedObject, bool bMedic)
{
	AYSBrookerNPC* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<AYSBrookerNPC> It(World); It; ++It)
	{
		// avec un objet : seulement ceux qui le prennent ; sans : Brookers ou Medics (le core d'un bateau est les deux)
		if (!It->IsAvailable() || (AcceptedObject ? !It->AcceptsObject(AcceptedObject) : (bMedic ? !It->IsMedic() : !It->IsBrooker())))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(It->GetActorLocation(), Location);
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = *It;
		}
	}
	return Nearest;
}

bool AYSBrookerNPC::TrySell(AYSInteractiveObject* Object)
{
	AYSBrookerNPC* Brooker = IsValid(Object) ? FindNearest(Object->GetWorld(), Object->GetActorLocation(), Object) : nullptr;
	return Brooker && Brooker->TryBuy(Object);
}
