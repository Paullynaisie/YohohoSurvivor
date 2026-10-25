#include "Upgrades/YSShardUpgrade.h"
#include "Abilities/YSAbilityComponent.h"
#include "Abilities/YSCoupDPetoirAbility.h"
#include "Abilities/YSSummonCrewAbility.h"
#include "Engine/DataTable.h"
#include "Player/YSPlayerCharacter.h"
#include "Weapons/YSWeapon.h"

namespace YSShardUpgrades
{
	const FName AllCharaRow(TEXT("AllChara"));

	/** Première action de ce type (clic ou attaque auto) des armes tenues qui passe Filter (main principale d'abord) */
	const UYSAbilityComponent* FindAction(const AYSPlayerCharacter* Character, bool bAuto, TFunctionRef<bool(const UYSAbilityComponent*)> Filter)
	{
		if (!Character)
		{
			return nullptr;
		}
		for (const AYSWeapon* Weapon : { Character->GetMainHandWeapon(), Character->GetOffHandWeapon() })
		{
			const UYSAbilityComponent* Action = !Weapon ? nullptr : bAuto ? Weapon->GetAutoAction() : Weapon->GetActiveAction();
			if (Action && Filter(Action))
			{
				return Action;
			}
		}
		return nullptr;
	}

	const UYSAbilityComponent* FindAction(const AYSPlayerCharacter* Character, bool bAuto)
	{
		return FindAction(Character, bAuto, [](const UYSAbilityComponent*) { return true; });
	}

	/** Atk speed : attaque auto qui n'est pas un Gun (l'Auto atk des Guns a le Shoot system) */
	bool IsAtkSpeedAction(const UYSAbilityComponent* Action)
	{
		return Action->WeaponType != EYSWeaponType::Gun || !Action->IsAutomatic();
	}

	/** Shoot system : attaque auto de Gun PewPew, Shotgun ou Sniper */
	const UYSCoupDPetoirAbility* FindShootSystemGun(const AYSPlayerCharacter* Character)
	{
		return Cast<UYSCoupDPetoirAbility>(FindAction(Character, true, [](const UYSAbilityComponent* Action)
		{
			const UYSCoupDPetoirAbility* Gun = Cast<UYSCoupDPetoirAbility>(Action);
			return Gun && Gun->WeaponType == EYSWeaponType::Gun && Gun->IsAutomatic()
				&& (Gun->ShootSystem == EYSShootSystem::PewPew || Gun->ShootSystem == EYSShootSystem::Shotgun || Gun->ShootSystem == EYSShootSystem::Sniper);
		}));
	}

	/** Summon system : Ability d'invocation (Summon the crew, Allez-y mes minions !) */
	const UYSSummonCrewAbility* FindSummonAction(const AYSPlayerCharacter* Character)
	{
		return Cast<UYSSummonCrewAbility>(FindAction(Character, false, [](const UYSAbilityComponent* Action)
		{
			return Cast<UYSSummonCrewAbility>(Action) != nullptr;
		}));
	}

	/** Chance (%) d'une rareté pour un niveau de Luck donné (affichage de la shard Luck) */
	float GetLuckChance(const AYSPlayerCharacter* Character, const UYSUpShard* Luck, EYSUpgradeRarity Rarity, int32 Level)
	{
		float PerLevel = 0.0f;
		switch (Rarity)
		{
		case EYSUpgradeRarity::Bronze:	PerLevel = Luck->BronzePerLevel; break;
		case EYSUpgradeRarity::Silver:	PerLevel = Luck->SilverPerLevel; break;
		case EYSUpgradeRarity::Gold:	PerLevel = Luck->GoldPerLevel; break;
		case EYSUpgradeRarity::Mythic:	PerLevel = Luck->MythicPerLevel; break;
		default:						break;
		}
		return FMath::Max(0.0f, Character->GetBaseRarityChance(Rarity) + PerLevel * Level);
	}
}

float UYSUpShard::GetFactor(int32 Level, float LevelIndex) const
{
	return Equation == EYSShardEquation::Divisible ? Base - LevelIndex * Level : Base + LevelIndex * Level;
}

