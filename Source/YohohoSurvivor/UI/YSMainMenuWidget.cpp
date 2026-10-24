#include "UI/YSMainMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/YSGameInstance.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Kismet/KismetSystemLibrary.h"

namespace YSMainMenu
{
	void SetFontSize(UTextBlock* Text, int32 Size)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}

	const FLinearColor StatusColor(0.8f, 0.8f, 0.8f);
	const FLinearColor ErrorColor(1.0f, 0.35f, 0.3f);
	const FLinearColor HeaderColor(1.0f, 0.85f, 0.5f);
}

// noms qualifiés : la unity build du packaging fusionne ce fichier avec les autres widgets (SetFontSize existe aussi ailleurs)
using YSMainMenu::StatusColor;
using YSMainMenu::ErrorColor;

bool UYSMainMenuWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// pas de Widget Blueprint : l'arbre est construit dès la création
	if (bSuperInitialized && WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}

	return bSuperInitialized;
}

void UYSMainMenuWidget::BuildWidgetTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Background"));
	Background->SetColorAndOpacity(BackgroundColor);
	UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Background);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Page"));
	UOverlaySlot* PageSlot = Root->AddChildToOverlay(Page);
	PageSlot->SetHorizontalAlignment(HAlign_Center);
	PageSlot->SetVerticalAlignment(VAlign_Center);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	Title->SetText(NSLOCTEXT("YS", "MainMenuTitle", "Yohoho Survivor"));
	YSMainMenu::SetFontSize(Title, 48);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UVerticalBoxSlot* TitleSlot = Page->AddChildToVerticalBox(Title);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 32.0f));

	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Columns"));
	UVerticalBoxSlot* ColumnsSlot = Page->AddChildToVerticalBox(Columns);
	ColumnsSlot->SetHorizontalAlignment(HAlign_Center);

	auto AddColumn = [this, Columns](FName Name, float RightPadding)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(ColumnWidth);
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), Name);
		Size->AddChild(Column);
		UHorizontalBoxSlot* ColumnSlot = Columns->AddChildToHorizontalBox(Size);
		ColumnSlot->SetVerticalAlignment(VAlign_Top);
		ColumnSlot->SetPadding(FMargin(0.0f, 0.0f, RightPadding, 0.0f));
		return Column;
	};

	// ---- gauche : Steam ----
	UVerticalBox* SteamColumn = AddColumn(TEXT("SteamColumn"), ColumnSpacing);
	UTextBlock* SteamHeader = AddText(SteamColumn, NSLOCTEXT("YS", "SteamHeader", "Steam (amis, sans IP)"), 26, FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	SteamHeader->SetColorAndOpacity(FSlateColor(YSMainMenu::HeaderColor));
	SteamStateText = AddText(SteamColumn, FText::GetEmpty(), 14, FMargin(0.0f, 0.0f, 0.0f, 8.0f));

	SteamHostButton = AddButton(SteamColumn, NSLOCTEXT("YS", "SteamHostButton", "Héberger (Steam)"), HostButtonColor, TEXT("SteamHostButton"));
	SteamHostButton->OnClicked.AddDynamic(this, &UYSMainMenuWidget::HandleSteamHostClicked);
	AddText(SteamColumn, NSLOCTEXT("YS", "SteamInviteHint", "Puis invite tes amis : bouton « Inviter des amis » ou Maj+Tab > Amis > Inviter à jouer."), 13, FMargin(0.0f, 0.0f, 0.0f, 16.0f));

	SteamSearchButton = AddButton(SteamColumn, NSLOCTEXT("YS", "SteamSearchButton", "Chercher les parties"), SteamButtonColor, TEXT("SteamSearchButton"));
	SteamSearchButton->OnClicked.AddDynamic(this, &UYSMainMenuWidget::HandleSteamSearchClicked);

	GameList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GameList"));
	SteamColumn->AddChildToVerticalBox(GameList)->SetHorizontalAlignment(HAlign_Fill);

	// ---- droite : IP ----
	UVerticalBox* IPColumn = AddColumn(TEXT("IPColumn"), 0.0f);
	UTextBlock* IPHeader = AddText(IPColumn, NSLOCTEXT("YS", "IPHeader", "Par IP (réseau local / VPN)"), 26, FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	IPHeader->SetColorAndOpacity(FSlateColor(YSMainMenu::HeaderColor));
	AddText(IPColumn, NSLOCTEXT("YS", "IPHint", "Port UDP 7777, même build pour tout le monde."), 14, FMargin(0.0f, 0.0f, 0.0f, 8.0f));

	UButton* HostButton = AddButton(IPColumn, NSLOCTEXT("YS", "HostButton", "Héberger en IP"), HostButtonColor, TEXT("HostButton"));
	HostButton->OnClicked.AddDynamic(this, &UYSMainMenuWidget::HandleHostClicked);

	LocalAddressText = AddText(IPColumn, FText::GetEmpty(), 14, FMargin(0.0f, 4.0f));

	UButton* CopyAddressButton = AddButton(IPColumn, NSLOCTEXT("YS", "CopyAddressButton", "Copier mon IP"), ButtonColor, TEXT("CopyAddressButton"));
	CopyAddressButton->OnClicked.AddDynamic(this, &UYSMainMenuWidget::HandleCopyAddressClicked);

	USizeBox* AddressSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AddressSize"));
	AddressSize->SetHeightOverride(48.0f);
	AddressBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("AddressBox"));
	AddressBox->SetHintText(NSLOCTEXT("YS", "AddressHint", "IP de l'hôte"));
	AddressBox->SetJustification(ETextJustify::Center);
	AddressBox->OnTextCommitted.AddDynamic(this, &UYSMainMenuWidget::HandleAddressCommitted);
	AddressSize->AddChild(AddressBox);
	UVerticalBoxSlot* AddressSlot = IPColumn->AddChildToVerticalBox(AddressSize);
	AddressSlot->SetHorizontalAlignment(HAlign_Fill);
	AddressSlot->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 6.0f));

	UButton* JoinButton = AddButton(IPColumn, NSLOCTEXT("YS", "JoinButton", "Rejoindre par IP"), ButtonColor, TEXT("JoinButton"));
	JoinButton->OnClicked.AddDynamic(this, &UYSMainMenuWidget::HandleJoinClicked);

	// ---- bas : Quitter et état ----
	USizeBox* QuitSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	QuitSize->SetWidthOverride(ColumnWidth);
	UVerticalBox* QuitColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	QuitSize->AddChild(QuitColumn);
	UVerticalBoxSlot* QuitSlot = Page->AddChildToVerticalBox(QuitSize);
	QuitSlot->SetHorizontalAlignment(HAlign_Center);
	QuitSlot->SetPadding(FMargin(0.0f, 32.0f, 0.0f, 0.0f));
	UButton* QuitButton = AddButton(QuitColumn, NSLOCTEXT("YS", "QuitButton", "Quitter"), ButtonColor, TEXT("QuitButton"));
	QuitButton->OnClicked.AddDynamic(this, &UYSMainMenuWidget::HandleQuitClicked);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Status"));
	YSMainMenu::SetFontSize(StatusText, 18);
	StatusText->SetColorAndOpacity(FSlateColor(StatusColor));
	StatusText->SetJustification(ETextJustify::Center);
	StatusText->SetAutoWrapText(true);
	USizeBox* StatusSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("StatusSize"));
	StatusSize->SetWidthOverride(ColumnWidth * 2.0f);
	StatusSize->AddChild(StatusText);
	UVerticalBoxSlot* StatusSlot = Page->AddChildToVerticalBox(StatusSize);
	StatusSlot->SetHorizontalAlignment(HAlign_Center);
	StatusSlot->SetPadding(FMargin(0.0f, 20.0f, 0.0f, 0.0f));
}

