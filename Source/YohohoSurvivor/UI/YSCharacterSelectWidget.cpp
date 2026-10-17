#include "UI/YSCharacterSelectWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
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

namespace YSCharacterSelect
{
	void SetFontSize(UTextBlock* Text, int32 Size)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}

	UTextBlock* MakeLabel(UWidgetTree* Tree, const FText& Text, int32 Size)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Label->SetText(Text);
		SetFontSize(Label, Size);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		return Label;
	}
}

void UYSCharacterSelectButtonProxy::HandleClicked()
{
	if (Menu.IsValid())
	{
		Menu->NotifyButtonClicked(Group, Index);
	}
}

bool UYSCharacterSelectWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// arbre construit en C++ ; un Widget Blueprint enfant (WBP_CharacterSelect) sert aux réglages : sa racine vide
	// éventuelle (Canvas Panel ajouté par défaut à la création) est remplacée
	const UPanelWidget* DesignerRoot = WidgetTree ? Cast<UPanelWidget>(WidgetTree->RootWidget) : nullptr;
	if (bSuperInitialized && WidgetTree && (!WidgetTree->RootWidget || (DesignerRoot && DesignerRoot->GetChildrenCount() == 0)))
	{
		BuildWidgetTree();
	}

	return bSuperInitialized;
}

