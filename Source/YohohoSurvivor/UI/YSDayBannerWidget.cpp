#include "UI/YSDayBannerWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"

bool UYSDayBannerWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// pas de Widget Blueprint : l'arbre est construit dès la création
	if (bSuperInitialized && WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}

	return bSuperInitialized;
}

void UYSDayBannerWidget::BuildWidgetTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
	FSlateFontInfo Font = Label->GetFont();
	Font.Size = FontSize;
	Font.TypefaceFontName = FName(TEXT("Bold"));
	Font.OutlineSettings.OutlineSize = 3;
	Font.OutlineSettings.OutlineColor = FLinearColor::Black;
	Label->SetFont(Font);
	Label->SetColorAndOpacity(FSlateColor(Color));

	UOverlaySlot* LabelSlot = Root->AddChildToOverlay(Label);
	LabelSlot->SetHorizontalAlignment(HAlign_Center);
	LabelSlot->SetVerticalAlignment(VAlign_Center);
}

void UYSDayBannerWidget::SetDay(int32 Day)
{
	if (Label)
	{
		Label->SetText(FText::Format(Format, Day));
	}
}