UButton* UYSMainMenuWidget::AddButton(UVerticalBox* Column, const FText& Label, const FLinearColor& Color, FName Name, float Height, int32 FontSize)
{
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetHeightOverride(Height > 0.0f ? Height : ButtonHeight);

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	Button->SetBackgroundColor(Color);
	Size->AddChild(Button);

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(Label);
	YSMainMenu::SetFontSize(Text, FontSize);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Button->AddChild(Text);

	UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(Size);
	ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	ButtonSlot->SetPadding(FMargin(0.0f, 6.0f));

	return Button;
}

UTextBlock* UYSMainMenuWidget::AddText(UVerticalBox* Column, const FText& Text, int32 FontSize, const FMargin& TextPadding)
{
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TextBlock->SetText(Text);
	YSMainMenu::SetFontSize(TextBlock, FontSize);
	TextBlock->SetColorAndOpacity(FSlateColor(StatusColor));
	TextBlock->SetJustification(ETextJustify::Center);
	TextBlock->SetAutoWrapText(true);
	UVerticalBoxSlot* TextSlot = Column->AddChildToVerticalBox(TextBlock);
	TextSlot->SetHorizontalAlignment(HAlign_Fill);
	TextSlot->SetPadding(TextPadding);
	return TextBlock;
}

void UYSMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UYSGameInstance* GameInstance = GetYSGameInstance();
	if (!GameInstance)
	{
		return;
	}

	// retour au menu (fin de partie, hôte parti, erreur) : on n'est plus dans le lobby Steam
	GameInstance->DestroyOnlineSession();

	OnlineStatusHandle = GameInstance->OnOnlineStatus.AddUObject(this, &UYSMainMenuWidget::HandleOnlineStatus);
	GamesFoundHandle = GameInstance->OnOnlineGamesFound.AddUObject(this, &UYSMainMenuWidget::HandleOnlineGamesFound);

	AddressBox->SetText(FText::FromString(GameInstance->GetLastJoinAddress()));

	const TArray<FString> LocalAddresses = UYSGameInstance::GetLocalIPv4Addresses();
	LocalAddressText->SetText(LocalAddresses.Num() > 0
		? FText::Format(NSLOCTEXT("YS", "LocalAddresses", "IP de ce PC : {0}"), FText::FromString(FString::Join(LocalAddresses, TEXT("  /  "))))
		: NSLOCTEXT("YS", "NoLocalAddress", "Aucune IP locale trouvée"));

	RefreshSteamState();

	// erreur réseau qui a ramené le joueur au menu
	const FText Error = GameInstance->ConsumePendingError();
	SetStatus(Error, !Error.IsEmpty());
}