float UYSUpShard::GetIndexFor(const AYSPlayerCharacter* Character) const
{
	if (Stat != EYSShardStat::ShootSystem)
	{
		return Index;
	}
	const UYSCoupDPetoirAbility* Gun = YSShardUpgrades::FindShootSystemGun(Character);
	if (!Gun)
	{
		return Index;
	}
	switch (Gun->ShootSystem)
	{
	case EYSShootSystem::Shotgun:	return ShotgunIndex;
	case EYSShootSystem::Sniper:	return SniperIndex;
	default:						return Index;
	}
}

namespace YSShardUpgrades
{
	/** Équation de la shard appliquée à Value (Base remplacée par BaseValue si donnée : Crit damage) */
	float Evaluate(const UYSUpShard* Shard, float Value, int32 Level, float LevelIndex)
	{
		const float Factor = Shard->GetFactor(Level, LevelIndex);
		switch (Shard->Equation)
		{
		case EYSShardEquation::Incremental:	return Value + Factor;
		case EYSShardEquation::Divisible:	return Value * FMath::Max(Shard->MinMultiplier, Factor);
		case EYSShardEquation::Truncated:	return Value + FMath::FloorToFloat(Factor + KINDA_SMALL_NUMBER);
		default:							return Value * Factor;
		}
	}
}

bool UYSUpShard::IsApplicable(const AYSPlayerCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	switch (Stat)
	{
	case EYSShardStat::AutoAtkDamage:	return YSShardUpgrades::FindAction(Character, true) != nullptr;
	case EYSShardStat::AbilityDamage:	return YSShardUpgrades::FindAction(Character, false) != nullptr;
	case EYSShardStat::CdReduction:		return YSShardUpgrades::FindAction(Character, false) != nullptr;
	case EYSShardStat::AtkSpeed:		return YSShardUpgrades::FindAction(Character, true, YSShardUpgrades::IsAtkSpeedAction) != nullptr;
	case EYSShardStat::ShootSystem:		return YSShardUpgrades::FindShootSystemGun(Character) != nullptr;
	case EYSShardStat::SummonSystem:	return YSShardUpgrades::FindSummonAction(Character) != nullptr;
	default:							return true;
	}
}

void UYSUpShard::Apply(AYSPlayerCharacter* Character, int32 Level) const
{
	if (!Character || IsSpecial())
	{
		return;
	}

	const float LevelIndex = GetIndexFor(Character);
	switch (Stat)
	{
	case EYSShardStat::AutoAtkDamage:	Character->SetDamageMultiplier(EYSActionSlot::AutoAtk, YSShardUpgrades::Evaluate(this, 1.0f, Level, LevelIndex)); break;
	case EYSShardStat::AbilityDamage:	Character->SetDamageMultiplier(EYSActionSlot::Ability, YSShardUpgrades::Evaluate(this, 1.0f, Level, LevelIndex)); break;
	case EYSShardStat::HpsBonus:		Character->SetMaxHealthMultiplier(YSShardUpgrades::Evaluate(this, 1.0f, Level, LevelIndex)); break;
	case EYSShardStat::MoveSpeed:		Character->SetMoveSpeedMultiplier(YSShardUpgrades::Evaluate(this, 1.0f, Level, LevelIndex)); break;
	case EYSShardStat::RegenHps:		Character->SetHealthRegen(YSShardUpgrades::Evaluate(this, Character->GetBaseHealthRegen(), Level, LevelIndex)); break;
	case EYSShardStat::Armor:			Character->SetArmor(YSShardUpgrades::Evaluate(this, 0.0f, Level, LevelIndex)); break;
	case EYSShardStat::CdReduction:		Character->SetCooldownMultiplier(EYSActionSlot::Ability, YSShardUpgrades::Evaluate(this, 1.0f, Level, LevelIndex)); break;
	case EYSShardStat::AtkSpeed:		Character->SetCooldownMultiplier(EYSActionSlot::AutoAtk, YSShardUpgrades::Evaluate(this, 1.0f, Level, LevelIndex)); break;
	case EYSShardStat::ShootSystem:		Character->SetShootSystemBonus(EYSActionSlot::AutoAtk, YSShardUpgrades::Evaluate(this, 0.0f, Level, LevelIndex)); break;
	case EYSShardStat::SummonSystem:	Character->SetSummonSystemBonus(YSShardUpgrades::Evaluate(this, 0.0f, Level, LevelIndex)); break;
	case EYSShardStat::CritDamage:
		// multiplicateur de base du personnage (1.5, Marksman : 2) à la place de Base : dégâts critiques = base + Index x Lvl
		Character->SetCriticalDamageAdd(LevelIndex * Level);
		break;
	case EYSShardStat::Luck:
		Character->SetRarityChanceOffsets(BronzePerLevel * Level, SilverPerLevel * Level, GoldPerLevel * Level, MythicPerLevel * Level);
		break;
	default:
		break;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSShard] %s : %s niveau %d / %d"), *Character->GetName(), *GetName(), Level, MaxLevel);
}

