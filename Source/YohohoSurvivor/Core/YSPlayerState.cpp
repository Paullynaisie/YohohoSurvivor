#include "Core/YSPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"

void AYSPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSPlayerState, SelectedCharacterIndex);
	DOREPLIFETIME(AYSPlayerState, bIsDead);
	DOREPLIFETIME(AYSPlayerState, bReady);
	DOREPLIFETIME(AYSPlayerState, UpgradeOffers);
	DOREPLIFETIME(AYSPlayerState, ChosenUpgradeOffer);
	DOREPLIFETIME(AYSPlayerState, ColorIndex);
	DOREPLIFETIME(AYSPlayerState, FaceIndex);
}

void AYSPlayerState::SetCustomization(int32 NewColorIndex, int32 NewFaceIndex)
{
	ColorIndex = NewColorIndex;
	FaceIndex = NewFaceIndex;
	// OnRep n'est pas appelé sur le serveur : application directe pour l'hôte
	ForceNetUpdate();
	OnRep_Customization();
}

void AYSPlayerState::OnRep_Customization()
{
	if (AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->RefreshCustomization();
	}
}
