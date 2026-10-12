#include "UI/YSHealthBarWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

bool UYSHealthBarWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// pas de Widget Blueprint : l'arbre est construit dès la création, pour pouvoir être rempli avant l'affichage
	if (bSuperInitialized && WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}

	return bSuperInitialized;
}

void UYSHealthBarWidget::BuildWidgetTree()
{
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Root"));
	Root->SetWidthOverride(BarSize.X);
	Root->SetHeightOverride(BarSize.Y);
	WidgetTree->RootWidget = Root;

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay"));
	Root->AddChild(Overlay);

	Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Bar"));
	FProgressBarStyle BarStyle = Bar->GetWidgetStyle();
	BarStyle.BackgroundImage.TintColor = FSlateColor(BackgroundColor);
	BarStyle.FillImage.TintColor = FSlateColor(FLinearColor::White);
	Bar->SetWidgetStyle(BarStyle);
	Bar->SetFillColorAndOpacity(FillColor);
	UOverlaySlot* BarSlot = Overlay->AddChildToOverlay(Bar);
	BarSlot->SetHorizontalAlignment(HAlign_Fill);
	BarSlot->SetVerticalAlignment(VAlign_Fill);

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

	SetVisibility(ESlateVisibility::Collapsed);
}

void UYSHealthBarWidget::SetHealth(float Current, float Max)
{
	if (!Bar)
	{
		return;
	}

	// PV pleins (ou pas encore reçus du serveur) : rien n'est affiché
	if (Current <= 0.0f || Max <= 0.0f || Current >= Max)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	FNumberFormattingOptions Format;
	Format.MaximumFractionalDigits = 1;

	Bar->SetPercent(Current / Max);
	Label->SetText(FText::Format(NSLOCTEXT("YS", "HealthBarLabel", "{0} / {1}"), FText::AsNumber(Current, &Format), FText::AsNumber(Max, &Format)));
	SetVisibility(ESlateVisibility::HitTestInvisible);
}
