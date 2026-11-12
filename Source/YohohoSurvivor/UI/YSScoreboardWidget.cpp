#include "UI/YSScoreboardWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

namespace YSScoreboard
{
	/** 1234567 -> « 1 234 567 » */
	FText FormatNumber(double Value)
	{
		const int64 Rounded = FMath::RoundToInt64(FMath::Max(Value, 0.0));
		FString Digits = FString::Printf(TEXT("%lld"), Rounded);
		for (int32 Index = Digits.Len() - 3; Index > 0; Index -= 3)
		{
			Digits.InsertAt(Index, TEXT(' '));
		}
		return FText::FromString(Digits);
	}

	FText FormatDuration(float Seconds)
	{
		const int32 Total = FMath::Max(FMath::FloorToInt32(Seconds), 0);
		const int32 Hours = Total / 3600;
		return FText::FromString(Hours > 0 ? FString::Printf(TEXT("%d:%02d:%02d"), Hours, (Total / 60) % 60, Total % 60) : FString::Printf(TEXT("%02d:%02d"), Total / 60, Total % 60));
	}

	/** Part de l'équipe en % (arrondie) */
	FText FormatShare(double Mine, double Team)
	{
		return FText::AsNumber(Team > 0.0 ? FMath::RoundToInt32(100.0 * Mine / Team) : 0);
	}
}

bool UYSScoreboardWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// arbre construit en C++ ; la racine vide d'un Widget Blueprint enfant (WBP_Scoreboard) est remplacée
	const UPanelWidget* DesignerRoot = WidgetTree ? Cast<UPanelWidget>(WidgetTree->RootWidget) : nullptr;
	if (bSuperInitialized && WidgetTree && (!WidgetTree->RootWidget || (DesignerRoot && DesignerRoot->GetChildrenCount() == 0)))
	{
		BuildWidgetTree();
	}
	return bSuperInitialized;
}

UTextBlock* UYSScoreboardWidget::MakeText(const FText& Text, int32 FontSize, const FLinearColor& Color, FName Name)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	Block->SetText(Text);
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = FontSize;
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetShadowOffset(FVector2D(2.0f, 2.0f));
	Block->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
	return Block;
}

