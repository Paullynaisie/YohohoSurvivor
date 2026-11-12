#include "Island/YSPierLadder.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "Player/YSPlayerCharacter.h"

AYSPierLadder::AYSPierLadder()
{
	// construite par l'île sur chaque machine, comme le ponton
	bReplicates = false;
	SetReplicatingMovement(false);

	DisplayName = NSLOCTEXT("YS", "PierLadderName", "Monter sur le ponton");
	bShowLadder = true;
	bAllowWalkers = false;
	// échelle seule contre le bord (pas de planche) : zone plus courte que celle d'un bateau
	AccessReach = 300.0f;
	AccessHalfWidth = 150.0f;

	if (UWidgetComponent* Prompt = FindComponentByClass<UWidgetComponent>())
	{
		Prompt->SetRelativeLocation(FVector(60.0f, 0.0f, 150.0f));
	}
}

void AYSPierLadder::SetupPier(float DeckHeight, float LandingInset)
{
	LadderTop = DeckHeight + 30.0f;
	LadderBottom = -120.0f;
	GetBoardPointComponent()->SetRelativeLocationAndRotation(FVector(-LandingInset, 0.0f, DeckHeight), FRotator(0.0f, 180.0f, 0.0f));
	BuildLadder();
}

bool AYSPierLadder::CanInteract(const AYSPlayerCharacter* Character) const
{
	if (!Character || !Character->IsAlive() || Character->IsAtStation())
	{
		return false;
	}
	// déjà sur le ponton (pieds au-dessus du point d'arrivée) : rien à faire
	const float FeetZ = Character->GetActorLocation().Z - Character->GetSimpleCollisionHalfHeight();
	if (FeetZ > GetBoardPointLocation().Z - 20.0f)
	{
		return false;
	}
	return Character->IsSwimming() ? bAllowSwimmers : bAllowWalkers;
}
