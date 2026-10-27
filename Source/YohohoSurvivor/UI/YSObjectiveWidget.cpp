#include "UI/YSObjectiveWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace YSObjective
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

bool UYSObjectiveWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// arbre construit en C++ ; la racine vide d'un Widget Blueprint enfant est remplacée
	const UPanelWidget* DesignerRoot = WidgetTree ? Cast<UPanelWidget>(WidgetTree->RootWidget) : nullptr;
	if (bSuperInitialized && WidgetTree && (!WidgetTree->RootWidget || (DesignerRoot && DesignerRoot->GetChildrenCount() == 0)))
	{
		BuildWidgetTree();
	}

	return bSuperInitialized;
}

void UYSObjectiveWidget::BuildWidgetTree()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	WidgetTree->RootWidget = Column;

	TitleLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	YSObjective::StyleText(TitleLabel, TitleFontSize);
	TitleLabel->SetText(Title);
	UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleLabel);
	TitleSlot->SetHorizontalAlignment(HAlign_Right);
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, TitleSpacing));

	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarBox"));
	Box->SetWidthOverride(BarSize.X);
	Box->SetHeightOverride(BarSize.Y);
	Column->AddChildToVerticalBox(Box);

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Box->AddChild(Overlay);

	Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Bar"));
	FProgressBarStyle Style = Bar->GetWidgetStyle();
	Style.BackgroundImage.TintColor = FSlateColor(BarBackgroundColor);
	Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
	Bar->SetWidgetStyle(Style);
	Bar->SetFillColorAndOpacity(BarColor);
	// se remplit depuis la droite (comme la barre de l'élite au-dessus)
	Bar->SetBarFillType(EProgressBarFillType::RightToLeft);
	UOverlaySlot* BarSlot = Overlay->AddChildToOverlay(Bar);
	BarSlot->SetHorizontalAlignment(HAlign_Fill);
	BarSlot->SetVerticalAlignment(VAlign_Fill);

	ValueLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Value"));
	YSObjective::StyleText(ValueLabel, ValueFontSize);
	UOverlaySlot* LabelSlot = Overlay->AddChildToOverlay(ValueLabel);
	LabelSlot->SetHorizontalAlignment(HAlign_Center);
	LabelSlot->SetVerticalAlignment(VAlign_Center);

	SetProgress(0, 0);
}

void UYSObjectiveWidget::SetProgress(int32 Doubloons, int32 Goal)
{
	if (!Bar || (Doubloons == DisplayedDoubloons && Goal == DisplayedGoal))
	{
		return;
	}

	DisplayedDoubloons = Doubloons;
	DisplayedGoal = Goal;
	Bar->SetPercent(Goal > 0 ? FMath::Clamp(static_cast<float>(Doubloons) / Goal, 0.0f, 1.0f) : 0.0f);
	Bar->SetFillColorAndOpacity(Goal > 0 && Doubloons >= Goal ? CompletedBarColor : BarColor);
	ValueLabel->SetText(FText::Format(NSLOCTEXT("YS", "ObjectiveValue", "{0} / {1}"), FText::AsNumber(Doubloons), FText::AsNumber(Goal)));
}

void UYSObjectiveWidget::SetShipLevel(int32 Level)
{
	if (!TitleLabel || Level == DisplayedShipLevel)
	{
		return;
	}
	DisplayedShipLevel = Level;
	TitleLabel->SetText(Level > 0 ? FText::Format(ShipLevelTitleFormat, FText::AsNumber(Level)) : Title);
}
