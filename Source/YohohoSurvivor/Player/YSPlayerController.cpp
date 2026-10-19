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
#include "UI/YSOffscreenIndicatorsWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "UI/YSEliteHealthWidget.h"
#include "UI/YSObjectiveWidget.h"
#include "Interaction/YSInteractiveObject.h"
#include "NPC/YSBrookerNPC.h"
#include "UI/YSDayBannerWidget.h"
#include "Upgrades/YSUpgrade.h"
#include "UI/YSDeathScreenWidget.h"
#include "Camera/CameraActor.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetRenderingLibrary.h"
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
			CharacterSelectMenu = CreateWidget<UYSCharacterSelectWidget>(this, CharacterSelectMenuClass ? CharacterSelectMenuClass.Get() : UYSCharacterSelectWidget::StaticClass());
			CharacterSelectMenu->OnCharacterChosen.BindUObject(this, &AYSPlayerController::YSSelectCharacter);
			CharacterSelectMenu->OnColorChosen.BindUObject(this, &AYSPlayerController::ChooseColor);
			CharacterSelectMenu->OnFaceChosen.BindUObject(this, &AYSPlayerController::ChooseFace);
			CharacterSelectMenu->OnHandednessChosen.BindUObject(this, &AYSPlayerController::ChooseHandedness);
			CharacterSelectMenu->OnNicknameCommitted.BindUObject(this, &AYSPlayerController::CommitNickname);
			CharacterSelectMenu->OnReadyClicked.BindUObject(this, &AYSPlayerController::ToggleReady);

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

			// icônes des autres joueurs hors écran, collées au bord (plein écran)
			OffscreenIndicators = CreateWidget<UYSOffscreenIndicatorsWidget>(this, UYSOffscreenIndicatorsWidget::StaticClass());
			OffscreenIndicators->SetVisibility(ESlateVisibility::Collapsed);
			FGameViewportWidgetSlot IndicatorsSlot;
			IndicatorsSlot.Anchors = FAnchors(0.0f, 0.0f, 1.0f, 1.0f);
			IndicatorsSlot.ZOrder = 2;
			ViewportSubsystem->AddWidgetForPlayer(OffscreenIndicators, GetLocalPlayer(), IndicatorsSlot);

			// barre de l'élite : haut droite (symétrique du HUD du joueur)
			EliteHealth = CreateWidget<UYSEliteHealthWidget>(this, UYSEliteHealthWidget::StaticClass());
			EliteHealth->SetVisibility(ESlateVisibility::Collapsed);
			FGameViewportWidgetSlot EliteSlot;
			EliteSlot.Anchors = FAnchors(1.0f, 0.0f);
			EliteSlot.Alignment = FVector2D(1.0f, 0.0f);
			EliteSlot.Offsets = FMargin(-24.0f, 24.0f, 0.0f, 0.0f);
			EliteSlot.ZOrder = 1;
			ViewportSubsystem->AddWidgetForPlayer(EliteHealth, GetLocalPlayer(), EliteSlot);

			// jauge d'objectif : haut droite, sous l'emplacement de la barre de l'élite (placement réglé dans le Widget Blueprint)
			Objective = CreateWidget<UYSObjectiveWidget>(this, ObjectiveWidgetClass ? ObjectiveWidgetClass.Get() : UYSObjectiveWidget::StaticClass());
			Objective->SetVisibility(ESlateVisibility::Collapsed);
			FGameViewportWidgetSlot ObjectiveSlot;
			ObjectiveSlot.Anchors = FAnchors(1.0f, 0.0f);
			ObjectiveSlot.Alignment = FVector2D(1.0f, 0.0f);
			ObjectiveSlot.Offsets = FMargin(-Objective->ScreenOffset.X, Objective->ScreenOffset.Y, 0.0f, 0.0f);
			ObjectiveSlot.ZOrder = 1;
			ViewportSubsystem->AddWidgetForPlayer(Objective, GetLocalPlayer(), ObjectiveSlot);

			// annonce « Jour X » : plein écran, texte centré
			DayBanner = CreateWidget<UYSDayBannerWidget>(this, UYSDayBannerWidget::StaticClass());
			DayBanner->SetVisibility(ESlateVisibility::Collapsed);
			FGameViewportWidgetSlot BannerSlot;
			BannerSlot.Anchors = FAnchors(0.0f, 0.0f, 1.0f, 1.0f);
			BannerSlot.ZOrder = 5;
			ViewportSubsystem->AddWidgetForPlayer(DayBanner, GetLocalPlayer(), BannerSlot);

			// choix d'upgrade : plein écran comme la sélection de personnage, boutons plus hauts (nom + description)
			UpgradeMenu = CreateWidget<UYSCharacterSelectWidget>(this, UYSCharacterSelectWidget::StaticClass());
			UpgradeMenu->ButtonSize = FVector2D(440.0f, 130.0f);
			UpgradeMenu->ButtonFontSize = 18;
			UpgradeMenu->OnCharacterChosen.BindUObject(this, &AYSPlayerController::ChooseUpgrade);
			UpgradeMenu->SetCustomizationVisible(false);
			UpgradeMenu->SetVisibility(ESlateVisibility::Collapsed);
			FGameViewportWidgetSlot UpgradeSlot;
			UpgradeSlot.Anchors = FAnchors(0.0f, 0.0f, 1.0f, 1.0f);
			UpgradeSlot.ZOrder = 15;
			ViewportSubsystem->AddWidgetForPlayer(UpgradeMenu, GetLocalPlayer(), UpgradeSlot);
		}

		// debug / tests automatisés : -YSAutoSelect=<index> choisit le personnage et se déclare prêt dès l'arrivée (y compris en client)
		int32 AutoSelectIndex = INDEX_NONE;
		if (FParse::Value(FCommandLine::Get(), TEXT("YSAutoSelect="), AutoSelectIndex))
		{
			YSSelectCharacter(AutoSelectIndex);
			YSReady();
		}

		// -YSExecOnJoin="cmd1, cmd2" : commandes lancées à l'arrivée dans la partie (un client lancé avec une IP
		// exécute -ExecCmds avant la connexion) ; avec YSAfter pour les différer
		FString JoinCommands;
		if (FParse::Value(FCommandLine::Get(), TEXT("YSExecOnJoin="), JoinCommands, false) && GetWorld()->GetNetMode() != NM_Standalone)
		{
			JoinCommands.TrimQuotesInline();
			TArray<FString> Commands;
			JoinCommands.ParseIntoArray(Commands, TEXT(","));
			for (FString& Command : Commands)
			{
				ConsoleCommand(Command.TrimStartAndEnd());
			}
		}
	}
}

void AYSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AYSPlayerController::YSLeaveGame);

#if !UE_BUILD_SHIPPING
	// codes de triche pour les tests (absents des builds Shipping)
	InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &AYSPlayerController::YSCheatAtkSpeed);
#endif
}

void AYSPlayerController::YSCheatAtkSpeed()
{
	ServerCheatAtkSpeed();
}

void AYSPlayerController::ServerCheatAtkSpeed_Implementation()
{
	AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(GetPawn());
	if (!PlayerCharacter)
	{
		return;
	}

	// Atk speed = variante Auto atk de la CD reduction ; 10 bonus Silver, sans dépasser le maximum de l'upgrade
	const TArray<TObjectPtr<UYSUpgrade>>& Pool = PlayerCharacter->GetUpgradePool();
	const int32 PoolIndex = Pool.IndexOfByPredicate([](const TObjectPtr<UYSUpgrade>& Upgrade) { return Cast<UYSHandCooldownReductionUpgrade>(Upgrade.Get()) != nullptr; });
	const UYSHandCooldownReductionUpgrade* AtkSpeed = Pool.IsValidIndex(PoolIndex) ? Cast<UYSHandCooldownReductionUpgrade>(Pool[PoolIndex].Get()) : nullptr;
	const int32 Variant = UYSHandStatUpgrade::MakeVariant(EYSActionSlot::AutoAtk);
	if (!AtkSpeed || !AtkSpeed->CanBeOffered(PlayerCharacter, Variant, PlayerCharacter->GetUpgradeProgress(PoolIndex, Variant)))
	{
		UE_LOG(LogTemp, Log, TEXT("[YSCheat] Atk speed : pas disponible (pas d'attaque auto éligible, ou maximum atteint)"));
		return;
	}

	FYSUpgradeOffer Offer;
	Offer.PoolIndex = PoolIndex;
	Offer.Variant = Variant;
	Offer.Rarity = EYSUpgradeRarity::Silver;
	int32 Given = 0;
	for (; Given < 10 && PlayerCharacter->GetUpgradeProgress(PoolIndex, Variant).Stacks < AtkSpeed->MaxStacks; ++Given)
	{
		PlayerCharacter->ApplyUpgrade(Offer);
	}
	UE_LOG(LogTemp, Log, TEXT("[YSCheat] Atk speed : %d bonus Silver donnés (%d / %d)"), Given, PlayerCharacter->GetUpgradeProgress(PoolIndex, Variant).Stacks, AtkSpeed->MaxStacks);
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
	UpdateCharacterPreview(!bHasCharacter && !bDead);
	if (bHasCharacter || bDead)
	{
		return;
	}

	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	if (!YSGameState)
	{
		return;
	}

	// personnalisation : boutons construits dès que les options sont arrivées du serveur, choix et pseudo du PlayerState
	if (!CharacterSelectMenu->HasCustomizationOptions() && YSGameState->GetCustomizationColors().Num() > 0)
	{
		TArray<FLinearColor> Colors;
		for (const FYSCustomizationColor& Color : YSGameState->GetCustomizationColors())
		{
			Colors.Add(Color.ButtonColor);
		}
		TArray<UTexture2D*> Faces;
		for (UTexture2D* Face : YSGameState->GetCustomizationFaces())
		{
			Faces.Add(Face);
		}
		CharacterSelectMenu->SetCustomizationOptions(Colors, Faces);
	}
	if (MyPlayerState)
	{
		CharacterSelectMenu->SetSelectedColor(MyPlayerState->GetColorIndex());
		CharacterSelectMenu->SetSelectedFace(MyPlayerState->GetFaceIndex());
		CharacterSelectMenu->SetSelectedHandedness(MyPlayerState->IsLeftHanded());
		CharacterSelectMenu->SetNickname(FText::FromString(MyPlayerState->GetPlayerName()));
		CharacterSelectMenu->SetReady(MyPlayerState->IsReady());

		// couleurs uniques : celles des autres joueurs sont grisées
		TArray<bool> TakenColors;
		TakenColors.SetNumZeroed(YSGameState->GetCustomizationColors().Num());
		for (const APlayerState* OtherPlayer : YSGameState->PlayerArray)
		{
			const AYSPlayerState* YSPlayer = Cast<AYSPlayerState>(OtherPlayer);
			if (YSPlayer && YSPlayer != MyPlayerState && TakenColors.IsValidIndex(YSPlayer->GetColorIndex()))
			{
				TakenColors[YSPlayer->GetColorIndex()] = true;
			}
		}
		CharacterSelectMenu->SetTakenColors(TakenColors);
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
		Ready += (YSPlayer && YSPlayer->IsReady()) ? 1 : 0;
	}
	CharacterSelectMenu->SetStatus(FText::Format(NSLOCTEXT("YS", "CharacterSelectStatus", "Joueurs prêts : {0} / {1}"), Ready, YSGameState->PlayerArray.Num()));
}

