#include "UI/YSPlayerHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "UI/YSPlayerIconWidget.h"

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
	// infos du joueur (icône + colonne de barres), puis la liste des autres joueurs
	UVerticalBox* Outer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Outer"));
	WidgetTree->RootWidget = Outer;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SelfRow"));
	Outer->AddChildToVerticalBox(Row);

	SelfIcon = CreateWidget<UYSPlayerIconWidget>(this, UYSPlayerIconWidget::StaticClass());
	UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(SelfIcon);
	IconSlot->SetVerticalAlignment(VAlign_Center);
	IconSlot->SetPadding(FMargin(0.0f, 0.0f, SelfIconSpacing, 0.0f));

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Row->AddChildToHorizontalBox(Column);

	// minions du joueur (Captain) : entre ses infos et les autres joueurs
	MinionsList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Minions"));
	UVerticalBoxSlot* MinionsSlot = Outer->AddChildToVerticalBox(MinionsList);
	MinionsSlot->SetPadding(FMargin(0.0f, MinionsTopSpacing, 0.0f, 0.0f));
	MinionsList->SetVisibility(ESlateVisibility::Collapsed);

	AlliesList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Allies"));
	UVerticalBoxSlot* AlliesSlot = Outer->AddChildToVerticalBox(AlliesList);
	AlliesSlot->SetPadding(FMargin(0.0f, AlliesTopSpacing, 0.0f, 0.0f));

	UProgressBar* Bar = nullptr;
	UTextBlock* Label = nullptr;

	// pseudo du joueur au-dessus de sa barre de PV
	NameLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlayerName"));
	YSPlayerHUD::StyleText(NameLabel, 18);
	UVerticalBoxSlot* NameSlot = Column->AddChildToVerticalBox(NameLabel);
	NameSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));

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

void UYSPlayerHUDWidget::SetPlayerName(const FText& Name)
{
	if (NameLabel && !NameLabel->GetText().EqualTo(Name))
	{
		NameLabel->SetText(Name);
	}
}

void UYSPlayerHUDWidget::SetSelfIcon(const FLinearColor& Color, UTexture2D* Face, float ViewportWidth)
{
	if (SelfIcon)
	{
		SelfIcon->SetIconSize(FMath::Max(16.0f, ViewportWidth * SelfIconScreenRatio));
		SelfIcon->SetIcon(Color, Face);
	}
}

void UYSPlayerHUDWidget::SetAllies(const TArray<FYSHUDPlayerInfo>& Allies)
{
	if (!AlliesList)
	{
		return;
	}

	// une ligne par joueur : icône, puis pseudo au-dessus d'une petite barre de PV
	if (AllyIcons.Num() != Allies.Num())
	{
		AlliesList->ClearChildren();
		AllyIcons.Reset();
		AllyNames.Reset();
		AllyBars.Reset();

		for (int32 Index = 0; Index < Allies.Num(); ++Index)
		{
			UHorizontalBox* AllyRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UVerticalBoxSlot* RowSlot = AlliesList->AddChildToVerticalBox(AllyRow);
			RowSlot->SetPadding(FMargin(0.0f, Index > 0 ? AllySpacing : 0.0f, 0.0f, 0.0f));

			UYSPlayerIconWidget* Icon = CreateWidget<UYSPlayerIconWidget>(this, UYSPlayerIconWidget::StaticClass());
			Icon->SetIconSize(AllyIconSize);
			UHorizontalBoxSlot* IconSlot = AllyRow->AddChildToHorizontalBox(Icon);
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

			UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			AllyRow->AddChildToHorizontalBox(Info)->SetVerticalAlignment(VAlign_Center);

			UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			YSPlayerHUD::StyleText(Name, 14);
			Info->AddChildToVerticalBox(Name)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));

			UWidget* BarBox = nullptr;
			UProgressBar* Bar = CreateSmallBar(AllyBarSize, BarBox);
			Info->AddChildToVerticalBox(BarBox);

			AllyIcons.Add(Icon);
			AllyNames.Add(Name);
			AllyBars.Add(Bar);
		}
	}

	for (int32 Index = 0; Index < Allies.Num(); ++Index)
	{
		const FYSHUDPlayerInfo& Ally = Allies[Index];
		AllyIcons[Index]->SetIcon(Ally.Color, Ally.Face);
		// mort (ou pas encore en jeu) : icône et pseudo estompés
		AllyIcons[Index]->SetRenderOpacity(Ally.bAlive ? 1.0f : 0.4f);
		if (!AllyNames[Index]->GetText().EqualTo(Ally.Name))
		{
			AllyNames[Index]->SetText(Ally.Name);
		}
		AllyNames[Index]->SetRenderOpacity(Ally.bAlive ? 1.0f : 0.4f);
		AllyBars[Index]->SetPercent(Ally.bAlive && Ally.MaxHealth > 0.0f ? Ally.Health / Ally.MaxHealth : 0.0f);
	}
}

