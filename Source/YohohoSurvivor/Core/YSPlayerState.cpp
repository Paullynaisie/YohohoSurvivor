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
	DOREPLIFETIME(AYSPlayerState, BoatSeat);
	DOREPLIFETIME(AYSPlayerState, UpgradeOffers);
	DOREPLIFETIME(AYSPlayerState, UpgradeRerollsLeft);
	DOREPLIFETIME(AYSPlayerState, ChosenUpgradeOffer);
	DOREPLIFETIME(AYSPlayerState, ColorIndex);
	DOREPLIFETIME(AYSPlayerState, FaceIndex);
	DOREPLIFETIME(AYSPlayerState, bLeftHanded);
	DOREPLIFETIME(AYSPlayerState, MatchStats);
	DOREPLIFETIME(AYSPlayerState, FinalUpgrades);
}

void AYSPlayerState::CaptureFinalUpgrades()
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetPawn());
	if (!Character)
	{
		Character = GetDeadBody();
	}
	FinalUpgrades = YSMatchStats::CaptureUpgrades(Character);
	UE_LOG(LogTemp, Log, TEXT("[YSScoreboard] %s : %d classe(s), %d upgrade(s) d'arme, %d golden, %d shard(s) ; %.0f dégâts (max %.0f), %d NME tués, %.0f subis, %.0f par l'armure, %d réanimation(s), %d objet(s) déposé(s), %d mort(s)"),
		*GetPlayerName(), FinalUpgrades.ClassUpgrades.Num(), FinalUpgrades.WeaponUpgrades.Num(), FinalUpgrades.GoldenUpgrades.Num(), FinalUpgrades.Shards.Num(),
		MatchStats.DamageDealt, MatchStats.BiggestHit, MatchStats.Kills, MatchStats.DamageTaken, MatchStats.ArmorReduced, MatchStats.Revives, MatchStats.ItemsDelivered, MatchStats.Deaths);
	ForceNetUpdate();
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
