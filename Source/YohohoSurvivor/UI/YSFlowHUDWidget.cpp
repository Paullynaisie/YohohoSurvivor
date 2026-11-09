#include "UI/YSFlowHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/PlatformTime.h"

namespace YSFlowHUD
{
	void StyleText(UTextBlock* Text, int32 Size, const FLinearColor& Color)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Font.OutlineSettings.OutlineSize = 2;
		Font.OutlineSettings.OutlineColor = FLinearColor::Black;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
	}

	/** mm:ss, ou h:mm:ss au-delà d'une heure */
	FText FormatTime(int32 Seconds)
	{
		Seconds = FMath::Max(Seconds, 0);
		const int32 Hours = Seconds / 3600;
		return FText::FromString(Hours > 0 ? FString::Printf(TEXT("%d:%02d:%02d"), Hours, (Seconds / 60) % 60, Seconds % 60) : FString::Printf(TEXT("%02d:%02d"), Seconds / 60, Seconds % 60));
	}
}

bool UYSFlowHUDWidget::Initialize()
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

void UYSFlowHUDWidget::BuildWidgetTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// chronomètre : tout en haut à droite
	ChronoLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Chrono"));
	YSFlowHUD::StyleText(ChronoLabel, ChronoFontSize, ChronoColor);
	ChronoLabel->SetVisibility(ESlateVisibility::Collapsed);
	UOverlaySlot* ChronoSlot = Root->AddChildToOverlay(ChronoLabel);
	ChronoSlot->SetHorizontalAlignment(HAlign_Right);
	ChronoSlot->SetVerticalAlignment(VAlign_Top);
	ChronoSlot->SetPadding(FMargin(0.0f, ChronoMargin.Y, ChronoMargin.X, 0.0f));

	// objectif : en haut au milieu
	ObjectiveColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ObjectiveColumn"));
	ObjectiveColumn->SetVisibility(ESlateVisibility::Collapsed);
	UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(ObjectiveColumn);
	ColumnSlot->SetHorizontalAlignment(HAlign_Center);
	ColumnSlot->SetVerticalAlignment(VAlign_Top);
	ColumnSlot->SetPadding(FMargin(0.0f, ObjectiveTopMargin, 0.0f, 0.0f));

	ObjectiveLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Objective"));
	YSFlowHUD::StyleText(ObjectiveLabel, ObjectiveFontSize, ObjectiveColor);
	ObjectiveColumn->AddChildToVerticalBox(ObjectiveLabel)->SetHorizontalAlignment(HAlign_Center);

	BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarBox"));
	BarBox->SetWidthOverride(BarSize.X);
	BarBox->SetHeightOverride(BarSize.Y);
	UVerticalBoxSlot* BarSlot = ObjectiveColumn->AddChildToVerticalBox(BarBox);
	BarSlot->SetHorizontalAlignment(HAlign_Center);
	BarSlot->SetPadding(FMargin(0.0f, BarSpacing, 0.0f, 0.0f));

	UOverlay* BarOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	BarBox->AddChild(BarOverlay);
	Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Bar"));
	FProgressBarStyle Style = Bar->GetWidgetStyle();
	Style.BackgroundImage.TintColor = FSlateColor(BarBackgroundColor);
	Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
	Bar->SetWidgetStyle(Style);
	// remplie depuis la gauche : elle se vide par la droite à mesure que le temps s'écoule
	Bar->SetBarFillType(EProgressBarFillType::LeftToRight);
	UOverlaySlot* FillSlot = BarOverlay->AddChildToOverlay(Bar);
	FillSlot->SetHorizontalAlignment(HAlign_Fill);
	FillSlot->SetVerticalAlignment(VAlign_Fill);

	RemainingLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Remaining"));
	YSFlowHUD::StyleText(RemainingLabel, RemainingFontSize, FLinearColor::White);
	RemainingLabel->SetVisibility(bShowRemainingTime ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	UOverlaySlot* RemainingSlot = BarOverlay->AddChildToOverlay(RemainingLabel);
	RemainingSlot->SetHorizontalAlignment(HAlign_Center);
	RemainingSlot->SetVerticalAlignment(VAlign_Center);

	OvertimeLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Overtime"));
	YSFlowHUD::StyleText(OvertimeLabel, OvertimeFontSize, OvertimeColor);
	OvertimeLabel->SetText(OvertimeText);
	OvertimeLabel->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBoxSlot* OvertimeSlot = ObjectiveColumn->AddChildToVerticalBox(OvertimeLabel);
	OvertimeSlot->SetHorizontalAlignment(HAlign_Center);
	OvertimeSlot->SetPadding(FMargin(0.0f, BarSpacing, 0.0f, 0.0f));

	BoardingLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Boarding"));
	YSFlowHUD::StyleText(BoardingLabel, BoardingFontSize, BoardingColor);
	BoardingLabel->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBoxSlot* BoardingSlot = ObjectiveColumn->AddChildToVerticalBox(BoardingLabel);
	BoardingSlot->SetHorizontalAlignment(HAlign_Center);
	BoardingSlot->SetPadding(FMargin(0.0f, BoardingSpacing, 0.0f, 0.0f));
}

