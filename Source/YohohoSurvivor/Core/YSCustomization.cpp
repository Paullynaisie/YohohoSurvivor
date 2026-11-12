#include "Core/YSCustomization.h"
#include "Core/YSGameState.h"
#include "Core/YSPlayerState.h"
#include "Player/YSPlayerCharacter.h"
#include "Summons/YSCaptainMinion.h"

const AYSPlayerCharacter* YSPlayerColor::FindPlayer(const AActor* Source)
{
	const AActor* Actor = Source;
	for (int32 Depth = 0; Actor && Depth < 8; ++Depth)
	{
		if (const AYSCaptainMinion* Minion = Cast<AYSCaptainMinion>(Actor))
		{
			return Minion->GetCaptain();
		}
		if (const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(Actor))
		{
			return Character;
		}
		const AActor* Next = Actor->GetInstigator();
		if (!Next || Next == Actor)
		{
			Next = Actor->GetOwner();
		}
		Actor = Next;
	}
	return nullptr;
}

const FYSCustomizationColor* YSPlayerColor::Find(const AActor* Source)
{
	// couleur du joueur (PlayerState) ; HiredMouss : la sienne
	const AYSPlayerCharacter* Player = FindPlayer(Source);
	const int32 ColorIndex = Player ? Player->GetCustomizationColorIndex() : INDEX_NONE;
	const UWorld* World = Source ? Source->GetWorld() : nullptr;
	const AYSGameState* YSGameState = World ? World->GetGameState<AYSGameState>() : nullptr;
	if (!YSGameState || !YSGameState->GetCustomizationColors().IsValidIndex(ColorIndex))
	{
		return nullptr;
	}
	return &YSGameState->GetCustomizationColors()[ColorIndex];
}
