#include "NPC/YSBrookerNPC.h"
#include "Core/YSGameMode.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Interaction/YSInteractiveObject.h"

AYSBrookerNPC::AYSBrookerNPC()
{
	PrimaryActorTick.bCanEverTick = false;
	// placé dans la map : présent sur toutes les machines, rien à répliquer
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	BrookerName = NSLOCTEXT("YS", "BrookerName", "Brooker");
}

void AYSBrookerNPC::BeginPlay()
{
	Super::BeginPlay();

	// posé au sol (même calcul sur toutes les machines)
	SetActorLocation(AYSInteractiveObject::FindGround(GetWorld(), GetActorLocation(), this));
}

bool AYSBrookerNPC::TryBuy(AYSInteractiveObject* Object)
{
	AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	if (!GameMode || !IsValid(Object) || Object->GetDoubloonValue() <= 0 || Object->GetCarrier()
		|| FVector::DistSquared2D(Object->GetActorLocation(), GetActorLocation()) > FMath::Square(AcceptRange))
	{
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[YSBrooker] %s rachète %s : +%d doublons"), *GetName(), *Object->GetName(), Object->GetDoubloonValue());
	GameMode->AddDoubloons(Object->GetDoubloonValue());
	Object->Destroy();
	return true;
}

AYSBrookerNPC* AYSBrookerNPC::FindNearest(const UWorld* World, const FVector& Location)
{
	AYSBrookerNPC* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<AYSBrookerNPC> It(World); It; ++It)
	{
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
	AYSBrookerNPC* Brooker = IsValid(Object) ? FindNearest(Object->GetWorld(), Object->GetActorLocation()) : nullptr;
	return Brooker && Brooker->TryBuy(Object);
}
