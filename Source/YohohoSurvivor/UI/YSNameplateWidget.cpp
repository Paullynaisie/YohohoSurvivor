#include "UI/YSNameplateWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

bool UYSNameplateWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// pas de Widget Blueprint : l'arbre est construit dès la création
	if (bSuperInitialized && WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}

	return bSuperInitialized;
}

void UYSNameplateWidget::BuildWidgetTree()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	WidgetTree->RootWidget = Column;

	NameLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Name"));
	FSlateFontInfo Font = NameLabel->GetFont();
	Font.Size = 14;
	Font.OutlineSettings.OutlineSize = 1;
	Font.OutlineSettings.OutlineColor = FLinearColor::Black;
	NameLabel->SetFont(Font);
	NameLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UVerticalBoxSlot* NameSlot = Column->AddChildToVerticalBox(NameLabel);
	NameSlot->SetHorizontalAlignment(HAlign_Center);
	NameSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));

	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarBox"));
	Box->SetWidthOverride(BarSize.X);
	Box->SetHeightOverride(BarSize.Y);
	Column->AddChildToVerticalBox(Box)->SetHorizontalAlignment(HAlign_Center);

	Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	FProgressBarStyle Style = Bar->GetWidgetStyle();
	Style.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
	Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
	Bar->SetWidgetStyle(Style);
	Bar->SetFillColorAndOpacity(BarColor);
	Box->AddChild(Bar);

	// stamina (HiredMouss) : cachée tant que SetPlayer ne donne pas de valeur
	StaminaBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("StaminaBox"));
	StaminaBox->SetWidthOverride(StaminaBarSize.X);
	StaminaBox->SetHeightOverride(StaminaBarSize.Y);
	StaminaBox->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBoxSlot* StaminaSlot = Column->AddChildToVerticalBox(StaminaBox);
	StaminaSlot->SetHorizontalAlignment(HAlign_Center);
	StaminaSlot->SetPadding(FMargin(0.0f, StaminaBarSpacing, 0.0f, 0.0f));
	StaminaBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	StaminaBar->SetWidgetStyle(Style);
	StaminaBar->SetFillColorAndOpacity(StaminaBarColor);
	StaminaBox->AddChild(StaminaBar);
}

void UYSNameplateWidget::SetPlayer(const FText& Name, float Health, float MaxHealth, float StaminaRatio)
{
	if (!NameLabel)
	{
		return;
	}

	if (!NameLabel->GetText().EqualTo(Name))
	{
		NameLabel->SetText(Name);
	}
	Bar->SetPercent(MaxHealth > 0.0f ? Health / MaxHealth : 0.0f);

	const ESlateVisibility StaminaVisibility = StaminaRatio >= 0.0f ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
	if (StaminaBox && StaminaBox->GetVisibility() != StaminaVisibility)
	{
		StaminaBox->SetVisibility(StaminaVisibility);
	}
	if (StaminaBar && StaminaRatio >= 0.0f)
	{
		StaminaBar->SetPercent(FMath::Clamp(StaminaRatio, 0.0f, 1.0f));
	}
}