void UYSCharacterSelectWidget::BuildWidgetTree()
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

	// pseudo : en haut au centre
	USizeBox* NicknameSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("NicknameRow"));
	NicknameSize->SetWidthOverride(NicknameWidth);
	NicknameRow = NicknameSize;
	NicknameBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Nickname"));
	NicknameBox->SetHintText(NSLOCTEXT("YS", "NicknameHint", "Ton pseudo"));
	NicknameBox->SetJustification(ETextJustify::Center);
	NicknameBox->OnTextCommitted.AddDynamic(this, &UYSCharacterSelectWidget::HandleNicknameCommitted);
	FEditableTextBoxStyle NicknameStyle = NicknameBox->WidgetStyle;
	NicknameStyle.TextStyle.Font.Size = NicknameFontSize;
	NicknameStyle.SetForegroundColor(FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f)));
	NicknameBox->WidgetStyle = NicknameStyle;
	NicknameSize->AddChild(NicknameBox);
	UVerticalBoxSlot* NicknameSlot = Page->AddChildToVerticalBox(NicknameSize);
	NicknameSlot->SetHorizontalAlignment(HAlign_Center);
	NicknameSlot->SetPadding(NicknamePadding);

	// 3 colonnes de même largeur
	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Columns"));
	UVerticalBoxSlot* ColumnsSlot = Page->AddChildToVerticalBox(Columns);
	ColumnsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	auto AddColumn = [this, Columns](UWidget* Content, float Weight, const FMargin& ColumnPadding)
	{
		UHorizontalBoxSlot* ColumnSlot = Columns->AddChildToHorizontalBox(Content);
		FSlateChildSize Size(ESlateSizeRule::Fill);
		Size.Value = Weight;
		ColumnSlot->SetSize(Size);
		ColumnSlot->SetPadding(ColumnPadding);
		ColumnSlot->SetHorizontalAlignment(HAlign_Center);
		ColumnSlot->SetVerticalAlignment(ColumnVerticalAlignment);
	};

	// gauche : choix principal (personnage / upgrade)
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	AddColumn(Column, LeftColumnWeight, LeftColumnPadding);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	TitleText = Title;
	Title->SetText(NSLOCTEXT("YS", "CharacterSelectTitle", "Choisis ton Mouss"));
	YSCharacterSelect::SetFontSize(Title, TitleFontSize);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, TitleSpacing));

	ButtonList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Buttons"));
	UVerticalBoxSlot* ListSlot = Column->AddChildToVerticalBox(ButtonList);
	ListSlot->SetHorizontalAlignment(HAlign_Center);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Status"));
	YSCharacterSelect::SetFontSize(StatusText, StatusFontSize);
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f)));
	UVerticalBoxSlot* StatusSlot = Column->AddChildToVerticalBox(StatusText);
	StatusSlot->SetHorizontalAlignment(HAlign_Center);
	StatusSlot->SetPadding(FMargin(0.0f, StatusSpacing, 0.0f, 0.0f));

	// centre : couleur et visage
	UVerticalBox* Customization = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Customization"));
	CustomizationColumn = Customization;
	AddColumn(Customization, CenterColumnWeight, CenterColumnPadding);

	UVerticalBoxSlot* ColorTitleSlot = Customization->AddChildToVerticalBox(YSCharacterSelect::MakeLabel(WidgetTree, NSLOCTEXT("YS", "ColorTitle", "Choisis ta couleur"), SectionTitleFontSize));
	ColorTitleSlot->SetHorizontalAlignment(HAlign_Center);
	ColorTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, SectionTitleSpacing));
	ColorRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Colors"));
	Customization->AddChildToVerticalBox(ColorRow)->SetHorizontalAlignment(HAlign_Center);

	UVerticalBoxSlot* FaceTitleSlot = Customization->AddChildToVerticalBox(YSCharacterSelect::MakeLabel(WidgetTree, NSLOCTEXT("YS", "FaceTitle", "Choisis ton visage"), SectionTitleFontSize));
	FaceTitleSlot->SetHorizontalAlignment(HAlign_Center);
	FaceTitleSlot->SetPadding(FMargin(0.0f, SectionSpacing, 0.0f, SectionTitleSpacing));
	FaceRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Faces"));
	Customization->AddChildToVerticalBox(FaceRow)->SetHorizontalAlignment(HAlign_Center);

	// droite : aperçu du personnage
	USizeBox* Preview = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Preview"));
	Preview->SetWidthOverride(PreviewSize.X);
	Preview->SetHeightOverride(PreviewSize.Y);
	PreviewColumn = Preview;
	PreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PreviewImage"));
	PreviewImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));
	Preview->AddChild(PreviewImage);
	AddColumn(Preview, RightColumnWeight, RightColumnPadding);

	// « Prêt » : en bas au centre, lance la partie quand tout le monde est prêt
	USizeBox* ReadySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ReadyRow"));
	ReadySize->SetWidthOverride(ReadyButtonSize.X);
	ReadySize->SetHeightOverride(ReadyButtonSize.Y);
	ReadyRow = ReadySize;
	ReadyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Ready"));
	ReadyButton->SetBackgroundColor(NormalColor);
	ReadyButton->AddChild(YSCharacterSelect::MakeLabel(WidgetTree, NSLOCTEXT("YS", "ReadyButton", "Prêt"), ReadyFontSize));
	ReadySize->AddChild(ReadyButton);
	UYSCharacterSelectButtonProxy* ReadyProxy = NewObject<UYSCharacterSelectButtonProxy>(this);
	ReadyProxy->Group = 3;
	ReadyProxy->Menu = this;
	ReadyButton->OnClicked.AddDynamic(ReadyProxy, &UYSCharacterSelectButtonProxy::HandleClicked);
	ButtonProxies.Add(ReadyProxy);
	UVerticalBoxSlot* ReadySlot = Page->AddChildToVerticalBox(ReadySize);
	ReadySlot->SetHorizontalAlignment(HAlign_Center);
	ReadySlot->SetPadding(ReadyPadding);
}

void UYSCharacterSelectWidget::SetReady(bool bIsReady)
{
	if (ReadyButton)
	{
		ReadyButton->SetBackgroundColor(bIsReady ? SelectedColor : NormalColor);
	}
}

void UYSCharacterSelectWidget::SetTakenColors(const TArray<bool>& Taken)
{
	for (int32 Index = 0; Index < ColorSwatches.Num(); ++Index)
	{
		const bool bTaken = Taken.IsValidIndex(Index) && Taken[Index];
		ColorSwatches[Index]->SetIsEnabled(!bTaken);
		ColorSwatches[Index]->SetBackgroundColor(bTaken ? ColorValues[Index] * FLinearColor(TakenColorDim, TakenColorDim, TakenColorDim, 1.0f) : ColorValues[Index]);
	}
}

