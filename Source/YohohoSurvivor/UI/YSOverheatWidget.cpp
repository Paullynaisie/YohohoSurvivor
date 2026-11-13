#include "UI/YSOverheatWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

bool UYSOverheatWidget::Initialize()
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

void UYSOverheatWidget::BuildWidgetTree()
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	WidgetTree->RootWidget = Row;

	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarBox"));
	BarBox->SetWidthOverride(BarSize.X);
	BarBox->SetHeightOverride(BarSize.Y);
	UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(BarBox);
	BarSlot->SetVerticalAlignment(VAlign_Center);

	HeatBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Heat"));
	FProgressBarStyle Style = HeatBar->GetWidgetStyle();
	Style.BackgroundImage.TintColor = FSlateColor(BackgroundColor);
	Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
	HeatBar->SetWidgetStyle(Style);
	HeatBar->SetFillColorAndOpacity(HeatColor);
	BarBox->AddChild(HeatBar);

	PercentText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Percent"));
	FSlateFontInfo Font = PercentText->GetFont();
	Font.Size = PercentFontSize;
	PercentText->SetFont(Font);
	PercentText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	PercentText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
	UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(PercentText);
	TextSlot->SetPadding(FMargin(PercentSpacing, 0.0f, 0.0f, 0.0f));
	TextSlot->SetVerticalAlignment(VAlign_Center);
	PercentText->SetVisibility(bShowPercent ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	SetVisibility(ESlateVisibility::Collapsed);
}

void UYSOverheatWidget::Refresh(bool bEnabled, float Heat, bool bCritical, bool bOverheated)
{
	if (!HeatBar)
	{
		return;
	}

	// pas la Golden Overheat : rien à afficher
	SetVisibility(bEnabled ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bEnabled)
	{
		return;
	}

	FLinearColor Color = bCritical ? CriticalColor : HeatColor;
	if (bOverheated)
	{
		// surchauffe : clignote entre la couleur de surchauffe et une version sombre
		const UWorld* World = GetWorld();
		const bool bBlinkOn = OverheatBlinkRate <= 0.0f || !World || FMath::Frac(World->GetRealTimeSeconds() * OverheatBlinkRate) < 0.5f;
		Color = bBlinkOn ? OverheatColor : OverheatColor * 0.35f;
	}
	HeatBar->SetPercent(FMath::Clamp(Heat, 0.0f, 1.0f));
	HeatBar->SetFillColorAndOpacity(Color);
	if (PercentText)
	{
		PercentText->SetText(FText::FromString(FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Heat * 100.0f))));
		PercentText->SetColorAndOpacity(FSlateColor(Color));
	}
}
