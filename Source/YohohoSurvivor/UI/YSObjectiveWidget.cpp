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

	// PV du bateau des joueurs (entre le chronomètre, au-dessus, et les doublons)
	ShipHealthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ShipHealthBox"));
	ShipHealthBox->SetWidthOverride(ShipHealthBarSize.X);
	ShipHealthBox->SetHeightOverride(ShipHealthBarSize.Y);
	ShipHealthBox->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBoxSlot* ShipSlot = Column->AddChildToVerticalBox(ShipHealthBox);
	ShipSlot->SetHorizontalAlignment(HAlign_Right);
	ShipSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, ShipHealthSpacing));
	UOverlay* ShipOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	ShipHealthBox->AddChild(ShipOverlay);
	ShipHealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ShipHealthBar"));
	FProgressBarStyle ShipStyle = ShipHealthBar->GetWidgetStyle();
	ShipStyle.BackgroundImage.TintColor = FSlateColor(BarBackgroundColor);
	ShipStyle.FillImage.TintColor = FSlateColor(FLinearColor::White);
	ShipHealthBar->SetWidgetStyle(ShipStyle);
	ShipHealthBar->SetFillColorAndOpacity(ShipHealthColor);
	ShipHealthBar->SetBarFillType(EProgressBarFillType::RightToLeft);
	UOverlaySlot* ShipBarSlot = ShipOverlay->AddChildToOverlay(ShipHealthBar);
	ShipBarSlot->SetHorizontalAlignment(HAlign_Fill);
	ShipBarSlot->SetVerticalAlignment(VAlign_Fill);
	ShipHealthLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShipHealth"));
	YSObjective::StyleText(ShipHealthLabel, ShipHealthFontSize);
	UOverlaySlot* ShipLabelSlot = ShipOverlay->AddChildToOverlay(ShipHealthLabel);
	ShipLabelSlot->SetHorizontalAlignment(HAlign_Center);
	ShipLabelSlot->SetVerticalAlignment(VAlign_Center);

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

	FeatherLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Feathers"));
	YSObjective::StyleText(FeatherLabel, FeatherFontSize);
	FeatherLabel->SetColorAndOpacity(FSlateColor(FeatherColor));
	UVerticalBoxSlot* FeatherSlot = Column->AddChildToVerticalBox(FeatherLabel);
	FeatherSlot->SetHorizontalAlignment(HAlign_Right);
	FeatherSlot->SetPadding(FMargin(0.0f, FeatherSpacing, 0.0f, 0.0f));

	CoreBoatLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CoreBoats"));
	YSObjective::StyleText(CoreBoatLabel, CoreBoatFontSize);
	CoreBoatLabel->SetColorAndOpacity(FSlateColor(CoreBoatColor));
	CoreBoatLabel->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBoxSlot* CoreBoatSlot = Column->AddChildToVerticalBox(CoreBoatLabel);
	CoreBoatSlot->SetHorizontalAlignment(HAlign_Right);
	CoreBoatSlot->SetPadding(FMargin(0.0f, CoreBoatSpacing, 0.0f, 0.0f));

	SetProgress(0, 0);
}

void UYSObjectiveWidget::SetShipHealth(bool bVisible, float Health, float MaxHealth)
{
	if (!ShipHealthBox)
	{
		return;
	}
	ShipHealthBox->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	const int32 Rounded = FMath::RoundToInt32(Health);
	const int32 RoundedMax = FMath::RoundToInt32(MaxHealth);
	if (!bVisible || (Rounded == DisplayedShipHealth && RoundedMax == DisplayedShipMaxHealth))
	{
		return;
	}
	DisplayedShipHealth = Rounded;
	DisplayedShipMaxHealth = RoundedMax;
	const bool bNegative = Health < 0.0f;
	ShipHealthBar->SetFillColorAndOpacity(bNegative ? ShipNegativeHealthColor : ShipHealthColor);
	ShipHealthBar->SetPercent(MaxHealth > 0.0f ? FMath::Clamp(FMath::Abs(Health) / MaxHealth, 0.0f, 1.0f) : 0.0f);
	ShipHealthLabel->SetText(FText::Format(ShipHealthFormat, FText::AsNumber(Rounded), FText::AsNumber(RoundedMax)));
}

void UYSObjectiveWidget::SetCoreBoats(bool bVisible, int32 Stock, int32 NextCost)
{
	if (!CoreBoatLabel)
	{
		return;
	}
	CoreBoatLabel->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bVisible || (Stock == DisplayedCoreBoats && NextCost == DisplayedCoreBoatCost))
	{
		return;
	}
	DisplayedCoreBoats = Stock;
	DisplayedCoreBoatCost = NextCost;
	CoreBoatLabel->SetText(NextCost > 0 ? FText::Format(CoreBoatFormat, FText::AsNumber(Stock), FText::AsNumber(NextCost)) : FText::Format(CoreBoatMaxFormat, FText::AsNumber(Stock)));
}

void UYSObjectiveWidget::SetPhoenixFeathers(int32 Feathers)
{
	if (!FeatherLabel || Feathers == DisplayedFeathers)
	{
		return;
	}
	DisplayedFeathers = Feathers;
	// < 0 : compteur caché (mode Hardcore : plumes inutilisables)
	FeatherLabel->SetVisibility(Feathers < 0 ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (Feathers >= 0)
	{
		FeatherLabel->SetText(FText::Format(FeatherFormat, FText::AsNumber(Feathers)));
	}
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
