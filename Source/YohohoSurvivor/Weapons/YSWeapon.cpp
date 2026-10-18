#include "Weapons/YSWeapon.h"
#include "Abilities/YSCoupDPetoirAbility.h"
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

UYSAbilityComponent* AYSWeapon::GetActiveAction() const
{
	TInlineComponentArray<UYSAbilityComponent*> Actions(this);
	for (UYSAbilityComponent* Action : Actions)
	{
		if (!Action->IsAutomatic())
		{
			return Action;
		}
	}
	return nullptr;
}

UYSAbilityComponent* AYSWeapon::GetAutoAction() const
{
	TInlineComponentArray<UYSAbilityComponent*> Actions(this);
	for (UYSAbilityComponent* Action : Actions)
	{
		if (Action->IsAutomatic())
		{
			return Action;
		}
	}
	return nullptr;
}

APawn* AYSWeapon::GetHolder() const
{
	return Cast<APawn>(GetOwner());
}

bool AYSWeapon::GetRecoil(float& OutPitch, float& OutKickBack) const
{
	// action de tir la plus récemment activée (l'Ability et l'Auto shot ont chacune leurs réglages)
	const UYSCoupDPetoirAbility* Latest = nullptr;
	TInlineComponentArray<UYSCoupDPetoirAbility*> Shots(this);
	for (const UYSCoupDPetoirAbility* Shot : Shots)
	{
		if (!Latest || Shot->GetTimeSinceActivation() < Latest->GetTimeSinceActivation())
		{
			Latest = Shot;
		}
	}

	const float Alpha = Latest ? Latest->GetRecoilAlpha() : 0.0f;
	OutPitch = Latest ? Latest->RecoilPitch * Alpha : 0.0f;
	OutKickBack = Latest ? Latest->RecoilKickBack * Alpha : 0.0f;
	return Alpha > 0.0f;
}

FVector AYSWeapon::GetMuzzleLocation() const
{
	return Muzzle->GetComponentLocation();
}

FText AYSWeapon::GetWeaponName() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	FString Name = GetClass()->GetName();
	Name.RemoveFromStart(TEXT("BP_"));
	Name.RemoveFromEnd(TEXT("_C"));
	return FText::FromString(Name);
}

FRotator AYSWeapon::GetReloadSpin() const
{
	// rechargement le plus récent parmi les actions de tir de l'arme
	const UYSCoupDPetoirAbility* Latest = nullptr;
	TInlineComponentArray<UYSCoupDPetoirAbility*> Shots(this);
	for (const UYSCoupDPetoirAbility* Shot : Shots)
	{
		if (!Latest || Shot->GetTimeSinceReload() < Latest->GetTimeSinceReload())
		{
			Latest = Shot;
		}
	}
	return Latest ? Latest->GetReloadSpin() : FRotator::ZeroRotator;
}
