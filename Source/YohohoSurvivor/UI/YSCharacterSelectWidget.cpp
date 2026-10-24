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

	// 4 colonnes : choix, infos, personnalisation, aperçu
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

	// gauche : choix du personnage
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	AddColumn(Column, LeftColumnWeight, LeftColumnPadding);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	TitleText = Title;
	Title->SetText(NSLOCTEXT("YS", "CharacterSelectTitle", "Choisis ta classe"));
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

	// infos : fiche du personnage choisi (remplie par SetCharacterInfo)
	InfoColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Info"));
	AddColumn(InfoColumn, InfoColumnWeight, InfoColumnPadding);

	// centre : couleur et visage
	UVerticalBox* Customization = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Customization"));
	AddColumn(Customization, CenterColumnWeight, CenterColumnPadding);

	UVerticalBoxSlot* ColorTitleSlot = Customization->AddChildToVerticalBox(YSCharacterSelect::MakeLabel(WidgetTree, NSLOCTEXT("YS", "ColorTitle", "Choisis ta couleur"), SectionTitleFontSize));
	ColorTitleSlot->SetHorizontalAlignment(HAlign_Center);
	ColorTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, SectionTitleSpacing));
	ColorRow = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Colors"));
	Customization->AddChildToVerticalBox(ColorRow)->SetHorizontalAlignment(HAlign_Center);

	UVerticalBoxSlot* FaceTitleSlot = Customization->AddChildToVerticalBox(YSCharacterSelect::MakeLabel(WidgetTree, NSLOCTEXT("YS", "FaceTitle", "Choisis ton visage"), SectionTitleFontSize));
	FaceTitleSlot->SetHorizontalAlignment(HAlign_Center);
	FaceTitleSlot->SetPadding(FMargin(0.0f, SectionSpacing, 0.0f, SectionTitleSpacing));
	FaceRow = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Faces"));
	Customization->AddChildToVerticalBox(FaceRow)->SetHorizontalAlignment(HAlign_Center);

	// droitier / gaucher
	UVerticalBoxSlot* HandTitleSlot = Customization->AddChildToVerticalBox(YSCharacterSelect::MakeLabel(WidgetTree, NSLOCTEXT("YS", "HandTitle", "Choisis ta main"), SectionTitleFontSize));
	HandTitleSlot->SetHorizontalAlignment(HAlign_Center);
	HandTitleSlot->SetPadding(FMargin(0.0f, SectionSpacing, 0.0f, SectionTitleSpacing));
	UHorizontalBox* HandRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Handedness"));
	Customization->AddChildToVerticalBox(HandRow)->SetHorizontalAlignment(HAlign_Center);
	const FText HandLabels[] = { NSLOCTEXT("YS", "RightHanded", "Droitier"), NSLOCTEXT("YS", "LeftHanded", "Gaucher") };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		USizeBox* HandSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		HandSize->SetWidthOverride(HandednessButtonSize.X);
		HandSize->SetHeightOverride(HandednessButtonSize.Y);
		UButton* HandButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		HandButton->SetBackgroundColor(NormalColor);
		HandButton->AddChild(YSCharacterSelect::MakeLabel(WidgetTree, HandLabels[Index], HandednessFontSize));
		HandSize->AddChild(HandButton);

		UYSCharacterSelectButtonProxy* HandProxy = NewObject<UYSCharacterSelectButtonProxy>(this);
		HandProxy->Group = 4;
		HandProxy->Index = Index;
		HandProxy->Menu = this;
		HandButton->OnClicked.AddDynamic(HandProxy, &UYSCharacterSelectButtonProxy::HandleClicked);
		ButtonProxies.Add(HandProxy);
		HandednessButtons.Add(HandButton);

		UHorizontalBoxSlot* HandSlot = HandRow->AddChildToHorizontalBox(HandSize);
		HandSlot->SetPadding(FMargin(HandednessSpacing * 0.5f, 0.0f));
	}

	// droite : aperçu du personnage
	USizeBox* Preview = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Preview"));
	Preview->SetWidthOverride(PreviewSize.X);
	Preview->SetHeightOverride(PreviewSize.Y);
	PreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PreviewImage"));
	PreviewImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));
	Preview->AddChild(PreviewImage);
	AddColumn(Preview, RightColumnWeight, RightColumnPadding);

	// « Prêt » : en bas au centre, lance la partie quand tout le monde est prêt
	USizeBox* ReadySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ReadyRow"));
	ReadySize->SetWidthOverride(ReadyButtonSize.X);
	ReadySize->SetHeightOverride(ReadyButtonSize.Y);
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

UBorder* UYSCharacterSelectWidget::AddSwatchButton(UVerticalBox* Grid, int32 Group, int32 Index, const FLinearColor& ButtonColor, UTexture2D* Icon, UButton*& OutButton)
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

	// lignes de SwatchesPerRow boutons ; une ligne incomplète commence sous les premiers (alignée à gauche)
	const int32 PerRow = FMath::Max(SwatchesPerRow, 1);
	if (Index % PerRow == 0 || Grid->GetChildrenCount() == 0)
	{
		UHorizontalBox* NewRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBoxSlot* RowSlot = Grid->AddChildToVerticalBox(NewRow);
		RowSlot->SetHorizontalAlignment(HAlign_Left);
		RowSlot->SetPadding(FMargin(0.0f, Grid->GetChildrenCount() > 1 ? SwatchRowSpacing : 0.0f, 0.0f, 0.0f));
	}
	UHorizontalBox* Row = Cast<UHorizontalBox>(Grid->GetChildAt(Grid->GetChildrenCount() - 1));

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

