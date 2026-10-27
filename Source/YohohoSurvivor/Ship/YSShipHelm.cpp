#include "Ship/YSShipHelm.h"
#include "Components/SceneComponent.h"
#include "Player/YSPlayerCharacter.h"
#include "Ship/YSPirateShip.h"

AYSShipHelm::AYSShipHelm()
{
	DisplayName = NSLOCTEXT("YS", "ShipHelmName", "Barre");

	// pilote derrière la roue (pieds sur le pont du gaillard), mains sur la jante de chaque côté
	GetOccupantPoint()->SetRelativeLocation(FVector(-90.0f, 0.0f, -120.0f));
	GetHandPoint(true)->SetRelativeLocation(FVector(-15.0f, -40.0f, 10.0f));
	GetHandPoint(false)->SetRelativeLocation(FVector(-15.0f, 40.0f, 10.0f));

	// caméra du pilote : centrée sur le bateau, dézoomée
	CameraArmLength = 3600.0f;
	bCameraOnShip = true;
	CameraOffsetMode = EYSStationCameraOffset::None;
}

bool AYSShipHelm::CanInteract(const AYSPlayerCharacter* Character) const
{
	const AYSPirateShip* Ship = GetShip();
	return Ship && Ship->IsDrivable() && Ship->GetHelm() == this && Super::CanInteract(Character);
}

void AYSShipHelm::ApplyPilotInput(AYSPlayerCharacter* Character, const FVector2D& Input)
{
	AYSPirateShip* Ship = GetShip();
	if (Ship && Ship->GetHelm() == this && Character == GetOccupant())
	{
		Ship->SetPilotInput(Input.Y, Input.X);
	}
}

void AYSShipHelm::OnOccupantChanged(AYSPlayerCharacter* Previous)
{
	if (HasAuthority())
	{
		if (AYSPirateShip* Ship = GetShip())
		{
			Ship->SetPilotInput(0.0f, 0.0f);
		}
	}
}
