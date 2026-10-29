#include "UI/YSUpgradeMenuWidget.h"
#include "Core/YSGamepad.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

void UYSUpgradeMenuButtonProxy::HandleClicked()
{
	if (Menu.IsValid())
	{
		Menu->NotifyButtonClicked(Index);
	}
}

bool UYSUpgradeMenuWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// arbre construit en C++ ; un Widget Blueprint enfant (WBP_UpgradeMenu) sert aux réglages : sa racine vide
	// éventuelle (Canvas Panel ajouté par défaut à la création) est remplacée
	const UPanelWidget* DesignerRoot = WidgetTree ? Cast<UPanelWidget>(WidgetTree->RootWidget) : nullptr;
	if (bSuperInitialized && WidgetTree && (!WidgetTree->RootWidget || (DesignerRoot && DesignerRoot->GetChildrenCount() == 0)))
	{
		BuildWidgetTree();
	}

	return bSuperInitialized;
}

UTextBlock* UYSUpgradeMenuWidget::MakeText(const FText& Text, int32 FontSize, const FLinearColor& Color, bool bBold, bool bWrap) const
{
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Label->SetText(Text);
	FSlateFontInfo Font = Label->GetFont();
	Font.Size = FontSize;
	Font.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
	Label->SetFont(Font);
	Label->SetColorAndOpacity(FSlateColor(Color));
	Label->SetJustification(ETextJustify::Center);
	Label->SetAutoWrapText(bWrap);
	return Label;
}

void UYSUpgradeMenuWidget::BuildWidgetTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Background"));
	Background->SetColorAndOpacity(BackgroundColor);
	UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Background);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Page"));
	UOverlaySlot* PageSlot = Root->AddChildToOverlay(Page);
	PageSlot->SetHorizontalAlignment(HAlign_Fill);
	PageSlot->SetVerticalAlignment(VAlign_Fill);

	// titre en haut au centre
	TitleText = MakeText(FText::GetEmpty(), TitleFontSize, FLinearColor::White, true, false);
	UVerticalBoxSlot* TitleSlot = Page->AddChildToVerticalBox(TitleText);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetPadding(TitlePadding);

	// cartes côte à côte, centrées dans l'espace restant
	CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Cards"));
	UVerticalBoxSlot* CardsSlot = Page->AddChildToVerticalBox(CardRow);
	CardsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CardsSlot->SetHorizontalAlignment(HAlign_Center);
	CardsSlot->SetVerticalAlignment(VAlign_Center);

	StatusText = MakeText(FText::GetEmpty(), StatusFontSize, FLinearColor(0.8f, 0.8f, 0.8f), false, false);
	UVerticalBoxSlot* StatusSlot = Page->AddChildToVerticalBox(StatusText);
	StatusSlot->SetHorizontalAlignment(HAlign_Center);
	StatusSlot->SetPadding(StatusPadding);

	// « Valider » puis « Relance » en bas, centrés ensemble
	UHorizontalBox* BottomRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BottomButtons"));
	UVerticalBoxSlot* BottomSlot = Page->AddChildToVerticalBox(BottomRow);
	BottomSlot->SetHorizontalAlignment(HAlign_Center);
	BottomSlot->SetPadding(RerollPadding);

	USizeBox* ValidateSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ValidateBox"));
	ValidateSize->SetWidthOverride(ValidateButtonSize.X);
	ValidateSize->SetHeightOverride(ValidateButtonSize.Y);
	ValidateButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Validate"));
	ValidateButton->SetBackgroundColor(ValidateDisabledColor);
	ValidateButton->AddChild(MakeText(NSLOCTEXT("YS", "UpgradeValidate", "Valider"), ValidateFontSize, FLinearColor::White, true, false));
	ValidateSize->AddChild(ValidateButton);
	ValidateProxy = NewObject<UYSUpgradeMenuButtonProxy>(this);
	ValidateProxy->Index = ValidateButtonIndex;
	ValidateProxy->Menu = this;
	ValidateButton->OnClicked.AddDynamic(ValidateProxy, &UYSUpgradeMenuButtonProxy::HandleClicked);
	BottomRow->AddChildToHorizontalBox(ValidateSize);

	USizeBox* RerollSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RerollRow"));
	RerollSize->SetWidthOverride(RerollButtonSize.X);
	RerollSize->SetHeightOverride(RerollButtonSize.Y);
	RerollRow = RerollSize;
	UButton* RerollButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Reroll"));
	RerollButton->SetBackgroundColor(RerollButtonColor);
	RerollButton->AddChild(MakeText(NSLOCTEXT("YS", "UpgradeReroll", "Relance"), RerollFontSize, FLinearColor::White, true, false));
	RerollSize->AddChild(RerollButton);
	RerollProxy = NewObject<UYSUpgradeMenuButtonProxy>(this);
	RerollProxy->Menu = this;
	RerollButton->OnClicked.AddDynamic(RerollProxy, &UYSUpgradeMenuButtonProxy::HandleClicked);
	UHorizontalBoxSlot* RerollSlot = BottomRow->AddChildToHorizontalBox(RerollSize);
	RerollSlot->SetPadding(FMargin(BottomButtonSpacing, 0.0f, 0.0f, 0.0f));
}

