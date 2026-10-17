#include "Upgrades/YSUpgrade.h"
#include "Abilities/YSAbilityComponent.h"
#include "Abilities/YSCoupDPetoirAbility.h"
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
	const float BaseValue = GetBaseStatValue(Character, Variant);

	FYSChoiceLine& OldValue = OutLines.AddDefaulted_GetRef();
	OldValue.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeValue", "{0} : {1}"), StatLabel, YSUpgradeText::FormatStat(GetStatValue(BaseValue, Progress.TotalValue)));
	OldValue.Color = FLinearColor(0.65f, 0.65f, 0.65f);
	OldValue.FontSize = 16;
	OldValue.bStrikethrough = true;

	FYSChoiceLine& NewValue = OutLines.AddDefaulted_GetRef();
	NewValue.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeValue", "{0} : {1}"), StatLabel, YSUpgradeText::FormatStat(GetStatValue(BaseValue, Progress.TotalValue + GetBonusValue(Variant, Rarity))));
	NewValue.FontSize = 18;
	NewValue.bBold = true;
}

void UYSStatUpgrade::Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	if (Character)
	{
		ApplyBonus(Character, Variant, Progress.TotalValue);
		UE_LOG(LogTemp, Log, TEXT("[YSStatUpgrade] %s : %s, somme des bonus %.3f (%d bonus) -> %.3f"), *Character->GetName(), *GetVariantNameFor(Character, Variant).ToString(),
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
	AkimboName = NSLOCTEXT("YS", "AkimboCooldownReduction", "Akimbo CD reduction");
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
	// akimbo : une seule CD reduction (variante 0) pour les deux mains
	if (Character->IsAkimbo())
	{
		Character->SetHandCooldownMultiplier(EYSHand::Right, GetMultiplier(TotalBonus));
		Character->SetHandCooldownMultiplier(EYSHand::Left, GetMultiplier(TotalBonus));
		return;
	}
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

UYSWeaponUpgrade::UYSWeaponUpgrade()
{
	UpgradeType = EYSUpgradeType::Weapon;
	RightHandLabel = NSLOCTEXT("YS", "WeaponUpgradeRightHand", "Right hand");
	LeftHandLabel = NSLOCTEXT("YS", "WeaponUpgradeLeftHand", "Left hand");
}

FText UYSWeaponUpgrade::GetVariantName(int32 Variant) const
{
	const FText& TierName = GetTier(Variant) == EYSWeaponUpgradeTier::Unlock ? UnlockName : GetTier(Variant) == EYSWeaponUpgradeTier::Bonus ? BonusName : SuperName;
	return FText::Format(NSLOCTEXT("YS", "WeaponUpgradeName", "{0} {1}"), GetHand(Variant) == EYSHand::Left ? LeftHandLabel : RightHandLabel, TierName);
}

UYSAbilityComponent* UYSWeaponUpgrade::GetRequiredAction(const AYSPlayerCharacter* Character, EYSHand Hand) const
{
	const AYSWeapon* Weapon = !Character ? nullptr : Hand == EYSHand::Left ? Character->GetLeftHandWeapon() : Character->GetRightHandWeapon();
	if (!Weapon || !RequiredActionClass)
	{
		return nullptr;
	}

	TInlineComponentArray<UYSAbilityComponent*> Actions(Weapon);
	for (UYSAbilityComponent* Action : Actions)
	{
		if (Action->IsA(RequiredActionClass))
		{
			return Action;
		}
	}
	return nullptr;
}

bool UYSWeaponUpgrade::CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	const EYSHand Hand = GetHand(Variant);
	if (!Character || !GetRequiredAction(Character, Hand))
	{
		return false;
	}

	// le déblocage d'abord ; ensuite il laisse la place au Bonus (cumulable) et au Super (une fois)
	const bool bUnlocked = Character->GetUpgradeProgress(Progress.PoolIndex, MakeVariant(Hand, EYSWeaponUpgradeTier::Unlock)).Stacks > 0;
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

	const float BaseValue = GetBonusBaseValue(Character, GetHand(Variant));

	FYSChoiceLine& OldValue = OutLines.AddDefaulted_GetRef();
	OldValue.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeValue", "{0} : {1}"), BonusStatLabel, YSUpgradeText::FormatStat(BaseValue * (1.0f + Progress.TotalValue)));
	OldValue.Color = FLinearColor(0.65f, 0.65f, 0.65f);
	OldValue.FontSize = 16;
	OldValue.bStrikethrough = true;

	FYSChoiceLine& NewValue = OutLines.AddDefaulted_GetRef();
	NewValue.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeValue", "{0} : {1}"), BonusStatLabel, YSUpgradeText::FormatStat(BaseValue * (1.0f + Progress.TotalValue + GetBonusValue(Variant, Rarity))));
	NewValue.FontSize = 18;
	NewValue.bBold = true;
}

void UYSWeaponUpgrade::Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	if (!Character)
	{
		return;
	}

	const EYSHand Hand = GetHand(Variant);
	const bool bUnlocked = Character->GetUpgradeProgress(Progress.PoolIndex, MakeVariant(Hand, EYSWeaponUpgradeTier::Unlock)).Stacks > 0;
	const float BonusTotal = Character->GetUpgradeProgress(Progress.PoolIndex, MakeVariant(Hand, EYSWeaponUpgradeTier::Bonus)).TotalValue;
	const bool bSuper = Character->GetUpgradeProgress(Progress.PoolIndex, MakeVariant(Hand, EYSWeaponUpgradeTier::Super)).Stacks > 0;
	ApplyWeaponProperty(Character, Hand, bUnlocked, BonusTotal, bSuper);
	UE_LOG(LogTemp, Log, TEXT("[YSWeaponUpgrade] %s : %s (débloqué %d, bonus %.3f, super %d)"), *Character->GetName(), *GetVariantNameFor(Character, Variant).ToString(), bUnlocked, BonusTotal, bSuper);
}

