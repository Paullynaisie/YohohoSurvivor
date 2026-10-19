#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Abilities/YSAbilityComponent.h"
#include "UI/YSCharacterSelectWidget.h"
#include "YSUpgrade.generated.h"

class AYSPlayerCharacter;
class AYSWeapon;
class UYSAbilityComponent;
class UYSWeaponSet;

/** Catégorie d'upgrade : définit son comportement */
UENUM(BlueprintType)
enum class EYSUpgradeType : uint8
{
	Stat		UMETA(DisplayName="Upgrade de stat"),
	Weapon		UMETA(DisplayName="Upgrade d'arme"),
	Special		UMETA(DisplayName="Upgrade spéciale"),
	Passive		UMETA(DisplayName="Upgrade passive"),
	Class		UMETA(DisplayName="Upgrade de classe"),
};

/** Rareté d'un bonus, tirée au moment où il est proposé */
UENUM(BlueprintType)
enum class EYSUpgradeRarity : uint8
{
	None,
	Bronze,
	Silver,
	Gold,
};

UENUM(BlueprintType)
enum class EYSHand : uint8
{
	Right,
	Left,
};

/** Une proposition du menu de montée de niveau */
USTRUCT(BlueprintType)
struct FYSUpgradeOffer
{
	GENERATED_BODY()

	/** Index dans l'UpgradePool du personnage */
	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	int32 PoolIndex = INDEX_NONE;

	/** Variante de l'upgrade (ex. Damage bonus : 0 = Ability, 1 = Auto atk) */
	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	int32 Variant = 0;

	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	EYSUpgradeRarity Rarity = EYSUpgradeRarity::None;

	bool operator==(const FYSUpgradeOffer& Other) const { return PoolIndex == Other.PoolIndex && Variant == Other.Variant && Rarity == Other.Rarity; }
};

/** Progression d'une variante d'upgrade du pool sur un personnage (répliquée) */
USTRUCT(BlueprintType)
struct FYSUpgradeProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	int32 PoolIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	int32 Variant = 0;

	/** Nombre de bonus déjà obtenus */
	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	int32 Stacks = 0;

	/** Somme des valeurs des bonus obtenus */
	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	float TotalValue = 0.0f;

	/** Valeur de chaque bonus obtenu */
	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	TArray<float> BonusValues;
};

/**
 *  Amélioration proposée au joueur à chaque montée de niveau de l'équipe.
 *  Chaque upgrade est un Data Asset (Content Browser > clic droit > Miscellaneous > Data Asset, choisir la classe),
 *  réglé une seule fois pour tous les personnages ; les personnages le référencent dans 3C > Upgrades > Upgrade Pool.
 *  Une upgrade peut avoir plusieurs variantes proposées et cumulées séparément (ex. Damage bonus : Ability / Auto atk).
 */