UWidget* UYSUpgradeMenuWidget::MakeCardContent(const FYSUpgradeCard& Card)
{
	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	// texte à la ligne : toute la largeur de la carte (centré par sa justification), sinon il se coupe trop tôt
	auto AddElement = [this, Content](UWidget* Element, float TopSpacing, bool bFillWidth = false)
	{
		UVerticalBoxSlot* ElementSlot = Content->AddChildToVerticalBox(Element);
		ElementSlot->SetHorizontalAlignment(bFillWidth ? HAlign_Fill : HAlign_Center);
		ElementSlot->SetPadding(FMargin(0.0f, Content->GetChildrenCount() > 1 ? TopSpacing : 0.0f, 0.0f, 0.0f));
	};

	// nom, rareté
	AddElement(MakeText(Card.Name, NameFontSize, Card.Color, true, true), CardElementSpacing, true);
	if (!Card.RarityText.IsEmpty())
	{
		AddElement(MakeText(Card.RarityText, RarityFontSize, Card.Color, false, false), CardElementSpacing);
	}

	// icône : carré de la couleur de la rareté, texture de l'upgrade par-dessus
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconBox->SetWidthOverride(IconSize);
	IconBox->SetHeightOverride(IconSize);
	UBorder* IconSquare = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	IconSquare->SetBrushColor(Card.Color);
	IconSquare->SetPadding(FMargin(IconFramePadding));
	IconBox->AddChild(IconSquare);
	if (Card.Icon)
	{
		UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		IconImage->SetBrushFromTexture(Card.Icon);
		IconSquare->SetContent(IconImage);
	}
	AddElement(IconBox, CardElementSpacing);

	// nombre de bonus, description
	if (!Card.StackText.IsEmpty())
	{
		AddElement(MakeText(Card.StackText, StackFontSize, StackColor, false, false), CardElementSpacing);
	}
	if (!Card.Description.IsEmpty())
	{
		AddElement(MakeText(Card.Description, DescriptionFontSize, DescriptionColor, false, true), CardElementSpacing, true);
	}

	// par valeur modifiée : son nom, puis sur une même ligne l'ancienne valeur rayée et la nouvelle plus grosse
	for (int32 Index = 0; Index < Card.StatChanges.Num(); ++Index)
	{
		const FYSUpgradeStatChange& Change = Card.StatChanges[Index];
		AddElement(MakeText(Change.Label, StatLabelFontSize, StackColor, false, true), Index == 0 ? StatsSpacing : StatLineSpacing, true);
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		// texte rayé : fine barre de la couleur du texte posée par-dessus, sur toute sa largeur
		UOverlay* Struck = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Struck->AddChildToOverlay(MakeText(Change.OldValue, OldValueFontSize, OldValueColor, false, false));
		USizeBox* StrikeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		StrikeBox->SetHeightOverride(FMath::Max(1.0f, OldValueFontSize / 8.0f));
		UImage* Strike = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Strike->SetColorAndOpacity(OldValueColor);
		StrikeBox->AddChild(Strike);
		UOverlaySlot* StrikeSlot = Struck->AddChildToOverlay(StrikeBox);
		StrikeSlot->SetHorizontalAlignment(HAlign_Fill);
		StrikeSlot->SetVerticalAlignment(VAlign_Center);
		UHorizontalBoxSlot* OldSlot = Line->AddChildToHorizontalBox(Struck);
		OldSlot->SetVerticalAlignment(VAlign_Center);

		UHorizontalBoxSlot* NewSlot = Line->AddChildToHorizontalBox(MakeText(Change.NewValue, NewValueFontSize, ValueColor, true, false));
		NewSlot->SetVerticalAlignment(VAlign_Center);
		NewSlot->SetPadding(FMargin(StatValueSpacing, 0.0f, 0.0f, 0.0f));

		AddElement(Line, StatLabelSpacing);
	}

	return Content;
}

