#include "Ship/YSShipHelm.h"
#include "Components/SceneComponent.h"
#include "Core/YSGameState.h"
#include "Engine/World.h"
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
	// flow : pas de pilotage tant que l'objectif le bloque (défendre le navire, attendre l'équipage)
	const AYSGameState* YSGameState = GetWorld() ? GetWorld()->GetGameState<AYSGameState>() : nullptr;
	if (YSGameState && YSGameState->IsHelmLockedFor(Ship))
	{
		return false;
	}
	return Ship && Ship->IsDrivable() && Ship->GetHelm() == this && Super::CanInteract(Character);
}

void AYSShipHelm::ApplyPilotInput(AYSPlayerCharacter* Character, const FVector2D& Input)
{
	AYSPirateShip* Ship = GetShip();
	const AYSGameState* YSGameState = GetWorld() ? GetWorld()->GetGameState<AYSGameState>() : nullptr;
	if (YSGameState && YSGameState->IsHelmLockedFor(Ship))
	{
		return;
	}
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
			Ship->SetRamming(false);
		}
	}
}

void AYSShipHelm::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AYSPirateShip* Ship = HasAuthority() ? GetShip() : nullptr;
	if (!Ship || Ship->GetHelm() != this)
	{
		return;
	}
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	AYSPlayerCharacter* Pilot = GetOccupant();
	const bool bRam = Pilot && Pilot->IsStationInputHeld(YSStationInput::Left) && Ship->IsDrivable() && !(YSGameState && YSGameState->IsHelmLockedFor(Ship));
	Ship->SetRamming(bRam, Pilot ? Pilot->GetController() : nullptr);
}

bool AYSShipHelm::GetActionIcon(UTexture2D*& OutIcon, FLinearColor& OutTint, float& OutRemaining, float& OutDuration) const
{
	if (!RamIcon)
	{
		return Super::GetActionIcon(OutIcon, OutTint, OutRemaining, OutDuration);
	}
	const AYSPirateShip* Ship = GetShip();
	OutIcon = RamIcon;
	OutTint = Ship && Ship->IsRamming() ? FLinearColor(1.0f, 0.6f, 0.2f) : FLinearColor::White;
	OutRemaining = 0.0f;
	OutDuration = 1.0f;
	return true;
}