bool UYSUpShard::GetDisplayValue(const AYSPlayerCharacter* Character, int32 Level, FText& OutLabel, float& OutValue) const
{
	if (!Character || IsSpecial())
	{
		return false;
	}

	const float LevelIndex = GetIndexFor(Character);
	switch (Stat)
	{
	case EYSShardStat::AutoAtkDamage:
	case EYSShardStat::AbilityDamage:
	{
		const UYSAbilityComponent* Action = YSShardUpgrades::FindAction(Character, Stat == EYSShardStat::AutoAtkDamage);
		if (!Action)
		{
			return false;
		}
		// « Une Mine ! » : dégâts des mines x Mine Damage Multiplier
		OutLabel = NSLOCTEXT("YS", "ShardDamageLabel", "Dégât");
		OutValue = YSShardUpgrades::Evaluate(this, Action->GetBaseDamage() * UYSMineUpgrade::GetMineDamageMultiplierFor(Character, Action), Level, LevelIndex);
		return true;
	}
	case EYSShardStat::CritDamage:
		OutLabel = NSLOCTEXT("YS", "ShardCritLabel", "Dégâts critiques (%)");
		OutValue = (Character->GetBaseCriticalDamageMultiplier() + LevelIndex * Level) * (1.0f + Character->GetCriticalDamageBonus()) * 100.0f;
		return true;
	case EYSShardStat::HpsBonus:
		OutLabel = NSLOCTEXT("YS", "ShardHpsLabel", "PV max");
		OutValue = YSShardUpgrades::Evaluate(this, Character->GetBaseMaxHealth(), Level, LevelIndex);
		return true;
	case EYSShardStat::MoveSpeed:
		OutLabel = NSLOCTEXT("YS", "ShardMoveLabel", "Vitesse");
		OutValue = YSShardUpgrades::Evaluate(this, Character->GetBaseMoveSpeed(), Level, LevelIndex);
		return true;
	case EYSShardStat::RegenHps:
		OutLabel = NSLOCTEXT("YS", "ShardRegenLabel", "Regen PV/s");
		OutValue = YSShardUpgrades::Evaluate(this, Character->GetBaseHealthRegen(), Level, LevelIndex);
		return true;
	case EYSShardStat::Armor:
		OutLabel = NSLOCTEXT("YS", "ShardArmorLabel", "Armure");
		OutValue = YSShardUpgrades::Evaluate(this, 0.0f, Level, LevelIndex);
		return true;
	case EYSShardStat::CdReduction:
	case EYSShardStat::AtkSpeed:
	{
		const bool bAuto = Stat == EYSShardStat::AtkSpeed;
		const UYSAbilityComponent* Action = bAuto ? YSShardUpgrades::FindAction(Character, true, YSShardUpgrades::IsAtkSpeedAction) : YSShardUpgrades::FindAction(Character, false);
		if (!Action)
		{
			return false;
		}
		OutLabel = NSLOCTEXT("YS", "ShardCooldownLabel", "Recharge (s)");
		OutValue = YSShardUpgrades::Evaluate(this, Action->GetBaseCooldown(), Level, LevelIndex);
		return true;
	}
	case EYSShardStat::ShootSystem:
	{
		const UYSCoupDPetoirAbility* Gun = YSShardUpgrades::FindShootSystemGun(Character);
		if (!Gun)
		{
			return false;
		}
		switch (Gun->ShootSystem)
		{
		case EYSShootSystem::Shotgun:
			OutLabel = NSLOCTEXT("YS", "ShardShotgunLabel", "Shotgun AMMO par tir");
			OutValue = YSShardUpgrades::Evaluate(this, Gun->ShotgunAmmoPerShot, Level, LevelIndex);
			break;
		case EYSShootSystem::Sniper:
			OutLabel = NSLOCTEXT("YS", "ShardSniperLabel", "NME touchés par balle");
			OutValue = YSShardUpgrades::Evaluate(this, Gun->SniperMaxHits, Level, LevelIndex);
			break;
		default:
			OutLabel = NSLOCTEXT("YS", "ShardPewPewLabel", "Balles par rafale");
			OutValue = YSShardUpgrades::Evaluate(this, Gun->BulletsPerBurst, Level, LevelIndex);
			break;
		}
		return true;
	}
	case EYSShardStat::SummonSystem:
	{
		const UYSSummonCrewAbility* Summon = YSShardUpgrades::FindSummonAction(Character);
		if (!Summon)
		{
			return false;
		}
		OutLabel = NSLOCTEXT("YS", "ShardSummonLabel", "Minions max");
		OutValue = YSShardUpgrades::Evaluate(this, Summon->BaseMaxMinions, Level, LevelIndex);
		return true;
	}
	case EYSShardStat::Luck:
		// une ligne de plus pour le Mythique dans la carte (UYSUpStats::GetOfferCard)
		OutLabel = NSLOCTEXT("YS", "ShardLuckGoldLabel", "Chance Gold (%)");
		OutValue = YSShardUpgrades::GetLuckChance(Character, this, EYSUpgradeRarity::Gold, Level);
		return true;
	default:
		return false;
	}
}

