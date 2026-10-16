#include "UI/YSPlayerHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace YSPlayerHUD
{
	void StyleText(UTextBlock* Text, int32 Size)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor::Black;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}

	FText FormatValue(float Value)
	{
		FNumberFormattingOptions Format;
		Format.MaximumFractionalDigits = 0;
		return FText::AsNumber(FMath::FloorToFloat(Value), &Format);
	}
}

bool UYSPlayerHUDWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// pas de Widget Blueprint : l'arbre est construit dès la création
	if (bSuperInitialized && WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}

	return bSuperInitialized;
}

UWidget* UYSPlayerHUDWidget::CreateBar(const FName& Name, const FVector2D& Size, const FLinearColor& FillColor, int32 FontSize, UProgressBar*& OutBar, UTextBlock*& OutLabel)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), Name);
	Box->SetWidthOverride(Size.X);
	Box->SetHeightOverride(Size.Y);

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Box->AddChild(Overlay);

	OutBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	FProgressBarStyle Style = OutBar->GetWidgetStyle();
	Style.BackgroundImage.TintColor = FSlateColor(BarBackgroundColor);
	Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
	OutBar->SetWidgetStyle(Style);
	OutBar->SetFillColorAndOpacity(FillColor);
	UOverlaySlot* BarSlot = Overlay->AddChildToOverlay(OutBar);
	BarSlot->SetHorizontalAlignment(HAlign_Fill);
	BarSlot->SetVerticalAlignment(VAlign_Fill);

	OutLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	YSPlayerHUD::StyleText(OutLabel, FontSize);
	UOverlaySlot* LabelSlot = Overlay->AddChildToOverlay(OutLabel);
	LabelSlot->SetHorizontalAlignment(HAlign_Center);
	LabelSlot->SetVerticalAlignment(VAlign_Center);

	return Box;
}

void UYSPlayerHUDWidget::BuildWidgetTree()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	WidgetTree->RootWidget = Column;

	UProgressBar* Bar = nullptr;
	UTextBlock* Label = nullptr;

	HealthGroup = CreateBar(TEXT("Health"), HealthBarSize, HealthColor, 14, Bar, Label);
	HealthBar = Bar;
	HealthLabel = Label;
	Column->AddChildToVerticalBox(HealthGroup);

	LevelLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Level"));
	YSPlayerHUD::StyleText(LevelLabel, 18);
	UVerticalBoxSlot* LevelSlot = Column->AddChildToVerticalBox(LevelLabel);
	LevelSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 2.0f));

	Column->AddChildToVerticalBox(CreateBar(TEXT("Experience"), ExperienceBarSize, ExperienceColor, 11, Bar, Label));
	ExperienceBar = Bar;
	ExperienceLabel = Label;
}

void UYSPlayerHUDWidget::SetHealth(bool bHasCharacter, float Health, float MaxHealth)
{
	if (!HealthGroup)
	{
		return;
	}

	HealthGroup->SetVisibility(bHasCharacter ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (bHasCharacter)
	{
		HealthBar->SetPercent(MaxHealth > 0.0f ? Health / MaxHealth : 0.0f);
		HealthLabel->SetText(FText::Format(NSLOCTEXT("YS", "HUDHealth", "{0} / {1}"), YSPlayerHUD::FormatValue(FMath::CeilToFloat(Health)), YSPlayerHUD::FormatValue(MaxHealth)));
	}
}

void UYSPlayerHUDWidget::SetLevel(int32 Level, float Experience, float ExperienceToNextLevel)
{
	if (!LevelLabel)
	{
		return;
	}

	LevelLabel->SetText(FText::Format(NSLOCTEXT("YS", "HUDLevel", "Lv {0}"), Level));
	ExperienceBar->SetPercent(ExperienceToNextLevel > 0.0f ? Experience / ExperienceToNextLevel : 0.0f);
	ExperienceLabel->SetText(FText::Format(NSLOCTEXT("YS", "HUDExperience", "{0} / {1} XP"), YSPlayerHUD::FormatValue(Experience), YSPlayerHUD::FormatValue(ExperienceToNextLevel)));
}
