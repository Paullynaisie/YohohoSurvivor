#include "UI/YSHealthBarWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

bool UYSHealthBarWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// arbre construit dès la création (pour être rempli avant l'affichage) ; un Widget Blueprint enfant (WBP_FoeHealthBar)
	// sert aux réglages : sa racine vide éventuelle (Canvas Panel ajouté par défaut à la création) est remplacée
	const UPanelWidget* DesignerRoot = WidgetTree ? Cast<UPanelWidget>(WidgetTree->RootWidget) : nullptr;
	if (bSuperInitialized && WidgetTree && (!WidgetTree->RootWidget || (DesignerRoot && DesignerRoot->GetChildrenCount() == 0)))
	{
		BuildWidgetTree();
	}

	return bSuperInitialized;
}

void UYSHealthBarWidget::BuildWidgetTree()
{
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// point critique : rond (boîte entièrement arrondie) au-dessus de la barre
	USizeBox* DotBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CriticalDotSize"));
	DotBox->SetWidthOverride(CriticalDotSize);
	DotBox->SetHeightOverride(CriticalDotSize);
	CriticalDot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CriticalDot"));
	FSlateBrush DotBrush;
	DotBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	DotBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
	DotBrush.OutlineSettings.Width = CriticalDotOutlineWidth;
	DotBrush.OutlineSettings.Color = FSlateColor(CriticalDotOutlineColor);
	DotBrush.TintColor = FSlateColor(CriticalDotColor);
	CriticalDot->SetBrush(DotBrush);
	DotBox->AddChild(CriticalDot);
	DotBox->SetRenderTranslation(FVector2D(CriticalDotOffsetX, 0.0f));
	UVerticalBoxSlot* DotSlot = Root->AddChildToVerticalBox(DotBox);
	DotSlot->SetHorizontalAlignment(HAlign_Center);
	DotSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, CriticalDotSpacing));

	BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarSize"));
	BarBox->SetWidthOverride(BarSize.X);
	BarBox->SetHeightOverride(BarSize.Y);
	Root->AddChildToVerticalBox(BarBox);

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay"));
	BarBox->AddChild(Overlay);

	Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Bar"));
	FProgressBarStyle BarStyle = Bar->GetWidgetStyle();
	BarStyle.BackgroundImage.TintColor = FSlateColor(BackgroundColor);
	BarStyle.FillImage.TintColor = FSlateColor(FLinearColor::White);
	Bar->SetWidgetStyle(BarStyle);
	Bar->SetFillColorAndOpacity(FillColor);
	UOverlaySlot* BarSlot = Overlay->AddChildToOverlay(Bar);
	BarSlot->SetHorizontalAlignment(HAlign_Fill);
	BarSlot->SetVerticalAlignment(VAlign_Fill);

	// bouclier de boss : par-dessus la barre de vie, fond transparent (la vie reste visible dans la partie vide)
	ShieldBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ShieldBar"));
	FProgressBarStyle ShieldStyle = ShieldBar->GetWidgetStyle();
	ShieldStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor::Transparent);
	ShieldStyle.FillImage.TintColor = FSlateColor(FLinearColor::White);
	ShieldBar->SetWidgetStyle(ShieldStyle);
	ShieldBar->SetFillColorAndOpacity(ShieldColor);
	ShieldBar->SetVisibility(ESlateVisibility::Collapsed);
	UOverlaySlot* ShieldSlot = Overlay->AddChildToOverlay(ShieldBar);
	ShieldSlot->SetHorizontalAlignment(HAlign_Fill);
	ShieldSlot->SetVerticalAlignment(VAlign_Fill);

	Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
	FSlateFontInfo Font = Label->GetFont();
	Font.Size = FontSize;
	Label->SetFont(Font);
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Label->SetShadowOffset(FVector2D(1.0f, 1.0f));
	Label->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
	UOverlaySlot* LabelSlot = Overlay->AddChildToOverlay(Label);
	LabelSlot->SetHorizontalAlignment(HAlign_Center);
	LabelSlot->SetVerticalAlignment(VAlign_Center);

	RefreshVisibility();
}

void UYSHealthBarWidget::SetHealth(float Current, float Max, bool bShowWhenFull)
{
	if (!Bar)
	{
		return;
	}

	// PV pleins (ou pas encore reçus du serveur) : pas de barre
	bBarVisible = Max > 0.0f && (bShowWhenFull || (Current > 0.0f && Current < Max));
	if (bBarVisible)
	{
		FNumberFormattingOptions Format;
		Format.MaximumFractionalDigits = 1;

		Bar->SetPercent(Current / Max);
		HealthText = FText::Format(NSLOCTEXT("YS", "HealthBarLabel", "{0} / {1}"), FText::AsNumber(Current, &Format), FText::AsNumber(Max, &Format));
		if (ShieldCurrent <= 0.0f)
		{
			Label->SetText(HealthText);
		}
	}
	RefreshVisibility();
}

void UYSHealthBarWidget::SetShield(float Current, float Max)
{
	if (!ShieldBar || !Label)
	{
		return;
	}
	ShieldCurrent = Current;
	ShieldMax = Max;
	const bool bShield = Current > 0.0f && Max > 0.0f;
	ShieldBar->SetVisibility(bShield ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bShield)
	{
		FNumberFormattingOptions Format;
		Format.MaximumFractionalDigits = 0;
		ShieldBar->SetPercent(Current / Max);
		Label->SetText(FText::Format(NSLOCTEXT("YS", "HealthBarShieldLabel", "Bouclier {0} / {1}"), FText::AsNumber(Current, &Format), FText::AsNumber(Max, &Format)));
	}
	else
	{
		Label->SetText(HealthText);
	}
}

void UYSHealthBarWidget::SetSizeMultiplier(float Multiplier)
{
	if (!BarBox || !Label)
	{
		return;
	}
	BarBox->SetWidthOverride(BarSize.X * Multiplier);
	BarBox->SetHeightOverride(BarSize.Y * Multiplier);
	FSlateFontInfo Font = Label->GetFont();
	Font.Size = FMath::RoundToInt(FontSize * Multiplier);
	Label->SetFont(Font);
}

void UYSHealthBarWidget::SetCriticalReady(bool bReady)
{
	bCriticalReady = bReady;
	RefreshVisibility();
}

void UYSHealthBarWidget::RefreshVisibility()
{
	if (!BarBox || !CriticalDot)
	{
		return;
	}
	// éléments cachés sans perdre leur place : le point et la barre restent à la même hauteur quand l'autre apparaît
	BarBox->SetVisibility(bBarVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	CriticalDot->SetVisibility(bCriticalReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	SetVisibility(bBarVisible || bCriticalReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
