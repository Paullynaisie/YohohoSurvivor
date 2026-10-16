#include "Core/YSMenuGameMode.h"
#include "Player/YSMenuPlayerController.h"

AYSMenuGameMode::AYSMenuGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AYSMenuPlayerController::StaticClass();
}
