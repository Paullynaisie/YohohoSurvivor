#include "UI/YSShipBoardingWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

bool UYSShipBoardingWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// arbre construit en C++ ; un Widget Blueprint enfant (WBP_ShipBoarding) sert aux réglages : sa racine vide éventuelle est remplacée
	const UPanelWidget* DesignerRoot = WidgetTree ? Cast<UPanelWidget>(WidgetTree->RootWidget) : nullptr;
	if (bSuperInitialized && WidgetTree && (!WidgetTree->RootWidget || (DesignerRoot && DesignerRoot->GetChildrenCount() == 0)))
	{
		BuildWidgetTree();
	}
	return bSuperInitialized;
}

void UYSShipBoardingWidget::BuildWidgetTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	auto MakeText = [this](const FText& Text, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Label->SetText(Text);
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = Size;
		Font.TypefaceFontName = FName(TEXT("Bold"));
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(Color));
		Label->SetShadowOffset(FVector2D(2.0f, 2.0f));
		Label->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		Label->SetJustification(ETextJustify::Center);
		return Label;
	};

	// « X/Y sur le bateau » : en bas au centre
	BoardingText = MakeText(FText::GetEmpty(), BoardingFontSize, BoardingColor);
	BoardingText->SetVisibility(ESlateVisibility::Collapsed);
	UOverlaySlot* BoardingSlot = Root->AddChildToOverlay(BoardingText);
	BoardingSlot->SetHorizontalAlignment(HAlign_Center);
	BoardingSlot->SetVerticalAlignment(VAlign_Bottom);
	BoardingSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BoardingBottomOffset));

	// « BRAVO » : voile plein écran + texte au centre
	UOverlay* Victory = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Victory"));
	VictoryPanel = Victory;
	Victory->SetVisibility(ESlateVisibility::Collapsed);
	UOverlaySlot* VictorySlot = Root->AddChildToOverlay(Victory);
	VictorySlot->SetHorizontalAlignment(HAlign_Fill);
	VictorySlot->SetVerticalAlignment(VAlign_Fill);

	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Background->SetColorAndOpacity(VictoryBackgroundColor);
	UOverlaySlot* BackgroundSlot = Victory->AddChildToOverlay(Background);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

	UOverlaySlot* TextSlot = Victory->AddChildToOverlay(MakeText(VictoryText, VictoryFontSize, VictoryColor));
	TextSlot->SetHorizontalAlignment(HAlign_Center);
	TextSlot->SetVerticalAlignment(VAlign_Center);
}

void UYSShipBoardingWidget::SetBoarding(int32 Aboard, int32 Total)
{
	if (BoardingText)
	{
		BoardingText->SetText(FText::Format(BoardingFormat, Aboard, Total));
		BoardingText->SetVisibility(Aboard > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UYSShipBoardingWidget::SetVictory(bool bVictory)
{
	if (VictoryPanel)
	{
		VictoryPanel->SetVisibility(bVictory ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
