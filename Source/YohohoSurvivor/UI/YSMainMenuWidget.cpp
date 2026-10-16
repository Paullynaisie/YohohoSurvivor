#include "UI/YSMainMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/YSGameInstance.h"
#include "Kismet/KismetSystemLibrary.h"

namespace YSMainMenu
{
	constexpr float MenuWidth = 380.0f;

	void SetFontSize(UTextBlock* Text, int32 Size)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}

	const FLinearColor StatusColor(0.8f, 0.8f, 0.8f);
	const FLinearColor ErrorColor(1.0f, 0.35f, 0.3f);
}

// noms qualifiés : la unity build du packaging fusionne ce fichier avec les autres widgets (SetFontSize existe aussi ailleurs)
using YSMainMenu::MenuWidth;
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

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Center);
	ColumnSlot->SetVerticalAlignment(VAlign_Center);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	Title->SetText(NSLOCTEXT("YS", "MainMenuTitle", "Yohoho Survivor"));
	YSMainMenu::SetFontSize(Title, 48);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 32.0f));

	UButton* HostButton = AddButton(Column, NSLOCTEXT("YS", "HostButton", "Héberger une partie"), HostButtonColor, TEXT("HostButton"));
	HostButton->OnClicked.AddDynamic(this, &UYSMainMenuWidget::HandleHostClicked);

	LocalAddressText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LocalAddress"));
	YSMainMenu::SetFontSize(LocalAddressText, 14);
	LocalAddressText->SetColorAndOpacity(FSlateColor(StatusColor));
	LocalAddressText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* LocalAddressSlot = Column->AddChildToVerticalBox(LocalAddressText);
	LocalAddressSlot->SetHorizontalAlignment(HAlign_Center);
	LocalAddressSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 32.0f));

	USizeBox* AddressSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AddressSize"));
	AddressSize->SetWidthOverride(MenuWidth);
	AddressSize->SetHeightOverride(48.0f);
	AddressBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("AddressBox"));
	AddressBox->SetHintText(NSLOCTEXT("YS", "AddressHint", "IP de l'hôte"));
	AddressBox->SetJustification(ETextJustify::Center);
	AddressBox->OnTextCommitted.AddDynamic(this, &UYSMainMenuWidget::HandleAddressCommitted);
	AddressSize->AddChild(AddressBox);
	UVerticalBoxSlot* AddressSlot = Column->AddChildToVerticalBox(AddressSize);
	AddressSlot->SetHorizontalAlignment(HAlign_Center);
	AddressSlot->SetPadding(FMargin(0.0f, 8.0f));

	UButton* JoinButton = AddButton(Column, NSLOCTEXT("YS", "JoinButton", "Rejoindre"), ButtonColor, TEXT("JoinButton"));
	JoinButton->OnClicked.AddDynamic(this, &UYSMainMenuWidget::HandleJoinClicked);

	UButton* QuitButton = AddButton(Column, NSLOCTEXT("YS", "QuitButton", "Quitter"), ButtonColor, TEXT("QuitButton"));
	QuitButton->OnClicked.AddDynamic(this, &UYSMainMenuWidget::HandleQuitClicked);
	if (UVerticalBoxSlot* QuitSlot = Cast<UVerticalBoxSlot>(QuitButton->GetParent()->Slot))
	{
		QuitSlot->SetPadding(FMargin(0.0f, 32.0f, 0.0f, 8.0f));
	}

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Status"));
	YSMainMenu::SetFontSize(StatusText, 18);
	StatusText->SetColorAndOpacity(FSlateColor(StatusColor));
	StatusText->SetJustification(ETextJustify::Center);
	StatusText->SetAutoWrapText(true);
	USizeBox* StatusSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("StatusSize"));
	StatusSize->SetWidthOverride(MenuWidth * 1.5f);
	StatusSize->AddChild(StatusText);
	UVerticalBoxSlot* StatusSlot = Column->AddChildToVerticalBox(StatusSize);
	StatusSlot->SetHorizontalAlignment(HAlign_Center);
	StatusSlot->SetPadding(FMargin(0.0f, 24.0f, 0.0f, 0.0f));
}

UButton* UYSMainMenuWidget::AddButton(UVerticalBox* Column, const FText& Label, const FLinearColor& Color, FName Name)
{
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetWidthOverride(MenuWidth);
	Size->SetHeightOverride(64.0f);

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	Button->SetBackgroundColor(Color);
	Size->AddChild(Button);

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(Label);
	YSMainMenu::SetFontSize(Text, 24);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Button->AddChild(Text);

	UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(Size);
	ButtonSlot->SetHorizontalAlignment(HAlign_Center);
	ButtonSlot->SetPadding(FMargin(0.0f, 8.0f));

	return Button;
}

void UYSMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UYSGameInstance* GameInstance = GetYSGameInstance();
	if (!GameInstance)
	{
		return;
	}

	AddressBox->SetText(FText::FromString(GameInstance->GetLastJoinAddress()));

	const TArray<FString> LocalAddresses = UYSGameInstance::GetLocalIPv4Addresses();
	LocalAddressText->SetText(LocalAddresses.Num() > 0
		? FText::Format(NSLOCTEXT("YS", "LocalAddresses", "IP de ce PC à donner aux autres joueurs : {0}"), FText::FromString(FString::Join(LocalAddresses, TEXT("  /  "))))
		: NSLOCTEXT("YS", "NoLocalAddress", "Aucune IP locale trouvée"));

	// erreur réseau qui a ramené le joueur au menu
	const FText Error = GameInstance->ConsumePendingError();
	SetStatus(Error, !Error.IsEmpty());
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

void UYSMainMenuWidget::HandleAddressCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		HandleJoinClicked();
	}
}
