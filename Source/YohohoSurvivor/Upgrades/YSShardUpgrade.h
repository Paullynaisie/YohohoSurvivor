#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Upgrades/YSUpgrade.h"
#include "YSShardUpgrade.generated.h"

class AYSPlayerCharacter;
class UDataTable;
class UTexture2D;
class UYSUpShard;
class UYSUpStats;

/** Valeur du personnage qu'une shard améliore */
UENUM(BlueprintType)
enum class EYSShardStat : uint8
{
	AutoAtkDamage	UMETA(DisplayName="Auto atk damage"),
	AbilityDamage	UMETA(DisplayName="Ability damage"),
	CritDamage		UMETA(DisplayName="Crit damage"),
	HpsBonus		UMETA(DisplayName="Hps bonus"),
	MoveSpeed		UMETA(DisplayName="Move speed"),
	RegenHps		UMETA(DisplayName="Regen Hps"),
	Armor			UMETA(DisplayName="Armor"),
	CdReduction		UMETA(DisplayName="Cd reduction (Ability)"),
	AtkSpeed		UMETA(DisplayName="Atk speed (Auto atk)"),
	ShootSystem		UMETA(DisplayName="Shoot system (Auto atk des Guns)"),
	Luck			UMETA(DisplayName="Luck"),
	MaxStats		UMETA(DisplayName="Max stats (spéciale)"),
	MinStats		UMETA(DisplayName="Min stats (spéciale)"),
	Random			UMETA(DisplayName="Random (spéciale)"),
};

/** Équation d'une shard (Value = valeur du personnage sans upgrade, Lvl = nombre de cette shard obtenues) */
UENUM(BlueprintType)
enum class EYSShardEquation : uint8
{
	Multiplicative	UMETA(DisplayName="Multiplicative : Value x (Base + Index x Lvl)"),
	Incremental		UMETA(DisplayName="Incrémentale : Value + (Base + Index x Lvl)"),
	Divisible		UMETA(DisplayName="Divisible : Value x (Base - Index x Lvl)"),
	Truncated		UMETA(DisplayName="Incrémentale tronquée : Value + Tronque(Base + Index x Lvl)"),
};

/** Niveau d'une shard sur un personnage (répliqué) */
USTRUCT(BlueprintType)
struct FYSShardLevel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	TObjectPtr<UYSUpShard> Shard;

	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	int32 Level = 0;
};

/**
 *  « DA_Up_Shard » : une valeur améliorable du personnage, de 0 (pas d'upgrade) à Max Level (30). Obtenue par les DA_Up_Stats
 *  (chaque entrée de leur tableau = +1 niveau). Effet = équation (Equation, Base, Index) appliquée à la valeur du personnage.
 *  Shards spéciales (Max stats, Min stats, Random) : remplacées au moment de la proposition par une vraie shard.
 */
UCLASS(BlueprintType, meta=(DisplayName="Upgrade : Shard (DA_Up_Shard)"))
class UYSUpShard : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Nom affiché sur la carte d'upgrade */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shard")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shard", meta=(MultiLine="true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shard")
	TObjectPtr<UTexture2D> Icon;

	/** Valeur du personnage modifiée */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shard")
	EYSShardStat Stat = EYSShardStat::AutoAtkDamage;

	/** Équation (sans effet pour Luck et les shards spéciales) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shard|Equation")
	EYSShardEquation Equation = EYSShardEquation::Multiplicative;

	/** Constante Base de l'équation. Crit damage : le multiplicateur de base du personnage remplace Base (1.5, Marksman du Cannoneer : 2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shard|Equation")
	float Base = 1.0f;

	/** Constante Index de l'équation (ajoutée par niveau). Shoot system : valeur du PewPew */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shard|Equation")
	float Index = 0.135f;

	/** Niveau maximum */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shard|Equation", meta=(ClampMin="1"))
	int32 MaxLevel = 30;

	/** Divisible : multiplicateur minimum (garde-fou, jamais une recharge nulle) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shard|Equation", meta=(ClampMin="0", ClampMax="1"))
	float MinMultiplier = 0.1f;

	/** Shoot system : Index pour le Shotgun (AMMO par tir) et le Sniper (NME traversés) ; le PewPew prend Index */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shard|Shoot System")
	float ShotgunIndex = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shard|Shoot System")
	float SniperIndex = 0.5f;

	/** Luck : points de % de chance ajoutés à chaque rareté par niveau (négatif = retirés) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shard|Luck")
	float BronzePerLevel = -1.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shard|Luck")
	float SilverPerLevel = -0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shard|Luck")
	float GoldPerLevel = 1.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shard|Luck")
	float MythicPerLevel = 0.65f;

	/**
	 *  Shard jumelle (Atk speed <-> Shoot system) : au changement de set d'arme, si cette shard ne concerne plus le personnage
	 *  mais que la jumelle oui, ses niveaux passent à la jumelle
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shard")
	TObjectPtr<UYSUpShard> TwinShard;

	/** Max stats, Min stats, Random : remplacées par une vraie shard à la proposition */
	bool IsSpecial() const { return Stat == EYSShardStat::MaxStats || Stat == EYSShardStat::MinStats || Stat == EYSShardStat::Random; }

	/** La shard concerne ce personnage (ex. Shoot system : une attaque auto de Gun ; Cd reduction : une Ability) */
	bool IsApplicable(const AYSPlayerCharacter* Character) const;

	/** Applique l'effet de ce niveau au personnage (serveur) */
	void Apply(AYSPlayerCharacter* Character, int32 Level) const;

	/** Valeur concrète affichée sur la carte pour un niveau ; OutLabel = nom de la valeur (faux = rien à afficher) */
	bool GetDisplayValue(const AYSPlayerCharacter* Character, int32 Level, FText& OutLabel, float& OutValue) const;

	/** Résultat de l'équation sans la valeur du personnage (Base + Index x Lvl, ou Base - Index x Lvl) */
	float GetFactor(int32 Level, float LevelIndex) const;

	/** Index de l'équation pour ce personnage (Shoot system : selon le shoot system de son attaque auto) */
	float GetIndexFor(const AYSPlayerCharacter* Character) const;
};

