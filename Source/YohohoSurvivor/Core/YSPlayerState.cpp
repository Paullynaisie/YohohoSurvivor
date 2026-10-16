#include "Core/YSPlayerState.h"
#include "Net/UnrealNetwork.h"

void AYSPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSPlayerState, SelectedCharacterIndex);
	DOREPLIFETIME(AYSPlayerState, bIsDead);
}
