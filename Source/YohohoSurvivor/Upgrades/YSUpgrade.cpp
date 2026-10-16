#include "Upgrades/YSUpgrade.h"
#include "Abilities/YSAbilityComponent.h"
#include "Player/YSPlayerCharacter.h"
#include "Weapons/YSWeapon.h"

UYSDamageUpgrade::UYSDamageUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "DamageUpgradeName", "Dégâts +10");
	Description = NSLOCTEXT("YS", "DamageUpgradeDesc", "Toutes tes armes infligent 10 dégâts de plus.");
}

void UYSDamageUpgrade::Apply(AYSPlayerCharacter* Character) const
{
	if (!Character)
	{
		return;
	}

	for (const AYSWeapon* Weapon : { Character->GetLeftHandWeapon(), Character->GetRightHandWeapon() })
	{
		if (!Weapon)
		{
			continue;
		}

		TInlineComponentArray<UYSAbilityComponent*> Actions(Weapon);
		for (UYSAbilityComponent* Action : Actions)
		{
			Action->AddBonusDamage(BonusDamage);
			UE_LOG(LogTemp, Log, TEXT("[YSDamageUpgrade] %s.%s : +%.0f dégâts"), *Weapon->GetName(), *Action->GetName(), BonusDamage);
		}
	}
}
