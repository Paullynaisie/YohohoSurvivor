#include "UI/YSEliteHealthWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace YSEliteHealth
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
}

bool UYSEliteHealthWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// arbre construit en C++ ; la racine vide d'un Widget Blueprint enfant (WBP_EliteHealth) est remplacée
	const UPanelWidget* DesignerRoot = WidgetTree ? Cast<UPanelWidget>(WidgetTree->RootWidget) : nullptr;
	if (bSuperInitialized && WidgetTree && (!WidgetTree->RootWidget || (DesignerRoot && DesignerRoot->GetChildrenCount() == 0)))
	{
		BuildWidgetTree();
	}

	return bSuperInitialized;
}

void UYSEliteHealthWidget::BuildWidgetTree()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	WidgetTree->RootWidget = Column;

	NameLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Name"));
	YSEliteHealth::StyleText(NameLabel, 18);
	UVerticalBoxSlot* NameSlot = Column->AddChildToVerticalBox(NameLabel);
	NameSlot->SetHorizontalAlignment(HAlign_Right);
	NameSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));

	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarBox"));
	Box->SetWidthOverride(BarSize.X);
	Box->SetHeightOverride(BarSize.Y);
	Column->AddChildToVerticalBox(Box);

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Box->AddChild(Overlay);

	Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	FProgressBarStyle Style = Bar->GetWidgetStyle();
	Style.BackgroundImage.TintColor = FSlateColor(BarBackgroundColor);
	Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
	Bar->SetWidgetStyle(Style);
	Bar->SetFillColorAndOpacity(BarColor);
	// se vide vers la droite (symétrique de la barre du joueur)
	Bar->SetBarFillType(EProgressBarFillType::RightToLeft);
	UOverlaySlot* BarSlot = Overlay->AddChildToOverlay(Bar);
	BarSlot->SetHorizontalAlignment(HAlign_Fill);
	BarSlot->SetVerticalAlignment(VAlign_Fill);

	ValueLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	YSEliteHealth::StyleText(ValueLabel, 14);
	UOverlaySlot* LabelSlot = Overlay->AddChildToOverlay(ValueLabel);
	LabelSlot->SetHorizontalAlignment(HAlign_Center);
	LabelSlot->SetVerticalAlignment(VAlign_Center);

	// Boss armor : bouclier puis rage, sous les PV
	USizeBox* NewShieldBox = nullptr;
	UTextBlock* NewShieldLabel = nullptr;
	ShieldBar = AddBossBar(Column, ShieldBarSize, ShieldBarColor, NewShieldBox, NewShieldLabel);
	ShieldBox = NewShieldBox;
	ShieldLabel = NewShieldLabel;
	USizeBox* NewRageBox = nullptr;
	UTextBlock* NewRageLabel = nullptr;
	RageBar = AddBossBar(Column, RageBarSize, RageBarColor, NewRageBox, NewRageLabel);
	RageBox = NewRageBox;
	RageLabel = NewRageLabel;
}

UProgressBar* UYSEliteHealthWidget::AddBossBar(UVerticalBox* Column, const FVector2D& Size, const FLinearColor& Color, USizeBox*& OutBox, UTextBlock*& OutLabel)
{
	OutBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	OutBox->SetWidthOverride(Size.X);
	OutBox->SetHeightOverride(Size.Y);
	OutBox->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBoxSlot* BoxSlot = Column->AddChildToVerticalBox(OutBox);
	BoxSlot->SetHorizontalAlignment(HAlign_Right);
	BoxSlot->SetPadding(FMargin(0.0f, BossBarSpacing, 0.0f, 0.0f));

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	OutBox->AddChild(Overlay);

	UProgressBar* NewBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	FProgressBarStyle Style = NewBar->GetWidgetStyle();
	Style.BackgroundImage.TintColor = FSlateColor(BarBackgroundColor);
	Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
	NewBar->SetWidgetStyle(Style);
	NewBar->SetFillColorAndOpacity(Color);
	NewBar->SetBarFillType(EProgressBarFillType::RightToLeft);
	UOverlaySlot* BarSlot = Overlay->AddChildToOverlay(NewBar);
	BarSlot->SetHorizontalAlignment(HAlign_Fill);
	BarSlot->SetVerticalAlignment(VAlign_Fill);

	OutLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	YSEliteHealth::StyleText(OutLabel, BossBarFontSize);
	UOverlaySlot* TextSlot = Overlay->AddChildToOverlay(OutLabel);
	TextSlot->SetHorizontalAlignment(HAlign_Center);
	TextSlot->SetVerticalAlignment(VAlign_Center);
	return NewBar;
}

void UYSEliteHealthWidget::SetBossState(float Shield, float MaxShield, float Rage, float MaxRage)
{
	if (!ShieldBar || !RageBar)
	{
		return;
	}
	FNumberFormattingOptions Format;
	Format.MaximumFractionalDigits = 0;

	const bool bShield = Shield > 0.0f && MaxShield > 0.0f;
	ShieldBox->SetVisibility(bShield ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bShield)
	{
		ShieldBar->SetPercent(Shield / MaxShield);
		ShieldLabel->SetText(FText::Format(NSLOCTEXT("YS", "EliteShield", "Bouclier {0} / {1}"), FText::AsNumber(FMath::CeilToFloat(Shield), &Format), FText::AsNumber(MaxShield, &Format)));
	}

	// rage : pas sous bouclier (elle ne monte pas)
	const bool bRage = MaxRage > 0.0f && !bShield;
	RageBox->SetVisibility(bRage ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bRage)
	{
		RageBar->SetPercent(Rage / MaxRage);
		RageLabel->SetText(FText::Format(NSLOCTEXT("YS", "EliteRage", "Rage {0} / {1}"), FText::AsNumber(FMath::FloorToFloat(Rage), &Format), FText::AsNumber(MaxRage, &Format)));
	}
}

void UYSEliteHealthWidget::SetElite(const FText& Name, float Health, float MaxHealth)
{
	if (!Bar)
	{
		return;
	}

	FNumberFormattingOptions Format;
	Format.MaximumFractionalDigits = 0;
	NameLabel->SetText(Name);
	Bar->SetPercent(MaxHealth > 0.0f ? Health / MaxHealth : 0.0f);
	ValueLabel->SetText(FText::Format(NSLOCTEXT("YS", "EliteHealth", "{0} / {1}"), FText::AsNumber(FMath::CeilToFloat(Health), &Format), FText::AsNumber(MaxHealth, &Format)));
}
