#include "Player/YSPlayerController.h"
#include "Abilities/YSAbilityComponent.h"
#include "Player/YSPlayerCharacter.h"
#include "Weapons/YSWeapon.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Enemies/YSFoeCharacter.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "UI/YSAbilityIconWidget.h"
#include "UI/YSCharacterSelectWidget.h"
#include "Core/YSGameInstance.h"
#include "Core/YSGameMode.h"
#include "Components/InputComponent.h"
#include "Core/YSGameState.h"
#include "Core/YSPlayerState.h"

AYSPlayerController::AYSPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
	AbilityIconClass = UYSAbilityIconWidget::StaticClass();
}

void AYSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		// le curseur reste visible même quand la fenêtre capture la souris
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);

		UGameViewportSubsystem* ViewportSubsystem = UGameViewportSubsystem::Get();
		if (AbilityIconClass && ViewportSubsystem)
		{
			AbilityIcon = CreateWidget<UYSAbilityIconWidget>(this, AbilityIconClass);

			// tout le slot en une fois : SetPositionInViewport / SetDesiredSizeInViewport remettent l'ancre en haut à gauche
			FGameViewportWidgetSlot ViewportSlot;
			ViewportSlot.Anchors = FAnchors(0.5f, 1.0f);
			ViewportSlot.Alignment = FVector2D(0.5f, 1.0f);
			ViewportSlot.Offsets = FMargin(0.0f, -AbilityIconBottomMargin, AbilityIcon->IconSize, AbilityIcon->IconSize);
			ViewportSubsystem->AddWidgetForPlayer(AbilityIcon, GetLocalPlayer(), ViewportSlot);
		}

		// menu de sélection plein écran, au-dessus du reste de l'UI
		if (ViewportSubsystem)
		{
			CharacterSelectMenu = CreateWidget<UYSCharacterSelectWidget>(this, UYSCharacterSelectWidget::StaticClass());
			CharacterSelectMenu->OnCharacterChosen.BindUObject(this, &AYSPlayerController::YSSelectCharacter);

			FGameViewportWidgetSlot MenuSlot;
			MenuSlot.Anchors = FAnchors(0.0f, 0.0f, 1.0f, 1.0f);
			MenuSlot.ZOrder = 10;
			ViewportSubsystem->AddWidgetForPlayer(CharacterSelectMenu, GetLocalPlayer(), MenuSlot);
		}
	}
}

void AYSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AYSPlayerController::YSLeaveGame);
}

void AYSPlayerController::YSLeaveGame()
{
	if (UYSGameInstance* GameInstance = GetGameInstance<UYSGameInstance>())
	{
		GameInstance->ReturnToMenu();
	}
}

void AYSPlayerController::YSSelectCharacter(int32 CharacterIndex)
{
	LocalSelectedCharacter = CharacterIndex;
	if (CharacterSelectMenu)
	{
		CharacterSelectMenu->SetSelectedIndex(CharacterIndex);
	}
	ServerSelectCharacter(CharacterIndex);
}

void AYSPlayerController::ServerSelectCharacter_Implementation(int32 CharacterIndex)
{
	if (AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>())
	{
		GameMode->PlayerSelectedCharacter(this, CharacterIndex);
	}
}

