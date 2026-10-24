#include "Upgrades/YSUpgrade.h"
#include "Abilities/YSAbilityComponent.h"
#include "Abilities/YSAutoSwingAbility.h"
#include "Abilities/YSCoupDPetoirAbility.h"
#include "Abilities/YSGoMinionsAbility.h"
#include "Abilities/YSRollAndBombAbility.h"
#include "Abilities/YSSummonCrewAbility.h"
#include "Player/YSPlayerCharacter.h"
#include "Weapons/YSWeapon.h"
#include "Weapons/YSWeaponSet.h"

namespace YSUpgradeText
{
	FText FormatStat(float Value)
	{
		FNumberFormattingOptions Format;
		Format.MinimumFractionalDigits = 0;
		Format.MaximumFractionalDigits = 3;
		return FText::AsNumber(Value, &Format);
	}

	FText RarityName(EYSUpgradeRarity Rarity)
	{
		switch (Rarity)
		{
		case EYSUpgradeRarity::Bronze:	return NSLOCTEXT("YS", "RarityBronze", "Bronze");
		case EYSUpgradeRarity::Silver:	return NSLOCTEXT("YS", "RaritySilver", "Silver");
		case EYSUpgradeRarity::Gold:	return NSLOCTEXT("YS", "RarityGold", "Gold");
		default:						return FText::GetEmpty();
		}
	}

	/** Noms des armes qui portent ces actions, sans doublon (« Epee », « Pistolet & Epee ») ; vide si aucune */
	FText WeaponsName(const TArray<UYSAbilityComponent*>& Actions)
	{
		TArray<FString> Names;
		for (const UYSAbilityComponent* Action : Actions)
		{
			if (const AYSWeapon* Weapon = Action ? Cast<AYSWeapon>(Action->GetOwner()) : nullptr)
			{
				Names.AddUnique(Weapon->GetWeaponName().ToString());
			}
		}
		return FText::FromString(FString::Join(Names, TEXT(" & ")));
	}
}

void UYSUpgrade::GetOfferCard(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, FYSUpgradeCard& OutCard) const
{
	OutCard.Name = GetVariantNameFor(Character, Variant);
	OutCard.RarityText = YSUpgradeText::RarityName(Rarity);
	OutCard.Color = GetRarityColor(Rarity);
	OutCard.Icon = Icon;
	OutCard.Description = GetDescription(Character, Variant);
}

UYSStatUpgrade::UYSStatUpgrade()
{
	UpgradeType = EYSUpgradeType::Stat;
}

bool UYSUpgrade::PassesRequirements(const AYSPlayerCharacter* Character) const
{
	// upgrade requise déjà obtenue (ex. Atk critique avant Atk speed / Shoot system), jumelle pas encore prise
	return Character
		&& (!RequiredUpgrade || Character->HasUpgrade(RequiredUpgrade))
		&& (!TwinUpgrade || !Character->HasUpgrade(TwinUpgrade));
}

bool UYSStatUpgrade::CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	return Progress.Stacks < MaxStacks && PassesRequirements(Character);
}

float UYSStatUpgrade::GetCriticalDamageBonus(EYSUpgradeRarity Rarity) const
{
	switch (Rarity)
	{
	case EYSUpgradeRarity::Bronze:	return CriticalDamageBronze;
	case EYSUpgradeRarity::Silver:	return CriticalDamageSilver;
	case EYSUpgradeRarity::Gold:	return CriticalDamageGold;
	default:						return 0.0f;
	}
}

bool UYSUpgrade::SupportsRarity(int32 Variant, EYSUpgradeRarity Rarity) const
{
	switch (Rarity)
	{
	case EYSUpgradeRarity::Bronze:	return bCanBeBronze;
	case EYSUpgradeRarity::Silver:	return bCanBeSilver;
	case EYSUpgradeRarity::Gold:	return bCanBeGold;
	default:						return true;
	}
}

bool UYSUpgrade::CanOfferRarity(const AYSPlayerCharacter* Character, int32 Variant, EYSUpgradeRarity Rarity) const
{
	// ex. Atk speed / Shoot system : seulement en Bronze tant que le joueur n'a pas Atk critique
	const bool bHigherAllowed = !HigherRaritiesUpgrade || (Character && Character->HasUpgrade(HigherRaritiesUpgrade));
	return SupportsRarity(Variant, Rarity) && (Rarity == EYSUpgradeRarity::Bronze || bHigherAllowed);
}

float UYSStatUpgrade::GetBonusValue(int32 Variant, EYSUpgradeRarity Rarity) const
{
	switch (Rarity)
	{
	case EYSUpgradeRarity::Bronze:	return BronzeValue;
	case EYSUpgradeRarity::Silver:	return SilverValue;
	case EYSUpgradeRarity::Gold:	return GoldValue;
	default:						return 0.0f;
	}
}

FLinearColor UYSUpgrade::GetRarityColor(EYSUpgradeRarity Rarity) const
{
	switch (Rarity)
	{
	case EYSUpgradeRarity::Bronze:	return BronzeColor;
	case EYSUpgradeRarity::Silver:	return SilverColor;
	case EYSUpgradeRarity::Gold:	return GoldColor;
	default:						return FLinearColor::White;
	}
}

void UYSStatUpgrade::GetOfferCard(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, FYSUpgradeCard& OutCard) const
{
	Super::GetOfferCard(Character, Variant, Progress, Rarity, OutCard);
	OutCard.StackText = FText::Format(NSLOCTEXT("YS", "StatUpgradeStacks", "Bonus {0} / {1}"), Progress.Stacks + 1, MaxStacks);

	// valeur concrète : actuelle (rayée) et nouvelle
	FYSUpgradeStatChange& Change = OutCard.StatChanges.AddDefaulted_GetRef();
	Change.Label = GetStatLabel(Character, Variant);
	Change.OldValue = YSUpgradeText::FormatStat(GetDisplayValue(Character, Variant, Progress.TotalValue));
	Change.NewValue = YSUpgradeText::FormatStat(GetDisplayValue(Character, Variant, Progress.TotalValue + GetBonusValue(Variant, Rarity)));

	// effet en plus : dégâts des coups critiques (Atk speed, Shoot system)
	const float CriticalBonus = GetCriticalDamageBonus(Rarity);
	if (CriticalBonus > 0.0f && Character)
	{
		FYSUpgradeStatChange& Critical = OutCard.StatChanges.AddDefaulted_GetRef();
		Critical.Label = NSLOCTEXT("YS", "CriticalDamageLabel", "Dégâts critiques (%)");
		Critical.OldValue = YSUpgradeText::FormatStat(Character->GetCriticalDamageMultiplier() * 100.0f);
		Critical.NewValue = YSUpgradeText::FormatStat((Character->GetBaseCriticalDamageMultiplier() + Character->GetCriticalDamageAdd()) * (1.0f + Character->GetCriticalDamageBonus() + CriticalBonus) * 100.0f);
	}
}

