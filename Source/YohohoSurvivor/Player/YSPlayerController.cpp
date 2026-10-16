#include "Player/YSPlayerController.h"
#include "Abilities/YSAbilityComponent.h"
#include "Player/YSPlayerCharacter.h"
#include "Weapons/YSWeapon.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Enemies/YSFoeCharacter.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Containers/Ticker.h"
#include "GameFramework/Pawn.h"
#include "UI/YSAbilityIconWidget.h"
#include "UI/YSCharacterSelectWidget.h"
#include "UI/YSPlayerHUDWidget.h"
#include "Upgrades/YSUpgrade.h"
#include "UI/YSDeathScreenWidget.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
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

			DeathScreen = CreateWidget<UYSDeathScreenWidget>(this, UYSDeathScreenWidget::StaticClass());
			DeathScreen->SetVisibility(ESlateVisibility::Collapsed);
			FGameViewportWidgetSlot DeathSlot;
			DeathSlot.Anchors = FAnchors(0.0f, 0.0f, 1.0f, 1.0f);
			DeathSlot.ZOrder = 20;
			ViewportSubsystem->AddWidgetForPlayer(DeathScreen, GetLocalPlayer(), DeathSlot);

			// HUD haut gauche (taille automatique)
			PlayerHUD = CreateWidget<UYSPlayerHUDWidget>(this, UYSPlayerHUDWidget::StaticClass());
			FGameViewportWidgetSlot HUDSlot;
			HUDSlot.Anchors = FAnchors(0.0f, 0.0f);
			HUDSlot.Offsets = FMargin(24.0f, 24.0f, 0.0f, 0.0f);
			HUDSlot.ZOrder = 1;
			ViewportSubsystem->AddWidgetForPlayer(PlayerHUD, GetLocalPlayer(), HUDSlot);

			// choix d'upgrade : plein écran comme la sélection de personnage, boutons plus hauts (nom + description)
			UpgradeMenu = CreateWidget<UYSCharacterSelectWidget>(this, UYSCharacterSelectWidget::StaticClass());
			UpgradeMenu->ButtonSize = FVector2D(440.0f, 130.0f);
			UpgradeMenu->ButtonFontSize = 18;
			UpgradeMenu->OnCharacterChosen.BindUObject(this, &AYSPlayerController::ChooseUpgrade);
			UpgradeMenu->SetVisibility(ESlateVisibility::Collapsed);
			FGameViewportWidgetSlot UpgradeSlot;
			UpgradeSlot.Anchors = FAnchors(0.0f, 0.0f, 1.0f, 1.0f);
			UpgradeSlot.ZOrder = 15;
			ViewportSubsystem->AddWidgetForPlayer(UpgradeMenu, GetLocalPlayer(), UpgradeSlot);
		}

		// debug / tests automatisés : -YSAutoSelect=<index> choisit le personnage dès l'arrivée (y compris en client)
		int32 AutoSelectIndex = INDEX_NONE;
		if (FParse::Value(FCommandLine::Get(), TEXT("YSAutoSelect="), AutoSelectIndex))
		{
			YSSelectCharacter(AutoSelectIndex);
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

	// mort : écran « You're dead » à la place du menu (pas de nouveau choix)
	const AYSPlayerState* MyPlayerState = GetPlayerState<AYSPlayerState>();
	const bool bDead = MyPlayerState && MyPlayerState->IsDead();
	if (DeathScreen)
	{
		DeathScreen->SetVisibility(bDead ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// le menu disparaît dès que le joueur a son personnage
	const bool bHasCharacter = Cast<AYSPlayerCharacter>(GetPawn()) != nullptr;
	CharacterSelectMenu->SetVisibility(bHasCharacter || bDead ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (bHasCharacter || bDead)
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

void AYSPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// en pause (choix d'upgrade), PlayerTick n'est plus appelé mais Tick si : l'UI reste vivante
	if (IsLocalController())
	{
		UpdateCharacterSelectMenu();
		UpdatePlayerHUD();
		UpdateUpgradeMenu();
	}
}

void AYSPlayerController::UpdatePlayerHUD()
{
	if (!PlayerHUD)
	{
		return;
	}

	const AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(GetPawn());
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	const bool bInMatch = YSGameState && YSGameState->IsMatchStarted();

	PlayerHUD->SetVisibility(bInMatch ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	PlayerHUD->SetHealth(PlayerCharacter != nullptr, PlayerCharacter ? PlayerCharacter->GetHealth() : 0.0f, PlayerCharacter ? PlayerCharacter->GetMaxHealth() : 0.0f);
	if (YSGameState)
	{
		PlayerHUD->SetLevel(YSGameState->GetTeamLevel(), YSGameState->GetTeamExperience(), YSGameState->GetExperienceToNextLevel());
	}
}

void AYSPlayerController::UpdateUpgradeMenu()
{
	if (!UpgradeMenu)
	{
		return;
	}

	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	const AYSPlayerState* MyState = GetPlayerState<AYSPlayerState>();
	const AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(GetPawn());
	const bool bShow = YSGameState && YSGameState->IsUpgradeSelectionActive() && MyState && MyState->GetUpgradeOffers().Num() > 0 && PlayerCharacter;

	UpgradeMenu->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!bShow)
	{
		DisplayedUpgradeOffers.Reset();
		return;
	}

	// nouvelles propositions (ou progression changée, pour des propositions identiques enchaînées) : reconstruire les boutons
	const int32 TotalStacks = PlayerCharacter->GetTotalUpgradeStacks();

	if (DisplayedUpgradeOffers != MyState->GetUpgradeOffers() || (DisplayedUpgradeStacks != TotalStacks && MyState->IsChoosingUpgrade()))
	{
		DisplayedUpgradeOffers = MyState->GetUpgradeOffers();
		DisplayedUpgradeStacks = TotalStacks;
		UpgradeMenu->ClearChoices();
		UpgradeMenu->SetTitle(FText::Format(NSLOCTEXT("YS", "UpgradeTitle", "Niveau {0} ! Choisis une amélioration"), YSGameState->GetTeamLevel()));

		TArray<TArray<FYSChoiceLine>> Choices;
		for (const FYSUpgradeOffer& Offer : DisplayedUpgradeOffers)
		{
			TArray<FYSChoiceLine>& Lines = Choices.AddDefaulted_GetRef();
			const UYSUpgrade* Upgrade = PlayerCharacter->GetUpgradePool().IsValidIndex(Offer.PoolIndex) ? PlayerCharacter->GetUpgradePool()[Offer.PoolIndex].Get() : nullptr;
			if (Upgrade)
			{
				Upgrade->GetOfferLines(PlayerCharacter, Offer.Variant, PlayerCharacter->GetUpgradeProgress(Offer.PoolIndex, Offer.Variant), Offer.Rarity, Lines);
			}
			else
			{
				Lines.AddDefaulted_GetRef().Text = FText::FromString(TEXT("?"));
			}
		}
		UpgradeMenu->SetChoices(Choices);
	}

	UpgradeMenu->SetSelectedIndex(MyState->GetChosenUpgradeOffer());

	// combien de joueurs concernés ont déjà choisi
	int32 Ready = 0;
	int32 Concerned = 0;
	for (const APlayerState* OtherPlayer : YSGameState->PlayerArray)
	{
		const AYSPlayerState* YSPlayer = Cast<AYSPlayerState>(OtherPlayer);
		if (YSPlayer && YSPlayer->GetUpgradeOffers().Num() > 0)
		{
			++Concerned;
			Ready += YSPlayer->IsChoosingUpgrade() ? 0 : 1;
		}
	}
	UpgradeMenu->SetStatus(FText::Format(NSLOCTEXT("YS", "UpgradeStatus", "Joueurs prêts : {0} / {1}"), Ready, Concerned));
}

void AYSPlayerController::ChooseUpgrade(int32 OfferIndex)
{
	const AYSPlayerState* MyState = GetPlayerState<AYSPlayerState>();
	if (MyState && MyState->IsChoosingUpgrade())
	{
		ServerChooseUpgrade(OfferIndex);
	}
}

void AYSPlayerController::ServerChooseUpgrade_Implementation(int32 OfferIndex)
{
	if (AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>())
	{
		GameMode->PlayerChoseUpgrade(this, OfferIndex);
	}
}

void AYSPlayerController::YSAddXP(float Amount)
{
	ServerAddXP(Amount);
}

void AYSPlayerController::YSChooseUpgrade(int32 OfferIndex)
{
	if (UpgradeMenu)
	{
		UpgradeMenu->SetSelectedIndex(OfferIndex);
	}
	ChooseUpgrade(OfferIndex);
}

void AYSPlayerController::YSAfter(float Delay, const FString& Command)
{
	// ticker hors monde : continue pendant la pause
	const FString FullCommand = Command.Replace(TEXT("_"), TEXT(" "));
	TWeakObjectPtr<AYSPlayerController> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, FullCommand](float)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->ConsoleCommand(FullCommand);
		}
		return false;
	}), Delay);
}