void UYSCharacterSelectWidget::SetCharacterInfo(const FYSCharacterInfo& Info)
{
	if (!InfoColumn || (bHasDisplayedInfo && DisplayedInfo == Info))
	{
		return;
	}
	DisplayedInfo = Info;
	bHasDisplayedInfo = true;
	InfoColumn->ClearChildren();

	auto AddWidget = [this](UWidget* Widget, float TopSpacing)
	{
		UVerticalBoxSlot* LineSlot = InfoColumn->AddChildToVerticalBox(Widget);
		LineSlot->SetHorizontalAlignment(HAlign_Left);
		LineSlot->SetPadding(FMargin(0.0f, InfoColumn->GetChildrenCount() > 1 ? TopSpacing : 0.0f, 0.0f, 0.0f));
	};
	// « Nom : valeur » (valeur en gras) ; rien si la valeur est vide
	auto AddLine = [this, &AddWidget](const FText& Label, const FText& Value, float TopSpacing)
	{
		if (Value.IsEmpty())
		{
			return;
		}
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* LabelText = YSCharacterSelect::MakeLabel(WidgetTree, FText::Format(NSLOCTEXT("YS", "InfoLabel", "{0} : "), Label), InfoLineFontSize);
		LabelText->SetColorAndOpacity(FSlateColor(InfoLabelColor));
		Line->AddChildToHorizontalBox(LabelText);
		UTextBlock* ValueText = YSCharacterSelect::MakeLabel(WidgetTree, Value, InfoLineFontSize);
		FSlateFontInfo Font = ValueText->GetFont();
		Font.TypefaceFontName = FName(TEXT("Bold"));
		ValueText->SetFont(Font);
		ValueText->SetColorAndOpacity(FSlateColor(InfoValueColor));
		Line->AddChildToHorizontalBox(ValueText);
		AddWidget(Line, TopSpacing);
	};

	FNumberFormattingOptions Format;
	Format.MinimumFractionalDigits = 0;
	Format.MaximumFractionalDigits = 2;

	// section 1 : le personnage (taille seulement si elle n'est pas normale)
	AddWidget(YSCharacterSelect::MakeLabel(WidgetTree, Info.Name, InfoNameFontSize), 0.0f);
	AddLine(NSLOCTEXT("YS", "InfoMaxHealth", "PV max"), FText::AsNumber(Info.MaxHealth, &Format), InfoLineSpacing);
	AddLine(NSLOCTEXT("YS", "InfoRegen", "Regen PV/s"), FText::AsNumber(Info.HealthRegenPerSecond, &Format), InfoLineSpacing);
	if (!FMath::IsNearlyEqual(Info.Size, 1.0f))
	{
		AddLine(NSLOCTEXT("YS", "InfoSize", "Taille"), FText::Format(NSLOCTEXT("YS", "InfoSizeValue", "x{0}"), FText::AsNumber(Info.Size, &Format)), InfoLineSpacing);
	}

	// section 2 : les armes (noms seulement)
	const int32 FirstWeaponLine = InfoColumn->GetChildrenCount();
	auto WeaponLineSpacing = [this, FirstWeaponLine]() { return InfoColumn->GetChildrenCount() == FirstWeaponLine ? InfoSectionSpacing : InfoLineSpacing; };
	AddLine(NSLOCTEXT("YS", "InfoWeaponSet", "Set d'arme"), Info.WeaponSetName, WeaponLineSpacing());
	AddLine(NSLOCTEXT("YS", "InfoAutoAtk", "Auto atk"), Info.AutoAtkName, WeaponLineSpacing());
	AddLine(NSLOCTEXT("YS", "InfoAbility", "Ability"), Info.AbilityName, WeaponLineSpacing());
	AddLine(NSLOCTEXT("YS", "InfoPassive", "Passif"), Info.PassiveName, WeaponLineSpacing());
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

void UYSCharacterSelectWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!NicknameBox)
	{
		return;
	}

	// focus pris (clic) : l'ancien pseudo est vidé (l'indication « Ton pseudo » s'affiche)
	const bool bHasFocus = NicknameBox->HasKeyboardFocus();
	if (bHasFocus && !bNicknameHadFocus)
	{
		NicknameBeforeEdit = NicknameBox->GetText();
		NicknameBox->SetText(FText::GetEmpty());
	}
	bNicknameHadFocus = bHasFocus;
}

void UYSCharacterSelectWidget::HandleNicknameCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	// validé sans rien taper : on garde l'ancien pseudo
	if (Text.ToString().TrimStartAndEnd().IsEmpty())
	{
		NicknameBox->SetText(NicknameBeforeEdit);
		return;
	}
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

void UYSCharacterSelectWidget::SetTakenCharacters(const TArray<bool>& Taken)
{
	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		const bool bTaken = Taken.IsValidIndex(Index) && Taken[Index];
		if (Buttons[Index]->GetIsEnabled() == bTaken)
		{
			Buttons[Index]->SetIsEnabled(!bTaken);
			Buttons[Index]->SetRenderOpacity(bTaken ? TakenCharacterOpacity : 1.0f);
		}
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
	case 4:
		SetSelectedHandedness(Index == 1);
		OnHandednessChosen.ExecuteIfBound(Index);
		break;
	default:
		SetSelectedIndex(Index);
		OnCharacterChosen.ExecuteIfBound(Index);
		break;
	}
}

void UYSCharacterSelectWidget::SetSelectedHandedness(bool bLeftHanded)
{
	for (int32 Index = 0; Index < HandednessButtons.Num(); ++Index)
	{
		HandednessButtons[Index]->SetBackgroundColor((Index == 1) == bLeftHanded ? SelectedColor : NormalColor);
	}
}