void UYSStatUpgrade::Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	if (Character)
	{
		ApplyBonus(Character, Variant, Progress.TotalValue);
		Character->RecomputeCriticalDamageBonus();
		UE_LOG(LogTemp, Log, TEXT("[YSStatUpgrade] %s : %s, somme des bonus %.3f (%d bonus) -> %.3f"), *Character->GetName(), *GetVariantNameFor(Character, Variant).ToString(),
			Progress.TotalValue, Progress.Stacks, GetStatValue(GetBaseStatValue(Character, Variant), Progress.TotalValue));
	}
}

UYSHandStatUpgrade::UYSHandStatUpgrade()
{
	AbilityLabel = NSLOCTEXT("YS", "HandStatAbility", "Ability");
	AutoAtkLabel = NSLOCTEXT("YS", "HandStatAutoAtk", "Auto atk");
}

FText UYSHandStatUpgrade::MakeName(const FText& WeaponsText, int32 Variant) const
{
	const FText& SlotLabel = GetSlot(Variant) == EYSActionSlot::AutoAtk ? AutoAtkLabel : AbilityLabel;
	return WeaponsText.IsEmpty()
		? FText::Format(NSLOCTEXT("YS", "SlotStatNameNoWeapon", "{0} {1}"), SlotLabel, BonusName)
		: FText::Format(NSLOCTEXT("YS", "SlotStatName", "{0} {1} {2}"), WeaponsText, SlotLabel, BonusName);
}

FText UYSHandStatUpgrade::GetVariantName(int32 Variant) const
{
	return MakeName(FText::GetEmpty(), Variant);
}

FText UYSHandStatUpgrade::GetVariantNameFor(const AYSPlayerCharacter* Character, int32 Variant) const
{
	return MakeName(YSUpgradeText::WeaponsName(GetSlotActions(Character, Variant)), Variant);
}

bool UYSHandStatUpgrade::CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	// variante proposée par cet asset, et une arme du set doit avoir une action de ce type (Ability = clic, Auto atk = attaque auto)
	return IsCompatibleWith(Character, Variant) && Super::CanBeOffered(Character, Variant, Progress);
}

bool UYSHandStatUpgrade::IsCompatibleWith(const AYSPlayerCharacter* Character, int32 Variant) const
{
	const bool bVariantEnabled = GetSlot(Variant) == EYSActionSlot::AutoAtk ? bAutoAtkVariant : bAbilityVariant;
	return bVariantEnabled && GetSlotAction(Character, Variant) != nullptr;
}

TArray<UYSAbilityComponent*> UYSHandStatUpgrade::GetSlotActions(const AYSPlayerCharacter* Character, int32 Variant) const
{
	TArray<UYSAbilityComponent*> Actions;
	if (!Character)
	{
		return Actions;
	}
	for (const AYSWeapon* Weapon : { Character->GetMainHandWeapon(), Character->GetOffHandWeapon() })
	{
		UYSAbilityComponent* Action = !Weapon ? nullptr : GetSlot(Variant) == EYSActionSlot::AutoAtk ? Weapon->GetAutoAction() : Weapon->GetActiveAction();
		if (Action && IsActionEligible(Action))
		{
			Actions.Add(Action);
		}
	}
	return Actions;
}

UYSAbilityComponent* UYSHandStatUpgrade::GetSlotAction(const AYSPlayerCharacter* Character, int32 Variant) const
{
	const TArray<UYSAbilityComponent*> Actions = GetSlotActions(Character, Variant);
	return Actions.Num() > 0 ? Actions[0] : nullptr;
}

UYSHandAtkBonusUpgrade::UYSHandAtkBonusUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "HandAtkBonusName", "Damage bonus");
	Description = NSLOCTEXT("YS", "HandAtkBonusDesc", "Augmente les dégâts de l'action");
	BonusName = NSLOCTEXT("YS", "HandAtkBonusBonus", "Damage bonus");
	StatLabel = NSLOCTEXT("YS", "HandAtkBonusStat", "Dégât");
}

float UYSHandAtkBonusUpgrade::GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const
{
	const UYSAbilityComponent* Action = GetSlotAction(Character, Variant);
	return Action ? Action->GetBaseDamage() : 0.0f;
}

void UYSHandAtkBonusUpgrade::ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const
{
	Character->SetDamageMultiplier(GetSlot(Variant), 1.0f + TotalBonus);
}

UYSHandCooldownReductionUpgrade::UYSHandCooldownReductionUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "HandCooldownReductionName", "CD reduction");
	BonusName = NSLOCTEXT("YS", "HandCooldownReductionBonus", "CD reduction");
	StatLabel = NSLOCTEXT("YS", "HandCooldownReductionStat", "Recharge (s)");
	AutoAtkBonusName = NSLOCTEXT("YS", "HandCooldownReductionAutoAtk", "Atk speed");
	Description = NSLOCTEXT("YS", "HandCooldownReductionDesc", "Réduit la recharge de l'Ability");
	AutoAtkDescription = NSLOCTEXT("YS", "AtkSpeedDesc", "Les attaques auto sont plus rapides");
	BronzeValue = 0.03f;
	SilverValue = 0.045f;
	GoldValue = 0.06f;
}

FText UYSHandCooldownReductionUpgrade::MakeName(const FText& WeaponsText, int32 Variant) const
{
	// la CD reduction des attaques auto s'appelle Atk speed (pas de « Auto atk » dans le nom)
	if (GetSlot(Variant) != EYSActionSlot::AutoAtk || AutoAtkBonusName.IsEmpty())
	{
		return Super::MakeName(WeaponsText, Variant);
	}
	return WeaponsText.IsEmpty() ? AutoAtkBonusName : FText::Format(NSLOCTEXT("YS", "AtkSpeedName", "{0} {1}"), WeaponsText, AutoAtkBonusName);
}

