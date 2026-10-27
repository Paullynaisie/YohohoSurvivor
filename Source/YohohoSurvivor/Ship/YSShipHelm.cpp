#include "Ship/YSShipHelm.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "Ship/YSPirateShip.h"

AYSShipHelm::AYSShipHelm()
{
	// vérifie que le pilote est toujours là (serveur)
	PrimaryActorTick.bCanEverTick = true;
	bCarriable = false;
	DisplayName = NSLOCTEXT("YS", "ShipHelmName", "Barre");

	// pilote derrière la roue (pieds sur le pont du gaillard), mains sur la jante de chaque côté
	PilotPoint = CreateDefaultSubobject<USceneComponent>(TEXT("PilotPoint"));
	PilotPoint->SetupAttachment(GetRootComponent());
	PilotPoint->SetRelativeLocation(FVector(-90.0f, 0.0f, -120.0f));
	GetHandPoint(true)->SetRelativeLocation(FVector(-15.0f, -40.0f, 10.0f));
	GetHandPoint(false)->SetRelativeLocation(FVector(-15.0f, 40.0f, 10.0f));
}

void AYSShipHelm::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSShipHelm, Pilot);
}

void AYSShipHelm::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// pilote parti (déconnexion, changement de classe, mort) : la barre est libre
	if (HasAuthority() && Pilot && (!IsValid(Pilot) || !Pilot->IsAlive() || Pilot->GetPilotedHelm() != this))
	{
		UE_LOG(LogTemp, Log, TEXT("[YSHelm] %s : pilote parti, barre libre"), *GetName());
		SetPilot(nullptr);
	}
}

bool AYSShipHelm::CanInteract(const AYSPlayerCharacter* Character) const
{
	const AYSPirateShip* Ship = GetShip();
	return Ship && Ship->IsDrivable() && !Pilot && Character && Character->IsAlive() && !Character->IsCarrying() && !Character->IsSwimming() && !Character->IsPiloting();
}

void AYSShipHelm::Interact(AYSPlayerCharacter* Character)
{
	if (HasAuthority() && CanInteract(Character))
	{
		Character->StartPiloting(this);
	}
}

void AYSShipHelm::SetPilot(AYSPlayerCharacter* NewPilot)
{
	if (!HasAuthority() || Pilot == NewPilot)
	{
		return;
	}
	Pilot = NewPilot;
	SetPromptVisible(false);
	if (AYSPirateShip* Ship = GetShip())
	{
		// personne à la barre : le bateau garde son élan et ralentit seul
		Ship->SetPilotInput(0.0f, 0.0f);
	}
	ForceNetUpdate();
}

AYSPirateShip* AYSShipHelm::GetShip() const
{
	// acteur enfant (HelmStation) : le parent est le bateau ; sinon le bateau auquel la barre est accrochée
	if (AYSPirateShip* Ship = Cast<AYSPirateShip>(GetParentActor()))
	{
		return Ship;
	}
	return Cast<AYSPirateShip>(GetAttachParentActor());
}

FVector AYSShipHelm::GetPilotFeetLocation() const
{
	return PilotPoint->GetComponentLocation();
}

FVector AYSShipHelm::GetPilotForward() const
{
	return PilotPoint->GetForwardVector().GetSafeNormal2D();
}
