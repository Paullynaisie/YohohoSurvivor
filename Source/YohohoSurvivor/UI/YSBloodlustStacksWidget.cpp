#include "UI/YSBloodlustStacksWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

bool UYSBloodlustStacksWidget::Initialize()
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

void UYSBloodlustStacksWidget::BuildWidgetTree()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	WidgetTree->RootWidget = Column;

	PipRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Pips"));
	Column->AddChildToVerticalBox(PipRow);

	TimerBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TimerBox"));
	TimerBox->SetHeightOverride(TimerBarHeight);
	UVerticalBoxSlot* TimerSlot = Column->AddChildToVerticalBox(TimerBox);
	TimerSlot->SetPadding(FMargin(0.0f, TimerBarSpacing, 0.0f, 0.0f));
	TimerSlot->SetHorizontalAlignment(HAlign_Fill);

	TimerBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Timer"));
	FProgressBarStyle Style = TimerBar->GetWidgetStyle();
	Style.BackgroundImage.TintColor = FSlateColor(TimerBarBackgroundColor);
	Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
	TimerBar->SetWidgetStyle(Style);
	TimerBar->SetFillColorAndOpacity(TimerBarColor);
	TimerBox->AddChild(TimerBar);

	SetVisibility(ESlateVisibility::Collapsed);
}

void UYSBloodlustStacksWidget::RebuildPips(int32 Count)
{
	PipRow->ClearChildren();
	Pips.Reset();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(PipSize.X);
		Box->SetHeightOverride(PipSize.Y);
		UImage* Pip = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Box->AddChild(Pip);
		UHorizontalBoxSlot* PipSlot = PipRow->AddChildToHorizontalBox(Box);
		PipSlot->SetPadding(FMargin(Index > 0 ? PipSpacing : 0.0f, 0.0f, 0.0f, 0.0f));
		Pips.Add(Pip);
	}
	// la barre du délai fait la largeur des pastilles
	TimerBox->SetWidthOverride(Count * PipSize.X + FMath::Max(0, Count - 1) * PipSpacing);
}

void UYSBloodlustStacksWidget::Refresh(int32 Stacks, int32 MaxStacks, float TimeRatio, bool bCritical)
{
	if (!PipRow)
	{
		return;
	}

	// Bloodlust pas débloqué : rien à afficher
	SetVisibility(MaxStacks > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (MaxStacks <= 0)
	{
		return;
	}

	if (Pips.Num() != MaxStacks)
	{
		RebuildPips(MaxStacks);
	}
	for (int32 Index = 0; Index < Pips.Num(); ++Index)
	{
		Pips[Index]->SetColorAndOpacity(Index < Stacks ? (bCritical ? CriticalColor : FilledColor) : EmptyColor);
	}
	TimerBar->SetPercent(TimeRatio);
	TimerBar->SetFillColorAndOpacity(bCritical ? CriticalColor : TimerBarColor);
}