float UYSHandCooldownReductionUpgrade::GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const
{
	const UYSAbilityComponent* Action = GetSlotAction(Character, Variant);
	return Action ? Action->GetBaseCooldown() : 0.0f;
}

FText UYSHandCooldownReductionUpgrade::GetDescription(const AYSPlayerCharacter* Character, int32 Variant) const
{
	return GetSlot(Variant) == EYSActionSlot::AutoAtk && !AutoAtkDescription.IsEmpty() ? AutoAtkDescription : Description;
}

void UYSHandCooldownReductionUpgrade::GetOfferCard(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, FYSUpgradeCard& OutCard) const
{
	Super::GetOfferCard(Character, Variant, Progress, Rarity, OutCard);

	// la recharge (première valeur ; la deuxième = dégâts critiques de l'Atk speed) : réduction totale appliquée à côté
	if (OutCard.StatChanges.Num() == 0)
	{
		return;
	}
	FNumberFormattingOptions Format;
	Format.MaximumFractionalDigits = 1;
	auto AppendReduction = [&Format](FText& Value, float Multiplier)
	{
		Value = FText::Format(NSLOCTEXT("YS", "CooldownReductionValue", "{0} (-{1} %)"), Value, FText::AsNumber((1.0f - Multiplier) * 100.0f, &Format));
	};
	AppendReduction(OutCard.StatChanges[0].OldValue, GetMultiplier(Progress.TotalValue));
	AppendReduction(OutCard.StatChanges[0].NewValue, GetMultiplier(Progress.TotalValue + GetBonusValue(Variant, Rarity)));
}

void UYSHandCooldownReductionUpgrade::ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const
{
	Character->SetCooldownMultiplier(GetSlot(Variant), GetMultiplier(TotalBonus));
}

bool UYSHandCooldownReductionUpgrade::IsActionEligible(const UYSAbilityComponent* Action) const
{
	// l'Auto atk des Guns a l'upgrade Shoot system à la place ; l'Ability des Guns garde la CD reduction
	return Action->WeaponType != EYSWeaponType::Gun || !Action->IsAutomatic();
}

UYSShootSystemUpgrade::UYSShootSystemUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "ShootSystemName", "Shoot system");
	Description = NSLOCTEXT("YS", "ShootSystemDesc", "Améliore le tir de l'attaque auto");
	BonusName = NSLOCTEXT("YS", "ShootSystemBonus", "Shoot system");
	StatLabel = NSLOCTEXT("YS", "ShootSystemStat", "Balles par rafale");
	ShotgunStatLabel = NSLOCTEXT("YS", "ShootSystemShotgunStat", "Shotgun AMMO par tir");
	SniperStatLabel = NSLOCTEXT("YS", "ShootSystemSniperStat", "NME touchés par balle");
	BronzeValue = 1.0f;
	SilverValue = 2.0f;
	GoldValue = 3.0f;
}

bool UYSShootSystemUpgrade::IsActionEligible(const UYSAbilityComponent* Action) const
{
	const UYSCoupDPetoirAbility* Gun = Cast<UYSCoupDPetoirAbility>(Action);
	return Gun && Gun->WeaponType == EYSWeaponType::Gun && Gun->IsAutomatic()
		&& (Gun->ShootSystem == EYSShootSystem::PewPew || Gun->ShootSystem == EYSShootSystem::Shotgun || Gun->ShootSystem == EYSShootSystem::Sniper);
}

float UYSShootSystemUpgrade::GetAmmoPerBonus(const AYSPlayerCharacter* Character, int32 Variant) const
{
	const UYSCoupDPetoirAbility* Gun = Cast<UYSCoupDPetoirAbility>(GetSlotAction(Character, Variant));
	if (!Gun)
	{
		return 1.0f;
	}
	switch (Gun->ShootSystem)
	{
	case EYSShootSystem::Shotgun:	return ShotgunAmmoPerLevel;
	case EYSShootSystem::Sniper:	return SniperHitsPerLevel;
	default:						return 1.0f;
	}
}

float UYSShootSystemUpgrade::GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const
{
	const UYSCoupDPetoirAbility* Gun = Cast<UYSCoupDPetoirAbility>(GetSlotAction(Character, Variant));
	if (!Gun)
	{
		return 0.0f;
	}
	switch (Gun->ShootSystem)
	{
	case EYSShootSystem::Shotgun:	return Gun->ShotgunAmmoPerShot;
	case EYSShootSystem::Sniper:	return Gun->SniperMaxHits;
	default:						return Gun->BulletsPerBurst;
	}
}

FText UYSShootSystemUpgrade::GetStatLabel(const AYSPlayerCharacter* Character, int32 Variant) const
{
	const UYSCoupDPetoirAbility* Gun = Cast<UYSCoupDPetoirAbility>(GetSlotAction(Character, Variant));
	if (!Gun)
	{
		return StatLabel;
	}
	switch (Gun->ShootSystem)
	{
	case EYSShootSystem::Shotgun:	return ShotgunStatLabel;
	case EYSShootSystem::Sniper:	return SniperStatLabel;
	default:						return StatLabel;
	}
}

float UYSShootSystemUpgrade::GetDisplayValue(const AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const
{
	return GetStatValue(GetBaseStatValue(Character, Variant), TotalBonus * GetAmmoPerBonus(Character, Variant));
}

void UYSShootSystemUpgrade::ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const
{
	// le bonus est gardé en points de rareté (1 / 2 / 3) : converti en balles selon le shoot system des armes tenues
	Character->SetShootSystemBonus(GetSlot(Variant), TotalBonus * GetAmmoPerBonus(Character, Variant));
}

UYSSummonSystemUpgrade::UYSSummonSystemUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "SummonSystemName", "Summon system");
	Description = NSLOCTEXT("YS", "SummonSystemDesc", "Plus de minions en même temps");
	BonusName = NSLOCTEXT("YS", "SummonSystemBonus", "Summon system");
	StatLabel = NSLOCTEXT("YS", "SummonSystemStat", "Minions max");
	bAutoAtkVariant = false;
	BronzeValue = 1.0f;
	SilverValue = 1.0f;
	GoldValue = 1.0f;
}

