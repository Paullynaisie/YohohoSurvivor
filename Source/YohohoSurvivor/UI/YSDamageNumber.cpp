#include "UI/YSDamageNumber.h"
#include "Components/WidgetComponent.h"
#include "UI/YSDamageNumberWidget.h"

AYSDamageNumber::AYSDamageNumber()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	Widget = CreateDefaultSubobject<UWidgetComponent>(TEXT("Widget"));
	Widget->SetWidgetSpace(EWidgetSpace::Screen);
	Widget->SetDrawAtDesiredSize(true);
	Widget->SetPivot(FVector2D(0.5f, 0.5f));
	Widget->SetWidgetClass(UYSDamageNumberWidget::StaticClass());
	Widget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Widget;
}

void AYSDamageNumber::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(Duration);
}

void AYSDamageNumber::Init(float Amount, EYSDamageNumberKind Kind, bool bCritical)
{
	Widget->InitWidget();
	UYSDamageNumberWidget* DamageWidget = Cast<UYSDamageNumberWidget>(Widget->GetUserWidgetObject());
	if (!DamageWidget)
	{
		return;
	}

	FLinearColor Color = DealtByOtherColor;
	switch (Kind)
	{
	case EYSDamageNumberKind::DealtByMe:    Color = DealtByMeColor; break;
	case EYSDamageNumberKind::TakenByMe:    Color = TakenByMeColor; break;
	case EYSDamageNumberKind::DealtByOther: Color = DealtByOtherColor; break;
	case EYSDamageNumberKind::TakenByOther: Color = TakenByOtherColor; break;
	}

	FNumberFormattingOptions Format;
	Format.MaximumFractionalDigits = 1;
	// critique : chiffre plus gros
	DamageWidget->SetDamage(FText::AsNumber(Amount, &Format), Color, FMath::RoundToInt(FontSize * (bCritical ? CriticalScale : 1.0f)));
}

void AYSDamageNumber::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	AddActorWorldOffset(FVector(0.0f, 0.0f, RiseSpeed * DeltaSeconds));

	// reste opaque au début puis s'efface
	if (UUserWidget* UserWidget = Widget->GetUserWidgetObject())
	{
		const float Alpha = FMath::Clamp(Age / Duration, 0.0f, 1.0f);
		UserWidget->SetRenderOpacity(1.0f - Alpha * Alpha);
	}
}
