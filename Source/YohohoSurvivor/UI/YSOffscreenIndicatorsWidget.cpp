#include "UI/YSOffscreenIndicatorsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "UI/YSPlayerIconWidget.h"

bool UYSOffscreenIndicatorsWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// pas de Widget Blueprint : l'arbre est construit dès la création
	if (bSuperInitialized && WidgetTree && !WidgetTree->RootWidget)
	{
		Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
		WidgetTree->RootWidget = Canvas;
	}

	return bSuperInitialized;
}

void UYSOffscreenIndicatorsWidget::SetIndicators(const TArray<FYSOffscreenIndicator>& Indicators)
{
	if (!Canvas)
	{
		return;
	}

	// une icône réutilisée par indicateur
	while (Icons.Num() < Indicators.Num())
	{
		UYSPlayerIconWidget* Icon = CreateWidget<UYSPlayerIconWidget>(this, UYSPlayerIconWidget::StaticClass());
		Icon->SetIconSize(IconSize);
		UCanvasPanelSlot* IconSlot = Canvas->AddChildToCanvas(Icon);
		IconSlot->SetAutoSize(true);
		IconSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		Icons.Add(Icon);
	}

	for (int32 Index = 0; Index < Icons.Num(); ++Index)
	{
		UYSPlayerIconWidget* Icon = Icons[Index];
		if (!Indicators.IsValidIndex(Index))
		{
			Icon->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		Icon->SetIcon(Indicators[Index].Color, Indicators[Index].Face);
		if (UCanvasPanelSlot* IconSlot = Cast<UCanvasPanelSlot>(Icon->Slot))
		{
			IconSlot->SetPosition(Indicators[Index].Position);
		}
	}
}