bool UYSSummonSystemUpgrade::IsActionEligible(const UYSAbilityComponent* Action) const
{
	return Cast<UYSSummonCrewAbility>(Action) != nullptr;
}

float UYSSummonSystemUpgrade::GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const
{
	const UYSSummonCrewAbility* Summon = Cast<UYSSummonCrewAbility>(GetSlotAction(Character, Variant));
	return Summon ? Summon->BaseMaxMinions : 0.0f;
}

FText UYSSummonSystemUpgrade::MakeName(const FText& WeaponsText, int32 Variant) const
{
	return WeaponsText.IsEmpty() ? BonusName : FText::Format(NSLOCTEXT("YS", "SummonSystemFullName", "{0} {1}"), WeaponsText, BonusName);
}

void UYSSummonSystemUpgrade::ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const
{
	Character->SetSummonSystemBonus(TotalBonus);
}

UYSMoveSpeedBonusUpgrade::UYSMoveSpeedBonusUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "MoveSpeedBonusName", "Move speed bonus");
	Description = NSLOCTEXT("YS", "MoveSpeedBonusDesc", "Augmente la vitesse de déplacement");
	OfferWeight = 5.0f;
	StatLabel = NSLOCTEXT("YS", "MoveSpeedBonusStat", "Vitesse");
	BronzeValue = 0.025f;
	SilverValue = 0.05f;
	GoldValue = 0.1f;
}

float UYSMoveSpeedBonusUpgrade::GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const
{
	return Character ? Character->GetBaseMoveSpeed() : 0.0f;
}

void UYSMoveSpeedBonusUpgrade::ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const
{
	Character->SetMoveSpeedMultiplier(1.0f + TotalBonus);
}

UYSHealthBonusUpgrade::UYSHealthBonusUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "HealthBonusName", "Hps bonus");
	Description = NSLOCTEXT("YS", "HealthBonusDesc", "Augmente les PV max");
	OfferWeight = 5.0f;
	StatLabel = NSLOCTEXT("YS", "HealthBonusStat", "PV max");
	BronzeValue = 0.05f;
	SilverValue = 0.1f;
	GoldValue = 0.2f;
}

float UYSHealthBonusUpgrade::GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const
{
	return Character ? Character->GetBaseMaxHealth() : 0.0f;
}

void UYSHealthBonusUpgrade::ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const
{
	Character->SetMaxHealthMultiplier(1.0f + TotalBonus);
}

UYSHealthRegenUpgrade::UYSHealthRegenUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "HealthRegenName", "Hps regen");
	Description = NSLOCTEXT("YS", "HealthRegenDesc", "Rend plus de PV par seconde\nquand on ne subit pas de dégât");
	StatLabel = NSLOCTEXT("YS", "HealthRegenStat", "Regen PV/s");
	OfferWeight = 5.0f;
	BronzeValue = 1.0f;
	SilverValue = 2.0f;
	GoldValue = 3.0f;
}

float UYSHealthRegenUpgrade::GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const
{
	return Character ? Character->GetBaseHealthRegen() : 0.0f;
}

void UYSHealthRegenUpgrade::ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const
{
	// regen de base du personnage + somme des bonus
	Character->SetHealthRegen(GetStatValue(GetBaseStatValue(Character, Variant), TotalBonus));
}

UYSArmorUpgrade::UYSArmorUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "ArmorName", "Armor");
	Description = NSLOCTEXT("YS", "ArmorDesc", "Réduit les dégâts de chaque coup reçu");
	StatLabel = NSLOCTEXT("YS", "ArmorStat", "Armure");
	OfferWeight = 5.0f;
	BronzeValue = 1.0f;
	SilverValue = 2.0f;
	GoldValue = 3.0f;
}

void UYSArmorUpgrade::ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const
{
	Character->SetArmor(TotalBonus);
}

UYSLuckUpgrade::UYSLuckUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "LuckName", "Luck");
	Description = NSLOCTEXT("YS", "LuckDesc", "Plus de chances d'obtenir des upgrades Gold");
	StatLabel = NSLOCTEXT("YS", "LuckStat", "Chance Gold (%)");
	OfferWeight = 10.0f;
	bCanBeBronze = false;
	bCanBeGold = false;
	BronzeValue = 5.0f;
	SilverValue = 5.0f;
	GoldValue = 5.0f;
}

float UYSLuckUpgrade::GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const
{
	return Character ? Character->GetBaseRarityChance(EYSUpgradeRarity::Gold) : 0.0f;
}

void UYSLuckUpgrade::ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const
{
	Character->SetLuck(TotalBonus);
}

UYSCriticalDamageUpgrade::UYSCriticalDamageUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "CriticalDamageName", "Dégât critique");
	Description = NSLOCTEXT("YS", "CriticalDamageDesc", "Augmente les dégâts des coups critiques");
	StatLabel = NSLOCTEXT("YS", "CriticalDamageStat", "Dégâts critiques (%)");
	OfferWeight = 10.0f;
	BronzeValue = 0.4f;
	SilverValue = 0.65f;
	GoldValue = 0.9f;
}

bool UYSCriticalDamageUpgrade::CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	return Super::CanBeOffered(Character, Variant, Progress) && Character && Character->CanCriticalHit();
}

float UYSCriticalDamageUpgrade::GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const
{
	return Character ? Character->GetBaseCriticalDamageMultiplier() : 1.5f;
}