void AYSPlayerController::UpdateCharacterPreview(bool bShow)
{
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	const AYSPlayerState* MyPlayerState = GetPlayerState<AYSPlayerState>();

	// personnage affiché : celui choisi (sinon le premier de la liste)
	TSubclassOf<AYSPlayerCharacter> PreviewClass;
	if (YSGameState && YSGameState->GetSelectableCharacters().Num() > 0)
	{
		const int32 Index = MyPlayerState && MyPlayerState->HasSelectedCharacter() ? MyPlayerState->GetSelectedCharacterIndex() : FMath::Max(0, LocalSelectedCharacter);
		PreviewClass = YSGameState->GetSelectableCharacters().IsValidIndex(Index) ? YSGameState->GetSelectableCharacters()[Index] : YSGameState->GetSelectableCharacters()[0];
	}

	if (!bShow || !PreviewClass)
	{
		if (PreviewCharacter)
		{
			PreviewCharacter->Destroy();
			PreviewCharacter = nullptr;
		}
		if (PreviewLight)
		{
			PreviewLight->Destroy();
			PreviewLight = nullptr;
		}
		if (PreviewCapture)
		{
			PreviewCapture->Destroy();
			PreviewCapture = nullptr;
		}
		if (CharacterSelectMenu)
		{
			CharacterSelectMenu->SetPreviewTexture(nullptr);
		}
		return;
	}

	// au-dessus de la map (rien ne lui fait d'ombre), loin de tout ; changement de personnage = nouvel aperçu
	const FVector PreviewLocation(0.0f, 0.0f, 20000.0f);
	const bool bPreviewLeftHanded = MyPlayerState && MyPlayerState->IsLeftHanded();
	if (PreviewCharacter && (PreviewCharacter->GetClass() != PreviewClass || PreviewCharacter->IsLeftHanded() != bPreviewLeftHanded))
	{
		PreviewCharacter->Destroy();
		PreviewCharacter = nullptr;
	}
	if (!PreviewCharacter)
	{
		// local uniquement : ni répliqué, ni contrôlé
		const FTransform SpawnTransform(FRotator::ZeroRotator, PreviewLocation);
		PreviewCharacter = GetWorld()->SpawnActorDeferred<AYSPlayerCharacter>(PreviewClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!PreviewCharacter)
		{
			return;
		}
		PreviewCharacter->SetReplicates(false);
		PreviewCharacter->SetLeftHanded(bPreviewLeftHanded);
		PreviewCharacter->FinishSpawning(SpawnTransform);
		PreviewCharacter->GetCharacterMovement()->DisableMovement();
		PreviewCharacter->GetCharacterMovement()->GravityScale = 0.0f;
	}

	if (!PreviewCapture)
	{
		if (!PreviewTarget)
		{
			// même proportion que l'image de l'aperçu dans le menu
			const FVector2D Size = CharacterSelectMenu->PreviewSize;
			PreviewTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, FMath::Max(16, FMath::RoundToInt(Size.X)), FMath::Max(16, FMath::RoundToInt(Size.Y)), RTF_RGBA8, FLinearColor::Transparent);
		}

		// face au personnage (qui regarde vers +X), légèrement au-dessus
		const FVector CaptureLocation = PreviewLocation + FVector(PreviewCameraDistance, 0.0f, PreviewCameraHeight);
		const FRotator CaptureRotation = (PreviewLocation - CaptureLocation).Rotation();
		PreviewCapture = GetWorld()->SpawnActor<ASceneCapture2D>(CaptureLocation, CaptureRotation);
		if (!PreviewCapture)
		{
			return;
		}
		USceneCaptureComponent2D* Capture = PreviewCapture->GetCaptureComponent2D();
		Capture->TextureTarget = PreviewTarget;
		Capture->FOVAngle = PreviewFOV;
		Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Capture->bCaptureEveryFrame = true;
		// fond noir et exposition fixe : seul le personnage compte (pas de ciel, pas de brouillard)
		Capture->ShowFlags.SetAtmosphere(false);
		Capture->ShowFlags.SetFog(false);
		Capture->ShowFlags.SetVolumetricFog(false);
		Capture->ShowFlags.SetCloud(false);
		Capture->ShowFlags.SetEyeAdaptation(false);
		Capture->ShowFlags.SetBloom(false);
		CharacterSelectMenu->SetPreviewTexture(PreviewTarget);

		// éclairage de face, un peu au-dessus de la capture
		PreviewLight = GetWorld()->SpawnActor<APointLight>(CaptureLocation + FVector(0.0f, 150.0f, 200.0f), FRotator::ZeroRotator);
		if (PreviewLight)
		{
			PreviewLight->PointLightComponent->SetIntensity(PreviewLightIntensity);
			PreviewLight->PointLightComponent->SetAttenuationRadius(2000.0f);
			PreviewLight->PointLightComponent->SetCastShadows(false);
		}
	}

	// seuls le personnage et ses armes (acteurs enfants) sont filmés
	USceneCaptureComponent2D* Capture = PreviewCapture->GetCaptureComponent2D();
	Capture->ShowOnlyActors.Reset();
	Capture->ShowOnlyActors.Add(PreviewCharacter);
	TArray<AActor*> Attached;
	PreviewCharacter->GetAttachedActors(Attached, true, true);
	Capture->ShowOnlyActors.Append(Attached);

	// personnalisation choisie
	if (YSGameState && MyPlayerState)
	{
		PreviewCharacter->ApplyCustomization(YSGameState->GetCustomizationMaterial(MyPlayerState->GetColorIndex()), YSGameState->GetCustomizationFace(MyPlayerState->GetFaceIndex()));
	}
}