/**
 *  « DA_Up_Stats » : une upgrade proposée au level up. Selon sa rareté, elle donne les premières shards de son tableau
 *  (Bronze : 0 et 1, Silver : 0 à 2, Gold : 0 à 3, Mythique : les 5). Interdite quand sa Blocking Shard est au maximum :
 *  une des upgrades de remplacement du GameMode (Roadmap, CatchUp, FullRandom) est alors proposée à sa place.
 *  Proposée seulement si sa Blocking Shard concerne le personnage (ex. Shoot system : une attaque auto de Gun).
 */
UCLASS(BlueprintType, meta=(DisplayName="Upgrade : Stats (DA_Up_Stats)"))
class UYSUpStats : public UDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Up Stats")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Up Stats", meta=(MultiLine="true"))
	FText Description;

	/** Icône de la carte (texture 256 x 256), posée sur un carré de la couleur de la rareté */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Up Stats")
	TObjectPtr<UTexture2D> Icon;

	/** Shards données selon la rareté : Bronze = 0, 1 ; Silver = 0 à 2 ; Gold = 0 à 3 ; Mythique = 0 à 4 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Up Stats", meta=(EditFixedSize))
	TArray<TObjectPtr<UYSUpShard>> Shards;

	/** Shard qui, au maximum, interdit cette upgrade (vide = jamais interdite) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Up Stats")
	TObjectPtr<UYSUpShard> BlockingShard;

	/** Poids de tirage parmi les upgrades proposables */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Up Stats", meta=(ClampMin="0"))
	float OfferWeight = 1.0f;

	/** Couleurs de rareté (nom, rareté et carré de l'icône) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Up Stats|Rarity")
	FLinearColor BronzeColor = FLinearColor(0.85f, 0.5f, 0.22f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Up Stats|Rarity")
	FLinearColor SilverColor = FLinearColor(0.8f, 0.85f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Up Stats|Rarity")
	FLinearColor GoldColor = FLinearColor(1.0f, 0.8f, 0.1f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Up Stats|Rarity")
	FLinearColor MythicColor = FLinearColor(0.85f, 0.2f, 1.0f);

	UYSUpStats();

	/** Nombre de shards données par une rareté (Bronze 2, Silver 3, Gold 4, Mythique 5) */
	static int32 GetShardCount(EYSUpgradeRarity Rarity);

	FLinearColor GetRarityColor(EYSUpgradeRarity Rarity) const;

	/** Proposable à ce personnage (Blocking Shard qui le concerne) */
	bool IsApplicable(const AYSPlayerCharacter* Character) const;

	/** Interdite : Blocking Shard au maximum */
	bool IsBlocked(const AYSPlayerCharacter* Character) const;

	/** Carte du menu d'upgrade : nom, rareté, icône, description, chaque shard (niveau et valeur concrète actuelle -> nouvelle) */
	void GetOfferCard(const AYSPlayerCharacter* Character, const FYSUpgradeOffer& Offer, FYSUpgradeCard& OutCard) const;
};

/**
 *  Ligne de DT_Chara_Upgrades : liste d'upgrades (DA_Up_Stats). Ligne « AllChara » = proposables à tous les personnages ;
 *  une ligne par personnage (nom = Upgrade Table Row du personnage, sinon son Character Name) = upgrades exclusives.
 */
USTRUCT(BlueprintType)
struct FYSCharaUpgradesRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrades")
	TArray<TObjectPtr<UYSUpStats>> Upgrades;
};

/** Tirage des propositions du système de shards (serveur) */
namespace YSShardUpgrades
{
	/** Nom de la ligne commune de DT_Chara_Upgrades */
	extern const FName AllCharaRow;

	/** Upgrades de la table pour ce personnage (AllChara + sa ligne), sans doublon */
	TArray<UYSUpStats*> GetAvailableUpStats(const AYSPlayerCharacter* Character, const UDataTable* Table);

	/** Toutes les vraies shards (pas spéciales) des upgrades disponibles, qui concernent le personnage */
	TArray<UYSUpShard*> GetCandidateShards(const AYSPlayerCharacter* Character, const UDataTable* Table, const TArray<UYSUpStats*>& Replacements);

	/**
	 *  Jusqu'à Count propositions différentes : pour chacune la rareté (chances du personnage, Luck comprise), puis une upgrade
	 *  proposable au poids ; interdite (Blocking Shard au maximum) = une des Replacements à la place. Les shards de la
	 *  rareté sont résolues (spéciales remplacées, shard déjà au maximum -> Random)
	 */
	TArray<FYSUpgradeOffer> PickOffers(const AYSPlayerCharacter* Character, const UDataTable* Table, const TArray<UYSUpStats*>& Replacements, int32 Count);

	/** Rareté tirée selon les chances du personnage (Bronze, Silver, Gold, Mythique) */
	EYSUpgradeRarity RollRarity(const AYSPlayerCharacter* Character);

	/** Shards réellement données par Stats dans cette rareté (spéciales résolues en tenant compte des précédentes) */
	TArray<TObjectPtr<UYSUpShard>> ResolveShards(const AYSPlayerCharacter* Character, const UYSUpStats* Stats, EYSUpgradeRarity Rarity, const TArray<UYSUpShard*>& Candidates);
}