float UYSCriticalDamageUpgrade::GetDisplayValue(const AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const
{
	const float BonusMultiplier = Character ? 1.0f + Character->GetCriticalDamageBonus() : 1.0f;
	return GetStatValue(GetBaseStatValue(Character, Variant), TotalBonus) * BonusMultiplier * 100.0f;
}

void UYSCriticalDamageUpgrade::ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const
{
	Character->SetCriticalDamageAdd(TotalBonus);
}

UYSWeaponUpgrade::UYSWeaponUpgrade()
{
	UpgradeType = EYSUpgradeType::Weapon;
}

FText UYSWeaponUpgrade::GetVariantName(int32 Variant) const
{
	switch (GetTier(Variant))
	{
	case EYSWeaponUpgradeTier::Unlock:	return UnlockName;
	case EYSWeaponUpgradeTier::Bonus:	return BonusName;
	default:							return SuperName;
	}
}

FText UYSWeaponUpgrade::GetVariantNameFor(const AYSPlayerCharacter* Character, int32 Variant) const
{
	// « Pistolet Ricochet » : armes du set concernées + palier
	const FText Weapons = YSUpgradeText::WeaponsName(GetRequiredActions(Character));
	return Weapons.IsEmpty() ? GetVariantName(Variant) : FText::Format(NSLOCTEXT("YS", "WeaponUpgradeName", "{0} {1}"), Weapons, GetVariantName(Variant));
}

TArray<UYSAbilityComponent*> UYSWeaponUpgrade::GetRequiredActions(const AYSPlayerCharacter* Character) const
{
	TArray<UYSAbilityComponent*> Required;
	if (!Character || !RequiredActionClass)
	{
		return Required;
	}

	for (const AYSWeapon* Weapon : { Character->GetMainHandWeapon(), Character->GetOffHandWeapon() })
	{
		// armes concernées / exclues par l'asset (ex. Bloodlust sans la GreatAxe, BloodRage seulement la GreatAxe)
		const auto IsWeaponOf = [Weapon](const TSubclassOf<AYSWeapon>& Class) { return Weapon && Class && Weapon->IsA(Class); };
		if (!Weapon || (RequiredWeaponClasses.Num() > 0 && !RequiredWeaponClasses.ContainsByPredicate(IsWeaponOf)) || ExcludedWeaponClasses.ContainsByPredicate(IsWeaponOf))
		{
			continue;
		}
		TInlineComponentArray<UYSAbilityComponent*> Actions(Weapon);
		for (UYSAbilityComponent* Action : Actions)
		{
			const bool bExcluded = ExcludedActionClasses.ContainsByPredicate([Action](const TSubclassOf<UYSAbilityComponent>& Excluded) { return Excluded && Action->IsA(Excluded); });
			if (Action->IsA(RequiredActionClass) && Action->GetActionSlot() == RequiredSlot && !bExcluded)
			{
				Required.Add(Action);
			}
		}
	}
	return Required;
}

bool UYSWeaponUpgrade::CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	if (!Character || GetRequiredActions(Character).Num() == 0)
	{
		return false;
	}

	// jumelles (Ricochet / Bloodlust) : jamais l'une si le joueur a déjà l'autre ; upgrade requise
	if (!PassesRequirements(Character))
	{
		return false;
	}

	// le déblocage d'abord ; ensuite il laisse la place au Bonus (cumulable) et au Super (une fois)
	const bool bUnlocked = Character->GetUpgradeProgress(Progress.PoolIndex, MakeVariant(EYSWeaponUpgradeTier::Unlock)).Stacks > 0;
	switch (GetTier(Variant))
	{
	case EYSWeaponUpgradeTier::Unlock:	return !bUnlocked;
	case EYSWeaponUpgradeTier::Bonus:	return bUnlocked && Progress.Stacks < MaxBonusStacks;
	default:							return bUnlocked && Progress.Stacks == 0;
	}
}

float UYSWeaponUpgrade::GetOfferWeight(int32 Variant) const
{
	switch (GetTier(Variant))
	{
	case EYSWeaponUpgradeTier::Unlock:	return UnlockWeight;
	case EYSWeaponUpgradeTier::Bonus:	return BonusWeight;
	default:							return SuperWeight;
	}
}

bool UYSWeaponUpgrade::SupportsRarity(int32 Variant, EYSUpgradeRarity Rarity) const
{
	switch (GetTier(Variant))
	{
	case EYSWeaponUpgradeTier::Unlock:	return Rarity == EYSUpgradeRarity::Silver;
	case EYSWeaponUpgradeTier::Bonus:	return Rarity == EYSUpgradeRarity::Silver || Rarity == EYSUpgradeRarity::Gold;
	default:							return Rarity == EYSUpgradeRarity::Gold;
	}
}

float UYSWeaponUpgrade::GetBonusValue(int32 Variant, EYSUpgradeRarity Rarity) const
{
	if (GetTier(Variant) != EYSWeaponUpgradeTier::Bonus)
	{
		return 0.0f;
	}
	return Rarity == EYSUpgradeRarity::Gold ? BonusGoldValue : Rarity == EYSUpgradeRarity::Silver ? BonusSilverValue : 0.0f;
}

FText UYSWeaponUpgrade::GetDescription(const AYSPlayerCharacter* Character, int32 Variant) const
{
	switch (GetTier(Variant))
	{
	case EYSWeaponUpgradeTier::Unlock:	return UnlockDescription;
	case EYSWeaponUpgradeTier::Bonus:	return BonusDescription.IsEmpty() ? Description : BonusDescription;
	default:							return SuperDescription;
	}
}

void UYSWeaponUpgrade::GetOfferCard(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, FYSUpgradeCard& OutCard) const
{
	Super::GetOfferCard(Character, Variant, Progress, Rarity, OutCard);
	if (GetTier(Variant) != EYSWeaponUpgradeTier::Bonus)
	{
		return;
	}

	// bonus : comme une upgrade de stat (valeur concrète actuelle et nouvelle)
	OutCard.StackText = FText::Format(NSLOCTEXT("YS", "StatUpgradeStacks", "Bonus {0} / {1}"), Progress.Stacks + 1, MaxBonusStacks);
	FYSUpgradeStatChange& Change = OutCard.StatChanges.AddDefaulted_GetRef();
	Change.Label = BonusStatLabel;
	Change.OldValue = YSUpgradeText::FormatStat(GetBonusDisplayValue(Character, Progress.TotalValue));
	Change.NewValue = YSUpgradeText::FormatStat(GetBonusDisplayValue(Character, Progress.TotalValue + GetBonusValue(Variant, Rarity)));
}

