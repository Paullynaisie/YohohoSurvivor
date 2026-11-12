#include "Core/YSMatchStats.h"
#include "Core/YSCustomization.h"
#include "Core/YSPlayerState.h"
#include "GameFramework/Controller.h"
#include "Player/YSPlayerCharacter.h"
#include "Upgrades/YSShardUpgrade.h"
#include "Upgrades/YSUpgrade.h"

AYSPlayerState* YSMatchStats::FindPlayerState(const AController* InstigatorController, const AActor* Causer)
{
	// joueur (même mort : son contrôleur garde son PlayerState) ; les IA (NME, minions) n'en ont pas
	if (AYSPlayerState* State = InstigatorController ? InstigatorController->GetPlayerState<AYSPlayerState>() : nullptr)
	{
		return State;
	}
	const AYSPlayerCharacter* Player = YSPlayerColor::FindPlayer(Causer);
	if (!Player && InstigatorController)
	{
		Player = YSPlayerColor::FindPlayer(InstigatorController->GetPawn());
	}
	return Player ? Player->GetPlayerState<AYSPlayerState>() : nullptr;
}

void YSMatchStats::RecordDamageDealt(const AActor* Victim, float Amount, const AController* InstigatorController, const AActor* Causer)
{
	if (Amount <= 0.0f || Cast<AYSPlayerCharacter>(Victim))
	{
		return;
	}
	if (AYSPlayerState* State = FindPlayerState(InstigatorController, Causer))
	{
		FYSMatchStats& Stats = State->EditMatchStats();
		Stats.DamageDealt += Amount;
		Stats.BiggestHit = FMath::Max(Stats.BiggestHit, Amount);
	}
}

void YSMatchStats::RecordKill(const AController* InstigatorController, const AActor* Causer)
{
	if (AYSPlayerState* State = FindPlayerState(InstigatorController, Causer))
	{
		++State->EditMatchStats().Kills;
	}
}

void YSMatchStats::RecordDamageTaken(AYSPlayerCharacter* Victim, float DamageBeforeArmor, float DamageAfterArmor)
{
	// minions : pas de PlayerState, pas comptés
	if (AYSPlayerState* State = Victim ? Victim->GetPlayerState<AYSPlayerState>() : nullptr)
	{
		FYSMatchStats& Stats = State->EditMatchStats();
		Stats.DamageTaken += FMath::Max(DamageAfterArmor, 0.0f);
		Stats.ArmorReduced += FMath::Max(DamageBeforeArmor - DamageAfterArmor, 0.0f);
	}
}

void YSMatchStats::RecordDelivery(AYSPlayerCharacter* Carrier, bool bCorpse)
{
	if (AYSPlayerState* State = Carrier ? Carrier->GetPlayerState<AYSPlayerState>() : nullptr)
	{
		FYSMatchStats& Stats = State->EditMatchStats();
		++(bCorpse ? Stats.Revives : Stats.ItemsDelivered);
	}
}

FYSScoreUpgrades YSMatchStats::CaptureUpgrades(const AYSPlayerCharacter* Character)
{
	FYSScoreUpgrades Result;
	Result.bCaptured = true;
	if (!Character)
	{
		return Result;
	}

	// classe, upgrades d'arme (Déblocage) et leur Golden (Super), dans l'ordre d'obtention
	const TArray<TObjectPtr<UYSUpgrade>>& Pool = Character->GetUpgradePool();
	for (const FYSUpgradeProgress& Progress : Character->GetAllUpgradeProgress())
	{
		const UYSUpgrade* Upgrade = Pool.IsValidIndex(Progress.PoolIndex) ? Pool[Progress.PoolIndex].Get() : nullptr;
		if (!Upgrade || Progress.Stacks <= 0)
		{
			continue;
		}
		TArray<FYSScoreUpgrade>* List = nullptr;
		if (Upgrade->GetUpgradeType() == EYSUpgradeType::Class)
		{
			List = &Result.ClassUpgrades;
		}
		else if (Upgrade->IsA<UYSWeaponUpgrade>())
		{
			const EYSWeaponUpgradeTier Tier = UYSWeaponUpgrade::GetTier(Progress.Variant);
			List = Tier == EYSWeaponUpgradeTier::Unlock ? &Result.WeaponUpgrades : Tier == EYSWeaponUpgradeTier::Super ? &Result.GoldenUpgrades : nullptr;
		}
		if (List)
		{
			FYSScoreUpgrade& Line = List->AddDefaulted_GetRef();
			Line.Icon = Upgrade->Icon;
			Line.Name = Upgrade->GetVariantNameFor(Character, Progress.Variant);
		}
	}

	for (const FYSShardLevel& ShardLevel : Character->GetShardLevels())
	{
		if (ShardLevel.Shard && ShardLevel.Level > 0)
		{
			FYSScoreShard& Line = Result.Shards.AddDefaulted_GetRef();
			Line.Name = ShardLevel.Shard->DisplayName.IsEmpty() ? FText::FromString(ShardLevel.Shard->GetName()) : ShardLevel.Shard->DisplayName;
			Line.Level = ShardLevel.Level;
			Line.MaxLevel = ShardLevel.Shard->MaxLevel;
		}
	}
	Result.Shards.StableSort([](const FYSScoreShard& A, const FYSScoreShard& B) { return A.Level > B.Level; });
	return Result;
}
