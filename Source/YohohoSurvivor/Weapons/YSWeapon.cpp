#include "Weapons/YSWeapon.h"
#include "Abilities/YSAbilityComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"

AYSWeapon::AYSWeapon()
{
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(Root);
}

UYSAbilityComponent* AYSWeapon::GetAction() const
{
	return FindComponentByClass<UYSAbilityComponent>();
}

APawn* AYSWeapon::GetHolder() const
{
	return Cast<APawn>(GetOwner());
}

float AYSWeapon::GetRecoilAlpha() const
{
	const UYSAbilityComponent* Action = GetAction();
	if (!Action)
	{
		return 0.0f;
	}

	const float Elapsed = Action->GetTimeSinceActivation();
	if (Elapsed < 0.0f || Elapsed >= RecoilKickDuration + RecoilRecoverDuration)
	{
		return 0.0f;
	}

	// coup sec puis retour plus doux
	if (Elapsed < RecoilKickDuration)
	{
		return FMath::InterpEaseOut(0.0f, 1.0f, Elapsed / RecoilKickDuration, 2.0f);
	}
	return FMath::InterpEaseInOut(1.0f, 0.0f, (Elapsed - RecoilKickDuration) / RecoilRecoverDuration, 2.0f);
}

FVector AYSWeapon::GetMuzzleLocation() const
{
	return Muzzle->GetComponentLocation();
}
