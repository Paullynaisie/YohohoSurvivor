#include "Player/YSPlayerController.h"
#include "Abilities/YSAbilityComponent.h"
#include "Abilities/YSBombThrowerAbility.h"
#include "Player/YSPlayerCharacter.h"
#include "Weapons/YSWeapon.h"
#include "Weapons/YSWeaponSet.h"
#include "Upgrades/YSPassive.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Enemies/YSFoeCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Containers/Ticker.h"
#include "GameFramework/Pawn.h"
#include "UI/YSAbilityIconWidget.h"
#include "UI/YSBloodlustStacksWidget.h"
#include "UI/YSCharacterSelectWidget.h"
#include "UI/YSUpgradeMenuWidget.h"
#include "UI/YSPlayerHUDWidget.h"
#include "UI/YSOffscreenIndicatorsWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "UI/YSEliteHealthWidget.h"
#include "UI/YSObjectiveWidget.h"
#include "UI/YSShipBoardingWidget.h"
#include "Interaction/YSInteractiveObject.h"
#include "Interaction/YSTreasureMap.h"
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
#include "AssetRegistry/IAssetRegistry.h"
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

		// BL stacks : bas de l'écran, à droite de l'icône (placement réglé dans le Widget Blueprint)
		if (ViewportSubsystem)
		{
			BloodlustStacks = CreateWidget<UYSBloodlustStacksWidget>(this, BloodlustWidgetClass ? BloodlustWidgetClass.Get() : UYSBloodlustStacksWidget::StaticClass());
			FGameViewportWidgetSlot BloodlustSlot;
			BloodlustSlot.Anchors = FAnchors(0.5f, 1.0f);
			BloodlustSlot.Alignment = FVector2D(0.0f, 1.0f);
			BloodlustSlot.Offsets = FMargin(BloodlustStacks->ScreenOffset.X, -BloodlustStacks->ScreenOffset.Y, 0.0f, 0.0f);
			ViewportSubsystem->AddWidgetForPlayer(BloodlustStacks, GetLocalPlayer(), BloodlustSlot);
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
			CharacterSelectMenu->OnInviteClicked.BindUObject(this, &AYSPlayerController::YSInviteFriends);

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

			// fin de jeu temporaire : plein écran (texte en bas, « BRAVO » au centre ; placements dans le Widget Blueprint)
			ShipBoarding = CreateWidget<UYSShipBoardingWidget>(this, ShipBoardingWidgetClass ? ShipBoardingWidgetClass.Get() : UYSShipBoardingWidget::StaticClass());
			ShipBoarding->SetVisibility(ESlateVisibility::HitTestInvisible);
			FGameViewportWidgetSlot BoardingSlot;
			BoardingSlot.Anchors = FAnchors(0.0f, 0.0f, 1.0f, 1.0f);
			BoardingSlot.ZOrder = 20;
			ViewportSubsystem->AddWidgetForPlayer(ShipBoarding, GetLocalPlayer(), BoardingSlot);

			// annonce « Jour X » : plein écran, texte centré
			DayBanner = CreateWidget<UYSDayBannerWidget>(this, UYSDayBannerWidget::StaticClass());
			DayBanner->SetVisibility(ESlateVisibility::Collapsed);
			FGameViewportWidgetSlot BannerSlot;
			BannerSlot.Anchors = FAnchors(0.0f, 0.0f, 1.0f, 1.0f);
			BannerSlot.ZOrder = 5;
			ViewportSubsystem->AddWidgetForPlayer(DayBanner, GetLocalPlayer(), BannerSlot);

			// choix d'upgrade : plein écran, une grande carte par proposition, « Relance » en bas (placements dans le Widget Blueprint)
			UpgradeMenu = CreateWidget<UYSUpgradeMenuWidget>(this, UpgradeMenuClass ? UpgradeMenuClass.Get() : UYSUpgradeMenuWidget::StaticClass());
			UpgradeMenu->OnOfferChosen.BindUObject(this, &AYSPlayerController::ChooseUpgrade);
			UpgradeMenu->OnRerollClicked.BindUObject(this, &AYSPlayerController::RerollUpgrades);
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
	InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AYSPlayerController::YSCheatLevelUp);