void UYSScoreboardWidget::BuildWidgetTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Background"));
	Background->SetColorAndOpacity(BackgroundColor);
	UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Background);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Main"));
	UOverlaySlot* MainSlot = Root->AddChildToOverlay(Main);
	MainSlot->SetHorizontalAlignment(HAlign_Fill);
	MainSlot->SetVerticalAlignment(VAlign_Fill);
	MainSlot->SetPadding(ScreenPadding);

	// haut au milieu : résultat puis durée
	TitleText = MakeText(VictoryTitle, TitleFontSize, VictoryColor, TEXT("Title"));
	Main->AddChildToVerticalBox(TitleText)->SetHorizontalAlignment(HAlign_Center);
	DurationText = MakeText(FText::GetEmpty(), DurationFontSize, DurationColor, TEXT("Duration"));
	Main->AddChildToVerticalBox(DurationText)->SetHorizontalAlignment(HAlign_Center);

	// deux colonnes : upgrades à gauche, statistiques à droite
	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Columns"));
	UVerticalBoxSlot* ColumnsSlot = Main->AddChildToVerticalBox(Columns);
	ColumnsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ColumnsSlot->SetPadding(FMargin(0.0f, ColumnsTopSpacing, 0.0f, 0.0f));

	const auto MakeColumn = [this, Columns](float Width, const FText& Header, FName Name, EHorizontalAlignment Align) -> UVerticalBox*
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *(Name.ToString() + TEXT("Size")));
		Size->SetWidthOverride(Width);
		UHorizontalBoxSlot* SizeSlot = Columns->AddChildToHorizontalBox(Size);
		SizeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SizeSlot->SetHorizontalAlignment(Align);
		SizeSlot->SetVerticalAlignment(VAlign_Fill);
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *(Name.ToString() + TEXT("Panel")));
		Panel->SetBrushColor(PanelColor);
		Panel->SetPadding(PanelPadding);
		Size->AddChild(Panel);
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), Name);
		Panel->SetContent(Column);
		UVerticalBoxSlot* HeaderSlot = Column->AddChildToVerticalBox(MakeText(Header, HeaderFontSize, HeaderColor));
		HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		return Column;
	};

	UVerticalBox* Left = MakeColumn(UpgradesWidth, UpgradesHeader, TEXT("UpgradesColumn"), HAlign_Left);
	UpgradesList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("UpgradesList"));
	UpgradesList->SetAlwaysShowScrollbar(bAlwaysShowScrollbar);
	UpgradesList->SetScrollbarThickness(ScrollbarThickness);
	UpgradesList->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	Left->AddChildToVerticalBox(UpgradesList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UVerticalBox* Right = MakeColumn(StatsWidth, StatsHeader, TEXT("StatsColumn"), HAlign_Right);
	StatsList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatsList"));
	Right->AddChildToVerticalBox(StatsList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// bas au milieu : retour au menu
	USizeBox* ReturnSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ReturnBox"));
	ReturnSize->SetWidthOverride(ReturnButtonSize.X);
	ReturnSize->SetHeightOverride(ReturnButtonSize.Y);
	ReturnButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Return"));
	ReturnButton->SetBackgroundColor(ReturnButtonColor);
	ReturnButton->OnClicked.AddDynamic(this, &UYSScoreboardWidget::HandleReturnClicked);
	ReturnButton->AddChild(MakeText(ReturnText, ReturnFontSize, FLinearColor::White, TEXT("ReturnLabel")));
	ReturnSize->AddChild(ReturnButton);
	UVerticalBoxSlot* ReturnSlot = Main->AddChildToVerticalBox(ReturnSize);
	ReturnSlot->SetHorizontalAlignment(HAlign_Center);
	ReturnSlot->SetPadding(FMargin(0.0f, ReturnTopSpacing, 0.0f, 0.0f));
}

void UYSScoreboardWidget::HandleReturnClicked()
{
	OnReturnClicked.ExecuteIfBound();
}

void UYSScoreboardWidget::AddUpgradeRow(UTexture2D* Icon, const FText& Name, const FLinearColor& Color, bool bShowIcon)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (bShowIcon)
	{
		USizeBox* IconSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IconSizeBox->SetWidthOverride(IconSize);
		IconSizeBox->SetHeightOverride(IconSize);
		UOverlay* IconOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* Square = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Square->SetColorAndOpacity(IconBackgroundColor);
		UOverlaySlot* SquareSlot = IconOverlay->AddChildToOverlay(Square);
		SquareSlot->SetHorizontalAlignment(HAlign_Fill);
		SquareSlot->SetVerticalAlignment(VAlign_Fill);
		if (Icon)
		{
			UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			IconImage->SetBrushFromTexture(Icon);
			UOverlaySlot* IconSlot = IconOverlay->AddChildToOverlay(IconImage);
			IconSlot->SetHorizontalAlignment(HAlign_Fill);
			IconSlot->SetVerticalAlignment(VAlign_Fill);
		}
		IconSizeBox->AddChild(IconOverlay);
		UHorizontalBoxSlot* IconRowSlot = Row->AddChildToHorizontalBox(IconSizeBox);
		IconRowSlot->SetVerticalAlignment(VAlign_Center);
		IconRowSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
	}
	UTextBlock* Label = MakeText(Name, bShowIcon ? UpgradeFontSize : ShardFontSize, Color);
	Label->SetAutoWrapText(true);
	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(UpgradesList->AddChild(Row));
	if (RowSlot)
	{
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, ScrollbarThickness.X + 8.0f, RowSpacing));
	}
}