void UYSMainMenuWidget::NativeDestruct()
{
	if (UYSGameInstance* GameInstance = GetYSGameInstance())
	{
		GameInstance->OnOnlineStatus.Remove(OnlineStatusHandle);
		GameInstance->OnOnlineGamesFound.Remove(GamesFoundHandle);
	}

	Super::NativeDestruct();
}

void UYSMainMenuWidget::RefreshSteamState()
{
	const UYSGameInstance* GameInstance = GetYSGameInstance();
	const bool bSteam = GameInstance && GameInstance->IsSteamAvailable();

	SteamStateText->SetText(bSteam
		? FText::Format(NSLOCTEXT("YS", "SteamConnected", "Connecté : {0}"), FText::FromString(GameInstance->GetSteamPersonaName()))
		: NSLOCTEXT("YS", "SteamMissing", "Steam non détecté : lance Steam avant le jeu (mode IP seulement)."));
	SteamStateText->SetColorAndOpacity(FSlateColor(bSteam ? StatusColor : ErrorColor));
	SteamHostButton->SetIsEnabled(bSteam);
	SteamSearchButton->SetIsEnabled(bSteam && !GameInstance->IsSearchingOnlineGames());
}

void UYSMainMenuWidget::RefreshGameList()
{
	GameList->ClearChildren();
	GameProxies.Reset();

	const UYSGameInstance* GameInstance = GetYSGameInstance();
	if (!GameInstance)
	{
		return;
	}

	const TArray<FYSOnlineGame>& Games = GameInstance->GetFoundOnlineGames();
	if (Games.Num() == 0)
	{
		AddText(GameList, NSLOCTEXT("YS", "SteamNoGame", "Aucune partie trouvée (les invitations marchent quand même)."), 14, FMargin(0.0f, 6.0f));
		return;
	}

	for (int32 Index = 0; Index < Games.Num(); ++Index)
	{
		const FYSOnlineGame& Game = Games[Index];
		const FText Label = FText::Format(NSLOCTEXT("YS", "SteamGameEntry", "{0}  ({1} / {2})"),
			FText::FromString(Game.HostName.IsEmpty() ? TEXT("?") : Game.HostName), Game.Players, Game.MaxPlayers);
		UButton* GameButton = AddButton(GameList, Label, ButtonColor, NAME_None, GameButtonHeight, 18);
		GameButton->SetIsEnabled(Game.Players < Game.MaxPlayers);

		UYSMainMenuButtonProxy* Proxy = NewObject<UYSMainMenuButtonProxy>(this);
		Proxy->Index = Index;
		Proxy->Menu = this;
		GameButton->OnClicked.AddDynamic(Proxy, &UYSMainMenuButtonProxy::HandleClicked);
		GameProxies.Add(Proxy);
	}
}

