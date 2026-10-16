#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "YSMenuGameMode.generated.h"

/**
 *  GameMode de la map MainMenu : pas de pawn, contrôleur AYSMenuPlayerController qui affiche le menu principal.
 *  Assigné dans les World Settings de /Game/Maps/MainMenu (script Tools/EditorScripts/setup_main_menu.py).
 */
UCLASS()
class AYSMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	AYSMenuGameMode();
};