void UYSWeaponUpgrade::Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	if (!Character)
	{
		return;
	}

	const bool bUnlocked = Character->GetUpgradeProgress(Progress.PoolIndex, MakeVariant(EYSWeaponUpgradeTier::Unlock)).Stacks > 0;
	const float BonusTotal = Character->GetUpgradeProgress(Progress.PoolIndex, MakeVariant(EYSWeaponUpgradeTier::Bonus)).TotalValue;
	const bool bSuper = Character->GetUpgradeProgress(Progress.PoolIndex, MakeVariant(EYSWeaponUpgradeTier::Super)).Stacks > 0;
	ApplyWeaponProperty(Character, bUnlocked, BonusTotal, bSuper);
	UE_LOG(LogTemp, Log, TEXT("[YSWeaponUpgrade] %s : %s (débloqué %d, bonus %.3f, super %d)"), *Character->GetName(), *GetVariantNameFor(Character, Variant).ToString(), bUnlocked, BonusTotal, bSuper);
}

const FName UYSRicochetUpgrade::RicochetCountStat(TEXT("Ricochets"));
const FName UYSRicochetUpgrade::RicochetDamageBonusStat(TEXT("RicochetDamageBonus"));

UYSRicochetUpgrade::UYSRicochetUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "RicochetName", "Ricochet");
	RequiredActionClass = UYSCoupDPetoirAbility::StaticClass();
	ExcludedActionClasses.Add(UYSRollAndBombAbility::StaticClass());
	UnlockName = NSLOCTEXT("YS", "RicochetUnlockName", "Ricochet");
	UnlockDescription = NSLOCTEXT("YS", "RicochetUnlockDesc", "Après avoir touché, la balle ricoche\nvers le NME le plus proche");
	BonusDescription = NSLOCTEXT("YS", "RicochetBonusDesc", "Chaque ricochet fait plus de dégâts");
	BonusName = NSLOCTEXT("YS", "RicochetBonusName", "Ricochet bonus");
	BonusStatLabel = NSLOCTEXT("YS", "RicochetBonusStat", "Dégât après ricochet");
	BonusSilverValue = 0.15f;
	BonusGoldValue = 0.25f;
	SuperName = NSLOCTEXT("YS", "RicochetSuperName", "Golden ricochet");
	SuperDescription = NSLOCTEXT("YS", "RicochetSuperDesc", "La balle ricoche une fois de plus");
}

float UYSRicochetUpgrade::GetBonusBaseValue(const AYSPlayerCharacter* Character) const
{
	const TArray<UYSAbilityComponent*> Actions = GetRequiredActions(Character);
	return Actions.Num() > 0 ? Actions[0]->GetBaseDamage() : 0.0f;
}

void UYSRicochetUpgrade::ApplyWeaponProperty(AYSPlayerCharacter* Character, bool bUnlocked, float BonusTotal, bool bSuper) const
{
	const int32 Ricochets = bUnlocked ? UnlockRicochets + (bSuper ? SuperExtraRicochets : 0) : 0;
	Character->SetWeaponStat(RicochetCountStat, Ricochets);
	Character->SetWeaponStat(RicochetDamageBonusStat, BonusTotal);
}

UYSBloodlustUpgrade::UYSBloodlustUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "BloodlustName", "Bloodlust");
	RequiredActionClass = UYSAutoSwingAbility::StaticClass();
	RequiredSlot = EYSActionSlot::AutoAtk;
	UnlockName = NSLOCTEXT("YS", "BloodlustUnlockName", "Bloodlust");
	UnlockDescription = NSLOCTEXT("YS", "BloodlustUnlockDesc", "Chaque Auto swing qui touche donne une BL stack (max 5) :\n+5 % de vitesse d'atk par stack, perdues après 3 s sans toucher");
	BonusDescription = NSLOCTEXT("YS", "BloodlustBonusDesc", "Plus de vitesse d'atk par BL stack");
	BonusName = NSLOCTEXT("YS", "BloodlustBonusName", "Bloodlust Atk speed");
	BonusStatLabel = NSLOCTEXT("YS", "BloodlustBonusStat", "Atk speed par BL stack (%)");
	BonusSilverValue = 0.01f;
	BonusGoldValue = 0.02f;
	SuperName = NSLOCTEXT("YS", "BloodlustSuperName", "Golden bloodlust");
	SuperDescription = NSLOCTEXT("YS", "BloodlustSuperDesc", "Avec toutes ses BL stacks,\nles Auto swing sont critiques");
}

void UYSBloodlustUpgrade::ApplyWeaponProperty(AYSPlayerCharacter* Character, bool bUnlocked, float BonusTotal, bool bSuper) const
{
	Character->SetBloodlust(bUnlocked ? MaxStacks : 0, StackDuration, AtkSpeedPerStack + BonusTotal, bUnlocked && bSuper);
}

UYSBloodRageUpgrade::UYSBloodRageUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "BloodRageName", "BloodRage");
	RequiredActionClass = UYSAutoSwingAbility::StaticClass();
	RequiredSlot = EYSActionSlot::AutoAtk;
	UnlockName = NSLOCTEXT("YS", "BloodRageUnlockName", "BloodRage");
	UnlockDescription = NSLOCTEXT("YS", "BloodRageUnlockDesc", "Chaque coup reçu donne une stack de rage (max 5) :\nau maximum, la prochaine attaque est critique.\nStacks perdues après 10 s sans subir de coup");
	BonusDescription = NSLOCTEXT("YS", "BloodRageBonusDesc", "Armure en plus par stack de rage");
	BonusName = NSLOCTEXT("YS", "BloodRageBonusName", "BloodRage ++");
	BonusStatLabel = NSLOCTEXT("YS", "BloodRageBonusStat", "Armure par stack de rage");
	BonusSilverValue = 5.0f;
	BonusGoldValue = 10.0f;
	SuperName = NSLOCTEXT("YS", "BloodRageSuperName", "Golden BloodRage");
	SuperDescription = NSLOCTEXT("YS", "BloodRageSuperDesc", "Les stacks de rage ne se perdent plus avec le temps");
}

void UYSBloodRageUpgrade::ApplyWeaponProperty(AYSPlayerCharacter* Character, bool bUnlocked, float BonusTotal, bool bSuper) const
{
	Character->SetBloodRage(bUnlocked ? MaxStacks : 0, StackDuration, BonusTotal, bUnlocked && bSuper);
}

const FName UYSMineUpgrade::MineStat(TEXT("Mine"));
const FName UYSMineUpgrade::MinePushbackStat(TEXT("MinePushback"));
const FName UYSMineUpgrade::MineDoubleStat(TEXT("MineDouble"));

