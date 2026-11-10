#include "Island/YSIslandNPC.h"
#include "Engine/World.h"
#include "Core/YSFlowMode.h"
#include "Core/YSGameState.h"
#include "Island/YSIsland.h"
#include "Ship/YSShipCore.h"
#include "Net/UnrealNetwork.h"
#include "Ship/YSPirateShip.h"

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
	// objectifs de l'île, et comme au core du bateau (demande de Paul du 2026-11-10 : déposer sans monter à bord) : coffres (doublons),
	// CoreBoats du stock, objets des zones d'intérêt
	if (!Object)
	{
		return false;
	}
	const AYSFlowMode* FlowMode = GetWorld()->GetAuthGameMode<AYSFlowMode>();
	return (Island && Island->IsObjectExpected(Object)) || AYSBrookerNPC::AcceptsObject(Object)
		|| (FlowMode && (FlowMode->IsStockedCoreBoat(Object) || FlowMode->IsZoneItem(Object)));
}

void AYSStartBoat::ReceiveObject(AYSInteractiveObject* Object)
{
	AYSFlowMode* FlowMode = GetWorld()->GetAuthGameMode<AYSFlowMode>();
	if (FlowMode && FlowMode->IsStockedCoreBoat(Object))
	{
		UE_LOG(LogTemp, Log, TEXT("[YSStartBoat] %s posé dans la zone -> stock"), *Object->GetName());
		FlowMode->StoreCoreBoat(Object);
		return;
	}
	if (Object->GetDoubloonValue() > 0)
	{
		AYSBrookerNPC::ReceiveObject(Object);
		return;
	}
	if (FlowMode && FlowMode->IsCompass(Object) && !Object->IsDelivered())
	{
		FlowMode->NotifyCompassDelivered(Object);
	}
	// île qui va couler (pas l'île de départ) : l'objet livré part sur le pont du bateau des joueurs (sinon il coulerait avec l'île)
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	AYSPirateShip* GameBoat = YSGameState ? YSGameState->GetGameBoat() : nullptr;
	if (GameBoat && Island && !Island->IsStartIsland() && GameBoat->GetCore())
	{
		const FVector Deck = GameBoat->GetCore()->GetActorLocation() + FVector(FMath::FRandRange(-150.0f, 150.0f), FMath::FRandRange(-150.0f, 150.0f), 0.0f);
		Object->TeleportTo(AYSInteractiveObject::FindGround(GetWorld(), Deck + FVector(0.0f, 0.0f, 500.0f), Object), Object->GetActorRotation());
		Object->AttachToShipBelow();
		UE_LOG(LogTemp, Log, TEXT("[YSStartBoat] %s posé dans la zone : envoyé sur le pont de %s"), *Object->GetName(), *GameBoat->GetName());
	}
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
		// posé sur le bateau des joueurs : il voyage avec lui ; sinon (zone de la plage) accroché à l'île
		if (AYSPirateShip* Ship = AYSPirateShip::FindShipBelow(World, GetActorLocation(), this))
		{
			NPC->AttachToActor(Ship, FAttachmentTransformRules::KeepWorldTransform);
		}
		else if (AYSIsland* Island = AYSIsland::FindAt(World, GetActorLocation()))
		{
			Island->AttachToIsland(NPC);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s reste au bateau : %s"), *GetName(), NPC ? *NPC->GetName() : TEXT("(pas de PNJ)"));
	Destroy();
}