void UYSFlowHUDWidget::Refresh(const FYSFlowStatus& Status, float PhaseRemaining, float MatchTime, bool bMatchStarted)
{
	if (!ChronoLabel)
	{
		return;
	}

	// chronomètre (arrêté à la victoire)
	ChronoLabel->SetVisibility(bMatchStarted ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	const int32 ChronoSeconds = FMath::FloorToInt32(Status.FinalMatchTime >= 0.0f ? Status.FinalMatchTime : MatchTime);
	if (ChronoSeconds != DisplayedChronoSeconds)
	{
		DisplayedChronoSeconds = ChronoSeconds;
		ChronoLabel->SetText(YSFlowHUD::FormatTime(ChronoSeconds));
	}

	const bool bShowObjective = bMatchStarted && Status.Objective != EYSFlowObjective::None;
	ObjectiveColumn->SetVisibility(bShowObjective ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bShowObjective)
	{
		return;
	}

	const bool bVictory = Status.Objective == EYSFlowObjective::Victory;
	if (Status.Objective != DisplayedObjective || Status.ObjectiveText.ToString() != DisplayedObjectiveText)
	{
		DisplayedObjective = Status.Objective;
		DisplayedObjectiveText = Status.ObjectiveText.ToString();
		ObjectiveLabel->SetText(Status.ObjectiveText);
		YSFlowHUD::StyleText(ObjectiveLabel, bVictory ? VictoryFontSize : ObjectiveFontSize, bVictory ? VictoryColor : ObjectiveColor);
	}

	// barre du temps restant, ou OVERTIME qui clignote
	const bool bBar = !bVictory && !Status.bOvertime && Status.PhaseDuration > 0.0f;
	BarBox->SetVisibility(bBar ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bBar)
	{
		Bar->SetPercent(FMath::Clamp(PhaseRemaining / Status.PhaseDuration, 0.0f, 1.0f));
		Bar->SetFillColorAndOpacity(Status.Objective == EYSFlowObjective::Evacuate ? EvacuationBarColor : Status.bNight ? NightBarColor : DayBarColor);
		const int32 RemainingSeconds = FMath::CeilToInt32(PhaseRemaining);
		if (RemainingSeconds != DisplayedRemainingSeconds)
		{
			DisplayedRemainingSeconds = RemainingSeconds;
			RemainingLabel->SetText(YSFlowHUD::FormatTime(RemainingSeconds));
		}
	}
	if (!bVictory && Status.bOvertime)
	{
		// temps réel : clignote aussi pendant la pause
		const double Phase = FMath::Frac(FPlatformTime::Seconds() * OvertimeBlinkFrequency);
		OvertimeLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
		OvertimeLabel->SetRenderOpacity(Phase < OvertimeVisibleRatio ? 1.0f : 0.0f);
	}
	else
	{
		OvertimeLabel->SetVisibility(ESlateVisibility::Collapsed);
	}

	// évacuation : joueurs à bord
	const bool bBoarding = Status.Objective == EYSFlowObjective::Evacuate;
	BoardingLabel->SetVisibility(bBoarding ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bBoarding && (Status.BoardedPlayers != DisplayedBoarded || Status.BoardingTotal != DisplayedBoardingTotal))
	{
		DisplayedBoarded = Status.BoardedPlayers;
		DisplayedBoardingTotal = Status.BoardingTotal;
		BoardingLabel->SetText(FText::Format(BoardingFormat, FText::AsNumber(Status.BoardedPlayers), FText::AsNumber(Status.BoardingTotal)));
	}
}