UYSMineUpgrade::UYSMineUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "MineName", "Une Mine !");
	RequiredActionClass = UYSRollAndBombAbility::StaticClass();
	UnlockName = NSLOCTEXT("YS", "MineUnlockName", "Une Mine !");
	UnlockDescription = NSLOCTEXT("YS", "MineUnlockDesc", "La bombe devient une mine : elle s'arme au lieu d'exploser,\npuis explose dès qu'un NME entre dans sa zone");
	BonusDescription = NSLOCTEXT("YS", "MineBonusDesc", "Les mines repoussent les NME");
	BonusName = NSLOCTEXT("YS", "MineBonusName", "Mine repulsante");
	BonusStatLabel = NSLOCTEXT("YS", "MineBonusStat", "Pushback des mines");
	BonusSilverValue = 100.0f;
	BonusGoldValue = 175.0f;
	SuperName = NSLOCTEXT("YS", "MineSuperName", "Golden Mine");
	SuperDescription = NSLOCTEXT("YS", "MineSuperDesc", "Les mines sont lancées par 2");
}

void UYSMineUpgrade::ApplyWeaponProperty(AYSPlayerCharacter* Character, bool bUnlocked, float BonusTotal, bool bSuper) const
{
	Character->SetWeaponStat(MineStat, bUnlocked ? 1.0f : 0.0f);
	Character->SetWeaponStat(MinePushbackStat, bUnlocked ? BonusTotal : 0.0f);
	Character->SetWeaponStat(MineDoubleStat, bUnlocked && bSuper ? 1.0f : 0.0f);
}

const FName UYSYoloUpgrade::YoloStat(TEXT("Yolo"));
const FName UYSYoloUpgrade::YoloSizeStat(TEXT("YoloSize"));
const FName UYSYoloUpgrade::YoloAtkSpeedStat(TEXT("YoloAtkSpeed"));

UYSYoloUpgrade::UYSYoloUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "YoloName", "Yoloooooo");
	RequiredActionClass = UYSAutoSwingAbility::StaticClass();
	RequiredSlot = EYSActionSlot::AutoAtk;
	UnlockName = NSLOCTEXT("YS", "YoloUnlockName", "Yoloooooo");
	UnlockDescription = NSLOCTEXT("YS", "YoloUnlockDesc", "L'attaque auto part en continu,\nmême sans cible");
	BonusDescription = NSLOCTEXT("YS", "YoloBonusDesc", "Chaîne, boulet et hitbox plus grands");
	BonusName = NSLOCTEXT("YS", "YoloBonusName", "Yoloooooo++");
	BonusStatLabel = NSLOCTEXT("YS", "YoloBonusStat", "Taille du boulet et de la chaîne (+%)");
	BonusSilverValue = 0.2f;
	BonusGoldValue = 0.35f;
	SuperName = NSLOCTEXT("YS", "YoloSuperName", "Golden Yoloooooo");
	SuperDescription = NSLOCTEXT("YS", "YoloSuperDesc", "Atk speed du swing +50 %");
}

void UYSYoloUpgrade::ApplyWeaponProperty(AYSPlayerCharacter* Character, bool bUnlocked, float BonusTotal, bool bSuper) const
{
	Character->SetWeaponStat(YoloStat, bUnlocked ? 1.0f : 0.0f);
	Character->SetWeaponStat(YoloSizeStat, bUnlocked ? BonusTotal : 0.0f);
	Character->SetWeaponStat(YoloAtkSpeedStat, bUnlocked && bSuper ? SuperAtkSpeed : 0.0f);
}

const FName UYSAllMyMinionsUpgrade::AllMyMinionsStat(TEXT("AllMyMinions"));
const FName UYSAllMyMinionsUpgrade::ForcedDamageStat(TEXT("ForcedDamage"));
const FName UYSAllMyMinionsUpgrade::ForcedCooldownResetStat(TEXT("ForcedCooldownReset"));

UYSAllMyMinionsUpgrade::UYSAllMyMinionsUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "AllMyMinionsName", "You're all my minions");
	RequiredActionClass = UYSGoMinionsAbility::StaticClass();
	UnlockName = NSLOCTEXT("YS", "AllMyMinionsUnlockName", "You're all my minions");
	UnlockDescription = NSLOCTEXT("YS", "AllMyMinionsUnlockDesc", "Allez-y mes minions ! force aussi l'Ability\ndes joueurs proches du Captain (sans leur recharge)");
	BonusDescription = NSLOCTEXT("YS", "AllMyMinionsBonusDesc", "Les abilities forcées font plus de dégâts");
	BonusName = NSLOCTEXT("YS", "AllMyMinionsBonusName", "You're all my minions ++");
	BonusStatLabel = NSLOCTEXT("YS", "AllMyMinionsBonusStat", "Dégâts des abilities forcées (+%)");
	BonusSilverValue = 0.2f;
	BonusGoldValue = 0.35f;
	SuperName = NSLOCTEXT("YS", "AllMyMinionsSuperName", "Golden minions");
	SuperDescription = NSLOCTEXT("YS", "AllMyMinionsSuperDesc", "Les abilities forcées rendent aussi leur recharge");
}

void UYSAllMyMinionsUpgrade::ApplyWeaponProperty(AYSPlayerCharacter* Character, bool bUnlocked, float BonusTotal, bool bSuper) const
{
	Character->SetWeaponStat(AllMyMinionsStat, bUnlocked ? 1.0f : 0.0f);
	Character->SetWeaponStat(ForcedDamageStat, bUnlocked ? BonusTotal : 0.0f);
	Character->SetWeaponStat(ForcedCooldownResetStat, bUnlocked && bSuper ? 1.0f : 0.0f);
}

UYSClassUpgrade::UYSClassUpgrade()
{
	UpgradeType = EYSUpgradeType::Class;
	OfferWeight = 0.0f;
}

FText UYSClassUpgrade::GetVariantName(int32 Variant) const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	if (TargetWeaponSet)
	{
		return TargetWeaponSet->DisplayName.IsEmpty() ? FText::FromString(TargetWeaponSet->GetName()) : TargetWeaponSet->DisplayName;
	}
	return TargetClass ? TargetClass->GetDefaultObject<AYSPlayerCharacter>()->GetCharacterName() : FText::GetEmpty();
}

