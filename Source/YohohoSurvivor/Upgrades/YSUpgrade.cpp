#include "Upgrades/YSUpgrade.h"
#include "Abilities/YSAbilityComponent.h"
#include "Abilities/YSCoupDPetoirAbility.h"
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
	// une arme du set doit avoir une action de ce type (Ability = clic, Auto atk = attaque auto)
	return GetSlotAction(Character, Variant) && Super::CanBeOffered(Character, Variant, Progress);
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
	BronzeValue = 0.03f;
	SilverValue = 0.045f;
	GoldValue = 0.06f;
}

float UYSHandCooldownReductionUpgrade::GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const
{
	const UYSAbilityComponent* Action = GetSlotAction(Character, Variant);
	return Action ? Action->GetBaseCooldown() : 0.0f;
}

void UYSHandCooldownReductionUpgrade::ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const
{
	Character->SetCooldownMultiplier(GetSlot(Variant), GetMultiplier(TotalBonus));
}

bool UYSHandCooldownReductionUpgrade::IsActionEligible(const UYSAbilityComponent* Action) const
{
	return Action->WeaponType != EYSWeaponType::Gun;
}

UYSShootSystemUpgrade::UYSShootSystemUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "ShootSystemName", "Shoot system");
	BonusName = NSLOCTEXT("YS", "ShootSystemBonus", "Shoot system");
	StatLabel = NSLOCTEXT("YS", "ShootSystemStat", "Balles par rafale");
	BronzeValue = 1.0f;
	SilverValue = 2.0f;
	GoldValue = 3.0f;
}

bool UYSShootSystemUpgrade::IsActionEligible(const UYSAbilityComponent* Action) const
{
	const UYSCoupDPetoirAbility* Gun = Cast<UYSCoupDPetoirAbility>(Action);
	return Gun && Gun->WeaponType == EYSWeaponType::Gun && Gun->ShootSystem == EYSShootSystem::PewPew;
}

float UYSShootSystemUpgrade::GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const
{
	const UYSCoupDPetoirAbility* Gun = Cast<UYSCoupDPetoirAbility>(GetSlotAction(Character, Variant));
	return Gun ? Gun->BulletsPerBurst : 0.0f;
}

void UYSShootSystemUpgrade::ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const
{
	Character->SetShootSystemBonus(GetSlot(Variant), TotalBonus);
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
		TInlineComponentArray<UYSAbilityComponent*> Actions(Weapon);
		for (UYSAbilityComponent* Action : Actions)
		{
			if (Action->IsA(RequiredActionClass) && Action->GetActionSlot() == RequiredSlot)
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

	const float BaseValue = GetBonusBaseValue(Character);

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

