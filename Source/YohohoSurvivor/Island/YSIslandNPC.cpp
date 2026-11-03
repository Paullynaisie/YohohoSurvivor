#include "Island/YSIslandNPC.h"
#include "Engine/World.h"
#include "Island/YSIsland.h"
#include "Net/UnrealNetwork.h"

AYSIslandNPC::AYSIslandNPC()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AYSIslandNPC::BeginPlay()
{
	// posé puis accroché à l'île, qui peut couler : tout doit pouvoir bouger
	TInlineComponentArray<USceneComponent*> Components(this);
	for (USceneComponent* Component : Components)
	{
		Component->SetMobility(EComponentMobility::Movable);
	}
	Super::BeginPlay();
}

AYSIslandMedicNPC::AYSIslandMedicNPC()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AYSIslandMedicNPC::BeginPlay()
{
	// posé puis accroché à l'île, qui peut couler : tout doit pouvoir bouger
	TInlineComponentArray<USceneComponent*> Components(this);
	for (USceneComponent* Component : Components)
	{
		Component->SetMobility(EComponentMobility::Movable);
	}
	Super::BeginPlay();
}

void AYSStartBoat::InitBoat(AYSIsland* InIsland, float InZoneRadius)
{
	Island = InIsland;
	ZoneRadius = InZoneRadius;
	AcceptRange = InZoneRadius;
}

void AYSStartBoat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSStartBoat, Island);
	DOREPLIFETIME(AYSStartBoat, ZoneRadius);
}

void AYSStartBoat::BeginPlay()
{
	if (ZoneRadius > 0.0f)
	{
		AcceptRange = ZoneRadius;
	}
	Super::BeginPlay();
}

void AYSStartBoat::OnRep_ZoneRadius()
{
	AcceptRange = ZoneRadius;
	ApplyZone();
}

bool AYSStartBoat::AcceptsObject(const AYSInteractiveObject* Object) const
{
	return Island && Island->IsObjectExpected(Object);
}

void AYSStartBoat::ReceiveObject(AYSInteractiveObject* Object)
{
	UE_LOG(LogTemp, Log, TEXT("[YSStartBoat] %s posé dans la zone du bateau"), *Object->GetName());
	Object->OnDelivered();
}

void AYSCarpenter::OnDelivered()
{
	UWorld* World = GetWorld();
	const FTransform Transform(FRotator(0.0f, GetActorRotation().Yaw, 0.0f), GetActorLocation());
	AYSBrookerNPC* NPC = DeliveredNPCClass ? World->SpawnActorDeferred<AYSBrookerNPC>(DeliveredNPCClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn) : nullptr;
	if (NPC)
	{
		NPC->FinishSpawning(Transform);
		if (AYSIsland* Island = AYSIsland::FindAt(World, GetActorLocation()))
		{
			Island->AttachToIsland(NPC);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s reste au bateau : %s"), *GetName(), NPC ? *NPC->GetName() : TEXT("(pas de PNJ)"));
	Destroy();
}
