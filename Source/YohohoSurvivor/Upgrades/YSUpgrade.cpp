#include "Upgrades/YSUpgrade.h"
#include "Player/YSPlayerCharacter.h"

namespace YSUpgradeText
{
	FText FormatStat(float Value)
	{
		FNumberFormattingOptions Format;
		Format.MinimumFractionalDigits = 0;
		Format.MaximumFractionalDigits = 2;
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

EYSUpgradeRarity UYSStatUpgrade::RollRarity() const
{
	const float Total = BronzeWeight + SilverWeight + GoldWeight;
	if (Total <= 0.0f)
	{
		return EYSUpgradeRarity::Bronze;
	}

	const float Roll = FMath::FRand() * Total;
	if (Roll < BronzeWeight)
	{
		return EYSUpgradeRarity::Bronze;
	}
	return Roll < BronzeWeight + SilverWeight ? EYSUpgradeRarity::Silver : EYSUpgradeRarity::Gold;
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

	// valeur actuelle rayée, nouvelle valeur en gras
	FYSChoiceLine& OldValue = OutLines.AddDefaulted_GetRef();
	OldValue.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeValue", "{0} x {1}"), GetStatLabel(), YSUpgradeText::FormatStat(GetStatValue(Progress.TotalValue)));
	OldValue.Color = FLinearColor(0.65f, 0.65f, 0.65f);
	OldValue.FontSize = 16;
	OldValue.bStrikethrough = true;

	FYSChoiceLine& NewValue = OutLines.AddDefaulted_GetRef();
	NewValue.Text = FText::Format(NSLOCTEXT("YS", "StatUpgradeValue", "{0} x {1}"), GetStatLabel(), YSUpgradeText::FormatStat(GetStatValue(Progress.TotalValue + GetBonusValue(Rarity))));
	NewValue.FontSize = 18;
	NewValue.bBold = true;
}

UYSHandAtkBonusUpgrade::UYSHandAtkBonusUpgrade()
{
	DisplayName = NSLOCTEXT("YS", "HandAtkBonusName", "Atk Bonus");
	RightHandName = NSLOCTEXT("YS", "RightHandAtkBonus", "Right hand Atk bonus");
	LeftHandName = NSLOCTEXT("YS", "LeftHandAtkBonus", "Left hand Atk bonus");
	StatLabel = NSLOCTEXT("YS", "HandAtkBonusStat", "Dégât");
}

bool UYSHandAtkBonusUpgrade::CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	const AYSWeapon* Weapon = !Character ? nullptr : GetHand(Variant) == EYSHand::Left ? Character->GetLeftHandWeapon() : Character->GetRightHandWeapon();
	return Weapon && Super::CanBeOffered(Character, Variant, Progress);
}

void UYSHandAtkBonusUpgrade::Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const
{
	if (Character)
	{
		Character->SetHandDamageMultiplier(GetHand(Variant), GetStatValue(Progress.TotalValue));
		UE_LOG(LogTemp, Log, TEXT("[YSHandAtkBonusUpgrade] %s main %s : dégâts x %.2f (%d bonus)"),
			*Character->GetName(), GetHand(Variant) == EYSHand::Left ? TEXT("gauche") : TEXT("droite"), GetStatValue(Progress.TotalValue), Progress.Stacks);
	}
}