void UYSMainMenuWidget::SetStatus(const FText& Status, bool bError)
{
	StatusText->SetText(Status);
	StatusText->SetColorAndOpacity(FSlateColor(bError ? ErrorColor : StatusColor));
}

UYSGameInstance* UYSMainMenuWidget::GetYSGameInstance() const
{
	return GetGameInstance<UYSGameInstance>();
}

void UYSMainMenuWidget::HandleOnlineStatus(const FText& Status, bool bError)
{
	SetStatus(Status, bError);
	if (bError)
	{
		// déjà affichée : ne pas la remontrer au prochain retour au menu
		if (UYSGameInstance* GameInstance = GetYSGameInstance())
		{
			GameInstance->ConsumePendingError();
		}
	}
}

void UYSMainMenuWidget::HandleOnlineGamesFound(bool bSuccess)
{
	RefreshSteamState();
	RefreshGameList();
	if (bSuccess)
	{
		const int32 Count = GetYSGameInstance() ? GetYSGameInstance()->GetFoundOnlineGames().Num() : 0;
		SetStatus(FText::Format(NSLOCTEXT("YS", "SteamGamesFound", "{0} partie(s) trouvée(s)."), Count));
	}
	else
	{
		SetStatus(NSLOCTEXT("YS", "SteamSearchFailed", "La recherche des parties Steam a échoué."), true);
	}
}

void UYSMainMenuWidget::HandleHostClicked()
{
	if (UYSGameInstance* GameInstance = GetYSGameInstance())
	{
		SetStatus(NSLOCTEXT("YS", "Hosting", "Création de la partie…"));
		GameInstance->HostGame();

		// erreur immédiate (config) : affichée sans quitter le menu
		const FText Error = GameInstance->ConsumePendingError();
		if (!Error.IsEmpty())
		{
			SetStatus(Error, true);
		}
	}
}

void UYSMainMenuWidget::HandleSteamHostClicked()
{
	if (UYSGameInstance* GameInstance = GetYSGameInstance())
	{
		GameInstance->HostOnlineGame();
	}
}

void UYSMainMenuWidget::HandleSteamSearchClicked()
{
	if (UYSGameInstance* GameInstance = GetYSGameInstance())
	{
		SetStatus(NSLOCTEXT("YS", "SteamSearching", "Recherche des parties Steam…"));
		GameInstance->FindOnlineGames();
		RefreshSteamState();
	}
}

void UYSMainMenuWidget::NotifyGameClicked(int32 Index)
{
	if (UYSGameInstance* GameInstance = GetYSGameInstance())
	{
		GameInstance->JoinOnlineGame(Index);
	}
}

void UYSMainMenuWidget::HandleJoinClicked()
{
	UYSGameInstance* GameInstance = GetYSGameInstance();
	const FString Address = AddressBox->GetText().ToString().TrimStartAndEnd();
	if (!GameInstance || !GameInstance->JoinGame(Address))
	{
		SetStatus(NSLOCTEXT("YS", "NoAddress", "Entre l'IP de l'hôte."), true);
		return;
	}

	SetStatus(FText::Format(NSLOCTEXT("YS", "Joining", "Connexion à {0}…"), FText::FromString(Address)));
}

void UYSMainMenuWidget::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UYSMainMenuWidget::HandleCopyAddressClicked()
{
	// première IP trouvée (réseau local, ou VPN s'il est le seul) ; les autres restent affichées au-dessus
	const TArray<FString> LocalAddresses = UYSGameInstance::GetLocalIPv4Addresses();
	if (LocalAddresses.Num() == 0)
	{
		SetStatus(NSLOCTEXT("YS", "NoLocalAddressToCopy", "Aucune IP locale à copier."), true);
		return;
	}

	FPlatformApplicationMisc::ClipboardCopy(*LocalAddresses[0]);
	SetStatus(FText::Format(NSLOCTEXT("YS", "AddressCopied", "IP copiée : {0}"), FText::FromString(LocalAddresses[0])));
}

void UYSMainMenuWidget::HandleAddressCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		HandleJoinClicked();
	}
}
