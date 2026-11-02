#include "UI/YSStaminaBarWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"

bool UYSStaminaBarWidget::Initialize()
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

void UYSStaminaBarWidget::BuildWidgetTree()
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Box"));
	Box->SetWidthOverride(BarSize.X);
	Box->SetHeightOverride(BarSize.Y);
	WidgetTree->RootWidget = Box;

	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Layers"));
	Box->AddChild(Layers);

	Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Bar"));
	FProgressBarStyle Style = Bar->GetWidgetStyle();
	Style.BackgroundImage.TintColor = FSlateColor(BackgroundColor);
	Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
	Bar->SetWidgetStyle(Style);
	UOverlaySlot* BarSlot = Layers->AddChildToOverlay(Bar);
	BarSlot->SetHorizontalAlignment(HAlign_Fill);
	BarSlot->SetVerticalAlignment(VAlign_Fill);

	USizeBox* NotchBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("NotchBox"));
	NotchBox->SetWidthOverride(NotchWidth);
	Notch = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Notch"));
	Notch->SetColorAndOpacity(NotchColor);
	NotchBox->AddChild(Notch);
	NotchSlot = Layers->AddChildToOverlay(NotchBox);
	NotchSlot->SetHorizontalAlignment(HAlign_Left);
	NotchSlot->SetVerticalAlignment(VAlign_Fill);

	SetVisibility(ESlateVisibility::Collapsed);
}

void UYSStaminaBarWidget::Refresh(float Stamina, float MaxStamina, float MoveActionCost)
{
	if (!Bar)
	{
		return;
	}

	SetVisibility(MaxStamina > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (MaxStamina <= 0.0f)
	{
		return;
	}

	Bar->SetPercent(Stamina / MaxStamina);
	Bar->SetFillColorAndOpacity(Stamina >= MoveActionCost ? ReadyColor : NotEnoughColor);
	NotchSlot->SetPadding(FMargin(FMath::Clamp(MoveActionCost / MaxStamina, 0.0f, 1.0f) * BarSize.X - NotchWidth * 0.5f, 0.0f, 0.0f, 0.0f));
}
