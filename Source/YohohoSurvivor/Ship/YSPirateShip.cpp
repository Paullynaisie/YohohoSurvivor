#include "Ship/YSPirateShip.h"
#include "Components/SceneComponent.h"

AYSPirateShip::AYSPirateShip()
{
	PrimaryActorTick.bCanEverTick = false;

	// le serveur déplacera le bateau : position répliquée, les persos posés sur le pont suivent
	bReplicates = true;
	SetReplicatingMovement(true);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	ShipName = NSLOCTEXT("YS", "PirateShipName", "Bateau pirate");
}