UYSUpStats::UYSUpStats()
{
	Shards.SetNum(5);
}

int32 UYSUpStats::GetShardCount(EYSUpgradeRarity Rarity)
{
	switch (Rarity)
	{
	case EYSUpgradeRarity::Bronze:	return 2;
	case EYSUpgradeRarity::Silver:	return 3;
	case EYSUpgradeRarity::Gold:	return 4;
	case EYSUpgradeRarity::Mythic:	return 5;
	default:						return 0;
	}
}

FLinearColor UYSUpStats::GetRarityColor(EYSUpgradeRarity Rarity) const
{
	switch (Rarity)
	{
	case EYSUpgradeRarity::Bronze:	return BronzeColor;
	case EYSUpgradeRarity::Silver:	return SilverColor;
	case EYSUpgradeRarity::Gold:	return GoldColor;
	case EYSUpgradeRarity::Mythic:	return MythicColor;
	default:						return FLinearColor::White;
	}
}

bool UYSUpStats::IsApplicable(const AYSPlayerCharacter* Character) const
{
	return Character && (!BlockingShard || BlockingShard->IsApplicable(Character));
}

bool UYSUpStats::IsBlocked(const AYSPlayerCharacter* Character) const
{
	return Character && BlockingShard && Character->GetShardLevel(BlockingShard) >= BlockingShard->MaxLevel;
}

void UYSUpStats::GetOfferCard(const AYSPlayerCharacter* Character, const FYSUpgradeOffer& Offer, FYSUpgradeCard& OutCard) const
{
	OutCard.Name = DisplayName.IsEmpty() ? FText::FromString(GetName()) : DisplayName;
	OutCard.RarityText = YSUpgradeText::RarityName(Offer.Rarity);
	OutCard.Color = GetRarityColor(Offer.Rarity);
	OutCard.Icon = Icon;
	OutCard.Description = Description;
	YSShardUpgrades::AddShardCardLines(Character, Offer.Shards, OutCard);
}

