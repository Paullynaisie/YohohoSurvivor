#include "UI/YSInteractPromptWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

bool UYSInteractPromptWidget::Initialize()
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

void UYSInteractPromptWidget::BuildWidgetTree()
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("KeyBox"));
	Box->SetWidthOverride(KeySize.X);
	Box->SetHeightOverride(KeySize.Y);
	WidgetTree->RootWidget = Box;

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Box->AddChild(Overlay);

	// touche : carré arrondi avec contour
	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Key"));
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Brush.OutlineSettings.CornerRadii = FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius);
	Brush.OutlineSettings.Width = OutlineWidth;
	Brush.OutlineSettings.Color = FSlateColor(OutlineColor);
	Brush.TintColor = FSlateColor(BackgroundColor);
	Background->SetBrush(Brush);
	UOverlaySlot* BackgroundSlot = Overlay->AddChildToOverlay(Background);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("KeyText"));
	FSlateFontInfo Font = Label->GetFont();
	Font.Size = FontSize;
	Font.TypefaceFontName = TEXT("Bold");
	Label->SetFont(Font);
	Label->SetColorAndOpacity(FSlateColor(TextColor));
	Label->SetText(KeyText);
	UOverlaySlot* LabelSlot = Overlay->AddChildToOverlay(Label);
	LabelSlot->SetHorizontalAlignment(HAlign_Center);
	LabelSlot->SetVerticalAlignment(VAlign_Center);
}