#endif

	// pavé numérique : apparition manuelle de NME (seulement si le GameMode de la map l'autorise, ex. TestMap)
	const FKey NumpadKeys[] = { EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour,
		EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };
	// Ctrl gauche + pavé numérique : circle spawn de ce NME sur le contour de la plage
	for (int32 Key = 0; Key < UE_ARRAY_COUNT(NumpadKeys); ++Key)
	{
		FInputKeyBinding Binding(FInputChord(NumpadKeys[Key]), IE_Pressed);
		Binding.KeyDelegate.GetDelegateForManualSet().BindWeakLambda(this, [this, Key]()
		{
			if (!IsInputKeyDown(EKeys::LeftControl))
			{
				YSNumpadSpawn(Key);
			}
		});
		InputComponent->KeyBindings.Add(Binding);

		FInputKeyBinding CtrlBinding(FInputChord(NumpadKeys[Key], false, true, false, false), IE_Pressed);
		CtrlBinding.KeyDelegate.GetDelegateForManualSet().BindWeakLambda(this, [this, Key]()
		{
			if (IsInputKeyDown(EKeys::LeftControl))
			{
				YSNumpadCircle(Key);
			}
		});
		InputComponent->KeyBindings.Add(CtrlBinding);
	}
}

void AYSPlayerController::YSNumpadSpawn(int32 Key)
{
	ServerNumpadSpawn(Key);
}

void AYSPlayerController::YSNumpadCircle(int32 Key)
{
	ServerNumpadCircle(Key);
}

void AYSPlayerController::ServerNumpadCircle_Implementation(int32 Key)
{
	if (AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>())
	{
		GameMode->SpawnNumpadCircle(Key);
	}
}

void AYSPlayerController::ServerNumpadSpawn_Implementation(int32 Key)
{
	if (AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>())
	{
		GameMode->SpawnNumpadFoe(this, Key);
	}
}

void AYSPlayerController::YSSetBloodlustStacks(int32 Stacks)
{
	ServerSetBloodlustStacks(Stacks);
}

void AYSPlayerController::ServerSetBloodlustStacks_Implementation(int32 Stacks)
{
	if (AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->DebugSetBloodlustStacks(Stacks);
	}
}

void AYSPlayerController::YSSetBloodRageStacks(int32 Stacks)
{
	ServerSetBloodRageStacks(Stacks);
}

void AYSPlayerController::ServerSetBloodRageStacks_Implementation(int32 Stacks)
{
	if (AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->DebugSetBloodRageStacks(Stacks);
	}
}

void AYSPlayerController::YSCheatLevelUp()
{
	ServerCheatLevelUp();
}

