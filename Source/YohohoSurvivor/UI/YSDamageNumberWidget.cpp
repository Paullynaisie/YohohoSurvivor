#include "UI/YSDamageNumberWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"

bool UYSDamageNumberWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// pas de Widget Blueprint : l'arbre est construit dès la création, pour pouvoir être rempli avant l'affichage
	if (bSuperInitialized && WidgetTree && !WidgetTree->RootWidget)
	{
		Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
		WidgetTree->RootWidget = Label;
	}

	return bSuperInitialized;
}

void UYSDamageNumberWidget::SetDamage(const FText& Text, const FLinearColor& Color, int32 FontSize)
{
	if (!Label)
	{
		return;
	}

	FSlateFontInfo Font = Label->GetFont();
	Font.Size = FontSize;
	Font.OutlineSettings.OutlineSize = 2;
	Font.OutlineSettings.OutlineColor = FLinearColor::Black;
	Label->SetFont(Font);
	Label->SetColorAndOpacity(FSlateColor(Color));
	Label->SetText(Text);
}
