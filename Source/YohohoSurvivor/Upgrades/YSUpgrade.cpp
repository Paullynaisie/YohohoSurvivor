#include "Upgrades/YSUpgrade.h"
#include "Abilities/YSAbilityComponent.h"
#include "Player/YSPlayerCharacter.h"
#include "Weapons/YSWeapon.h"

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

bool UYSStatUpgrade::CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	return Progress.Stacks < MaxStacks;
}

bool UYSUpgrade::SupportsRarity(EYSUpgradeRarity Rarity) const
{
	switch (Rarity)
	{
	case EYSUpgradeRarity::Bronze:	return bCanBeBronze;
	case EYSUpgradeRarity::Silver:	return bCanBeSilver;
	case EYSUpgradeRarity::Gold:	return bCanBeGold;
	default:						return true;
	}
}

float UYSStatUpgrade::GetBonusValue(EYSUpgradeRarity Rarity) const
{
	switch (Rarity)
	{
	case EYSUpgradeRarity::Bronze:	return BronzeValue;
	case EYSUpgradeRarity::Silver:	return SilverValue;
	case EYSUpgradeRarity::Gold:	return GoldValue;
	default:						return 0.0f;
	}
}

FLinearColor UYSStatUpgrade::GetRarityColor(EYSUpgradeRarity Rarity) const
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
	Title.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeTitle", "{0} ({1})"), GetVariantName(Variant), YSUpgradeText::RarityName(Rarity));
	Title.Color = GetRarityColor(Rarity);
	Title.FontSize = 20;
	Title.bBold = true;

	FYSChoiceLine& StackLine = OutLines.AddDefaulted_GetRef();
	StackLine.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeStacks", "Bonus {0} / {1}"), Progress.Stacks + 1, MaxStacks);
	StackLine.Color = FLinearColor(0.7f, 0.7f, 0.7f);
	StackLine.FontSize = 13;

	// valeurs concrètes : actuelle rayée, nouvelle en gras
	const float BaseValue = GetBaseStatValue(Character, Variant);

	FYSChoiceLine& OldValue = OutLines.AddDefaulted_GetRef();
	OldValue.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeValue", "{0} : {1}"), StatLabel, YSUpgradeText::FormatStat(GetStatValue(BaseValue, Progress.TotalValue)));
	OldValue.Color = FLinearColor(0.65f, 0.65f, 0.65f);
	OldValue.FontSize = 16;
	OldValue.bStrikethrough = true;

	FYSChoiceLine& NewValue = OutLines.AddDefaulted_GetRef();
	NewValue.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeValue", "{0} : {1}"), StatLabel, YSUpgradeText::FormatStat(GetStatValue(BaseValue, Progress.TotalValue + GetBonusValue(Rarity))));
	NewValue.FontSize = 18;
	NewValue.bBold = true;
}

void UYSStatUpgrade::Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	if (Character)
	{
		ApplyBonus(Character, Variant, Progress.TotalValue);
		UE_LOG(LogTemp, Log, TEXT("[YSStatUpgrade] %s : %s, somme des bonus %.3f (%d bonus) -> %.3f"), *Character->GetName(), *GetVariantName(Variant).ToString(),
			Progress.TotalValue, Progress.Stacks, GetStatValue(GetBaseStatValue(Character, Variant), Progress.TotalValue));
	}
}

bool UYSHandStatUpgrade::CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	return GetHandWeapon(Character, Variant) && Super::CanBeOffered(Character, Variant, Progress);
}

AYSWeapon* UYSHandStatUpgrade::GetHandWeapon(const AYSPlayerCharacter* Character, int32 Variant)
{
	return !Character ? nullptr : GetHand(Variant) == EYSHand::Left ? Character->GetLeftHandWeapon() : Character->GetRightHandWeapon();
}

UYSAbilityComponent* UYSHandStatUpgrade::GetHandAction(const AYSPlayerCharacter* Character, int32 Variant)
{
	const AYSWeapon* Weapon = GetHandWeapon(Character, Variant);
	if (!Weapon)
	{
		return nullptr;
	}
	UYSAbilityComponent* Action = Weapon->GetActiveAction();
	return Action ? Action : Weapon->GetAutoAction();
}

UYSHandAtkBonusUpgrade::UYSHandAtkBonusUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "HandAtkBonusName", "Atk Bonus");
	RightHandName = NSLOCTEXT("YS", "RightHandAtkBonus", "Right hand Atk bonus");
	LeftHandName = NSLOCTEXT("YS", "LeftHandAtkBonus", "Left hand Atk bonus");
	StatLabel = NSLOCTEXT("YS", "HandAtkBonusStat", "Dégât");
}

float UYSHandAtkBonusUpgrade::GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const
{
	const UYSAbilityComponent* Action = GetHandAction(Character, Variant);
	return Action ? Action->GetBaseDamage() : 0.0f;
}

void UYSHandAtkBonusUpgrade::ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const
{
	Character->SetHandDamageMultiplier(GetHand(Variant), 1.0f + TotalBonus);
}

UYSHandCooldownReductionUpgrade::UYSHandCooldownReductionUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "HandCooldownReductionName", "CD Reduction");
	RightHandName = NSLOCTEXT("YS", "RightHandCooldownReduction", "Right hand CD reduction");
	LeftHandName = NSLOCTEXT("YS", "LeftHandCooldownReduction", "Left hand CD reduction");
	StatLabel = NSLOCTEXT("YS", "HandCooldownReductionStat", "Recharge (s)");
	BronzeValue = 0.03f;
	SilverValue = 0.045f;
	GoldValue = 0.06f;
}

float UYSHandCooldownReductionUpgrade::GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const
{
	const UYSAbilityComponent* Action = GetHandAction(Character, Variant);
	return Action ? Action->GetBaseCooldown() : 0.0f;
}

void UYSHandCooldownReductionUpgrade::ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const
{
	Character->SetHandCooldownMultiplier(GetHand(Variant), GetMultiplier(TotalBonus));
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

void UYSHealthRegenUpgrade::ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const
{
	Character->SetHealthRegen(TotalBonus);
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