void AYSPlayerController::UpdateCharacterSelectMenu()
{
	if (!CharacterSelectMenu)
	{
		return;
	}

	// le menu disparaît dès que le joueur a son personnage
	const bool bHasCharacter = Cast<AYSPlayerCharacter>(GetPawn()) != nullptr;
	CharacterSelectMenu->SetVisibility(bHasCharacter ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (bHasCharacter)
	{
		return;
	}

	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	if (!YSGameState)
	{
		return;
	}

	// boutons construits dès que la liste est arrivée du serveur
	if (!CharacterSelectMenu->HasCharacters() && YSGameState->GetSelectableCharacters().Num() > 0)
	{
		TArray<FText> Names;
		for (const TSubclassOf<AYSPlayerCharacter>& CharacterClass : YSGameState->GetSelectableCharacters())
		{
			const AYSPlayerCharacter* Default = CharacterClass ? CharacterClass->GetDefaultObject<AYSPlayerCharacter>() : nullptr;
			Names.Add(Default ? Default->GetCharacterName() : FText::FromString(TEXT("?")));
		}
		CharacterSelectMenu->SetCharacters(Names);

		// choix déjà connu par le serveur (reconnexion, etc.)
		const AYSPlayerState* MyState = GetPlayerState<AYSPlayerState>();
		CharacterSelectMenu->SetSelectedIndex(MyState && MyState->HasSelectedCharacter() ? MyState->GetSelectedCharacterIndex() : LocalSelectedCharacter);
	}

	// combien de joueurs ont déjà choisi
	int32 Ready = 0;
	for (const APlayerState* OtherPlayer : YSGameState->PlayerArray)
	{
		const AYSPlayerState* YSPlayer = Cast<AYSPlayerState>(OtherPlayer);
		Ready += (YSPlayer && YSPlayer->HasSelectedCharacter()) ? 1 : 0;
	}
	CharacterSelectMenu->SetStatus(FText::Format(NSLOCTEXT("YS", "CharacterSelectStatus", "Joueurs prêts : {0} / {1}"), Ready, YSGameState->PlayerArray.Num()));
}

void AYSPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	UpdateAim();
	UpdateCharacterSelectMenu();

	if (DebugWalkRemaining > 0.0f)
	{
		DebugWalkRemaining -= DeltaTime;
		if (APawn* ControlledPawn = GetPawn())
		{
			ControlledPawn->AddMovementInput(DebugWalkDirection);
		}
	}

	if (AbilityIcon)
	{
		// icône de l'action de la main principale (droite), sinon de la main gauche
		const AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(GetPawn());
		const AYSWeapon* MainWeapon = PlayerCharacter ? PlayerCharacter->GetRightHandWeapon() : nullptr;
		const AYSWeapon* OffWeapon = PlayerCharacter ? PlayerCharacter->GetLeftHandWeapon() : nullptr;
		UYSAbilityComponent* Action = MainWeapon ? MainWeapon->GetActiveAction() : nullptr;
		if (!Action && OffWeapon)
		{
			Action = OffWeapon->GetActiveAction();
		}
		AbilityIcon->Refresh(Action);
	}
}

void AYSPlayerController::YSDamageFoes(float Amount, float Delay, bool bScreenshot)
{
	FTimerHandle Timer;
	GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, Amount, bScreenshot]()
	{
		if (HasAuthority())
		{
			DamageAllFoes(Amount);
		}
		else
		{
			ServerDamageFoes(Amount);
		}

		if (bScreenshot)
		{
			// laisse le temps aux chiffres et barres d'apparaître
			FTimerHandle ShotTimer;
			GetWorldTimerManager().SetTimer(ShotTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				ConsoleCommand(TEXT("shot showui"));
			}), 0.25f, false);
		}
	}), FMath::Max(Delay, 0.01f), false);
}

void AYSPlayerController::YSDebugWalk(float X, float Y, float Duration, int32 Shots)
{
	DebugWalkDirection = FVector(X, Y, 0.0f).GetSafeNormal();
	DebugWalkRemaining = Duration;

	for (int32 Index = 1; Index <= Shots; ++Index)
	{
		FTimerHandle ShotTimer;
		GetWorldTimerManager().SetTimer(ShotTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			ConsoleCommand(TEXT("shot showui"));
		}), Duration * Index / (Shots + 1), false);
	}
}

void AYSPlayerController::ServerDamageFoes_Implementation(float Amount)
{
	DamageAllFoes(Amount);
}

void AYSPlayerController::DamageAllFoes(float Amount)
{
	for (TActorIterator<AYSFoeCharacter> It(GetWorld()); It; ++It)
	{
		// pas de DamageCauser : le chiffre s'affiche sur le NME lui-même
		It->TakeDamage(Amount, FDamageEvent(), this, nullptr);
	}
}

void AYSPlayerController::UpdateAim()
{
	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	FVector RayOrigin, RayDirection;
	if (!DeprojectMousePositionToWorld(RayOrigin, RayDirection) || FMath::IsNearlyZero(RayDirection.Z))
	{
		return;
	}

	// intersection du rayon souris avec le plan horizontal passant par le pawn
	const FVector PawnLocation = ControlledPawn->GetActorLocation();
	const float Distance = (PawnLocation.Z - RayOrigin.Z) / RayDirection.Z;
	if (Distance <= 0.0f)
	{
		return;
	}

	AimLocation = RayOrigin + RayDirection * Distance;

	const FVector ToAim = AimLocation - PawnLocation;
	if (ToAim.SizeSquared2D() > 1.0f)
	{
		SetControlRotation(FRotator(0.0f, ToAim.Rotation().Yaw, 0.0f));
	}
}