void UYSScoreboardWidget::AddStat(const FText& Label, const FText& Value)
{
	StatsList->AddChildToVerticalBox(MakeText(Label, StatLabelFontSize, StatLabelColor));
	UVerticalBoxSlot* ValueSlot = StatsList->AddChildToVerticalBox(MakeText(Value, StatValueFontSize, StatValueColor));
	ValueSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, StatSpacing));
}

void UYSScoreboardWidget::SetResult(bool bVictory, float DurationSeconds, const FYSScoreUpgrades& Upgrades, const FYSMatchStats& MyStats, const TArray<FYSMatchStats>& TeamStats)
{
	if (!TitleText)
	{
		return;
	}
	TitleText->SetText(bVictory ? VictoryTitle : GameOverTitle);
	TitleText->SetColorAndOpacity(FSlateColor(bVictory ? VictoryColor : GameOverColor));
	DurationText->SetText(FText::Format(DurationFormat, YSScoreboard::FormatDuration(DurationSeconds)));

	// gauche : classe, upgrade d'arme, golden (une ligne vide si rien), puis les shards de la plus haute à la plus basse
	UpgradesList->ClearChildren();
	const auto AddGroup = [this](const TArray<FYSScoreUpgrade>& Lines, const FText& EmptyText, const FLinearColor& Color)
	{
		for (const FYSScoreUpgrade& Line : Lines)
		{
			AddUpgradeRow(Line.Icon, Line.Name, Color, true);
		}
		if (Lines.IsEmpty())
		{
			AddUpgradeRow(nullptr, EmptyText, EmptyColor, true);
		}
	};
	AddGroup(Upgrades.ClassUpgrades, NoClassText, UpgradeColor);
	AddGroup(Upgrades.WeaponUpgrades, NoWeaponText, UpgradeColor);
	AddGroup(Upgrades.GoldenUpgrades, NoGoldenText, GoldenColor);
	for (const FYSScoreShard& Shard : Upgrades.Shards)
	{
		AddUpgradeRow(nullptr, FText::Format(ShardFormat, Shard.Name, FText::AsNumber(Shard.Level), FText::AsNumber(Shard.MaxLevel)), ShardColor, false);
	}

	// droite : valeur du joueur et part de l'équipe (somme de tous les joueurs)
	FYSMatchStats Team;
	for (const FYSMatchStats& Stats : TeamStats)
	{
		Team.DamageDealt += Stats.DamageDealt;
		Team.Kills += Stats.Kills;
		Team.DamageTaken += Stats.DamageTaken;
		Team.ArmorReduced += Stats.ArmorReduced;
		Team.Revives += Stats.Revives;
		Team.ItemsDelivered += Stats.ItemsDelivered;
		Team.Deaths += Stats.Deaths;
	}
	const auto Shared = [this](const FText& Unit, double Mine, double TeamTotal)
	{
		return FText::Format(TeamShareFormat, FText::Format(Unit, YSScoreboard::FormatNumber(Mine)), YSScoreboard::FormatShare(Mine, TeamTotal));
	};
	StatsList->ClearChildren();
	AddStat(DamageDealtLabel, Shared(DamageDealtUnit, MyStats.DamageDealt, Team.DamageDealt));
	AddStat(BiggestHitLabel, FText::Format(BiggestHitUnit, YSScoreboard::FormatNumber(MyStats.BiggestHit)));
	AddStat(KillsLabel, Shared(KillsUnit, MyStats.Kills, Team.Kills));
	AddStat(DamageTakenLabel, Shared(DamageTakenUnit, MyStats.DamageTaken, Team.DamageTaken));
	AddStat(ArmorLabel, Shared(ArmorUnit, MyStats.ArmorReduced, Team.ArmorReduced));
	AddStat(RevivesLabel, Shared(RevivesUnit, MyStats.Revives, Team.Revives));
	AddStat(ItemsLabel, Shared(ItemsUnit, MyStats.ItemsDelivered, Team.ItemsDelivered));
	AddStat(DeathsLabel, Shared(DeathsUnit, MyStats.Deaths, Team.Deaths));
}
