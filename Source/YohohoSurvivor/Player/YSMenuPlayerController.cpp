#include "Player/YSMenuPlayerController.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "UI/YSMainMenuWidget.h"

AYSMenuPlayerController::AYSMenuPlayerController()
{
	bShowMouseCursor = true;
}

void AYSMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	UGameViewportSubsystem* ViewportSubsystem = UGameViewportSubsystem::Get();
	if (!IsLocalController() || !ViewportSubsystem)
	{
		return;
	}

	MainMenu = CreateWidget<UYSMainMenuWidget>(this, UYSMainMenuWidget::StaticClass());

	FGameViewportWidgetSlot MenuSlot;
	MenuSlot.Anchors = FAnchors(0.0f, 0.0f, 1.0f, 1.0f);
	ViewportSubsystem->AddWidgetForPlayer(MainMenu, GetLocalPlayer(), MenuSlot);

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}