bool UYSClassUpgrade::CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	if (!Character || Progress.Stacks > 0 || (!TargetWeaponSet && !TargetClass))
	{
		return false;
	}
	const bool bSetMatches = !SourceWeaponSet || Character->GetWeaponSet() == SourceWeaponSet;
	const bool bClassMatches = !SourceClass || Character->GetClass() == SourceClass || ExtraSourceClasses.Contains(Character->GetClass());
	return bSetMatches && bClassMatches;
}

void UYSClassUpgrade::GetOfferCard(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, FYSUpgradeCard& OutCard) const
{
	OutCard.Name = GetVariantName(Variant);
	OutCard.RarityText = NSLOCTEXT("YS", "ClassUpgradeKind", "Classe");
	OutCard.Color = GoldColor;
	OutCard.Icon = Icon;
	OutCard.Description = Description;
}

void UYSClassUpgrade::Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	if (Character && TargetWeaponSet)
	{
		Character->ChangeWeaponSet(TargetWeaponSet);
	}
	else if (Character && TargetClass)
	{
		Character->ChangeClass(TargetClass);
	}
}


UYSCriticalAttackUpgrade::UYSCriticalAttackUpgrade()
{
	UpgradeType = EYSUpgradeType::Special;
	DisplayName = NSLOCTEXT("YS", "CriticalAttackName", "Atk critique");
	SwingDescription = NSLOCTEXT("YS", "CriticalAttackSwing", "La {0}e atk est toujours critique");
	BulletDescription = NSLOCTEXT("YS", "CriticalAttackBullet", "La {0}e balle sur un même NME\nest toujours critique");
	ShotgunDescription = NSLOCTEXT("YS", "CriticalAttackShotgun", "Les AMMO sont critiques sur les NME\nà moins de {0} du joueur");
	SniperDescription = NSLOCTEXT("YS", "CriticalAttackSniper", "Les balles sont critiques sur les NME\nà plus de {0} du joueur");
	bCanBeBronze = false;
	bCanBeSilver = false;
}

bool UYSCriticalAttackUpgrade::HasSwingAutoAttack(const AYSPlayerCharacter* Character)
{
	for (const AYSWeapon* Weapon : Character ? Character->GetHeldWeapons() : TArray<AYSWeapon*>())
	{
		const UYSAutoSwingAbility* Swing = Weapon ? Cast<UYSAutoSwingAbility>(Weapon->GetAutoAction()) : nullptr;
		if (Swing && Swing->MeleeSystem == EYSMeleeSystem::Swing)
		{
			return true;
		}
	}
	return false;
}

bool UYSCriticalAttackUpgrade::HasGunAutoAttack(const AYSPlayerCharacter* Character, EYSShootSystem System)
{
	for (const AYSWeapon* Weapon : Character ? Character->GetHeldWeapons() : TArray<AYSWeapon*>())
	{
		const UYSCoupDPetoirAbility* Gun = Weapon ? Cast<UYSCoupDPetoirAbility>(Weapon->GetAutoAction()) : nullptr;
		if (Gun && Gun->WeaponType == EYSWeaponType::Gun && Gun->ShootSystem == System)
		{
			return true;
		}
	}
	return false;
}

bool UYSCriticalAttackUpgrade::HasPewPewAutoAttack(const AYSPlayerCharacter* Character)
{
	return HasGunAutoAttack(Character, EYSShootSystem::PewPew);
}

bool UYSCriticalAttackUpgrade::HasShotgunAutoAttack(const AYSPlayerCharacter* Character)
{
	return HasGunAutoAttack(Character, EYSShootSystem::Shotgun);
}

bool UYSCriticalAttackUpgrade::HasSniperAutoAttack(const AYSPlayerCharacter* Character)
{
	return HasGunAutoAttack(Character, EYSShootSystem::Sniper);
}

bool UYSCriticalAttackUpgrade::IsCompatibleWith(const AYSPlayerCharacter* Character, int32 Variant) const
{
	return HasSwingAutoAttack(Character) || HasPewPewAutoAttack(Character) || HasShotgunAutoAttack(Character) || HasSniperAutoAttack(Character);
}

bool UYSCriticalAttackUpgrade::CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	// unique : une seule fois, pour un joueur qui a une attaque auto Swing, PewPew, Shotgun ou Sniper
	return Progress.Stacks == 0 && PassesRequirements(Character) && IsCompatibleWith(Character, Variant);
}

void UYSCriticalAttackUpgrade::GetOfferCard(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, FYSUpgradeCard& OutCard) const
{
	Super::GetOfferCard(Character, Variant, Progress, Rarity, OutCard);

	// description selon l'attaque auto du joueur (toutes celles qu'il a)
	TArray<FString> Lines;
	if (HasSwingAutoAttack(Character))
	{
		Lines.Add(FText::Format(SwingDescription, SwingCriticalInterval).ToString());
	}
	if (HasPewPewAutoAttack(Character))
	{
		Lines.Add(FText::Format(BulletDescription, BulletCriticalInterval).ToString());
	}
	if (HasShotgunAutoAttack(Character))
	{
		Lines.Add(FText::Format(ShotgunDescription, FMath::RoundToInt(ShotgunCriticalMaxDistance)).ToString());
	}
	if (HasSniperAutoAttack(Character))
	{
		Lines.Add(FText::Format(SniperDescription, FMath::RoundToInt(SniperCriticalMinDistance)).ToString());
	}
	if (Lines.Num() > 0)
	{
		OutCard.Description = FText::FromString(FString::Join(Lines, TEXT("\n")));
	}
}

void UYSCriticalAttackUpgrade::Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	if (Character && Progress.Stacks > 0)
	{
		Character->SetAutoAttackCriticals(SwingCriticalInterval, BulletCriticalInterval, ShotgunCriticalMaxDistance, SniperCriticalMinDistance);
		UE_LOG(LogTemp, Log, TEXT("[YSCritical] %s : Atk critique (1 swing sur %d, 1 balle sur %d par NME, Shotgun à moins de %.0f, Sniper à plus de %.0f)"), *Character->GetName(),
			SwingCriticalInterval, BulletCriticalInterval, ShotgunCriticalMaxDistance, SniperCriticalMinDistance);
	}
}