UBorder* UYSCharacterSelectWidget::AddSwatchButton(UHorizontalBox* Row, int32 Group, int32 Index, const FLinearColor& ButtonColor, UTexture2D* Icon, UButton*& OutButton)
{
	// cadre (sélection) > bouton carré (couleur) > icône (visage)
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetPadding(FMargin(SwatchFrameThickness));
	Frame->SetBrushColor(FLinearColor::Transparent);

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetWidthOverride(SwatchSize);
	Size->SetHeightOverride(SwatchSize);
	Frame->SetContent(Size);

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetBackgroundColor(ButtonColor);
	Size->AddChild(Button);

	if (Icon)
	{
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Image->SetBrushFromTexture(Icon);
		Button->AddChild(Image);
	}

	UYSCharacterSelectButtonProxy* Proxy = NewObject<UYSCharacterSelectButtonProxy>(this);
	Proxy->Group = Group;
	Proxy->Index = Index;
	Proxy->Menu = this;
	Button->OnClicked.AddDynamic(Proxy, &UYSCharacterSelectButtonProxy::HandleClicked);
	ButtonProxies.Add(Proxy);

	UHorizontalBoxSlot* FrameSlot = Row->AddChildToHorizontalBox(Frame);
	FrameSlot->SetPadding(FMargin(SwatchSpacing * 0.5f, 0.0f));
	OutButton = Button;
	return Frame;
}

void UYSCharacterSelectWidget::SetCustomizationOptions(const TArray<FLinearColor>& Colors, const TArray<UTexture2D*>& Faces)
{
	if (!ColorRow || ColorButtons.Num() > 0)
	{
		return;
	}

	for (int32 Index = 0; Index < Colors.Num(); ++Index)
	{
		UButton* Swatch = nullptr;
		ColorButtons.Add(AddSwatchButton(ColorRow, 1, Index, Colors[Index], nullptr, Swatch));
		ColorSwatches.Add(Swatch);
		ColorValues.Add(Colors[Index]);
	}
	for (int32 Index = 0; Index < Faces.Num(); ++Index)
	{
		UButton* Swatch = nullptr;
		FaceButtons.Add(AddSwatchButton(FaceRow, 2, Index, FLinearColor(0.85f, 0.85f, 0.85f), Faces[Index], Swatch));
	}
}

void UYSCharacterSelectWidget::SetSelectedColor(int32 SelectedIndex)
{
	for (int32 Index = 0; Index < ColorButtons.Num(); ++Index)
	{
		ColorButtons[Index]->SetBrushColor(Index == SelectedIndex ? SwatchSelectedFrameColor : FLinearColor::Transparent);
	}
}

void UYSCharacterSelectWidget::SetSelectedFace(int32 SelectedIndex)
{
	for (int32 Index = 0; Index < FaceButtons.Num(); ++Index)
	{
		FaceButtons[Index]->SetBrushColor(Index == SelectedIndex ? SwatchSelectedFrameColor : FLinearColor::Transparent);
	}
}

void UYSCharacterSelectWidget::SetCustomizationVisible(bool bVisible)
{
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	for (UWidget* Widget : { NicknameRow.Get(), CustomizationColumn.Get(), PreviewColumn.Get(), ReadyRow.Get() })
	{
		if (Widget)
		{
			Widget->SetVisibility(NewVisibility);
		}
	}
}

void UYSCharacterSelectWidget::SetNickname(const FText& Nickname)
{
	// le joueur est en train de taper : on ne touche pas au texte
	if (NicknameBox && !NicknameBox->HasKeyboardFocus() && !NicknameBox->GetText().EqualTo(Nickname))
	{
		NicknameBox->SetText(Nickname);
	}
}

void UYSCharacterSelectWidget::SetPreviewTexture(UTexture* Texture)
{
	if (PreviewImage)
	{
		PreviewImage->SetBrushResourceObject(Texture);
		PreviewImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, Texture ? 1.0f : 0.0f));
	}
}

void UYSCharacterSelectWidget::HandleNicknameCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	OnNicknameCommitted.ExecuteIfBound(Text.ToString());
}

void UYSCharacterSelectWidget::SetCharacters(const TArray<FText>& Names)
{
	if (!ButtonList || Buttons.Num() > 0)
	{
		return;
	}

	for (const FText& Name : Names)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Label->SetText(Name);
		YSCharacterSelect::SetFontSize(Label, ButtonFontSize);
		Label->SetJustification(ETextJustify::Center);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		AddChoiceButton(Label);
	}
}