void AYSPlayerController::ServerCheatLevelUp_Implementation()
{
	// juste l'XP qui manque pour le niveau suivant : un choix d'upgrade (ou de classe) comme un vrai passage de niveau
	AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	const AYSGameState* State = GetWorld()->GetGameState<AYSGameState>();
	if (!GameMode || !State || !State->IsMatchStarted())
	{
		return;
	}
	const float Missing = FMath::Max(State->GetExperienceToNextLevel() - State->GetTeamExperience(), 0.01f);
	UE_LOG(LogTemp, Log, TEXT("[YSCheat] %s : +1 niveau (%.1f XP)"), *GetName(), Missing);
	GameMode->AddTeamExperience(Missing);
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

	// Atk speed = upgrade de recharge de l'Auto atk (DA_Upgrade_AtkSpeed) ; 10 bonus Silver, sans dépasser le maximum de l'upgrade
	const TArray<TObjectPtr<UYSUpgrade>>& Pool = PlayerCharacter->GetUpgradePool();
	const int32 PoolIndex = Pool.IndexOfByPredicate([](const TObjectPtr<UYSUpgrade>& Upgrade)
	{
		const UYSHandCooldownReductionUpgrade* Cooldown = Cast<UYSHandCooldownReductionUpgrade>(Upgrade.Get());
		return Cooldown && Cooldown->bAutoAtkVariant;
	});
	const UYSHandCooldownReductionUpgrade* AtkSpeed = Pool.IsValidIndex(PoolIndex) ? Cast<UYSHandCooldownReductionUpgrade>(Pool[PoolIndex].Get()) : nullptr;
	const int32 Variant = UYSHandStatUpgrade::MakeVariant(EYSActionSlot::AutoAtk);

	// upgrade requise (Atk critique) donnée d'abord si le joueur ne l'a pas
	const int32 RequiredIndex = AtkSpeed && AtkSpeed->RequiredUpgrade ? Pool.IndexOfByKey(AtkSpeed->RequiredUpgrade) : INDEX_NONE;
	if (RequiredIndex != INDEX_NONE && !PlayerCharacter->HasUpgrade(AtkSpeed->RequiredUpgrade)
		&& Pool[RequiredIndex]->CanBeOffered(PlayerCharacter, 0, PlayerCharacter->GetUpgradeProgress(RequiredIndex, 0)))
	{
		FYSUpgradeOffer Required;
		Required.PoolIndex = RequiredIndex;
		Required.Rarity = EYSUpgradeRarity::Silver;
		PlayerCharacter->ApplyUpgrade(Required);
		UE_LOG(LogTemp, Log, TEXT("[YSCheat] Atk speed : %s donnée d'abord (requise)"), *Pool[RequiredIndex]->GetName());
	}
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

void AYSPlayerController::YSInviteFriends()
{
	UYSGameInstance* GameInstance = GetGameInstance<UYSGameInstance>();
	const bool bShown = GameInstance && GameInstance->ShowInviteFriendsUI();
	UE_LOG(LogTemp, Log, TEXT("[YSSteam] Fenêtre d'invitation : %s"), bShown ? TEXT("ouverte") : TEXT("indisponible (pas de partie Steam)"));
}

void AYSPlayerController::YSSelectCharacter(int32 CharacterIndex)
{
	// personnage unique déjà pris par un autre joueur : le serveur refuserait
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	if (YSGameState && YSGameState->IsCharacterTaken(CharacterIndex, PlayerState))
	{
		return;
	}

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

	// fiche du personnage choisi, lue sur l'aperçu (même classe, mêmes armes)
	if (PreviewCharacter)
	{
		FYSCharacterInfo Info;
		Info.Name = PreviewCharacter->GetCharacterName();
		Info.MaxHealth = PreviewCharacter->GetMaxHealth();
		Info.HealthRegenPerSecond = PreviewCharacter->GetBaseHealthRegenPerSecond();
		Info.Size = PreviewCharacter->GetCharacterSize();
		Info.WeaponSetName = PreviewCharacter->GetWeaponSet() ? PreviewCharacter->GetWeaponSet()->DisplayName : FText::GetEmpty();
		// noms des actions des deux armes, sans doublon (akimbo) : « Auto swing », « Auto shot & Auto swing »
		TArray<FString> AutoAtkNames;
		TArray<FString> AbilityNames;
		for (const AYSWeapon* Weapon : { PreviewCharacter->GetMainHandWeapon(), PreviewCharacter->GetOffHandWeapon() })
		{
			const UYSAbilityComponent* AutoAtk = Weapon ? Weapon->GetAutoAction() : nullptr;
			const UYSAbilityComponent* Ability = Weapon ? Weapon->GetActiveAction() : nullptr;
			if (AutoAtk && !AutoAtk->DisplayName.IsEmpty())
			{
				AutoAtkNames.AddUnique(AutoAtk->DisplayName.ToString());
			}
			if (Ability && !Ability->DisplayName.IsEmpty())
			{
				AbilityNames.AddUnique(Ability->DisplayName.ToString());
			}
		}
		Info.AutoAtkName = FText::FromString(FString::Join(AutoAtkNames, TEXT(" & ")));
		Info.AbilityName = FText::FromString(FString::Join(AbilityNames, TEXT(" & ")));
		if (const UYSPassive* Passive = PreviewCharacter->GetPassive())
		{
			Info.PassiveName = Passive->DisplayName;
			Info.PassiveDescription = Passive->Description;
		}
		CharacterSelectMenu->SetCharacterInfo(Info);
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
		// partie Steam : bouton d'invitation d'amis (overlay Steam)
		const UYSGameInstance* GameInstance = GetGameInstance<UYSGameInstance>();
		CharacterSelectMenu->SetInviteAvailable(GameInstance && GameInstance->HasOnlineSession());

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

	// personnages uniques déjà choisis par un autre joueur : grisés
	TArray<bool> TakenCharacters;
	for (int32 Index = 0; Index < YSGameState->GetSelectableCharacters().Num(); ++Index)
	{
		TakenCharacters.Add(YSGameState->IsCharacterTaken(Index, MyPlayerState));
	}
	CharacterSelectMenu->SetTakenCharacters(TakenCharacters);

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
		UpdateShipBoardingUI(DeltaSeconds);
	}
}

void AYSPlayerController::UpdateShipBoardingUI(float DeltaSeconds)
{
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	if (!YSGameState || !ShipBoarding)
	{
		return;
	}
	const bool bVictory = YSGameState->IsVictory();
	ShipBoarding->SetBoarding(YSGameState->IsShipPresent() && !bVictory ? YSGameState->GetShipAboardPlayers() : 0, YSGameState->GetShipTotalPlayers());
	ShipBoarding->SetVictory(bVictory);

	// « BRAVO » puis retour au menu ; l'hôte part un peu après ses clients (sinon ils voient une erreur réseau)
	if (bVictory && VictoryReturnTimer == -1.0f)
	{
		VictoryReturnTimer = YSGameState->GetVictoryReturnDelay() + (HasAuthority() && GetNetMode() != NM_Standalone ? 0.5f : 0.0f);
		UE_LOG(LogTemp, Log, TEXT("[YSShip] %s : BRAVO, retour au menu dans %.1f s"), *GetName(), VictoryReturnTimer);
	}
	if (VictoryReturnTimer >= 0.0f)
	{
		VictoryReturnTimer -= DeltaSeconds;
		if (VictoryReturnTimer < 0.0f)
		{
			VictoryReturnTimer = -2.0f;
			YSLeaveGame();
		}
	}
}

void AYSPlayerController::YSShipArrive()
{
	ServerLoopDebug(0);
}

void AYSPlayerController::YSStartOvertime()
{
	ServerLoopDebug(1);
}

void AYSPlayerController::YSSinkIsland()
{
	ServerLoopDebug(2);
}

void AYSPlayerController::YSTeleport(float X, float Y, float Z)
{
	ServerTeleport(FVector(X, Y, Z));
}

void AYSPlayerController::ServerTeleport_Implementation(FVector Location)
{
	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->TeleportTo(Location, ControlledPawn->GetActorRotation());
		UE_LOG(LogTemp, Log, TEXT("[YSDebug] %s téléporté en %s"), *ControlledPawn->GetName(), *Location.ToCompactString());
	}
}

