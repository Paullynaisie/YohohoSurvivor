#include "Player/YSPlayerController.h"
#include "Abilities/YSCoupDPetoirAbility.h"
#include "GameFramework/Pawn.h"
#include "UI/YSAbilityIconWidget.h"

AYSPlayerController::AYSPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
	AbilityIconClass = UYSAbilityIconWidget::StaticClass();
}

void AYSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		// le curseur reste visible même quand la fenêtre capture la souris
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);

		if (AbilityIconClass)
		{
			AbilityIcon = CreateWidget<UYSAbilityIconWidget>(this, AbilityIconClass);
			AbilityIcon->AddToViewport();
			AbilityIcon->SetAnchorsInViewport(FAnchors(0.5f, 1.0f));
			AbilityIcon->SetAlignmentInViewport(FVector2D(0.5f, 1.0f));
			AbilityIcon->SetDesiredSizeInViewport(FVector2D(AbilityIcon->IconSize));
			AbilityIcon->SetPositionInViewport(FVector2D(0.0f, -AbilityIconBottomMargin), false);
		}
	}
}

void AYSPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	UpdateAim();

	if (AbilityIcon)
	{
		const APawn* ControlledPawn = GetPawn();
		AbilityIcon->Refresh(ControlledPawn ? ControlledPawn->FindComponentByClass<UYSCoupDPetoirAbility>() : nullptr);
	}
}

void AYSPlayerController::UpdateAim()
{
	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	FVector RayOrigin, RayDirection;
	if (!DeprojectMousePositionToWorld(RayOrigin, RayDirection) || FMath::IsNearlyZero(RayDirection.Z))
	{
		return;
	}

	// intersection du rayon souris avec le plan horizontal passant par le pawn
	const FVector PawnLocation = ControlledPawn->GetActorLocation();
	const float Distance = (PawnLocation.Z - RayOrigin.Z) / RayDirection.Z;
	if (Distance <= 0.0f)
	{
		return;
	}

	AimLocation = RayOrigin + RayDirection * Distance;

	const FVector ToAim = AimLocation - PawnLocation;
	if (ToAim.SizeSquared2D() > 1.0f)
	{
		SetControlRotation(FRotator(0.0f, ToAim.Rotation().Yaw, 0.0f));
	}
}