void UYSUpgradeMenuWidget::SetCards(const TArray<FYSUpgradeCard>& Cards)
{
	if (!CardRow)
	{
		return;
	}

	CardRow->ClearChildren();
	CardButtons.Reset();
	ButtonProxies.Reset();
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(CardSize.X);
		Size->SetHeightOverride(CardSize.Y);

		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetBackgroundColor(CardColor);
		Size->AddChild(Button);

		// contenu en haut de la carte, avec ses marges
		UBorder* ContentBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		ContentBox->SetBrushColor(FLinearColor::Transparent);
		ContentBox->SetPadding(CardPadding);
		ContentBox->SetHorizontalAlignment(HAlign_Fill);
		ContentBox->SetVerticalAlignment(VAlign_Top);
		ContentBox->SetContent(MakeCardContent(Cards[Index]));
		Button->AddChild(ContentBox);
		if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(ContentBox->Slot))
		{
			ContentSlot->SetHorizontalAlignment(HAlign_Fill);
			ContentSlot->SetVerticalAlignment(VAlign_Fill);
			ContentSlot->SetPadding(FMargin(0.0f));
		}

		UYSUpgradeMenuButtonProxy* Proxy = NewObject<UYSUpgradeMenuButtonProxy>(this);
		Proxy->Index = Index;
		Proxy->Menu = this;
		Button->OnClicked.AddDynamic(Proxy, &UYSUpgradeMenuButtonProxy::HandleClicked);

		UHorizontalBoxSlot* CardSlot = CardRow->AddChildToHorizontalBox(Size);
		CardSlot->SetPadding(FMargin(CardSpacing * 0.5f, 0.0f));
		CardSlot->SetVerticalAlignment(VAlign_Center);

		CardButtons.Add(Button);
		ButtonProxies.Add(Proxy);
	}
	SelectedIndex = INDEX_NONE;
	ValidatedIndex = INDEX_NONE;
	RefreshSelection();
}

void UYSUpgradeMenuWidget::SetTitle(const FText& Title)
{
	if (TitleText)
	{
		TitleText->SetText(Title);
	}
}

void UYSUpgradeMenuWidget::SetSelectedIndex(int32 Index)
{
	SelectedIndex = Index;
	RefreshSelection();
}

void UYSUpgradeMenuWidget::SetValidatedIndex(int32 Index)
{
	if (ValidatedIndex == Index)
	{
		return;
	}
	ValidatedIndex = Index;
	RefreshSelection();
}

void UYSUpgradeMenuWidget::RefreshSelection()
{
	// carte validée en vert, carte sélectionnée (pas encore validée) en orange
	for (int32 CardIndex = 0; CardIndex < CardButtons.Num(); ++CardIndex)
	{
		const FLinearColor Color = CardIndex == ValidatedIndex ? ValidatedCardColor : CardIndex == SelectedIndex ? SelectedCardColor : CardColor;
		CardButtons[CardIndex]->SetBackgroundColor(Color);
	}
	// « Valider » actif seulement avec une carte sélectionnée qui n'est pas déjà celle validée
	if (ValidateButton)
	{
		const bool bCanValidate = SelectedIndex != INDEX_NONE && SelectedIndex != ValidatedIndex;
		ValidateButton->SetBackgroundColor(bCanValidate ? ValidateButtonColor : ValidateDisabledColor);
		ValidateButton->SetIsEnabled(bCanValidate);
	}
}

void UYSUpgradeMenuWidget::SetStatus(const FText& Status)
{
	if (StatusText)
	{
		StatusText->SetText(Status);
	}
}

void UYSUpgradeMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// manette : focus sur la première carte, navigation au stick gauche (A = choisir la carte, puis Valider)
	YSGamepad::UpdateMenuFocus(this, CardButtons.Num() > 0 ? CardButtons[0].Get() : nullptr, GamepadFocusScale);
}

void UYSUpgradeMenuWidget::SetRerollVisible(bool bVisible)
{
	if (RerollRow)
	{
		// retiré de la ligne : « Valider » reste seul, centré (la hauteur de la ligne ne change pas, les cartes ne bougent pas)
		RerollRow->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UYSUpgradeMenuWidget::NotifyButtonClicked(int32 Index)
{
	if (Index == INDEX_NONE)
	{
		OnRerollClicked.ExecuteIfBound();
		return;
	}
	if (Index == ValidateButtonIndex)
	{
		if (SelectedIndex != INDEX_NONE && SelectedIndex != ValidatedIndex)
		{
			OnOfferChosen.ExecuteIfBound(SelectedIndex);
		}
		return;
	}
	// carte : sélectionnée seulement, envoyée par « Valider »
	SetSelectedIndex(Index);
}