void AYSPlayerController::ServerLoopDebug_Implementation(int32 Action)
{
	AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	if (!GameMode)
	{
		return;
	}
	switch (Action)
	{
	case 0:		GameMode->ShipArrives(TEXT("debug"));	break;
	case 1:		GameMode->StartOvertime();				break;
	default:	GameMode->StartIslandSink();			break;
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
	if (const AYSPlayerState* MyPlayerState = GetPlayerState<AYSPlayerState>())
	{
		PlayerHUD->SetPlayerName(FText::FromString(MyPlayerState->GetDisplayName()));
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
		Info.Name = FText::FromString(OtherState->GetDisplayName());
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

	// objet porté vendable : bulle du Brooker le plus proche (au-dessus de lui, ou au bord de l'écran)
	if (PlayerCharacter && PlayerCharacter->IsCarrying() && PlayerCharacter->GetCarriedObject()->GetDoubloonValue() > 0)
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

	// nouvelles propositions (ou progression changée, pour des propositions identiques enchaînées) : reconstruire les cartes
	const int32 TotalStacks = PlayerCharacter->GetTotalUpgradeStacks();
	const UYSUpgrade* FirstUpgrade = MyState->GetUpgradeOffers().Num() > 0 && PlayerCharacter->GetUpgradePool().IsValidIndex(MyState->GetUpgradeOffers()[0].PoolIndex) ? PlayerCharacter->GetUpgradePool()[MyState->GetUpgradeOffers()[0].PoolIndex].Get() : nullptr;
	const bool bClassChoice = FirstUpgrade && FirstUpgrade->GetUpgradeType() == EYSUpgradeType::Class;

	if (DisplayedUpgradeOffers != MyState->GetUpgradeOffers() || (DisplayedUpgradeStacks != TotalStacks && MyState->IsChoosingUpgrade()))
	{
		DisplayedUpgradeOffers = MyState->GetUpgradeOffers();
		DisplayedUpgradeStacks = TotalStacks;
		// choix de classe (upgrades de classe) ou d'amélioration
		UpgradeMenu->SetTitle(FText::Format(bClassChoice ? NSLOCTEXT("YS", "ClassUpgradeTitle", "Niveau {0} ! Choisis ta classe") : NSLOCTEXT("YS", "UpgradeTitle", "Niveau {0} ! Choisis une amélioration"), YSGameState->GetUpgradeSelectionLevel()));

		TArray<FYSUpgradeCard> Cards;
		for (const FYSUpgradeOffer& Offer : DisplayedUpgradeOffers)
		{
			FYSUpgradeCard& Card = Cards.AddDefaulted_GetRef();
			const UYSUpgrade* Upgrade = PlayerCharacter->GetUpgradePool().IsValidIndex(Offer.PoolIndex) ? PlayerCharacter->GetUpgradePool()[Offer.PoolIndex].Get() : nullptr;
			if (Upgrade)
			{
				Upgrade->GetOfferCard(PlayerCharacter, Offer.Variant, PlayerCharacter->GetUpgradeProgress(Offer.PoolIndex, Offer.Variant), Offer.Rarity, Card);
			}
			else
			{
				Card.Name = FText::FromString(TEXT("?"));
			}
		}
		UpgradeMenu->SetCards(Cards);
	}

	UpgradeMenu->SetSelectedIndex(MyState->GetChosenUpgradeOffer());
	// relance : seulement tant que le joueur n'a pas choisi, jamais pour un choix de classe
	const AYSGameMode* AuthGameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	UpgradeMenu->SetRerollVisible(MyState->IsChoosingUpgrade() && !bClassChoice && (!AuthGameMode || AuthGameMode->IsUpgradeRerollAllowed()));

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

void AYSPlayerController::RerollUpgrades()
{
	const AYSPlayerState* MyState = GetPlayerState<AYSPlayerState>();
	if (MyState && MyState->IsChoosingUpgrade())
	{
		ServerRerollUpgrades();
	}
}

void AYSPlayerController::ServerRerollUpgrades_Implementation()
{
	if (AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>())
	{
		GameMode->PlayerRerolledUpgrades(this);
	}
}

void AYSPlayerController::YSRerollUpgrades()
{
	RerollUpgrades();
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
	// « _ » = espace, « __ » = vrai « _ » (noms d'assets)
	const FString FullCommand = Command.Replace(TEXT("__"), TEXT("\x01")).Replace(TEXT("_"), TEXT(" ")).Replace(TEXT("\x01"), TEXT("_"));
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

void AYSPlayerController::YSFireAt(int32 Hand, float X, float Y)
{
	const AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(GetPawn());
	const AYSWeapon* Weapon = !PlayerCharacter ? nullptr : Hand == 1 ? PlayerCharacter->GetLeftHandWeapon() : PlayerCharacter->GetRightHandWeapon();
	if (UYSAbilityComponent* Action = Weapon ? Weapon->GetActiveAction() : nullptr)
	{
		Action->TryActivate(FVector(X, Y, PlayerCharacter->GetActorLocation().Z));
	}
}

void AYSPlayerController::YSBombLanding(float X, float Y)
{
	const AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(GetPawn());
	for (const AYSWeapon* Weapon : PlayerCharacter ? PlayerCharacter->GetHeldWeapons() : TArray<AYSWeapon*>())
	{
		if (const UYSBombThrowerAbility* Thrower = Weapon ? Weapon->FindComponentByClass<UYSBombThrowerAbility>() : nullptr)
		{
			const FVector Landing = Thrower->FindLandingPoint(FVector(X, Y, PlayerCharacter->GetActorLocation().Z));
			UE_LOG(LogTemp, Log, TEXT("[YSBombThrower] curseur (%.0f, %.0f), joueur (%.0f, %.0f) -> impact (%.0f, %.0f, %.0f), %.0f cm du joueur"), X, Y,
				PlayerCharacter->GetActorLocation().X, PlayerCharacter->GetActorLocation().Y, Landing.X, Landing.Y, Landing.Z, FVector::Dist2D(Landing, PlayerCharacter->GetActorLocation()));
		}
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
			SlotCounts.SetNumZeroed(6);
			++SlotCounts[Slot];
			++SlotCounts[2 + static_cast<int32>(Offers[Slot].Rarity)];
			++RarityCounts[static_cast<int32>(Offers[Slot].Rarity)];
			++TotalOffers;
		}
	}

	for (const TPair<FString, TArray<int32>>& Entry : Counts)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSUpgradeOdds] %-22s proposition 1 : %5.1f %%  2 : %5.1f %%  3 : %5.1f %%  (Bronze %d / Silver %d / Gold %d)"), *Entry.Key,
			100.0f * Entry.Value[0] / Draws, 100.0f * Entry.Value[1] / Draws, 100.0f * Entry.Value[2] / Draws, Entry.Value[3], Entry.Value[4], Entry.Value[5]);
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

	if (BloodlustStacks)
	{
		// même affichage pour la BloodRage (GreatAxe) quand elle est débloquée : pastilles de rage, délai, couleur critique au maximum
		const AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(GetPawn());
		if (PlayerCharacter && PlayerCharacter->GetBloodRageMaxStacks() > 0)
		{
			BloodlustStacks->Refresh(PlayerCharacter->GetBloodRageStacks(), PlayerCharacter->GetBloodRageMaxStacks(), PlayerCharacter->GetBloodRageTimeRatio(), PlayerCharacter->IsBloodRageReady());
		}
		else
		{
			BloodlustStacks->Refresh(PlayerCharacter ? PlayerCharacter->GetBloodlustStacks() : 0, PlayerCharacter ? PlayerCharacter->GetBloodlustMaxStacks() : 0,
				PlayerCharacter ? PlayerCharacter->GetBloodlustTimeRatio() : 0.0f, PlayerCharacter && PlayerCharacter->IsBloodlustCritical());
		}
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

namespace YSPlayerControllerDebug
{
	// classe d'un Blueprint nommé Name rangé n'importe où sous Root (sous-dossiers compris), sinon nullptr
	UClass* FindBlueprintClass(const FString& Root, const FString& Name, UClass* BaseClass)
	{
		FARFilter Filter;
		Filter.PackagePaths.Add(FName(*Root));
		Filter.bRecursivePaths = true;
		TArray<FAssetData> Assets;
		IAssetRegistry::GetChecked().GetAssets(Filter, Assets);
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetName.ToString() == Name)
			{
				return StaticLoadClass(BaseClass, nullptr, *FString::Printf(TEXT("%s.%s_C"), *Asset.PackageName.ToString(), *Name));
			}
		}
		return nullptr;
	}
}

void AYSPlayerController::YSEquipWeapon(int32 Hand, const FString& WeaponName)
{
	AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(GetPawn());
	// préfixe BP_ facultatif (YSAfter remplace les _ par des espaces)
	const FString AssetName = WeaponName.StartsWith(TEXT("BP_")) ? WeaponName : TEXT("BP_") + WeaponName;
	UClass* WeaponClass = YSPlayerControllerDebug::FindBlueprintClass(TEXT("/Game/Placeable/Weapons"), AssetName, AYSWeapon::StaticClass());
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

void AYSPlayerController::YSSpawnTreasureMap(float Distance)
{
	const APawn* MyPawn = GetPawn();
	AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	if (!MyPawn || !GameMode)
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSSpawnTreasureMap] impossible (perso %d, hôte %d)"), MyPawn != nullptr, GameMode != nullptr);
		return;
	}
	const AYSTreasureMap* TreasureMap = Distance > 0.0f
		? GameMode->SpawnTreasureMapAt(AYSInteractiveObject::FindGround(GetWorld(), MyPawn->GetActorLocation() + MyPawn->GetActorForwardVector() * Distance, MyPawn))
		: GameMode->SpawnTreasureMap();
	UE_LOG(LogTemp, Log, TEXT("[YSSpawnTreasureMap] %s"), TreasureMap ? *TreasureMap->GetName() : TEXT("échec (Treasure Map Class vide, ou pas de chemin complet depuis ce point)"));
}