const FName UYSRicochetUpgrade::RicochetCountStat(TEXT("Ricochets"));
const FName UYSRicochetUpgrade::RicochetDamageBonusStat(TEXT("RicochetDamageBonus"));

UYSRicochetUpgrade::UYSRicochetUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "RicochetName", "Ricochet");
	RequiredActionClass = UYSCoupDPetoirAbility::StaticClass();
	UnlockName = NSLOCTEXT("YS", "RicochetUnlockName", "Ricochet");
	UnlockDescription = NSLOCTEXT("YS", "RicochetUnlockDesc", "Après avoir touché, la balle ricoche\nvers le NME le plus proche");
	BonusName = NSLOCTEXT("YS", "RicochetBonusName", "Ricochet bonus");
	BonusStatLabel = NSLOCTEXT("YS", "RicochetBonusStat", "Dégât après ricochet");
	BonusSilverValue = 0.15f;
	BonusGoldValue = 0.25f;
	SuperName = NSLOCTEXT("YS", "RicochetSuperName", "Golden ricochet");
	SuperDescription = NSLOCTEXT("YS", "RicochetSuperDesc", "La balle ricoche une fois de plus");
}

float UYSRicochetUpgrade::GetBonusBaseValue(const AYSPlayerCharacter* Character, EYSHand Hand) const
{
	const UYSAbilityComponent* Action = GetRequiredAction(Character, Hand);
	return Action ? Action->GetBaseDamage() : 0.0f;
}

void UYSRicochetUpgrade::ApplyWeaponProperty(AYSPlayerCharacter* Character, EYSHand Hand, bool bUnlocked, float BonusTotal, bool bSuper) const
{
	const int32 Ricochets = bUnlocked ? UnlockRicochets + (bSuper ? SuperExtraRicochets : 0) : 0;
	Character->SetHandWeaponStat(Hand, RicochetCountStat, Ricochets);
	Character->SetHandWeaponStat(Hand, RicochetDamageBonusStat, BonusTotal);
}

UYSClassUpgrade::UYSClassUpgrade()
{
	UpgradeType = EYSUpgradeType::Class;
	OfferWeight = 0.0f;
}

FText UYSClassUpgrade::GetVariantName(int32 Variant) const
{
	if (!DisplayName.IsEmpty() || !TargetClass)
	{
		return DisplayName;
	}
	return TargetClass->GetDefaultObject<AYSPlayerCharacter>()->GetCharacterName();
}

bool UYSClassUpgrade::CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	return Character && TargetClass && Character->GetClass() == SourceClass && Progress.Stacks == 0;
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
	if (Character && TargetClass)
	{
		Character->ChangeClass(TargetClass);
	}
}

bool UYSHandCooldownReductionUpgrade::CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	if (Character && Character->IsAkimbo() && GetHand(Variant) == EYSHand::Left)
	{
		return false;
	}
	return Super::CanBeOffered(Character, Variant, Progress);
}

FText UYSHandCooldownReductionUpgrade::GetVariantNameFor(const AYSPlayerCharacter* Character, int32 Variant) const
{
	return Character && Character->IsAkimbo() ? AkimboName : GetVariantName(Variant);
}

void UYSHandCooldownReductionUpgrade::MergeProgress(const AYSPlayerCharacter* Character, int32 PoolIndex, TArray<FYSUpgradeProgress>& Progress) const
{
	if (!Character || !Character->IsAkimbo())
	{
		return;
	}

	// tous les bonus des deux mains, du meilleur au moins bon ; les MaxStacks meilleurs restent (variante 0)
	TArray<float> Values;
	for (int32 Index = Progress.Num() - 1; Index >= 0; --Index)
	{
		const FYSUpgradeProgress& Entry = Progress[Index];
		if (Entry.PoolIndex != PoolIndex)
		{
			continue;
		}

		if (Entry.BonusValues.Num() > 0)
		{
			Values.Append(Entry.BonusValues);
		}
		else
		{
			// progression sans détail : bonus de valeur moyenne
			for (int32 Stack = 0; Stack < Entry.Stacks; ++Stack)
			{
				Values.Add(Entry.TotalValue / FMath::Max(1, Entry.Stacks));
			}
		}
		Progress.RemoveAt(Index);
	}
	if (Values.Num() == 0)
	{
		return;
	}

	Values.Sort(TGreater<float>());
	const int32 Dropped = FMath::Max(0, Values.Num() - MaxStacks);
	Values.SetNum(FMath::Min(Values.Num(), MaxStacks));

	FYSUpgradeProgress& Merged = Progress.AddDefaulted_GetRef();
	Merged.PoolIndex = PoolIndex;
	Merged.Variant = 0;
	Merged.BonusValues = Values;
	Merged.Stacks = Values.Num();
	for (const float Value : Values)
	{
		Merged.TotalValue += Value;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSHandCooldownReduction] %s passe en akimbo : %d bonus gardés (somme %.3f), %d en trop jetés"),
		*Character->GetName(), Merged.Stacks, Merged.TotalValue, Dropped);
}
