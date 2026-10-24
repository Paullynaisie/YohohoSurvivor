#include "Ship/YSPirateShip.h"
#include "Components/SceneComponent.h"
#include "Core/YSGameState.h"
#include "Engine/World.h"

AYSPirateShip::AYSPirateShip()
{
	// présence (cachée tant que l'objectif de doublons n'est pas atteint dans GameMap) : vérifiée à chaque image, sur toutes les machines
	PrimaryActorTick.bCanEverTick = true;

	// le serveur déplacera le bateau : position répliquée, les persos posés sur le pont suivent
	bReplicates = true;
	SetReplicatingMovement(true);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	ShipName = NSLOCTEXT("YS", "PirateShipName", "Bateau pirate");
}

void AYSPirateShip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	const int8 Presence = !YSGameState || YSGameState->IsShipPresent() ? 1 : 0;
	if (Presence != AppliedPresence)
	{
		AppliedPresence = Presence;
		SetActorHiddenInGame(Presence == 0);
		SetActorEnableCollision(Presence == 1);
		UE_LOG(LogTemp, Log, TEXT("[YSShip] %s %s"), *GetName(), Presence ? TEXT("présent") : TEXT("absent (caché, sans collision)"));
	}
}