void UYSCharacterSelectWidget::SetChoices(const TArray<TArray<FYSChoiceLine>>& Choices)
{
	if (!ButtonList || Buttons.Num() > 0)
	{
		return;
	}

	for (const TArray<FYSChoiceLine>& Lines : Choices)
	{
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		for (const FYSChoiceLine& Line : Lines)
		{
			UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Label->SetText(Line.Text);
			FSlateFontInfo Font = Label->GetFont();
			Font.Size = Line.FontSize;
			Font.TypefaceFontName = Line.bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
			Label->SetFont(Font);
			Label->SetColorAndOpacity(FSlateColor(Line.Color));
			Label->SetJustification(ETextJustify::Center);

			UWidget* LineWidget = Label;
			if (Line.bStrikethrough)
			{
				// texte rayé : fine barre de la couleur du texte posée par-dessus, sur toute sa largeur
				UOverlay* Struck = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
				Struck->AddChildToOverlay(Label);

				USizeBox* LineBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				LineBox->SetHeightOverride(FMath::Max(1.0f, Line.FontSize / 8.0f));
				UImage* Strike = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				Strike->SetColorAndOpacity(Line.Color);
				LineBox->AddChild(Strike);

				UOverlaySlot* StrikeSlot = Struck->AddChildToOverlay(LineBox);
				StrikeSlot->SetHorizontalAlignment(HAlign_Fill);
				StrikeSlot->SetVerticalAlignment(VAlign_Center);
				LineWidget = Struck;
			}

			UVerticalBoxSlot* LineSlot = Column->AddChildToVerticalBox(LineWidget);
			LineSlot->SetHorizontalAlignment(HAlign_Center);
		}
		AddChoiceButton(Column);
	}
}

void UYSCharacterSelectWidget::AddChoiceButton(UWidget* Content)
{
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetWidthOverride(ButtonSize.X);
	Size->SetHeightOverride(ButtonSize.Y);

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetBackgroundColor(NormalColor);
	Size->AddChild(Button);
	Button->AddChild(Content);

	UYSCharacterSelectButtonProxy* Proxy = NewObject<UYSCharacterSelectButtonProxy>(this);
	Proxy->Index = Buttons.Num();
	Proxy->Menu = this;
	Button->OnClicked.AddDynamic(Proxy, &UYSCharacterSelectButtonProxy::HandleClicked);

	UVerticalBoxSlot* ButtonSlot = ButtonList->AddChildToVerticalBox(Size);
	ButtonSlot->SetPadding(FMargin(0.0f, ButtonSpacing * 0.5f));

	Buttons.Add(Button);
	ButtonProxies.Add(Proxy);
}

void UYSCharacterSelectWidget::ClearChoices()
{
	if (ButtonList)
	{
		ButtonList->ClearChildren();
	}
	Buttons.Reset();
	ButtonProxies.Reset();
}

void UYSCharacterSelectWidget::SetTitle(const FText& Title)
{
	if (TitleText)
	{
		TitleText->SetText(Title);
	}
}

void UYSCharacterSelectWidget::SetSelectedIndex(int32 SelectedIndex)
{
	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		Buttons[Index]->SetBackgroundColor(Index == SelectedIndex ? SelectedColor : NormalColor);
	}
}

void UYSCharacterSelectWidget::SetStatus(const FText& Status)
{
	if (StatusText)
	{
		StatusText->SetText(Status);
	}
}

void UYSCharacterSelectWidget::NotifyButtonClicked(int32 Group, int32 Index)
{
	switch (Group)
	{
	case 1:
		SetSelectedColor(Index);
		OnColorChosen.ExecuteIfBound(Index);
		break;
	case 2:
		SetSelectedFace(Index);
		OnFaceChosen.ExecuteIfBound(Index);
		break;
	case 3:
		OnReadyClicked.ExecuteIfBound();
		break;
	default:
		SetSelectedIndex(Index);
		OnCharacterChosen.ExecuteIfBound(Index);
		break;
	}
}