void YSShardUpgrades::AddShardCardLines(const AYSPlayerCharacter* Character, const TArray<TObjectPtr<UYSUpShard>>& Shards, FYSUpgradeCard& OutCard)
{
	if (!Character)
	{
		return;
	}

	// chaque shard donnée (une fois par shard, même si elle est donnée plusieurs fois) : niveau atteint et valeur concrète
	TArray<const UYSUpShard*> Distinct;
	for (const TObjectPtr<UYSUpShard>& Shard : Shards)
	{
		if (Shard)
		{
			Distinct.AddUnique(Shard.Get());
		}
	}
	for (const UYSUpShard* Shard : Distinct)
	{
		const int32 Given = Shards.FilterByPredicate([Shard](const TObjectPtr<UYSUpShard>& Other) { return Other == Shard; }).Num();
		const int32 OldLevel = Character->GetShardLevel(Shard);
		const int32 NewLevel = FMath::Min(Shard->MaxLevel, OldLevel + Given);
		const FText ShardName = Shard->DisplayName.IsEmpty() ? FText::FromString(Shard->GetName()) : Shard->DisplayName;

		FText OldLabel;
		FText NewLabel;
		float OldValue = 0.0f;
		float NewValue = 0.0f;
		FYSUpgradeStatChange& Change = OutCard.StatChanges.AddDefaulted_GetRef();
		if (Shard->GetDisplayValue(Character, OldLevel, OldLabel, OldValue) && Shard->GetDisplayValue(Character, NewLevel, NewLabel, NewValue))
		{
			Change.Label = FText::Format(NSLOCTEXT("YS", "ShardChangeLabel", "{0} +{1} ({2} / {3}) : {4}"), ShardName, Given, NewLevel, Shard->MaxLevel, NewLabel);
			Change.OldValue = YSUpgradeText::FormatStat(OldValue);
			Change.NewValue = YSUpgradeText::FormatStat(NewValue);
		}
		else
		{
			Change.Label = FText::Format(NSLOCTEXT("YS", "ShardLevelLabel", "{0} +{1}"), ShardName, Given);
			Change.OldValue = FText::Format(NSLOCTEXT("YS", "ShardLevelValue", "{0} / {1}"), OldLevel, Shard->MaxLevel);
			Change.NewValue = FText::Format(NSLOCTEXT("YS", "ShardLevelValue", "{0} / {1}"), NewLevel, Shard->MaxLevel);
		}

		// Luck : la chance de Mythique aussi
		if (Shard->Stat == EYSShardStat::Luck)
		{
			FYSUpgradeStatChange& Mythic = OutCard.StatChanges.AddDefaulted_GetRef();
			Mythic.Label = NSLOCTEXT("YS", "ShardLuckMythicLabel", "Chance Mythique (%)");
			Mythic.OldValue = YSUpgradeText::FormatStat(YSShardUpgrades::GetLuckChance(Character, Shard, EYSUpgradeRarity::Mythic, OldLevel));
			Mythic.NewValue = YSUpgradeText::FormatStat(YSShardUpgrades::GetLuckChance(Character, Shard, EYSUpgradeRarity::Mythic, NewLevel));
		}
	}
}

TArray<UYSUpStats*> YSShardUpgrades::GetAvailableUpStats(const AYSPlayerCharacter* Character, const UDataTable* Table)
{
	TArray<UYSUpStats*> Result;
	if (!Character || !Table || Table->GetRowStruct() != FYSCharaUpgradesRow::StaticStruct())
	{
		return Result;
	}
	for (const FName RowName : { AllCharaRow, Character->GetUpgradeTableRow() })
	{
		if (const FYSCharaUpgradesRow* Row = Table->FindRow<FYSCharaUpgradesRow>(RowName, TEXT("YSShardUpgrades"), false))
		{
			for (const TObjectPtr<UYSUpStats>& Stats : Row->Upgrades)
			{
				if (Stats)
				{
					Result.AddUnique(Stats.Get());
				}
			}
		}
	}
	return Result;
}

