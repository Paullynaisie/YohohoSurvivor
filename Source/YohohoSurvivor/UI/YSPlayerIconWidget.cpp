#include "UI/YSPlayerIconWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"

bool UYSPlayerIconWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// pas de Widget Blueprint : l'arbre est construit dès la création
	if (bSuperInitialized && WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}

	return bSuperInitialized;
}

void UYSPlayerIconWidget::BuildWidgetTree()
{
	SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Size"));
	WidgetTree->RootWidget = SizeBox;

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	SizeBox->AddChild(Overlay);

	// rond : boîte entièrement arrondie de la couleur du joueur
	Circle = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Circle"));
	FSlateBrush CircleBrush;
	CircleBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	CircleBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
	CircleBrush.OutlineSettings.Width = 2.0f;
	CircleBrush.OutlineSettings.Color = FSlateColor(FLinearColor::Black);
	CircleBrush.TintColor = FSlateColor(FLinearColor::White);
	Circle->SetBrush(CircleBrush);
	UOverlaySlot* CircleSlot = Overlay->AddChildToOverlay(Circle);
	CircleSlot->SetHorizontalAlignment(HAlign_Fill);
	CircleSlot->SetVerticalAlignment(VAlign_Fill);

	// visage centré par-dessus
	FaceBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("FaceSize"));
	FaceImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Face"));
	FaceImage->SetVisibility(ESlateVisibility::Collapsed);
	FaceBox->AddChild(FaceImage);
	UOverlaySlot* FaceSlot = Overlay->AddChildToOverlay(FaceBox);
	FaceSlot->SetHorizontalAlignment(HAlign_Center);
	FaceSlot->SetVerticalAlignment(VAlign_Center);

	SetIconSize(48.0f);
}

void UYSPlayerIconWidget::SetIconSize(float Size)
{
	if (!SizeBox || FMath::IsNearlyEqual(Size, CurrentSize))
	{
		return;
	}

	CurrentSize = Size;
	SizeBox->SetWidthOverride(Size);
	SizeBox->SetHeightOverride(Size);
	FaceBox->SetWidthOverride(Size * FaceScale);
	FaceBox->SetHeightOverride(Size * FaceScale);
}

void UYSPlayerIconWidget::SetIcon(const FLinearColor& Color, UTexture2D* Face)
{
	if (!Circle)
	{
		return;
	}

	Circle->SetColorAndOpacity(Color);
	if (Face != CurrentFace)
	{
		CurrentFace = Face;
		if (Face)
		{
			FaceImage->SetBrushFromTexture(Face);
		}
		FaceImage->SetVisibility(Face ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
