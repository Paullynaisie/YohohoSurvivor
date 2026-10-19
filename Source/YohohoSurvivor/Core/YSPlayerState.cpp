#include "Core/YSPlayerState.h"
#include "Core/YSGameState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"

FString AYSPlayerState::GetDisplayName() const
{
	// personnage en jeu, sinon celui choisi dans le menu (aussi après la mort)
	const AYSPlayerCharacter* Character = GetPawn<AYSPlayerCharacter>();
	if (!Character)
	{
		const AYSGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AYSGameState>() : nullptr;
		const TArray<TSubclassOf<AYSPlayerCharacter>>* Characters = GameState ? &GameState->GetSelectableCharacters() : nullptr;
		if (Characters && Characters->IsValidIndex(SelectedCharacterIndex) && (*Characters)[SelectedCharacterIndex])
		{
			Character = (*Characters)[SelectedCharacterIndex]->GetDefaultObject<AYSPlayerCharacter>();
		}
	}
	return Character ? FString::Printf(TEXT("%s %s"), *Character->GetCharacterName().ToString(), *GetPlayerName()) : GetPlayerName();
}

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
	DOREPLIFETIME(AYSPlayerState, bLeftHanded);
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
