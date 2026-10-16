#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "YSMenuPlayerController.generated.h"

class UYSMainMenuWidget;

/** Contrôleur du menu principal : affiche UYSMainMenuWidget en plein écran, souris en mode UI */
UCLASS()
class AYSMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	AYSMenuPlayerController();

	virtual void BeginPlay() override;

private:

	UPROPERTY(Transient)
	TObjectPtr<UYSMainMenuWidget> MainMenu;
};
