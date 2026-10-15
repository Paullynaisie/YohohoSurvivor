#include "UI/YSCharacterSelectWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	void SetFontSize(UTextBlock* Text, int32 Size)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}
}

void UYSCharacterSelectButtonProxy::HandleClicked()
{
	if (Menu.IsValid())
	{
		Menu->NotifyButtonClicked(Index);
	}
}

bool UYSCharacterSelectWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// pas de Widget Blueprint : l'arbre est construit dès la création
	if (bSuperInitialized && WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}

	return bSuperInitialized;
}

void UYSCharacterSelectWidget::BuildWidgetTree()
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
	Title->SetText(NSLOCTEXT("YS", "CharacterSelectTitle", "Choisis ton Mouss"));
	SetFontSize(Title, 36);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));

	ButtonList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Buttons"));
	UVerticalBoxSlot* ListSlot = Column->AddChildToVerticalBox(ButtonList);
	ListSlot->SetHorizontalAlignment(HAlign_Center);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Status"));
	SetFontSize(StatusText, 18);
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f)));
	UVerticalBoxSlot* StatusSlot = Column->AddChildToVerticalBox(StatusText);
	StatusSlot->SetHorizontalAlignment(HAlign_Center);
	StatusSlot->SetPadding(FMargin(0.0f, 24.0f, 0.0f, 0.0f));
}

void UYSCharacterSelectWidget::SetCharacters(const TArray<FText>& Names)
{
	if (!ButtonList || Buttons.Num() > 0)
	{
		return;
	}

	for (int32 Index = 0; Index < Names.Num(); ++Index)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(380.0f);
		Size->SetHeightOverride(64.0f);

		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetBackgroundColor(NormalColor);
		Size->AddChild(Button);

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Label->SetText(Names[Index]);
		SetFontSize(Label, 24);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Button->AddChild(Label);

		UYSCharacterSelectButtonProxy* Proxy = NewObject<UYSCharacterSelectButtonProxy>(this);
		Proxy->Index = Index;
		Proxy->Menu = this;
		Button->OnClicked.AddDynamic(Proxy, &UYSCharacterSelectButtonProxy::HandleClicked);

		UVerticalBoxSlot* ButtonSlot = ButtonList->AddChildToVerticalBox(Size);
		ButtonSlot->SetPadding(FMargin(0.0f, 8.0f));

		Buttons.Add(Button);
		ButtonProxies.Add(Proxy);
	}
}

void UYSCharacterSelectWidget::SetSelectedIndex(int32 SelectedIndex)
{
	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		Buttons[Index]->SetBackgroundColor(Index == SelectedIndex ? SelectedColor : NormalColor);
	}
}

void UYSCharacterSelectWidget::SetStatus(const FText& Status)
{
	if (StatusText)
	{
		StatusText->SetText(Status);
	}
}

void UYSCharacterSelectWidget::NotifyButtonClicked(int32 Index)
{
	SetSelectedIndex(Index);
	OnCharacterChosen.ExecuteIfBound(Index);
}