TArray<UYSUpShard*> YSShardUpgrades::GetCandidateShards(const AYSPlayerCharacter* Character, const UDataTable* Table, const TArray<UYSUpStats*>& Replacements)
{
	TArray<UYSUpStats*> AllStats = GetAvailableUpStats(Character, Table);
	for (UYSUpStats* Replacement : Replacements)
	{
		AllStats.AddUnique(Replacement);
	}

	TArray<UYSUpShard*> Shards;
	for (const UYSUpStats* Stats : AllStats)
	{
		for (const TObjectPtr<UYSUpShard>& Shard : Stats ? Stats->Shards : TArray<TObjectPtr<UYSUpShard>>())
		{
			if (Shard && !Shard->IsSpecial() && Shard->IsApplicable(Character))
			{
				Shards.AddUnique(Shard.Get());
			}
		}
		if (Stats && Stats->BlockingShard && !Stats->BlockingShard->IsSpecial() && Stats->BlockingShard->IsApplicable(Character))
		{
			Shards.AddUnique(Stats->BlockingShard.Get());
		}
	}
	return Shards;
}

EYSUpgradeRarity YSShardUpgrades::RollRarity(const AYSPlayerCharacter* Character)
{
	static const EYSUpgradeRarity Rarities[] = { EYSUpgradeRarity::Bronze, EYSUpgradeRarity::Silver, EYSUpgradeRarity::Gold, EYSUpgradeRarity::Mythic };
	float Total = 0.0f;
	for (const EYSUpgradeRarity Rarity : Rarities)
	{
		Total += Character->GetRarityChance(Rarity);
	}
	if (Total <= 0.0f)
	{
		return EYSUpgradeRarity::Bronze;
	}
	float Roll = FMath::FRand() * Total;
	for (const EYSUpgradeRarity Rarity : Rarities)
	{
		const float Chance = Character->GetRarityChance(Rarity);
		if (Chance > 0.0f)
		{
			Roll -= Chance;
			if (Roll < 0.0f)
			{
				return Rarity;
			}
		}
	}
	return EYSUpgradeRarity::Bronze;
}

TArray<TObjectPtr<UYSUpShard>> YSShardUpgrades::ResolveShards(const AYSPlayerCharacter* Character, const UYSUpStats* Stats, EYSUpgradeRarity Rarity, const TArray<UYSUpShard*>& Candidates)
{
	return Stats ? ResolveShardList(Character, Stats->Shards, UYSUpStats::GetShardCount(Rarity), Candidates) : TArray<TObjectPtr<UYSUpShard>>();
}

TArray<TObjectPtr<UYSUpShard>> YSShardUpgrades::ResolveShardList(const AYSPlayerCharacter* Character, const TArray<TObjectPtr<UYSUpShard>>& Source, int32 SourceCount, const TArray<UYSUpShard*>& Candidates)
{
	TArray<TObjectPtr<UYSUpShard>> Result;
	if (!Character)
	{
		return Result;
	}

	// niveaux simulés : les shards déjà résolues de cette upgrade comptent pour les suivantes (Max stats, maximum atteint)
	TMap<const UYSUpShard*, int32> Added;
	auto LevelOf = [Character, &Added](const UYSUpShard* Shard) { return Character->GetShardLevel(Shard) + Added.FindRef(Shard); };
	auto PickRandom = [](const TArray<const UYSUpShard*>& Choices) -> const UYSUpShard*
	{
		return Choices.Num() > 0 ? Choices[FMath::RandRange(0, Choices.Num() - 1)] : nullptr;
	};

	auto Resolve = [&](EYSShardStat Special) -> const UYSUpShard*
	{
		// Max stats / Min stats : parmi les shards déjà possédées pas au maximum, la plus haute / la plus basse (égalité : au hasard)
		if (Special == EYSShardStat::MaxStats || Special == EYSShardStat::MinStats)
		{
			int32 BestLevel = Special == EYSShardStat::MaxStats ? 0 : TNumericLimits<int32>::Max();
			TArray<const UYSUpShard*> Best;
			for (const UYSUpShard* Shard : Candidates)
			{
				const int32 Level = LevelOf(Shard);
				if (Level <= 0 || Level >= Shard->MaxLevel)
				{
					continue;
				}
				const bool bBetter = Special == EYSShardStat::MaxStats ? Level > BestLevel : Level < BestLevel;
				if (bBetter)
				{
					BestLevel = Level;
					Best.Reset();
				}
				if (Level == BestLevel)
				{
					Best.Add(Shard);
				}
			}
			if (Best.Num() > 0)
			{
				return PickRandom(Best);
			}
			// aucune shard possédée : au hasard
		}

		// Random : parmi les shards pas au maximum
		TArray<const UYSUpShard*> Open;
		for (const UYSUpShard* Shard : Candidates)
		{
			if (LevelOf(Shard) < Shard->MaxLevel)
			{
				Open.Add(Shard);
			}
		}
		return PickRandom(Open);
	};

	const int32 Count = FMath::Min(SourceCount, Source.Num());
	for (int32 Slot = 0; Slot < Count; ++Slot)
	{
		const UYSUpShard* Shard = Source[Slot];
		if (!Shard)
		{
			continue;
		}
		// spéciale : remplacée ; shard au maximum ou qui ne concerne pas le personnage : une au hasard à la place
		const UYSUpShard* Resolved = Shard->IsSpecial() ? Resolve(Shard->Stat)
			: (LevelOf(Shard) >= Shard->MaxLevel || !Shard->IsApplicable(Character)) ? Resolve(EYSShardStat::Random) : Shard;
		if (Resolved)
		{
			Result.Add(const_cast<UYSUpShard*>(Resolved));
			++Added.FindOrAdd(Resolved);
		}
	}
	return Result;
}

