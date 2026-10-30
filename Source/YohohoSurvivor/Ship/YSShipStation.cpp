#include "Ship/YSShipStation.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "Ship/YSPirateShip.h"
#include "Summons/YSCaptainMinion.h"

AYSShipStation::AYSShipStation()
{
	// vérifie que l'occupant est toujours là (serveur), animations des postes
	PrimaryActorTick.bCanEverTick = true;
	bCarriable = false;
	DisplayName = NSLOCTEXT("YS", "ShipStationName", "Poste");

	OccupantPoint = CreateDefaultSubobject<USceneComponent>(TEXT("OccupantPoint"));
	OccupantPoint->SetupAttachment(GetRootComponent());

	ExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ExitPoint"));
	ExitPoint->SetupAttachment(GetRootComponent());
	ExitPoint->SetRelativeLocation(FVector(-120.0f, 0.0f, 0.0f));
}

void AYSShipStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSShipStation, Occupant);
	DOREPLIFETIME(AYSShipStation, Health);
}

void AYSShipStation::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		Health = MaxHealth;
	}
}

void AYSShipStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// poste détruit (chaloupe) : l'occupant n'y est plus
	if (HasAuthority() && IsValid(Occupant) && Occupant->GetCurrentStation() == this)
	{
		Occupant->LeaveStation(TEXT("poste détruit"), true);
	}

	Super::EndPlay(EndPlayReason);
}

void AYSShipStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// occupant parti (déconnexion, changement de classe, mort) : le poste est libre
	if (HasAuthority() && Occupant && (!IsValid(Occupant) || !Occupant->IsAlive() || Occupant->GetCurrentStation() != this))
	{
		UE_LOG(LogTemp, Log, TEXT("%s %s : occupant parti, poste libre"), GetLogTag(), *GetName());
		SetOccupant(nullptr);
	}
}

bool AYSShipStation::IsShipAvailable() const
{
	const AYSPirateShip* Ship = GetShip();
	return Ship && Ship->IsPresent() && !Ship->IsActorBeingDestroyed();
}

bool AYSShipStation::CanInteract(const AYSPlayerCharacter* Character) const
{
	return IsShipAvailable() && (!Occupant || CanTakeOverFrom(Character)) && Character && Character->IsAlive() && !Character->IsCarrying() && (bAllowSwimmingEntry || !Character->IsSwimming()) && !Character->IsAtStation();
}

bool AYSShipStation::CanTakeOverFrom(const AYSPlayerCharacter* Character) const
{
	return Occupant && Occupant->IsA<AYSCaptainMinion>() && Character && !Character->IsA<AYSCaptainMinion>();
}

void AYSShipStation::Interact(AYSPlayerCharacter* Character)
{
	if (!HasAuthority() || !CanInteract(Character))
	{
		return;
	}
	// place volée à un minion : il est reposé sur le côté et ira voir ailleurs
	if (AYSPlayerCharacter* Replaced = Occupant)
	{
		UE_LOG(LogTemp, Log, TEXT("%s %s prend la place de %s à %s"), GetLogTag(), *Character->GetName(), *Replaced->GetName(), *GetName());
		Replaced->LeaveStation(TEXT("place prise par un joueur"), true);
		Replaced->TeleportToFeetLocation(GetOccupantFeetLocation() + OccupantPoint->GetRightVector().GetSafeNormal2D() * ReplacedOccupantOffset, GetOccupantForward());
	}
	Character->EnterStation(this);
}

void AYSShipStation::SetOccupant(AYSPlayerCharacter* NewOccupant)
{
	if (!HasAuthority() || Occupant == NewOccupant)
	{
		return;
	}
	AYSPlayerCharacter* Previous = Occupant;
	Occupant = NewOccupant;
	SetPromptVisible(false);
	OnOccupantChanged(Previous);
	ForceNetUpdate();
}

void AYSShipStation::OnRep_Occupant()
{
	SetPromptVisible(false);
	OnOccupantChanged(nullptr);
}

AYSPirateShip* AYSShipStation::GetShip() const
{
	return AYSPirateShip::GetShipOf(this);
}

FVector AYSShipStation::GetOccupantFeetLocation() const
{
	return OccupantPoint->GetComponentLocation();
}

FVector AYSShipStation::GetOccupantForward() const
{
	return OccupantPoint->GetForwardVector().GetSafeNormal2D();
}

FVector AYSShipStation::GetExitFeetLocation() const
{
	return ExitPoint->GetComponentLocation();
}
