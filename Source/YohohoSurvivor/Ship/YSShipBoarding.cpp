#include "Ship/YSShipBoarding.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/YSPlayerCharacter.h"
#include "Ship/YSPirateShip.h"

AYSShipBoarding::AYSShipBoarding()
{
	bCarriable = false;
	DisplayName = NSLOCTEXT("YS", "ShipBoardingName", "Monter à bord");

	// pont du grand bateau : 200 vers l'intérieur depuis le flanc (+X de l'acteur = le large), dessus du pont à +210
	BoardPoint = CreateDefaultSubobject<USceneComponent>(TEXT("BoardPoint"));
	BoardPoint->SetupAttachment(GetRootComponent());
	BoardPoint->SetRelativeLocationAndRotation(FVector(-200.0f, 0.0f, 210.0f), FRotator(0.0f, 180.0f, 0.0f));

	// invite « E » au-dessus de la planche, côté large
	if (UWidgetComponent* Prompt = FindComponentByClass<UWidgetComponent>())
	{
		Prompt->SetRelativeLocation(FVector(250.0f, 0.0f, 200.0f));
	}
}

AYSPirateShip* AYSShipBoarding::GetShip() const
{
	return AYSPirateShip::GetShipOf(GetParentActor());
}

bool AYSShipBoarding::CanInteract(const AYSPlayerCharacter* Character) const
{
	const AYSPirateShip* Ship = GetShip();
	if (!Ship || !Ship->IsPresent() || !Character || !Character->IsAlive() || Character->IsCarrying() || Character->IsAtStation())
	{
		return false;
	}
	if (Character->IsSwimming() ? !bAllowSwimmers : !bAllowWalkers)
	{
		return false;
	}
	// déjà sur ce bateau (pont, planche d'embarquement) : rien à faire
	const UPrimitiveComponent* Base = Character->GetCharacterMovement() ? Cast<UPrimitiveComponent>(Character->GetCharacterMovement()->GetMovementBaseObject()) : nullptr;
	return !Base || AYSPirateShip::GetShipOf(Base->GetOwner()) != Ship;
}

FVector AYSShipBoarding::GetInteractLocation(const FVector& From) const
{
	// point du rectangle d'accès (repère de l'acteur, à plat) le plus proche
	const FVector Local = GetActorTransform().InverseTransformPosition(From);
	const FVector Clamped(FMath::Clamp(Local.X, 0.0f, AccessReach), FMath::Clamp(Local.Y, -AccessHalfWidth, AccessHalfWidth), 0.0f);
	return GetActorTransform().TransformPosition(Clamped);
}

void AYSShipBoarding::Interact(AYSPlayerCharacter* Character)
{
	if (!HasAuthority() || !CanInteract(Character))
	{
		return;
	}
	const bool bWasSwimming = Character->IsSwimming();
	Character->TeleportToFeetLocation(BoardPoint->GetComponentLocation(), BoardPoint->GetForwardVector());
	UE_LOG(LogTemp, Log, TEXT("[YSBoarding] %s monte à bord de %s%s (pieds en %s)"), *Character->GetName(), *GetNameSafe(GetShip()),
		bWasSwimming ? TEXT(" depuis l'eau") : TEXT(""), *BoardPoint->GetComponentLocation().ToCompactString());
}
