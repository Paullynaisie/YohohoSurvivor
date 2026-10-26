#include "UI/YSAbilityIconWidget.h"
#include "Abilities/YSAbilityComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

bool UYSAbilityIconWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// pas de Widget Blueprint : l'arbre est construit dès la création, pour pouvoir être rempli avant l'affichage
	if (bSuperInitialized && WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}

	return bSuperInitialized;
}

void UYSAbilityIconWidget::BuildWidgetTree()
{
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Root"));
	Root->SetWidthOverride(IconSize);
	Root->SetHeightOverride(IconSize);
	WidgetTree->RootWidget = Root;

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay"));
	Root->AddChild(Overlay);

	auto AddLayer = [Overlay](UWidget* Widget, EHorizontalAlignment HAlign, EVerticalAlignment VAlign)
	{
		UOverlaySlot* OverlaySlot = Overlay->AddChildToOverlay(Widget);
		OverlaySlot->SetHorizontalAlignment(HAlign);
		OverlaySlot->SetVerticalAlignment(VAlign);
	};

	IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Icon"));
	AddLayer(IconImage, HAlign_Fill, VAlign_Fill);

	// voile de recharge : barre transparente qui se remplit du bas vers le haut
	CooldownBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Cooldown"));
	FProgressBarStyle BarStyle = CooldownBar->GetWidgetStyle();
	BarStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor::Transparent);
	BarStyle.FillImage.TintColor = FSlateColor(FLinearColor::White);
	CooldownBar->SetWidgetStyle(BarStyle);
	CooldownBar->SetBarFillType(EProgressBarFillType::BottomToTop);
	CooldownBar->SetFillColorAndOpacity(CooldownColor);
	CooldownBar->SetPercent(0.0f);
	AddLayer(CooldownBar, HAlign_Fill, VAlign_Fill);

	CooldownText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CooldownText"));
	FSlateFontInfo Font = CooldownText->GetFont();
	Font.Size = FontSize;
	CooldownText->SetFont(Font);
	CooldownText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	CooldownText->SetShadowOffset(FVector2D(2.0f, 2.0f));
	CooldownText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
	AddLayer(CooldownText, HAlign_Center, VAlign_Center);
}

void UYSAbilityIconWidget::Refresh(const UYSAbilityComponent* Ability, bool bBlocked)
{
	if (!IconImage)
	{
		return;
	}

	if (!Ability)
	{
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (Ability->Icon && IconImage->GetBrush().GetResourceObject() != Ability->Icon)
	{
		IconImage->SetBrushFromTexture(Ability->Icon);
	}
	IconImage->SetColorAndOpacity(Ability->IconTint);

	const float Remaining = Ability->GetCooldownRemaining();
	const float Duration = Ability->GetActivationLockDuration();
	// rechargée mais impossible à lancer : voile plein (Blocked Color)
	const bool bShowBlocked = bBlocked && Remaining <= 0.0f;
	CooldownBar->SetFillColorAndOpacity(bShowBlocked ? BlockedColor : CooldownColor);
	CooldownBar->SetPercent(bShowBlocked ? 1.0f : Duration > 0.0f ? Remaining / Duration : 0.0f);
	CooldownText->SetText(Remaining > 0.0f ? FText::AsNumber(FMath::CeilToInt(Remaining)) : FText::GetEmpty());
}