void AYSPlayerController::ChooseColor(int32 ColorIndex)
{
	const AYSPlayerState* MyPlayerState = GetPlayerState<AYSPlayerState>();
	ServerSetCustomization(ColorIndex, MyPlayerState ? MyPlayerState->GetFaceIndex() : 0);
}

void AYSPlayerController::ChooseFace(int32 FaceIndex)
{
	const AYSPlayerState* MyPlayerState = GetPlayerState<AYSPlayerState>();
	ServerSetCustomization(MyPlayerState ? MyPlayerState->GetColorIndex() : 0, FaceIndex);
}

void AYSPlayerController::CommitNickname(const FString& Nickname)
{
	ServerSetNickname(Nickname);
}

void AYSPlayerController::ServerSetCustomization_Implementation(int32 ColorIndex, int32 FaceIndex)
{
	if (AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>())
	{
		GameMode->PlayerSetCustomization(this, ColorIndex, FaceIndex);
	}
}

void AYSPlayerController::ServerSetNickname_Implementation(const FString& Nickname)
{
	if (AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>())
	{
		GameMode->PlayerSetNickname(this, Nickname);
	}
}

void AYSPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// en pause (choix d'upgrade), PlayerTick n'est plus appelé mais Tick si : l'UI reste vivante
	// suivi de la pause (vérification réseau : les clients doivent aussi être en pause)
	if (IsLocalController() && IsPaused() != bWasPaused)
	{
		bWasPaused = IsPaused();
		UE_LOG(LogTemp, Log, TEXT("[YSPause] %s : partie %s"), *GetName(), bWasPaused ? TEXT("en pause") : TEXT("reprise"));
	}

	if (IsLocalController())
	{
		UpdateCharacterSelectMenu();
		UpdatePlayerHUD();
		UpdateUpgradeMenu();
		UpdateDayNightUI(DeltaSeconds);
	}
}

