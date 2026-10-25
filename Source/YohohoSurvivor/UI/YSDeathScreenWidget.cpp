#include "UI/YSDeathScreenWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace YSDeathScreen
{
	void SetFontSize(UTextBlock* Text, int32 Size)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}
}

bool UYSDeathScreenWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// pas de Widget Blueprint : l'arbre est construit dès la création
	if (bSuperInitialized && WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}

	return bSuperInitialized;
}

void UYSDeathScreenWidget::BuildWidgetTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	BackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Background"));
	BackgroundImage->SetColorAndOpacity(BackgroundColor);
	UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(BackgroundImage);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Center);
	ColumnSlot->SetVerticalAlignment(VAlign_Center);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	TitleText->SetText(Title);
	YSDeathScreen::SetFontSize(TitleText, TitleFontSize);
	TitleText->SetColorAndOpacity(FSlateColor(TitleColor));
	UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleText);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);

	HintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Hint"));
	HintText->SetText(Hint);
	YSDeathScreen::SetFontSize(HintText, HintFontSize);
	HintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f)));
	HintText->SetShadowOffset(FVector2D(2.0f, 2.0f));
	HintText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
	UVerticalBoxSlot* HintSlot = Column->AddChildToVerticalBox(HintText);
	HintSlot->SetHorizontalAlignment(HAlign_Center);
	HintSlot->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 0.0f));

	SpectateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Spectate"));
	YSDeathScreen::SetFontSize(SpectateText, SpectateFontSize);
	SpectateText->SetColorAndOpacity(FSlateColor(SpectateColor));
	SpectateText->SetShadowOffset(FVector2D(2.0f, 2.0f));
	SpectateText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
	SpectateText->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBoxSlot* SpectateSlot = Column->AddChildToVerticalBox(SpectateText);
	SpectateSlot->SetHorizontalAlignment(HAlign_Center);
	SpectateSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
}

void UYSDeathScreenWidget::SetSpectating(bool bSpectating, const FText& TargetName)
{
	if (!TitleText || (bSpectating == bShownSpectating && TargetName.EqualTo(ShownTargetName)))
	{
		return;
	}
	bShownSpectating = bSpectating;
	ShownTargetName = TargetName;

	// spectateur : on voit le jeu (pas de voile ni de titre), le message « Échap » reste au milieu
	BackgroundImage->SetVisibility(bSpectating ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	TitleText->SetVisibility(bSpectating ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	YSDeathScreen::SetFontSize(HintText, bSpectating ? SpectateHintFontSize : HintFontSize);
	SpectateText->SetVisibility(bSpectating && !TargetName.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SpectateText->SetText(FText::Format(SpectateFormat, TargetName));
}