UProgressBar* UYSPlayerHUDWidget::CreateSmallBar(const FVector2D& Size, UWidget*& OutContainer)
{
	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BarBox->SetWidthOverride(Size.X);
	BarBox->SetHeightOverride(Size.Y);
	UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	FProgressBarStyle Style = Bar->GetWidgetStyle();
	Style.BackgroundImage.TintColor = FSlateColor(BarBackgroundColor);
	Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
	Bar->SetWidgetStyle(Style);
	Bar->SetFillColorAndOpacity(HealthColor);
	BarBox->AddChild(Bar);
	OutContainer = BarBox;
	return Bar;
}

void UYSPlayerHUDWidget::SetMinions(const TArray<FYSHUDPlayerInfo>& Minions)
{
	if (!MinionsList)
	{
		return;
	}

	const ESlateVisibility ListVisibility = Minions.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (MinionsList->GetVisibility() != ListVisibility)
	{
		MinionsList->SetVisibility(ListVisibility);
	}

	// une ligne par minion : petite icône (couleur du Captain + visage du minion), barre de PV avec « PV / max »
	if (MinionIcons.Num() != Minions.Num())
	{
		MinionsList->ClearChildren();
		MinionIcons.Reset();
		MinionBars.Reset();
		MinionLabels.Reset();

		for (int32 Index = 0; Index < Minions.Num(); ++Index)
		{
			UHorizontalBox* MinionRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UVerticalBoxSlot* RowSlot = MinionsList->AddChildToVerticalBox(MinionRow);
			RowSlot->SetPadding(FMargin(0.0f, Index > 0 ? MinionSpacing : 0.0f, 0.0f, 0.0f));

			UYSPlayerIconWidget* Icon = CreateWidget<UYSPlayerIconWidget>(this, UYSPlayerIconWidget::StaticClass());
			Icon->SetIconSize(MinionIconSize);
			UHorizontalBoxSlot* IconSlot = MinionRow->AddChildToHorizontalBox(Icon);
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));

			USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			BarBox->SetWidthOverride(MinionBarSize.X);
			BarBox->SetHeightOverride(MinionBarSize.Y);
			UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
			BarBox->AddChild(Overlay);
			UWidget* InnerBox = nullptr;
			UProgressBar* Bar = CreateSmallBar(MinionBarSize, InnerBox);
			UOverlaySlot* BarSlot = Overlay->AddChildToOverlay(InnerBox);
			BarSlot->SetHorizontalAlignment(HAlign_Fill);
			BarSlot->SetVerticalAlignment(VAlign_Fill);
			UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			YSPlayerHUD::StyleText(Label, FMath::Max(7, FMath::RoundToInt(MinionBarSize.Y * 0.75f)));
			UOverlaySlot* LabelSlot = Overlay->AddChildToOverlay(Label);
			LabelSlot->SetHorizontalAlignment(HAlign_Center);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
			MinionRow->AddChildToHorizontalBox(BarBox)->SetVerticalAlignment(VAlign_Center);

			MinionIcons.Add(Icon);
			MinionBars.Add(Bar);
			MinionLabels.Add(Label);
		}
	}

	for (int32 Index = 0; Index < Minions.Num(); ++Index)
	{
		const FYSHUDPlayerInfo& Minion = Minions[Index];
		MinionIcons[Index]->SetIcon(Minion.Color, Minion.Face);
		MinionBars[Index]->SetPercent(Minion.MaxHealth > 0.0f ? Minion.Health / Minion.MaxHealth : 0.0f);
		MinionLabels[Index]->SetText(FText::Format(NSLOCTEXT("YS", "HUDMinionHealth", "{0} / {1}"),
			YSPlayerHUD::FormatValue(FMath::CeilToFloat(Minion.Health)), YSPlayerHUD::FormatValue(Minion.MaxHealth)));
	}
}
