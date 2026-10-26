#include "Interaction/YSPlayerCorpse.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"

AYSPlayerCorpse::AYSPlayerCorpse()
{
	DisplayName = NSLOCTEXT("YS", "PlayerCorpse", "Cadavre");
	// visible de tous, même loin (le corps du personnage mort l'est aussi)
	bAlwaysRelevant = true;
	// lâché dans la mer : il flotte
	bFloatsOnWater = true;
}

void AYSPlayerCorpse::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSPlayerCorpse, Body);
	DOREPLIFETIME(AYSPlayerCorpse, BodyLocation);
	DOREPLIFETIME(AYSPlayerCorpse, BodyRotation);
	DOREPLIFETIME(AYSPlayerCorpse, bEverCarried);
	DOREPLIFETIME(AYSPlayerCorpse, OwnerPlayerState);
	DOREPLIFETIME(AYSPlayerCorpse, ViewOffset);
}

void AYSPlayerCorpse::InitCorpse(AYSPlayerCharacter* InBody, const FVector& InBodyLocation, const FRotator& InBodyRotation, APlayerState* InOwnerPlayerState, const FVector& InViewOffset)
{
	Body = InBody;
	BodyLocation = InBodyLocation;
	BodyRotation = InBodyRotation;
	OwnerPlayerState = InOwnerPlayerState;
	ViewOffset = InViewOffset;
}

void AYSPlayerCorpse::BeginPlay()
{
	Super::BeginPlay();
	ApplyBodyPose();
}

void AYSPlayerCorpse::Interact(AYSPlayerCharacter* Character)
{
	if (HasAuthority() && CanInteract(Character))
	{
		bEverCarried = true;
	}
	Super::Interact(Character);
}

void AYSPlayerCorpse::OnCarrierChanged()
{
	ApplyBodyPose();
}

void AYSPlayerCorpse::OnRep_Body()
{
	ApplyBodyPose();
}

void AYSPlayerCorpse::ApplyBodyPose()
{
	if (Body)
	{
		Body->AttachBodyTo(GetRootComponent(), bEverCarried ? CarriedBodyLocation : BodyLocation, bEverCarried ? CarriedBodyRotation : BodyRotation);
	}
}