void AYSPlayerController::UpdateDayNightUI(float DeltaSeconds)
{
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	if (!YSGameState || !EliteHealth || !DayBanner)
	{
		return;
	}

	// annonce d'un nouveau jour (pas du jour en cours à l'arrivée dans la partie)
	if (YSGameState->IsMatchStarted())
	{
		if (LastSeenDay != INDEX_NONE && YSGameState->GetDayNumber() > LastSeenDay)
		{
			DayBanner->SetDay(YSGameState->GetDayNumber());
			DayBannerRemaining = DayBanner->DisplayDuration;
		}
		LastSeenDay = YSGameState->GetDayNumber();
	}
	DayBannerRemaining = FMath::Max(0.0f, DayBannerRemaining - DeltaSeconds);
	DayBanner->SetVisibility(DayBannerRemaining > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	// élite : sa barre de vie est toujours dans le HUD (pas au-dessus de sa tête)
	const AYSFoeCharacter* Elite = YSGameState->GetCurrentElite();
	const bool bShowElite = IsValid(Elite) && Elite->GetHealth() > 0.0f;
	if (bShowElite)
	{
		EliteHealth->SetElite(Elite->GetFoeName(), Elite->GetHealth(), Elite->GetMaxHealth());
	}
	EliteHealth->SetVisibility(bShowElite ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	// objectif commun : doublons de l'équipe
	if (Objective)
	{
		Objective->SetProgress(YSGameState->GetDoubloons(), YSGameState->GetDoubloonGoal());
		Objective->SetVisibility(YSGameState->IsMatchStarted() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void AYSPlayerController::YSStartNight()
{
	ServerStartNight();
}

void AYSPlayerController::ServerStartNight_Implementation()
{
	if (AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>())
	{
		GameMode->StartNight();
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
	if (const APlayerState* MyPlayerState = GetPlayerState<APlayerState>())
	{
		PlayerHUD->SetPlayerName(FText::FromString(MyPlayerState->GetPlayerName()));
	}
	if (YSGameState)
	{
		PlayerHUD->SetLevel(YSGameState->GetTeamLevel(), YSGameState->GetTeamExperience(), YSGameState->GetExperienceToNextLevel());
	}
	if (!YSGameState)
	{
		return;
	}

	// taille de l'écran en unités Slate (icônes, indicateurs)
	int32 ViewportX = 0;
	int32 ViewportY = 0;
	GetViewportSize(ViewportX, ViewportY);
	const float ViewportScale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));

	// icône du joueur (couleur + visage choisis)
	const AYSPlayerState* MyState = GetPlayerState<AYSPlayerState>();
	auto GetIcon = [YSGameState](const AYSPlayerState* State, FLinearColor& OutColor, UTexture2D*& OutFace)
	{
		const TArray<FYSCustomizationColor>& Colors = YSGameState->GetCustomizationColors();
		OutColor = Colors.IsValidIndex(State->GetColorIndex()) ? Colors[State->GetColorIndex()].ButtonColor : FLinearColor::Gray;
		OutFace = YSGameState->GetCustomizationFace(State->GetFaceIndex());
	};
	if (MyState)
	{
		FLinearColor Color;
		UTexture2D* Face = nullptr;
		GetIcon(MyState, Color, Face);
		PlayerHUD->SetSelfIcon(Color, Face, ViewportX / ViewportScale);
	}

	// autres joueurs : liste sous les infos du joueur, et icône au bord de l'écran quand leur personnage est hors écran
	TArray<FYSHUDPlayerInfo> Allies;
	TArray<FYSOffscreenIndicator> Indicators;
	const float EdgeMargin = OffscreenIndicators ? OffscreenIndicators->IconSize * 0.5f + OffscreenIndicatorMargin : 0.0f;
	const FVector2D Screen(ViewportX / ViewportScale, ViewportY / ViewportScale);

	// icône pour un point du monde : à sa place s'il est à l'écran (si bShowOnScreen), sinon au bord dans sa direction
	auto AddIndicator = [this, &Indicators, &Screen, EdgeMargin, ViewportScale](const FVector& WorldLocation, const FLinearColor& Color, UTexture2D* Face, bool bShowOnScreen)
	{
		FVector2D ScreenPosition;
		if (!ProjectWorldLocationToScreen(WorldLocation, ScreenPosition))
		{
			return;
		}
		const FVector2D Position = ScreenPosition / ViewportScale;
		const bool bOnScreen = Position.X >= 0.0f && Position.Y >= 0.0f && Position.X <= Screen.X && Position.Y <= Screen.Y;
		if (bOnScreen && !bShowOnScreen)
		{
			return;
		}

		FYSOffscreenIndicator& Indicator = Indicators.AddDefaulted_GetRef();
		Indicator.Color = Color;
		Indicator.Face = Face;
		Indicator.Position = Position;
		if (!bOnScreen)
		{
			// hors écran : point du bord (moins la marge) sur la ligne centre de l'écran -> cible
			const FVector2D Center = Screen * 0.5f;
			const FVector2D Direction = Position - Center;
			const FVector2D Half(FMath::Max(1.0f, Center.X - EdgeMargin), FMath::Max(1.0f, Center.Y - EdgeMargin));
			const float Scale = FMath::Min(Direction.X != 0.0f ? Half.X / FMath::Abs(Direction.X) : BIG_NUMBER, Direction.Y != 0.0f ? Half.Y / FMath::Abs(Direction.Y) : BIG_NUMBER);
			Indicator.Position = Center + Direction * Scale;
		}
	};
	for (const APlayerState* OtherPlayer : YSGameState->PlayerArray)
	{
		const AYSPlayerState* OtherState = Cast<AYSPlayerState>(OtherPlayer);
		if (!OtherState || OtherState == MyState)
		{
			continue;
		}

		const AYSPlayerCharacter* OtherCharacter = Cast<AYSPlayerCharacter>(OtherState->GetPawn());
		FYSHUDPlayerInfo& Info = Allies.AddDefaulted_GetRef();
		Info.Name = FText::FromString(OtherState->GetPlayerName());
		GetIcon(OtherState, Info.Color, Info.Face);
		Info.bAlive = OtherCharacter && OtherCharacter->IsAlive();
		Info.Health = OtherCharacter ? OtherCharacter->GetHealth() : 0.0f;
		Info.MaxHealth = OtherCharacter ? OtherCharacter->GetMaxHealth() : 0.0f;

		if (Info.bAlive)
		{
			AddIndicator(OtherCharacter->GetActorLocation(), Info.Color, Info.Face, false);
		}
	}
	PlayerHUD->SetAllies(Allies);

	// objet porté : bulle du Brooker le plus proche (au-dessus de lui, ou au bord de l'écran)
	if (PlayerCharacter && PlayerCharacter->IsCarrying())
	{
		if (const AYSBrookerNPC* Brooker = AYSBrookerNPC::FindNearest(GetWorld(), PlayerCharacter->GetActorLocation()))
		{
			AddIndicator(Brooker->GetIndicatorLocation(), Brooker->GetIndicatorColor(), Brooker->GetIndicatorIcon(), true);
		}
	}

	if (OffscreenIndicators)
	{
		OffscreenIndicators->SetVisibility(bInMatch ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		OffscreenIndicators->SetIndicators(Indicators);
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
	if (bShow != UpgradeMenu->IsVisible())
	{
		UE_LOG(LogTemp, Log, TEXT("[YSUpgradeMenu] %s : menu d'upgrade %s (sélection active %d, %d proposition(s))"), *GetName(), bShow ? TEXT("affiché") : TEXT("caché"),
			YSGameState ? YSGameState->IsUpgradeSelectionActive() : 0, MyState ? MyState->GetUpgradeOffers().Num() : -1);
	}

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
		// choix de classe (upgrades de classe) ou d'amélioration
		const UYSUpgrade* FirstUpgrade = DisplayedUpgradeOffers.Num() > 0 && PlayerCharacter->GetUpgradePool().IsValidIndex(DisplayedUpgradeOffers[0].PoolIndex) ? PlayerCharacter->GetUpgradePool()[DisplayedUpgradeOffers[0].PoolIndex].Get() : nullptr;
		const bool bClassChoice = FirstUpgrade && FirstUpgrade->GetUpgradeType() == EYSUpgradeType::Class;
		UpgradeMenu->SetTitle(FText::Format(bClassChoice ? NSLOCTEXT("YS", "ClassUpgradeTitle", "Niveau {0} ! Choisis ta classe") : NSLOCTEXT("YS", "UpgradeTitle", "Niveau {0} ! Choisis une amélioration"), YSGameState->GetUpgradeSelectionLevel()));

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

	// debug / tests automatisés en réseau : -YSAutoUpgrade choisit la première proposition
	if (MyState->IsChoosingUpgrade() && FParse::Param(FCommandLine::Get(), TEXT("YSAutoUpgrade")))
	{
		ChooseUpgrade(0);
	}

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

void AYSPlayerController::YSGiveUpgrade(int32 PoolIndex, int32 Variant, int32 Rarity)
{
	ServerGiveUpgrade(PoolIndex, Variant, Rarity);
}

void AYSPlayerController::ServerGiveUpgrade_Implementation(int32 PoolIndex, int32 Variant, int32 Rarity)
{
	if (AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(GetPawn()))
	{
		FYSUpgradeOffer Offer;
		Offer.PoolIndex = PoolIndex;
		Offer.Variant = Variant;
		Offer.Rarity = static_cast<EYSUpgradeRarity>(FMath::Clamp(Rarity, 0, 3));
		PlayerCharacter->ApplyUpgrade(Offer);
	}
}

void AYSPlayerController::YSFireAtFoe(int32 Hand)
{
	const AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(GetPawn());
	const AYSWeapon* Weapon = !PlayerCharacter ? nullptr : Hand == 1 ? PlayerCharacter->GetLeftHandWeapon() : PlayerCharacter->GetRightHandWeapon();
	UYSAbilityComponent* Action = Weapon ? Weapon->GetActiveAction() : nullptr;
	if (!Action)
	{
		return;
	}

	const AActor* Nearest = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AYSFoeCharacter> It(GetWorld()); It; ++It)
	{
		const float DistanceSquared = FVector::DistSquared(It->GetActorLocation(), PlayerCharacter->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			Nearest = *It;
		}
	}
	if (Nearest)
	{
		Action->TryActivate(Nearest->GetActorLocation());
	}
}

void AYSPlayerController::YSUpgradeOdds(int32 Draws)
{
	// local : le pool et la progression sont connus du client
	const AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(GetPawn());
	if (!PlayerCharacter || Draws <= 0)
	{
		return;
	}

	TMap<FString, TArray<int32>> Counts;
	int32 RarityCounts[4] = { 0, 0, 0, 0 };
	int32 TotalOffers = 0;
	for (int32 Draw = 0; Draw < Draws; ++Draw)
	{
		const TArray<FYSUpgradeOffer> Offers = PlayerCharacter->PickUpgradeOffers(3);
		for (int32 Slot = 0; Slot < Offers.Num(); ++Slot)
		{
			const FString Name = PlayerCharacter->GetUpgradePool()[Offers[Slot].PoolIndex]->GetVariantNameFor(PlayerCharacter, Offers[Slot].Variant).ToString();
			TArray<int32>& SlotCounts = Counts.FindOrAdd(Name);
			SlotCounts.SetNumZeroed(3);
			++SlotCounts[Slot];
			++RarityCounts[static_cast<int32>(Offers[Slot].Rarity)];
			++TotalOffers;
		}
	}

	for (const TPair<FString, TArray<int32>>& Entry : Counts)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSUpgradeOdds] %-22s proposition 1 : %5.1f %%  2 : %5.1f %%  3 : %5.1f %%"), *Entry.Key,
			100.0f * Entry.Value[0] / Draws, 100.0f * Entry.Value[1] / Draws, 100.0f * Entry.Value[2] / Draws);
	}
	UE_LOG(LogTemp, Log, TEXT("[YSUpgradeOdds] Raretés (toutes propositions) : Bronze %.1f %%  Silver %.1f %%  Gold %.1f %%  (chances : %.0f / %.0f / %.0f, Luck %.0f)"),
		100.0f * RarityCounts[1] / FMath::Max(1, TotalOffers), 100.0f * RarityCounts[2] / FMath::Max(1, TotalOffers), 100.0f * RarityCounts[3] / FMath::Max(1, TotalOffers),
		PlayerCharacter->GetRarityChance(EYSUpgradeRarity::Bronze), PlayerCharacter->GetRarityChance(EYSUpgradeRarity::Silver), PlayerCharacter->GetRarityChance(EYSUpgradeRarity::Gold), PlayerCharacter->GetLuck());
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
		const AYSWeapon* MainWeapon = PlayerCharacter ? PlayerCharacter->GetMainHandWeapon() : nullptr;
		const AYSWeapon* OffWeapon = PlayerCharacter ? PlayerCharacter->GetOffHandWeapon() : nullptr;
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

	// regard figé par une action (Dashing Slash) : la souris n'est plus suivie jusqu'à la fin
	FVector LockedDirection;
	const AYSPlayerCharacter* YSCharacter = Cast<AYSPlayerCharacter>(ControlledPawn);
	if (YSCharacter && YSCharacter->GetLockedAimDirection(LockedDirection))
	{
		SetControlRotation(FRotator(0.0f, LockedDirection.Rotation().Yaw, 0.0f));
		return;
	}

	const FVector ToAim = AimLocation - PawnLocation;
	if (ToAim.SizeSquared2D() > 1.0f)
	{
		SetControlRotation(FRotator(0.0f, ToAim.Rotation().Yaw, 0.0f));
	}
}

void AYSPlayerController::YSSetColor(int32 ColorIndex)
{
	ChooseColor(ColorIndex);
}

void AYSPlayerController::YSSetFace(int32 FaceIndex)
{
	ChooseFace(FaceIndex);
}

void AYSPlayerController::YSSetName(const FString& Nickname)
{
	CommitNickname(Nickname);
}

void AYSPlayerController::ToggleReady()
{
	const AYSPlayerState* MyPlayerState = GetPlayerState<AYSPlayerState>();
	ServerSetReady(!(MyPlayerState && MyPlayerState->IsReady()));
}

void AYSPlayerController::YSReady()
{
	ServerSetReady(true);
}

void AYSPlayerController::ServerSetReady_Implementation(bool bReady)
{
	if (AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>())
	{
		GameMode->PlayerSetReady(this, bReady);
	}
}

void AYSPlayerController::YSFire()
{
	if (AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->TriggerFire();
	}
}

void AYSPlayerController::ChooseHandedness(int32 Handedness)
{
	ServerSetLeftHanded(Handedness == 1);
}

void AYSPlayerController::YSSetLeftHanded(int32 bLeftHanded)
{
	ChooseHandedness(bLeftHanded != 0 ? 1 : 0);
}

void AYSPlayerController::ServerSetLeftHanded_Implementation(bool bLeftHanded)
{
	if (AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>())
	{
		GameMode->PlayerSetLeftHanded(this, bLeftHanded);
	}
}

void AYSPlayerController::YSEquipWeapon(int32 Hand, const FString& WeaponName)
{
	AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(GetPawn());
	// préfixe BP_ facultatif (YSAfter remplace les _ par des espaces)
	const FString AssetName = WeaponName.StartsWith(TEXT("BP_")) ? WeaponName : TEXT("BP_") + WeaponName;
	UClass* WeaponClass = LoadClass<AYSWeapon>(nullptr, *FString::Printf(TEXT("/Game/Placeable/Weapons/%s.%s_C"), *AssetName, *AssetName));
	if (!PlayerCharacter || !WeaponClass || !HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSEquipWeapon] impossible (perso %d, arme %s trouvée %d, hôte %d)"), PlayerCharacter != nullptr, *WeaponName, WeaponClass != nullptr, HasAuthority());
		return;
	}
	PlayerCharacter->DebugEquipWeapon(Hand == 1 ? EYSHand::Left : EYSHand::Right, WeaponClass);
	UE_LOG(LogTemp, Log, TEXT("[YSEquipWeapon] %s dans la main %s"), *WeaponName, Hand == 1 ? TEXT("gauche") : TEXT("droite"));
}

void AYSPlayerController::YSInteract()
{
	if (AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->TriggerInteract();
	}
}

void AYSPlayerController::YSSpawnChest(float Distance)
{
	const APawn* MyPawn = GetPawn();
	UClass* ChestClass = LoadClass<AYSInteractiveObject>(nullptr, TEXT("/Game/Placeable/InteractiveObjects/BP_InteractiveObject_Chest.BP_InteractiveObject_Chest_C"));
	if (!MyPawn || !ChestClass || !HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSSpawnChest] impossible (perso %d, coffre trouvé %d, hôte %d)"), MyPawn != nullptr, ChestClass != nullptr, HasAuthority());
		return;
	}
	const AYSInteractiveObject* Chest = AYSInteractiveObject::SpawnOnGround(GetWorld(), ChestClass, MyPawn->GetActorLocation() + MyPawn->GetActorForwardVector() * Distance, MyPawn->GetActorRotation().Yaw);
	UE_LOG(LogTemp, Log, TEXT("[YSSpawnChest] %s"), Chest ? *Chest->GetName() : TEXT("échec"));
}

void AYSPlayerController::YSGoToBrooker(float Distance)
{
	APawn* MyPawn = GetPawn();
	const AYSBrookerNPC* Brooker = MyPawn ? AYSBrookerNPC::FindNearest(GetWorld(), MyPawn->GetActorLocation()) : nullptr;
	if (!Brooker || !HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSGoToBrooker] impossible (Brooker trouvé %d, hôte %d)"), Brooker != nullptr, HasAuthority());
		return;
	}
	// à Distance du Brooker, du côté du personnage, tourné vers lui
	const FVector FromBrooker = (MyPawn->GetActorLocation() - Brooker->GetActorLocation()).GetSafeNormal2D();
	const FVector Location = Brooker->GetActorLocation() + (FromBrooker.IsNearlyZero() ? FVector::ForwardVector : FromBrooker) * Distance + FVector(0.0f, 0.0f, 100.0f);
	MyPawn->TeleportTo(Location, (-FromBrooker).Rotation());
	UE_LOG(LogTemp, Log, TEXT("[YSGoToBrooker] %s à %.0f cm de %s"), *MyPawn->GetName(), Distance, *Brooker->GetName());
}
