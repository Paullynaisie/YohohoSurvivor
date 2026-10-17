#include "UI/YSEliteHealthWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace YSEliteHealth
{
	void StyleText(UTextBlock* Text, int32 Size)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor::Black;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}
}

bool UYSEliteHealthWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// pas de Widget Blueprint : l'arbre est construit dès la création
	if (bSuperInitialized && WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}

	return bSuperInitialized;
}

void UYSEliteHealthWidget::BuildWidgetTree()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	WidgetTree->RootWidget = Column;

	NameLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Name"));
	YSEliteHealth::StyleText(NameLabel, 18);
	UVerticalBoxSlot* NameSlot = Column->AddChildToVerticalBox(NameLabel);
	NameSlot->SetHorizontalAlignment(HAlign_Right);
	NameSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));

	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarBox"));
	Box->SetWidthOverride(BarSize.X);
	Box->SetHeightOverride(BarSize.Y);
	Column->AddChildToVerticalBox(Box);

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Box->AddChild(Overlay);

	Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	FProgressBarStyle Style = Bar->GetWidgetStyle();
	Style.BackgroundImage.TintColor = FSlateColor(BarBackgroundColor);
	Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
	Bar->SetWidgetStyle(Style);
	Bar->SetFillColorAndOpacity(BarColor);
	// se vide vers la droite (symétrique de la barre du joueur)
	Bar->SetBarFillType(EProgressBarFillType::RightToLeft);
	UOverlaySlot* BarSlot = Overlay->AddChildToOverlay(Bar);
	BarSlot->SetHorizontalAlignment(HAlign_Fill);
	BarSlot->SetVerticalAlignment(VAlign_Fill);

	ValueLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	YSEliteHealth::StyleText(ValueLabel, 14);
	UOverlaySlot* LabelSlot = Overlay->AddChildToOverlay(ValueLabel);
	LabelSlot->SetHorizontalAlignment(HAlign_Center);
	LabelSlot->SetVerticalAlignment(VAlign_Center);
}

void UYSEliteHealthWidget::SetElite(const FText& Name, float Health, float MaxHealth)
{
	if (!Bar)
	{
		return;
	}

	FNumberFormattingOptions Format;
	Format.MaximumFractionalDigits = 0;
	NameLabel->SetText(Name);
	Bar->SetPercent(MaxHealth > 0.0f ? Health / MaxHealth : 0.0f);
	ValueLabel->SetText(FText::Format(NSLOCTEXT("YS", "EliteHealth", "{0} / {1}"), FText::AsNumber(FMath::CeilToFloat(Health), &Format), FText::AsNumber(MaxHealth, &Format)));
}