void AYSPlayerController::YSSpawnFoe(const FString& Name, float Distance)
{
	const APawn* MyPawn = GetPawn();
	const FString Path = TEXT("/Game/Placeable/Character/.../") + Name;
	UClass* FoeClass = YSPlayerControllerDebug::FindBlueprintClass(TEXT("/Game/Placeable/Character"), Name, AYSFoeCharacter::StaticClass());
	if (!MyPawn || !FoeClass || !HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSSpawnFoe] impossible (perso %d, classe %s trouvée %d, hôte %d)"), MyPawn != nullptr, *Path, FoeClass != nullptr, HasAuthority());
		return;
	}

	// au sol devant le personnage, tourné vers lui
	AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	const AActor* Foe = GameMode ? GameMode->SpawnFoeInFront(FoeClass, MyPawn, Distance) : nullptr;
	UE_LOG(LogTemp, Log, TEXT("[YSSpawnFoe] %s"), Foe ? *Foe->GetName() : TEXT("échec"));
}

void AYSPlayerController::YSTreasureNextPoint()
{
	AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(GetPawn());
	const AYSTreasureMap* TreasureMap = PlayerCharacter ? Cast<AYSTreasureMap>(PlayerCharacter->GetCarriedObject()) : nullptr;
	if (!TreasureMap || !HasAuthority() || TreasureMap->GetReachedPointIndex() + 1 >= TreasureMap->GetPathPoints().Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSTreasureNextPoint] impossible (carte portée %d, hôte %d)"), TreasureMap != nullptr, HasAuthority());
		return;
	}
	const FVector Point = TreasureMap->GetPathPoints()[TreasureMap->GetReachedPointIndex() + 1];
	PlayerCharacter->TeleportTo(Point + FVector(0.0f, 0.0f, 100.0f), PlayerCharacter->GetActorRotation());
	UE_LOG(LogTemp, Log, TEXT("[YSTreasureNextPoint] %s -> point %d (%.0f, %.0f)"), *PlayerCharacter->GetName(), TreasureMap->GetReachedPointIndex() + 1, Point.X, Point.Y);
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
