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

void UYSUpgrade::GetOfferLines(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, TArray<FYSChoiceLine>& OutLines) const
{
	FYSChoiceLine& Title = OutLines.AddDefaulted_GetRef();
	Title.Text = GetVariantName(Variant);
	Title.FontSize = 20;
	Title.bBold = true;
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

void UYSStatUpgrade::GetOfferLines(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, TArray<FYSChoiceLine>& OutLines) const
{
	// nom (couleur de la rareté) + rareté et nombre de bonus après ce choix
	FYSChoiceLine& Title = OutLines.AddDefaulted_GetRef();
	Title.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeTitle", "{0} ({1})"), GetVariantNameFor(Character, Variant), YSUpgradeText::RarityName(Rarity));
	Title.Color = GetRarityColor(Rarity);
	Title.FontSize = 20;
	Title.bBold = true;

	FYSChoiceLine& StackLine = OutLines.AddDefaulted_GetRef();
	StackLine.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeStacks", "Bonus {0} / {1}"), Progress.Stacks + 1, MaxStacks);
	StackLine.Color = FLinearColor(0.7f, 0.7f, 0.7f);
	StackLine.FontSize = 13;

	// valeurs concrètes : actuelle rayée, nouvelle en gras

	FYSChoiceLine& OldValue = OutLines.AddDefaulted_GetRef();
	OldValue.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeValue", "{0} : {1}"), GetStatLabel(Character, Variant), YSUpgradeText::FormatStat(GetDisplayValue(Character, Variant, Progress.TotalValue)));
	OldValue.Color = FLinearColor(0.65f, 0.65f, 0.65f);
	OldValue.FontSize = 16;
	OldValue.bStrikethrough = true;

	FYSChoiceLine& NewValue = OutLines.AddDefaulted_GetRef();
	NewValue.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeValue", "{0} : {1}"), GetStatLabel(Character, Variant), YSUpgradeText::FormatStat(GetDisplayValue(Character, Variant, Progress.TotalValue + GetBonusValue(Variant, Rarity))));
	NewValue.FontSize = 18;
	NewValue.bBold = true;

	// effet en plus : dégâts des coups critiques (Atk speed, Shoot system)
	const float CriticalBonus = GetCriticalDamageBonus(Rarity);
	if (CriticalBonus > 0.0f && Character)
	{
		FYSChoiceLine& OldCritical = OutLines.AddDefaulted_GetRef();
		OldCritical.Text = FText::Format(NSLOCTEXT("YS", "CriticalDamageValue", "Dégâts critiques (%) : {0}"), YSUpgradeText::FormatStat(Character->GetCriticalDamageMultiplier() * 100.0f));
		OldCritical.Color = FLinearColor(0.65f, 0.65f, 0.65f);
		OldCritical.FontSize = 14;
		OldCritical.bStrikethrough = true;

		FYSChoiceLine& NewCritical = OutLines.AddDefaulted_GetRef();
		NewCritical.Text = FText::Format(NSLOCTEXT("YS", "CriticalDamageValue", "Dégâts critiques (%) : {0}"),
			YSUpgradeText::FormatStat((Character->GetBaseCriticalDamageMultiplier() + Character->GetCriticalDamageAdd()) * (1.0f + Character->GetCriticalDamageBonus() + CriticalBonus) * 100.0f));
		NewCritical.FontSize = 16;
		NewCritical.bBold = true;
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

void UYSHandCooldownReductionUpgrade::GetOfferLines(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, TArray<FYSChoiceLine>& OutLines) const
{
	const int32 FirstLine = OutLines.Num();
	Super::GetOfferLines(Character, Variant, Progress, Rarity, OutLines);

	// valeurs concrètes (actuelle rayée, nouvelle en gras = les deux dernières lignes) : réduction totale appliquée à côté
	if (OutLines.Num() - FirstLine < 2)
	{
		return;
	}
	FNumberFormattingOptions Format;
	Format.MaximumFractionalDigits = 1;
	auto AppendReduction = [&Format](FYSChoiceLine& Line, float Multiplier)
	{
		Line.Text = FText::Format(NSLOCTEXT("YS", "CooldownReductionValue", "{0} (-{1} %)"), Line.Text, FText::AsNumber((1.0f - Multiplier) * 100.0f, &Format));
	};
	AppendReduction(OutLines[OutLines.Num() - 2], GetMultiplier(Progress.TotalValue));
	AppendReduction(OutLines.Last(), GetMultiplier(Progress.TotalValue + GetBonusValue(Variant, Rarity)));
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

void UYSWeaponUpgrade::GetOfferLines(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, TArray<FYSChoiceLine>& OutLines) const
{
	FYSChoiceLine& Title = OutLines.AddDefaulted_GetRef();
	Title.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeTitle", "{0} ({1})"), GetVariantNameFor(Character, Variant), YSUpgradeText::RarityName(Rarity));
	Title.Color = GetRarityColor(Rarity);
	Title.FontSize = 20;
	Title.bBold = true;

	if (GetTier(Variant) != EYSWeaponUpgradeTier::Bonus)
	{
		// déblocage / super : description de la propriété
		FYSChoiceLine& Description = OutLines.AddDefaulted_GetRef();
		Description.Text = GetTier(Variant) == EYSWeaponUpgradeTier::Unlock ? UnlockDescription : SuperDescription;
		Description.FontSize = 15;
		Description.bBold = true;
		return;
	}

	// bonus : comme une upgrade de stat (valeur concrète actuelle rayée, nouvelle en gras)
	FYSChoiceLine& StackLine = OutLines.AddDefaulted_GetRef();
	StackLine.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeStacks", "Bonus {0} / {1}"), Progress.Stacks + 1, MaxBonusStacks);
	StackLine.Color = FLinearColor(0.7f, 0.7f, 0.7f);
	StackLine.FontSize = 13;

	FYSChoiceLine& OldValue = OutLines.AddDefaulted_GetRef();
	OldValue.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeValue", "{0} : {1}"), BonusStatLabel, YSUpgradeText::FormatStat(GetBonusDisplayValue(Character, Progress.TotalValue)));
	OldValue.Color = FLinearColor(0.65f, 0.65f, 0.65f);
	OldValue.FontSize = 16;
	OldValue.bStrikethrough = true;

	FYSChoiceLine& NewValue = OutLines.AddDefaulted_GetRef();
	NewValue.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeValue", "{0} : {1}"), BonusStatLabel, YSUpgradeText::FormatStat(GetBonusDisplayValue(Character, Progress.TotalValue + GetBonusValue(Variant, Rarity))));
	NewValue.FontSize = 18;
	NewValue.bBold = true;
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

const FName UYSAllMyMinionsUpgrade::AllMyMinionsStat(TEXT("AllMyMinions"));
const FName UYSAllMyMinionsUpgrade::ForcedDamageStat(TEXT("ForcedDamage"));
const FName UYSAllMyMinionsUpgrade::ForcedCooldownResetStat(TEXT("ForcedCooldownReset"));

UYSAllMyMinionsUpgrade::UYSAllMyMinionsUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "AllMyMinionsName", "You're all my minions");
	RequiredActionClass = UYSGoMinionsAbility::StaticClass();
	UnlockName = NSLOCTEXT("YS", "AllMyMinionsUnlockName", "You're all my minions");
	UnlockDescription = NSLOCTEXT("YS", "AllMyMinionsUnlockDesc", "Allez-y mes minions ! force aussi l'Ability\ndes joueurs proches du Captain (sans leur recharge)");
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

void UYSClassUpgrade::GetOfferLines(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, TArray<FYSChoiceLine>& OutLines) const
{
	FYSChoiceLine& Kind = OutLines.AddDefaulted_GetRef();
	Kind.Text = NSLOCTEXT("YS", "ClassUpgradeKind", "Classe");
	Kind.Color = FLinearColor(0.7f, 0.7f, 0.7f);
	Kind.FontSize = 13;

	FYSChoiceLine& Title = OutLines.AddDefaulted_GetRef();
	Title.Text = GetVariantName(Variant);
	Title.Color = GoldColor;
	Title.FontSize = 22;
	Title.bBold = true;

	if (!Description.IsEmpty())
	{
		FYSChoiceLine& Line = OutLines.AddDefaulted_GetRef();
		Line.Text = Description;
		Line.FontSize = 14;
	}
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

void UYSCriticalAttackUpgrade::GetOfferLines(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, TArray<FYSChoiceLine>& OutLines) const
{
	FYSChoiceLine& Title = OutLines.AddDefaulted_GetRef();
	Title.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeTitle", "{0} ({1})"), DisplayName, YSUpgradeText::RarityName(Rarity));
	Title.Color = GetRarityColor(Rarity);
	Title.FontSize = 20;
	Title.bBold = true;

	// description selon l'attaque auto du joueur (toutes celles qu'il a)
	auto AddDescription = [&OutLines](const FText& Text, int32 Value)
	{
		FYSChoiceLine& Line = OutLines.AddDefaulted_GetRef();
		Line.Text = FText::Format(Text, Value);
		Line.FontSize = 15;
		Line.bBold = true;
	};
	if (HasSwingAutoAttack(Character))
	{
		AddDescription(SwingDescription, SwingCriticalInterval);
	}
	if (HasPewPewAutoAttack(Character))
	{
		AddDescription(BulletDescription, BulletCriticalInterval);
	}
	if (HasShotgunAutoAttack(Character))
	{
		AddDescription(ShotgunDescription, FMath::RoundToInt(ShotgunCriticalMaxDistance));
	}
	if (HasSniperAutoAttack(Character))
	{
		AddDescription(SniperDescription, FMath::RoundToInt(SniperCriticalMinDistance));
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