void AYSPlayerController::ServerAddXP_Implementation(float Amount)
{
	if (AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>())
	{
		GameMode->AddTeamExperience(Amount);
	}
}

void AYSPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	UpdateAim();

	// la destruction du personnage (UnPossess) remet la vue sur le contrôleur : on garde la caméra de mort
	if (DeathCamera && GetViewTarget() != DeathCamera)
	{
		SetViewTarget(DeathCamera);
	}

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

void AYSPlayerController::YSDamageSelf(float Amount)
{
	ServerDamageSelf(Amount);
}

void AYSPlayerController::ServerDamageSelf_Implementation(float Amount)
{
	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->TakeDamage(Amount, FDamageEvent(), nullptr, nullptr);
	}
}

void AYSPlayerController::ClientPlayerDied_Implementation(FVector CameraLocation, FRotator CameraRotation, float CameraFOV)
{
	if (!DeathCamera)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		DeathCamera = GetWorld()->SpawnActor<ACameraActor>(CameraLocation, CameraRotation, Params);
	}

	if (DeathCamera)
	{
		DeathCamera->SetActorLocationAndRotation(CameraLocation, CameraRotation);
		DeathCamera->GetCameraComponent()->SetFieldOfView(CameraFOV);
		SetViewTarget(DeathCamera);
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