TArray<FYSUpgradeOffer> YSShardUpgrades::PickOffers(const AYSPlayerCharacter* Character, const UDataTable* Table, const TArray<UYSUpStats*>& Replacements, int32 Count)
{
	TArray<FYSUpgradeOffer> Offers;
	if (!Character)
	{
		return Offers;
	}

	// candidats : les DA_Up_Stats de la table et chaque palier proposable des upgrades d'arme du pool (système de shards)
	struct FCandidate
	{
		UYSUpStats* Stats = nullptr;
		const UYSWeaponUpgrade* Weapon = nullptr;
		int32 PoolIndex = INDEX_NONE;
		int32 Variant = 0;
	};
	TArray<FCandidate> Pool;
	for (UYSUpStats* Stats : GetAvailableUpStats(Character, Table))
	{
		if (Stats->OfferWeight > 0.0f && Stats->IsApplicable(Character))
		{
			Pool.Add({ Stats });
		}
	}
	const TArray<TObjectPtr<UYSUpgrade>>& UpgradePool = Character->GetUpgradePool();
	for (int32 PoolIndex = 0; PoolIndex < UpgradePool.Num(); ++PoolIndex)
	{
		const UYSWeaponUpgrade* Weapon = Cast<UYSWeaponUpgrade>(UpgradePool[PoolIndex]);
		for (int32 Variant = 0; Weapon && Weapon->bShardSystem && Variant < Weapon->GetVariantCount(); ++Variant)
		{
			if (Weapon->GetOfferWeight(Variant) > 0.0f && Weapon->CanBeOffered(Character, Variant, Character->GetUpgradeProgress(PoolIndex, Variant)))
			{
				Pool.Add({ nullptr, Weapon, PoolIndex, Variant });
			}
		}
	}
	// poids dans la rareté tirée (une upgrade d'arme n'existe pas dans toutes les raretés : jamais en Bronze)
	const auto WeightOf = [Character](const FCandidate& Candidate, EYSUpgradeRarity Rarity)
	{
		return Candidate.Stats ? Candidate.Stats->OfferWeight
			: Candidate.Weapon->CanOfferRarity(Character, Candidate.Variant, Rarity) ? Candidate.Weapon->GetOfferWeight(Candidate.Variant) : 0.0f;
	};

	const TArray<UYSUpShard*> Candidates = GetCandidateShards(Character, Table, Replacements);
	TArray<UYSUpStats*> ValidReplacements = Replacements.FilterByPredicate([](const UYSUpStats* Stats) { return Stats != nullptr; });

	// garde-fou : raretés tirées sans aucun candidat (ex. Bronze alors qu'il ne reste que des upgrades d'arme)
	int32 EmptyRolls = 0;
	while (Offers.Num() < Count && Pool.Num() > 0 && EmptyRolls < 50)
	{
		const EYSUpgradeRarity Rarity = RollRarity(Character);

		// upgrade au poids, sans remise
		float TotalWeight = 0.0f;
		for (const FCandidate& Candidate : Pool)
		{
			TotalWeight += WeightOf(Candidate, Rarity);
		}
		if (TotalWeight <= 0.0f)
		{
			++EmptyRolls;
			continue;
		}
		int32 Picked = INDEX_NONE;
		float Roll = FMath::FRand() * TotalWeight;
		for (int32 Index = 0; Index < Pool.Num(); ++Index)
		{
			if (WeightOf(Pool[Index], Rarity) > 0.0f)
			{
				Picked = Index;
				Roll -= WeightOf(Pool[Index], Rarity);
				if (Roll < 0.0f)
				{
					break;
				}
			}
		}
		const FCandidate Chosen = Pool[Picked];
		Pool.RemoveAt(Picked);

		// upgrade d'arme : palier + shards du palier selon la rareté (Gold : la première, Mythique : les deux)
		if (Chosen.Weapon)
		{
			FYSUpgradeOffer Offer;
			Offer.PoolIndex = Chosen.PoolIndex;
			Offer.Variant = Chosen.Variant;
			Offer.Rarity = Rarity;
			Offer.Shards = ResolveShardList(Character, Chosen.Weapon->GetTierShards(Chosen.Variant), UYSWeaponUpgrade::GetShardCount(Rarity), Candidates);
			UE_LOG(LogTemp, Verbose, TEXT("[YSShard] %s : proposition %s palier %d %s + %d shard(s)"), *Character->GetName(), *Chosen.Weapon->GetName(), Chosen.Variant,
				*YSUpgradeText::RarityName(Rarity).ToString(), Offer.Shards.Num());
			Offers.Add(Offer);
			continue;
		}

		UYSUpStats* Stats = Chosen.Stats;

		// interdite (Blocking Shard au maximum) : une upgrade de remplacement pas encore proposée (sinon n'importe laquelle)
		if (Stats->IsBlocked(Character))
		{
			TArray<UYSUpStats*> Unused = ValidReplacements.FilterByPredicate([&Offers](const UYSUpStats* Replacement)
			{
				return !Offers.ContainsByPredicate([Replacement](const FYSUpgradeOffer& Offer) { return Offer.UpStats == Replacement; });
			});
			const TArray<UYSUpStats*>& Choices = Unused.Num() > 0 ? Unused : ValidReplacements;
			UE_LOG(LogTemp, Verbose, TEXT("[YSShard] %s : %s interdite (%s au maximum)"), *Character->GetName(), *Stats->GetName(), *Stats->BlockingShard->GetName());
			Stats = Choices.Num() > 0 ? Choices[FMath::RandRange(0, Choices.Num() - 1)] : nullptr;
		}
		if (!Stats)
		{
			continue;
		}

		FYSUpgradeOffer Offer;
		Offer.UpStats = Stats;
		Offer.Rarity = Rarity;
		Offer.Shards = ResolveShards(Character, Stats, Rarity, Candidates);
		if (Offer.Shards.Num() == 0)
		{
			continue;
		}
		TArray<FString> ShardNames;
		for (const TObjectPtr<UYSUpShard>& Shard : Offer.Shards)
		{
			ShardNames.Add(Shard->GetName().Replace(TEXT("DA_Up_Shard_"), TEXT("")));
		}
		UE_LOG(LogTemp, Verbose, TEXT("[YSShard] %s : proposition %s %s -> %s"), *Character->GetName(), *Stats->GetName(), *YSUpgradeText::RarityName(Rarity).ToString(), *FString::Join(ShardNames, TEXT(", ")));
		Offers.Add(Offer);
	}
	return Offers;
}