UCLASS(Abstract, BlueprintType)
class UYSUpgrade : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Nom affiché sur le bouton */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FText DisplayName;

	/** Catégorie (fixée par la classe) */
	UFUNCTION(BlueprintPure, Category="Upgrade")
	EYSUpgradeType GetUpgradeType() const { return UpgradeType; }

	/** Poids de tirage : plus il est élevé, plus l'upgrade (chaque variante) a de chances d'être proposée */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade", meta=(ClampMin="0"))
	float OfferWeight = 10.0f;

	/** Raretés dans lesquelles l'upgrade peut être proposée (la rareté est tirée d'abord, selon la Luck du joueur) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Rarity")
	bool bCanBeBronze = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Rarity")
	bool bCanBeSilver = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Rarity")
	bool bCanBeGold = true;

	/** Couleur du nom selon la rareté */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Rarity")
	FLinearColor BronzeColor = FLinearColor(0.85f, 0.5f, 0.22f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Rarity")
	FLinearColor SilverColor = FLinearColor(0.8f, 0.85f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Rarity")
	FLinearColor GoldColor = FLinearColor(1.0f, 0.8f, 0.1f);

	/** Nombre de variantes (chacune proposée et cumulée séparément) */
	virtual int32 GetVariantCount() const { return 1; }

	/** Nom affiché d'une variante */
	virtual FText GetVariantName(int32 Variant) const { return DisplayName; }

	/** Nom affiché d'une variante pour ce personnage (ex. avec le nom de ses armes) */
	virtual FText GetVariantNameFor(const AYSPlayerCharacter* Character, int32 Variant) const { return GetVariantName(Variant); }

	/** Peut encore être proposée à ce personnage (Progress = ce qu'il a déjà obtenu de cette variante) */
	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const { return true; }

	/** Poids de tirage d'une variante (par défaut OfferWeight) */
	virtual float GetOfferWeight(int32 Variant) const { return OfferWeight; }

	/** La variante peut être proposée dans cette rareté (par défaut bCanBeBronze / Silver / Gold) */
	virtual bool SupportsRarity(int32 Variant, EYSUpgradeRarity Rarity) const;

	/** Valeur ajoutée à Progress.TotalValue par un bonus de cette rareté */
	virtual float GetBonusValue(int32 Variant, EYSUpgradeRarity Rarity) const { return 0.0f; }

	/** Lignes du bouton pour une proposition */
	virtual void GetOfferLines(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, TArray<FYSChoiceLine>& OutLines) const;

	/** Applique l'état de la variante au personnage après un nouveau bonus (serveur ; Progress déjà mis à jour) */
	virtual void Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const {}

	FLinearColor GetRarityColor(EYSUpgradeRarity Rarity) const;

protected:

	UPROPERTY(VisibleDefaultsOnly, Category="Upgrade")
	EYSUpgradeType UpgradeType = EYSUpgradeType::Stat;
};

/**
 *  Upgrade de stat : modifie une statistique pure du joueur, stat = valeur de base x (1 + somme des bonus)
 *  (ou base + somme pour les stats additives : regen, armure).
 *  Chaque variante cumule jusqu'à MaxStacks bonus (ensuite elle n'est plus proposée) ;
 *  chaque bonus a une rareté (tirée selon la Luck du joueur) qui fixe sa valeur.
 *  Nouvelle stat = sous-classe qui surcharge GetBaseStatValue (affichage), ApplyBonus et si besoin GetStatValue.
 */
UCLASS(Abstract)
class UYSStatUpgrade : public UYSUpgrade
{
	GENERATED_BODY()

public:

	UYSStatUpgrade();

	/** Nom de la stat sur le bouton (« Dégât : 40 ») */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FText StatLabel;

	/** Nombre maximum de bonus cumulables (par variante) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade", meta=(ClampMin="1"))
	int32 MaxStacks = 10;

	/** Valeur d'un bonus selon sa rareté (ajoutée au multiplicateur) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Rarity")
	float BronzeValue = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Rarity")
	float SilverValue = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Rarity")
	float GoldValue = 0.9f;

	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

	virtual float GetBonusValue(int32 Variant, EYSUpgradeRarity Rarity) const override;

	virtual void GetOfferLines(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, TArray<FYSChoiceLine>& OutLines) const override;

	/** Applique la somme des bonus au personnage (ApplyBonus) */
	virtual void Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

protected:

	/** Valeur de base de la stat, sans bonus (affichage des valeurs concrètes) */
	virtual float GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const { return 1.0f; }

	/** Valeur de la stat pour une somme de bonus : base x (1 + somme) par défaut (additive : base + somme) */
	virtual float GetStatValue(float BaseValue, float TotalBonus) const { return BaseValue * (1.0f + TotalBonus); }

	/** Applique la somme des bonus au personnage (serveur) */
	virtual void ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const {}
};

/**
 *  Upgrade de stat par type d'action, pour tout le set d'arme : 2 bonus distincts cumulés séparément
 *  (variante 0 = Ability, l'action du clic ; 1 = Auto atk, l'attaque automatique), appliqués à toutes les armes tenues.
 *  Une variante n'est proposée que si une arme tenue a une action de ce type.
 *  Nom affiché : « <armes> <type> <BonusName> » (ex. « Epee Auto atk Damage bonus », « Pistolet & Epee ... » si deux armes différentes).
 *  (Nom de classe historique : les bonus étaient par main.)
 */
UCLASS(Abstract)
class UYSHandStatUpgrade : public UYSStatUpgrade
{
	GENERATED_BODY()

public:

	UYSHandStatUpgrade();

	/** Morceaux du nom affiché */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Names")
	FText AbilityLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Names")
	FText AutoAtkLabel;

	/** Nom du bonus (« Damage bonus », « CD reduction ») */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Names")
	FText BonusName;

	static EYSActionSlot GetSlot(int32 Variant) { return Variant == 1 ? EYSActionSlot::AutoAtk : EYSActionSlot::Ability; }

	static int32 MakeVariant(EYSActionSlot Slot) { return Slot == EYSActionSlot::AutoAtk ? 1 : 0; }

	virtual int32 GetVariantCount() const override { return 2; }

	virtual FText GetVariantName(int32 Variant) const override;

	/** Nom avec les armes du set qui ont ce type d'action */
	virtual FText GetVariantNameFor(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

protected:

	/** Actions de ce type parmi les armes tenues, concernées par l'upgrade (main principale d'abord) */
	TArray<UYSAbilityComponent*> GetSlotActions(const AYSPlayerCharacter* Character, int32 Variant) const;

	/** Première de ces actions (nullptr si aucune) */
	UYSAbilityComponent* GetSlotAction(const AYSPlayerCharacter* Character, int32 Variant) const;

	/** L'upgrade concerne cette action (ex. CD reduction : pas les Guns) */
	virtual bool IsActionEligible(const UYSAbilityComponent* Action) const { return true; }

	/** « <armes> <type> <BonusName> » */
	FText MakeName(const FText& WeaponsText, int32 Variant) const;
};

/** Damage bonus (par type d'action, tout le set) : dégâts des actions x (1 + somme des bonus) */
UCLASS(meta=(DisplayName="Upgrade de stat : Damage bonus (Ability / Auto atk du set)"))
class UYSHandAtkBonusUpgrade : public UYSHandStatUpgrade
{
	GENERATED_BODY()

public:

	UYSHandAtkBonusUpgrade();

protected:

	/** Dégâts de base de l'action */
	virtual float GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual void ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const override;
};

/**
 *  CD reduction (par type d'action, tout le set) : recharge des actions x (1 - somme des bonus), jamais sous
 *  MinCooldownMultiplier. Coup d'Petoir / Auto shot : Cooldown ; Auto swing (Swing) : Auto Atk Cooldown ET Recover Duration,
 *  dans la même proportion (valeur affichée = leur somme).
 */
UCLASS(meta=(DisplayName="Upgrade de stat : CD reduction (Ability / Auto atk du set)"))
class UYSHandCooldownReductionUpgrade : public UYSHandStatUpgrade
{
	GENERATED_BODY()

public:

	UYSHandCooldownReductionUpgrade();

	/** Multiplicateur minimum de la recharge (garde-fou) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade", meta=(ClampMin="0", ClampMax="1"))
	float MinCooldownMultiplier = 0.1f;

	/** Lignes du bouton : en plus des valeurs concrètes, la réduction appliquée (« Recharge (s) : 1.41 (-6 %) ») */
	virtual void GetOfferLines(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, TArray<FYSChoiceLine>& OutLines) const override;

protected:

	/** L'Auto atk des Guns n'a pas de CD reduction (elle a l'upgrade Shoot system) */
	virtual bool IsActionEligible(const UYSAbilityComponent* Action) const override;

	/** Recharge de base de l'action */
	virtual float GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual float GetStatValue(float BaseValue, float TotalBonus) const override { return BaseValue * GetMultiplier(TotalBonus); }

	virtual void ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const override;

	float GetMultiplier(float TotalBonus) const { return FMath::Max(MinCooldownMultiplier, 1.0f - TotalBonus); }
};

/**
 *  Shoot system (Auto atk des Guns du set seulement ; leur Ability garde la CD reduction) : améliore le shoot system.
 *  PewPew : Bronze / Silver / Gold Value (1 / 2 / 3) balles de plus à chaque rafale (même durée de rafale).
 *  Shotgun / Piercing : à venir (pas proposée pour ces systèmes).
 */
UCLASS(meta=(DisplayName="Upgrade de stat : Shoot system (Guns du set)"))
class UYSShootSystemUpgrade : public UYSHandStatUpgrade
{
	GENERATED_BODY()

public:

	UYSShootSystemUpgrade();

protected:

	/** Auto atk de Gun dont le shoot system a une amélioration (PewPew pour l'instant) */
	virtual bool IsActionEligible(const UYSAbilityComponent* Action) const override;

	/** PewPew : balles par rafale de base */
	virtual float GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual float GetStatValue(float BaseValue, float TotalBonus) const override { return BaseValue + TotalBonus; }

	virtual void ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const override;
};

/** Move speed bonus : vitesse de déplacement (MoveSpeed du personnage) x (1 + somme des bonus) */
UCLASS(meta=(DisplayName="Upgrade de stat : Move speed bonus"))
class UYSMoveSpeedBonusUpgrade : public UYSStatUpgrade
{
	GENERATED_BODY()

public:

	UYSMoveSpeedBonusUpgrade();

protected:

	virtual float GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual void ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const override;
};

/** Hps bonus : PV max (MaxHealth du personnage) x (1 + somme des bonus) ; les PV gagnés s'ajoutent aux PV actuels */
UCLASS(meta=(DisplayName="Upgrade de stat : Hps bonus"))
class UYSHealthBonusUpgrade : public UYSStatUpgrade
{
	GENERATED_BODY()

public:

	UYSHealthBonusUpgrade();

protected:

	virtual float GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual void ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const override;
};

/**
 *  Hps regen : sans dégât subi depuis HealthRegenDelay (5 s, réglable sur le personnage), rend la somme des bonus en PV
 *  toutes les HealthRegenInterval (1 s). Mécanique inactive tant que le joueur n'a pas pris l'upgrade.
 */
UCLASS(meta=(DisplayName="Upgrade de stat : Hps regen"))
class UYSHealthRegenUpgrade : public UYSStatUpgrade
{
	GENERATED_BODY()

public:

	UYSHealthRegenUpgrade();

protected:

	virtual float GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const override { return 0.0f; }

	virtual float GetStatValue(float BaseValue, float TotalBonus) const override { return BaseValue + TotalBonus; }

	virtual void ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const override;
};

/** Armor : chaque coup subi est réduit de la somme des bonus. Mécanique inactive tant que le joueur n'a pas pris l'upgrade. */
UCLASS(meta=(DisplayName="Upgrade de stat : Armor"))
class UYSArmorUpgrade : public UYSStatUpgrade
{
	GENERATED_BODY()

public:

	UYSArmorUpgrade();

protected:

	virtual float GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const override { return 0.0f; }

	virtual float GetStatValue(float BaseValue, float TotalBonus) const override { return BaseValue + TotalBonus; }

	virtual void ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const override;
};

/**
 *  Luck : chaque bonus déplace LuckValue (Silver Value, 5) points de % de chance du Bronze vers le Gold
 *  pour les prochaines propositions (chances de base sur le personnage : 3C > Upgrades). N'existe qu'en Silver.
 */
UCLASS(meta=(DisplayName="Upgrade de stat : Luck"))
class UYSLuckUpgrade : public UYSStatUpgrade
{
	GENERATED_BODY()

public:

	UYSLuckUpgrade();

protected:

	/** Affichage : chance de Gold actuelle (%) */
	virtual float GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual float GetStatValue(float BaseValue, float TotalBonus) const override { return BaseValue + TotalBonus; }

	virtual void ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const override;
};

/** Palier d'une upgrade d'arme */
UENUM(BlueprintType)
enum class EYSWeaponUpgradeTier : uint8
{
	Unlock	UMETA(DisplayName="Déblocage"),
	Bonus	UMETA(DisplayName="Bonus"),
	Super	UMETA(DisplayName="Super"),
};

/**
 *  Upgrade d'arme pour tout le set, en 3 paliers (variante = palier : 0 Déblocage, 1 Bonus, 2 Super) :
 *  - Déblocage (Silver uniquement, une fois) : donne la propriété aux armes concernées ; débloque les 2 autres paliers ;
 *  - Bonus (Silver / Gold, MaxBonusStacks fois) : améliore la propriété, comme une upgrade de stat ;
 *  - Super (Gold uniquement, une fois) : comportement prédéfini.
 *  Proposée seulement si une arme tenue a une action RequiredActionClass du type RequiredSlot (ex. Coup d'Petoir en Ability).
 *  Nouvelle upgrade d'arme = sous-classe qui surcharge ApplyWeaponProperty (et GetBonusBaseValue pour l'affichage).
 */
UCLASS(Abstract)
class UYSWeaponUpgrade : public UYSUpgrade
{
	GENERATED_BODY()

public:

	UYSWeaponUpgrade();

	/** Action d'arme requise (classe ou sous-classe) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	TSubclassOf<UYSAbilityComponent> RequiredActionClass;

	/** Type d'action concerné : Ability (clic) ou Auto atk (ex. le Ricochet ne touche que le Coup d'Petoir en Ability, pas l'Auto shot) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	EYSActionSlot RequiredSlot = EYSActionSlot::Ability;

	/** Déblocage : nom et description */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Unlock")
	FText UnlockName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Unlock", meta=(MultiLine="true"))
	FText UnlockDescription;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Unlock", meta=(ClampMin="0"))
	float UnlockWeight = 10.0f;

	/** Bonus : nom, nom de la valeur affichée, nombre max, valeurs Silver / Gold */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Bonus")
	FText BonusName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Bonus")
	FText BonusStatLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Bonus", meta=(ClampMin="1"))
	int32 MaxBonusStacks = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Bonus")
	float BonusSilverValue = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Bonus")
	float BonusGoldValue = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Bonus", meta=(ClampMin="0"))
	float BonusWeight = 10.0f;

	/** Super : nom et description */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Super")
	FText SuperName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Super", meta=(MultiLine="true"))
	FText SuperDescription;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Super", meta=(ClampMin="0"))
	float SuperWeight = 10.0f;

	static EYSWeaponUpgradeTier GetTier(int32 Variant) { return static_cast<EYSWeaponUpgradeTier>(FMath::Clamp(Variant, 0, 2)); }

	static int32 MakeVariant(EYSWeaponUpgradeTier Tier) { return static_cast<int32>(Tier); }

	virtual int32 GetVariantCount() const override { return 3; }

	virtual FText GetVariantName(int32 Variant) const override;

	virtual FText GetVariantNameFor(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

	virtual float GetOfferWeight(int32 Variant) const override;

	virtual bool SupportsRarity(int32 Variant, EYSUpgradeRarity Rarity) const override;

	virtual float GetBonusValue(int32 Variant, EYSUpgradeRarity Rarity) const override;

	virtual void GetOfferLines(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, TArray<FYSChoiceLine>& OutLines) const override;

	/** Recalcule la propriété des armes à partir des 3 paliers obtenus (ApplyWeaponProperty) */
	virtual void Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

protected:

	/** Valeur affichée pour le palier Bonus sans bonus (ex. dégâts de base de l'arme) ; valeur = base x (1 + somme) */
	virtual float GetBonusBaseValue(const AYSPlayerCharacter* Character) const { return 1.0f; }

	/** Applique la propriété aux armes : débloquée ?, somme des bonus, super obtenu ? (serveur) */
	virtual void ApplyWeaponProperty(AYSPlayerCharacter* Character, bool bUnlocked, float BonusTotal, bool bSuper) const {}

	/** Actions requises des armes tenues (main principale d'abord) */
	TArray<UYSAbilityComponent*> GetRequiredActions(const AYSPlayerCharacter* Character) const;
};

/**
 *  Ricochet (Coup d'Petoir) : la balle repart vers le NME le plus proche pas encore touché après chaque dégât.
 *  Déblocage : UnlockRicochets ricochet(s) ; Bonus « Ricochet bonus » : dégâts x (1 + somme) après chaque ricochet ;
 *  Super « Golden ricochet » : SuperExtraRicochets ricochet(s) de plus.
 */
UCLASS(meta=(DisplayName="Upgrade d'arme : Ricochet"))
class UYSRicochetUpgrade : public UYSWeaponUpgrade
{
	GENERATED_BODY()

public:

	UYSRicochetUpgrade();

	/** Ricochets donnés par le déblocage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Unlock", meta=(ClampMin="0"))
	int32 UnlockRicochets = 1;

	/** Ricochets ajoutés par le Golden ricochet */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Super", meta=(ClampMin="0"))
	int32 SuperExtraRicochets = 1;

	/** Noms des propriétés d'arme posées sur le personnage (lues par le Coup d'Petoir) */
	static const FName RicochetCountStat;
	static const FName RicochetDamageBonusStat;

protected:

	/** Dégâts de base du Coup d'Petoir */
	virtual float GetBonusBaseValue(const AYSPlayerCharacter* Character) const override;

	virtual void ApplyWeaponProperty(AYSPlayerCharacter* Character, bool bUnlocked, float BonusTotal, bool bSuper) const override;
};

/**
 *  Upgrade de classe : proposée (sans hasard, sans rareté) au niveau d'équipe AYSGameMode::ClassUpgradeLevel ;
 *  les choix sont toutes les upgrades de classe proposables du pool.
 *  Normalement : change le SET D'ARME du personnage (SourceWeaponSet -> TargetWeaponSet), qui garde toutes ses upgrades.
 *  Ancien mode (si TargetWeaponSet est vide) : remplace le personnage par un TargetClass (AYSPlayerCharacter::ChangeClass).
 *  Conditions remplies seulement si renseignées : set d'arme actuel = SourceWeaponSet, classe = SourceClass / ExtraSourceClasses.
 */
UCLASS(meta=(DisplayName="Upgrade de classe"))
class UYSClassUpgrade : public UYSUpgrade
{
	GENERATED_BODY()

public:

	UYSClassUpgrade();

	/** Set d'arme que le personnage doit avoir (vide = n'importe lequel) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Class")
	TObjectPtr<UYSWeaponSet> SourceWeaponSet;

	/** Set d'arme obtenu (le personnage reste le même) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Class")
	TObjectPtr<UYSWeaponSet> TargetWeaponSet;

	/** Ancien mode : personnage qui peut prendre cette classe (classe exacte ; vide = n'importe lequel) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Class")
	TSubclassOf<AYSPlayerCharacter> SourceClass;

	/** Autres personnages qui peuvent prendre cette classe (ex. JohnnyPlaytest, copie de test de Mouss) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Class")
	TArray<TSubclassOf<AYSPlayerCharacter>> ExtraSourceClasses;

	/** Ancien mode (sans TargetWeaponSet) : personnage obtenu */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Class")
	TSubclassOf<AYSPlayerCharacter> TargetClass;

	/** Description sous le nom */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Class", meta=(MultiLine="true"))
	FText Description;

	/** Nom : DisplayName, sinon le nom du set d'arme (ou du personnage) obtenu */
	virtual FText GetVariantName(int32 Variant) const override;

	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

	virtual void GetOfferLines(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, TArray<FYSChoiceLine>& OutLines) const override;

	/** Change le set d'arme (ou remplace le personnage par TargetClass) (serveur) */
	virtual void Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;
};
