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

FVector AYSWeapon::GetMuzzleLocation() const
{
	return Muzzle->GetComponentLocation();
}
