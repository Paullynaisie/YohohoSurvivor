#include "UI/YSDeathScreenWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
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

	// arbre construit en C++ ; la racine vide d'un Widget Blueprint enfant (WBP_DeathScreen) est remplacée
	const UPanelWidget* DesignerRoot = WidgetTree ? Cast<UPanelWidget>(WidgetTree->RootWidget) : nullptr;
	if (bSuperInitialized && WidgetTree && (!WidgetTree->RootWidget || (DesignerRoot && DesignerRoot->GetChildrenCount() == 0)))
	{
		BuildWidgetTree();
	}

	return bSuperInitialized;
}

void UYSDeathScreenWidget::BuildWidgetTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;
	// seul le bouton de réanimation reçoit les clics
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	BackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Background"));
	BackgroundImage->SetColorAndOpacity(BackgroundColor);
	UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(BackgroundImage);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Column->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
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

	USizeBox* ReviveSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ReviveBox"));
	ReviveSize->SetWidthOverride(ReviveButtonSize.X);
	ReviveSize->SetHeightOverride(ReviveButtonSize.Y);
	ReviveSize->SetVisibility(ESlateVisibility::Collapsed);
	ReviveBox = ReviveSize;
	UButton* ReviveButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Revive"));
	ReviveButton->SetBackgroundColor(ReviveButtonColor);	ReviveButton->OnClicked.AddDynamic(this, &UYSDeathScreenWidget::HandleReviveClicked);
	UTextBlock* ReviveLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ReviveLabel"));
	ReviveLabel->SetText(ReviveText);
	YSDeathScreen::SetFontSize(ReviveLabel, ReviveFontSize);
	ReviveButton->AddChild(ReviveLabel);
	ReviveSize->AddChild(ReviveButton);
	UVerticalBoxSlot* ReviveSlot = Column->AddChildToVerticalBox(ReviveSize);
	ReviveSlot->SetHorizontalAlignment(HAlign_Center);
	ReviveSlot->SetPadding(FMargin(0.0f, ReviveTopSpacing, 0.0f, 0.0f));
}

void UYSDeathScreenWidget::HandleReviveClicked()
{
	OnReviveClicked.ExecuteIfBound();
}

void UYSDeathScreenWidget::SetReviveAvailable(bool bAvailable)
{
	if (ReviveBox)
	{
		ReviveBox->SetVisibility(bAvailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
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
