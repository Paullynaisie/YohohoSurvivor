#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Abilities/YSAbilityComponent.h"
#include "YSUpgrade.generated.h"

class AYSPlayerCharacter;
class AYSWeapon;
class UYSAbilityComponent;
class UYSWeaponSet;
class UTexture2D;
enum class EYSShootSystem : uint8;

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

/** Une valeur modifiée par une proposition (carte d'upgrade : actuelle rayée à gauche, nouvelle plus grosse à droite) */
USTRUCT(BlueprintType)
struct FYSUpgradeStatChange
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	FText OldValue;

	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	FText NewValue;
};

/** Contenu de la carte d'une proposition du menu d'upgrade (de haut en bas) */
USTRUCT(BlueprintType)
struct FYSUpgradeCard
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	FText Name;

	/** Rareté (« Bronze », « Classe »…) */
	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	FText RarityText;

	/** Couleur de la rareté : nom, rareté et carré de l'icône */
	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	FLinearColor Color = FLinearColor::White;

	/** Image posée sur le carré de couleur (vide = carré seul) */
	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	TObjectPtr<UTexture2D> Icon;

	/** Nombre de bonus après ce choix (« Bonus 2 / 10 » ; vide = rien) */
	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	FText StackText;

	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	TArray<FYSUpgradeStatChange> StatChanges;
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

	/** Rareté de chaque bonus obtenu (conversion vers l'upgrade jumelle au changement de set d'arme) */
	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	TArray<EYSUpgradeRarity> BonusRarities;
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

	/** Description affichée sur la carte du menu d'upgrade (sous le nombre de bonus) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade", meta=(MultiLine="true"))
	FText Description;

	/** Icône de la carte du menu d'upgrade (texture 256 x 256), posée sur un carré de la couleur de la rareté ; vide = carré seul */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	TObjectPtr<UTexture2D> Icon;

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

	/** Upgrade requise : celle-ci n'est proposée que si le joueur a déjà l'autre (ex. Atk speed / Shoot system après Atk critique) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Requirements")
	TObjectPtr<UYSUpgrade> RequiredUpgrade;

	/** Tant que le joueur n'a pas cette upgrade, celle-ci n'est proposée qu'en Bronze (ex. Atk speed / Shoot system avant Atk critique) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Requirements")
	TObjectPtr<UYSUpgrade> HigherRaritiesUpgrade;

	/**
	 *  Upgrade jumelle (Ricochet <-> Bloodlust, Atk speed <-> Shoot system) : jamais proposée si le joueur a déjà l'autre ;
	 *  au changement de set d'arme, si ce set n'est pas compatible avec celle-ci mais l'est avec la jumelle, la progression
	 *  est convertie (même variante, chaque bonus gardant sa rareté)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Twin")
	TObjectPtr<UYSUpgrade> TwinUpgrade;

	/** Le personnage a ce qu'il faut pour cette variante (arme, type d'action) ; conversion des jumelles */
	virtual bool IsCompatibleWith(const AYSPlayerCharacter* Character, int32 Variant) const { return true; }

	/** Upgrade requise obtenue et jumelle pas encore prise */
	bool PassesRequirements(const AYSPlayerCharacter* Character) const;

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

	/** Proposable à ce joueur dans cette rareté : SupportsRarity, et au-dessus du Bronze seulement s'il a Higher Rarities Upgrade */
	bool CanOfferRarity(const AYSPlayerCharacter* Character, int32 Variant, EYSUpgradeRarity Rarity) const;

	/** Valeur ajoutée à Progress.TotalValue par un bonus de cette rareté */
	virtual float GetBonusValue(int32 Variant, EYSUpgradeRarity Rarity) const { return 0.0f; }

	/** Description d'une variante pour ce personnage (par défaut Description) */
	virtual FText GetDescription(const AYSPlayerCharacter* Character, int32 Variant) const { return Description; }

	/** Carte du menu d'upgrade pour une proposition (nom, rareté, icône, bonus, description, valeurs modifiées) */
	virtual void GetOfferCard(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, FYSUpgradeCard& OutCard) const;

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

	/**
	 *  Effet en plus : chaque bonus augmente aussi les dégâts des coups critiques selon sa rareté (0 = rien ; Atk speed et
	 *  Shoot system : 0.1 / 0.2 / 0.3). Dégâts critiques = Critical Damage Multiplier du perso x (1 + somme de ces bonus)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Critical", meta=(ClampMin="0"))
	float CriticalDamageBronze = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Critical", meta=(ClampMin="0"))
	float CriticalDamageSilver = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Critical", meta=(ClampMin="0"))
	float CriticalDamageGold = 0.0f;

	/** Bonus aux dégâts critiques d'un bonus de cette rareté */
	float GetCriticalDamageBonus(EYSUpgradeRarity Rarity) const;

	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

	virtual float GetBonusValue(int32 Variant, EYSUpgradeRarity Rarity) const override;

	virtual void GetOfferCard(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, FYSUpgradeCard& OutCard) const override;

	/** Applique la somme des bonus au personnage (ApplyBonus) */
	virtual void Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

protected:

	/** Valeur de base de la stat, sans bonus (affichage des valeurs concrètes) */
	virtual float GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const { return 1.0f; }

	/** Valeur de la stat pour une somme de bonus : base x (1 + somme) par défaut (additive : base + somme) */
	virtual float GetStatValue(float BaseValue, float TotalBonus) const { return BaseValue * (1.0f + TotalBonus); }

	/** Nom de la stat sur le bouton (défaut : StatLabel) */
	virtual FText GetStatLabel(const AYSPlayerCharacter* Character, int32 Variant) const { return StatLabel; }

	/** Valeur concrète affichée sur le bouton pour une somme de bonus (défaut : GetStatValue de la valeur de base) */
	virtual float GetDisplayValue(const AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const { return GetStatValue(GetBaseStatValue(Character, Variant), TotalBonus); }

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

	/** Variantes proposées : Ability (action du clic) et / ou Auto atk (ex. CD reduction : Ability seule ; Atk speed : Auto atk seule) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Variants")
	bool bAbilityVariant = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Variants")
	bool bAutoAtkVariant = true;

	/** Variante proposée par cet asset et une arme tenue a une action éligible de ce type */
	virtual bool IsCompatibleWith(const AYSPlayerCharacter* Character, int32 Variant) const override;

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
	virtual FText MakeName(const FText& WeaponsText, int32 Variant) const;
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

	/** Nom de la variante Auto atk (« Epee Atk speed ») ; la variante Ability garde « <armes> Ability CD reduction » */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Names")
	FText AutoAtkBonusName;

	/** Description de la variante Auto atk (Atk speed) ; la variante Ability garde Description */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Names", meta=(MultiLine="true"))
	FText AutoAtkDescription;

	virtual FText GetDescription(const AYSPlayerCharacter* Character, int32 Variant) const override;

	/** Carte : en plus des valeurs concrètes, la réduction appliquée (« Recharge (s) : 1.41 (-6 %) ») */
	virtual void GetOfferCard(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, FYSUpgradeCard& OutCard) const override;

protected:

	/** L'Auto atk des Guns n'a pas de CD reduction (elle a l'upgrade Shoot system) */
	virtual bool IsActionEligible(const UYSAbilityComponent* Action) const override;

	/** Recharge de base de l'action */
	virtual float GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual float GetStatValue(float BaseValue, float TotalBonus) const override { return BaseValue * GetMultiplier(TotalBonus); }

	/** Auto atk : « <armes> Atk speed » */
	virtual FText MakeName(const FText& WeaponsText, int32 Variant) const override;

	virtual void ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const override;

	float GetMultiplier(float TotalBonus) const { return FMath::Max(MinCooldownMultiplier, 1.0f - TotalBonus); }
};

/**
 *  Shoot system (Auto atk des Guns du set seulement ; leur Ability garde la CD reduction) : améliore le shoot system.
 *  PewPew : Bronze / Silver / Gold Value (1 / 2 / 3) balles de plus à chaque rafale (même durée de rafale).
 *  Shotgun : Shotgun Ammo Per Level (2) AMMO de plus par tir et par niveau de rareté (Bronze 2 / Silver 4 / Gold 6).
 *  Sniper : Sniper Hits Per Level (1) NME de plus traversés par chaque balle et par point de bonus.
 */
UCLASS(meta=(DisplayName="Upgrade de stat : Shoot system (Guns du set)"))
class UYSShootSystemUpgrade : public UYSHandStatUpgrade
{
	GENERATED_BODY()

public:

	UYSShootSystemUpgrade();

	/** Shotgun : AMMO de plus par tir pour chaque point de bonus (Bronze / Silver / Gold Value 1 / 2 / 3 -> 2 / 4 / 6) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Shotgun", meta=(ClampMin="0"))
	float ShotgunAmmoPerLevel = 2.0f;

	/** Shotgun : nom de la stat sur le bouton */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Shotgun")
	FText ShotgunStatLabel;

	/** Sniper : NME de plus touchés par chaque balle pour chaque point de bonus */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Sniper", meta=(ClampMin="0"))
	float SniperHitsPerLevel = 1.0f;

	/** Sniper : nom de la stat sur le bouton */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Sniper")
	FText SniperStatLabel;

protected:

	/** Auto atk de Gun dont le shoot system a une amélioration (PewPew, Shotgun, Sniper) */
	virtual bool IsActionEligible(const UYSAbilityComponent* Action) const override;

	/** PewPew : balles par rafale de base ; Shotgun : AMMO par tir ; Sniper : NME touchés par balle */
	virtual float GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual float GetStatValue(float BaseValue, float TotalBonus) const override { return BaseValue + TotalBonus; }

	virtual FText GetStatLabel(const AYSPlayerCharacter* Character, int32 Variant) const override;

	/** Shotgun : chaque point de bonus vaut Shotgun Ammo Per Level AMMO */
	virtual float GetDisplayValue(const AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const override;

	virtual void ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const override;

	/** Ajout par point de bonus pour le shoot system de l'action du type Variant (PewPew 1 balle, Shotgun Ammo Per Level AMMO, Sniper Hits Per Level NME) */
	float GetAmmoPerBonus(const AYSPlayerCharacter* Character, int32 Variant) const;
};

/**
 *  Summon system (armes d'invocation : Ability de type Summon, ex. Summon the crew du Crochet du capitaine) : remplace pour ces
 *  armes le Shoot system / l'Atk speed. Chaque bonus ajoute Bronze / Silver / Gold Value (1 par défaut) minions au maximum
 *  de minions vivants en même temps.
 */
UCLASS(meta=(DisplayName="Upgrade de stat : Summon system (armes d'invocation du set)"))
class UYSSummonSystemUpgrade : public UYSHandStatUpgrade
{
	GENERATED_BODY()

public:

	UYSSummonSystemUpgrade();

protected:

	/** Ability d'invocation (Summon the crew) */
	virtual bool IsActionEligible(const UYSAbilityComponent* Action) const override;

	/** Minions max de base de l'invocation */
	virtual float GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual float GetStatValue(float BaseValue, float TotalBonus) const override { return BaseValue + TotalBonus; }

	/** « <armes> Summon system » (sans « Ability ») */
	virtual FText MakeName(const FText& WeaponsText, int32 Variant) const override;

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

	/** Regen de base du personnage (3C|Health) */
	virtual float GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const override;

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

/**
 *  Dégât critique : la somme des bonus s'ajoute au multiplicateur de base des coups critiques (1.5 + 0.9 = x2.4),
 *  avant les bonus critiques d'Atk speed / Shoot system. Proposée seulement si le personnage peut faire des critiques.
 */
UCLASS(meta=(DisplayName="Upgrade de stat : Dégât critique"))
class UYSCriticalDamageUpgrade : public UYSStatUpgrade
{
	GENERATED_BODY()

public:

	UYSCriticalDamageUpgrade();

	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

protected:

	virtual float GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual float GetStatValue(float BaseValue, float TotalBonus) const override { return BaseValue + TotalBonus; }

	/** Affichage : dégâts critiques concrets (%), bonus d'Atk speed / Shoot system compris */
	virtual float GetDisplayValue(const AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const override;

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

	/** Sous-classes de Required Action Class non concernées (ex. Ricochet : pas la Roll and Bomb, qui ne tire pas de balle) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	TArray<TSubclassOf<UYSAbilityComponent>> ExcludedActionClasses;

	/** Type d'action concerné : Ability (clic) ou Auto atk (ex. le Ricochet ne touche que le Coup d'Petoir en Ability, pas l'Auto shot) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	EYSActionSlot RequiredSlot = EYSActionSlot::Ability;

	/** Armes concernées seulement (classe ou Blueprint enfant ; vide = toutes), ex. BloodRage : la GreatAxe */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	TArray<TSubclassOf<AYSWeapon>> RequiredWeaponClasses;

	/** Armes jamais concernées, ex. Bloodlust : pas la GreatAxe (elle a la BloodRage) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	TArray<TSubclassOf<AYSWeapon>> ExcludedWeaponClasses;

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

	/** Description du palier Bonus (vide = Description) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Bonus", meta=(MultiLine="true"))
	FText BonusDescription;

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

	/** Une arme tenue a l'action requise (jumelle : Upgrade > Twin Upgrade, conversion palier par palier) */
	virtual bool IsCompatibleWith(const AYSPlayerCharacter* Character, int32 Variant) const override { return GetRequiredActions(Character).Num() > 0; }

	static EYSWeaponUpgradeTier GetTier(int32 Variant) { return static_cast<EYSWeaponUpgradeTier>(FMath::Clamp(Variant, 0, 2)); }

	static int32 MakeVariant(EYSWeaponUpgradeTier Tier) { return static_cast<int32>(Tier); }

	virtual int32 GetVariantCount() const override { return 3; }

	virtual FText GetVariantName(int32 Variant) const override;

	virtual FText GetVariantNameFor(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

	virtual float GetOfferWeight(int32 Variant) const override;

	virtual bool SupportsRarity(int32 Variant, EYSUpgradeRarity Rarity) const override;

	virtual float GetBonusValue(int32 Variant, EYSUpgradeRarity Rarity) const override;

	/** Description du palier : Unlock / Bonus / Super Description */
	virtual FText GetDescription(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual void GetOfferCard(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, FYSUpgradeCard& OutCard) const override;

	/** Recalcule la propriété des armes à partir des 3 paliers obtenus (ApplyWeaponProperty) */
	virtual void Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

protected:

	/** Valeur affichée pour le palier Bonus sans bonus (ex. dégâts de base de l'arme) ; valeur = base x (1 + somme) */
	virtual float GetBonusBaseValue(const AYSPlayerCharacter* Character) const { return 1.0f; }

	/** Valeur concrète affichée pour une somme de bonus (par défaut base x (1 + somme)) */
	virtual float GetBonusDisplayValue(const AYSPlayerCharacter* Character, float BonusTotal) const { return GetBonusBaseValue(Character) * (1.0f + BonusTotal); }

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
 *  Bloodlust (Auto swing des épées, jumeau du Ricochet) : chaque Auto swing qui touche donne une BL stack (une seule par
 *  attaque), jusqu'à MaxStacks ; toutes perdues après StackDuration sans toucher (délai relancé à chaque stack ou coup au max).
 *  Chaque stack = AtkSpeedPerStack de vitesse d'atk en plus, appliquée APRÈS l'Atk speed (recharge et retour de l'Auto swing).
 *  Bonus « Bloodlust Atk speed » : + Silver / Gold Value de vitesse d'atk PAR STACK (additif : 3 Silver = 8 % par stack) ;
 *  Super « Golden bloodlust » : Auto swing critiques au maximum de stacks (dégâts x CriticalDamageMultiplier du perso).
 */
UCLASS(meta=(DisplayName="Upgrade d'arme : Bloodlust"))
class UYSBloodlustUpgrade : public UYSWeaponUpgrade
{
	GENERATED_BODY()

public:

	UYSBloodlustUpgrade();

	/** BL stacks maximum */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Unlock", meta=(ClampMin="1"))
	int32 MaxStacks = 5;

	/** Durée sans toucher avant de perdre toutes les stacks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Unlock", meta=(ClampMin="0.1", Units="Seconds"))
	float StackDuration = 3.0f;

	/** Vitesse d'atk par stack (0.05 = 5 %) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Unlock", meta=(ClampMin="0"))
	float AtkSpeedPerStack = 0.05f;

protected:

	/** Vitesse d'atk par stack, en % */
	virtual float GetBonusDisplayValue(const AYSPlayerCharacter* Character, float BonusTotal) const override { return (AtkSpeedPerStack + BonusTotal) * 100.0f; }

	virtual void ApplyWeaponProperty(AYSPlayerCharacter* Character, bool bUnlocked, float BonusTotal, bool bSuper) const override;
};

/**
 *  BloodRage (Auto swing de la GreatAxe, à la place du Bloodlust) : chaque coup REÇU donne une stack de rage (une par coup),
 *  jusqu'à MaxStacks ; au maximum, la prochaine attaque du joueur (Auto swing ou Bumping Slash) est critique et consomme les
 *  stacks ; toutes perdues après StackDuration sans subir de coup (délai relancé à chaque coup reçu).
 *  Bonus « BloodRage ++ » : + Silver / Gold Value d'armure PAR STACK (additif) ;
 *  Super « Golden BloodRage » : les stacks ne se perdent plus avec le temps.
 */
UCLASS(meta=(DisplayName="Upgrade d'arme : BloodRage"))
class UYSBloodRageUpgrade : public UYSWeaponUpgrade
{
	GENERATED_BODY()

public:

	UYSBloodRageUpgrade();

	/** Stacks de rage maximum (au maximum : prochaine attaque critique) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Unlock", meta=(ClampMin="1"))
	int32 MaxStacks = 5;

	/** Durée sans subir de coup avant de perdre toutes les stacks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Unlock", meta=(ClampMin="0.1", Units="Seconds"))
	float StackDuration = 10.0f;

protected:

	/** Armure par stack (somme des bonus) */
	virtual float GetBonusDisplayValue(const AYSPlayerCharacter* Character, float BonusTotal) const override { return BonusTotal; }

	virtual void ApplyWeaponProperty(AYSPlayerCharacter* Character, bool bUnlocked, float BonusTotal, bool bSuper) const override;
};

/**
 *  « Une Mine ! » (Cannoneer) : la bombe de l'Ability est remplacée par une mine (Mine Class de l'action : même bombe, mode
 *  mine, rayon / 1.5, en cube) qui s'arme au lieu d'exploser puis explose quand un NME entre dans son rayon.
 *  Deux assets jumeaux (comme Ricochet / Bloodlust) : DA_Upgrade_Mine (Roll and Bomb : Bomb, Shotgun) et
 *  DA_Upgrade_MineThrower (BombThrower : Sniper), progression convertie au changement de set.
 *  Bonus « Mine repulsante » : + Silver / Gold Value de pushback (additif) en plus du pushback naturel des NME touchés ;
 *  Super « Golden Mine » : mines par 2 (roulade : la 2e au point d'arrivée ; lancer : la 2e entre le joueur et la 1re).
 *  Dès le Déblocage : dégâts des mines x Mine Damage Multiplier, recharge de l'Ability x Mine Cooldown Multiplier.
 */
UCLASS(meta=(DisplayName="Upgrade d'arme : Une Mine !"))
class UYSMineUpgrade : public UYSWeaponUpgrade
{
	GENERATED_BODY()

public:

	UYSMineUpgrade();

	/** Noms des propriétés d'arme posées sur le personnage (lues par la Roll and Bomb et le BombThrower) */
	static const FName MineStat;
	static const FName MinePushbackStat;
	static const FName MineDoubleStat;
	static const FName MineDamageStat;
	static const FName MineCooldownStat;

	/** Déblocage : multiplicateur des dégâts des mines (en plus du Damage bonus de l'Ability) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Mine", meta=(ClampMin="0"))
	float MineDamageMultiplier = 2.0f;

	/** Déblocage : multiplicateur de la recharge de l'Ability (Cooldown, en plus de la CD reduction ; pas la roulade) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Mine", meta=(ClampMin="0.01"))
	float MineCooldownMultiplier = 0.5f;

protected:

	/** Pushback ajouté par les mines (somme des bonus) */
	virtual float GetBonusDisplayValue(const AYSPlayerCharacter* Character, float BonusTotal) const override { return BonusTotal; }

	virtual void ApplyWeaponProperty(AYSPlayerCharacter* Character, bool bUnlocked, float BonusTotal, bool bSuper) const override;
};

/**
 *  « Yoloooooo » (MorningStar du Quartermaster, Auto swing en Auto atk ; armes : Required Weapon Classes) :
 *  Déblocage : la hitbox de l'arme frappe en permanence, même sans attaquer (dégâts secondaires hors coupe) ;
 *  Bonus « Yoloooooo++ » : boulet et hitbox x (1 + somme des Silver / Gold Value), longueur de la chaîne du boulet
 *  x (1 + somme des Length Silver / Gold Value) (additifs) ; Super « Golden Yoloooooo » : Atk speed du swing + Super Atk Speed
 *  (recharge et retour x (1 - Super Atk Speed)) et l'attaque auto part en continu, même sans cible.
 */
UCLASS(meta=(DisplayName="Upgrade d'arme : Yoloooooo"))
class UYSYoloUpgrade : public UYSWeaponUpgrade
{
	GENERATED_BODY()

public:

	UYSYoloUpgrade();

	/** Golden Yoloooooo : vitesse d'attaque en plus (0.5 = recharge et retour du swing x 0.5) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Super", meta=(ClampMin="0", ClampMax="0.9"))
	float SuperAtkSpeed = 0.5f;

	/** Yoloooooo++ : longueur de la chaîne du boulet en plus par bonus selon sa rareté (additif ; 0.5 = +50 %) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Bonus", meta=(ClampMin="0"))
	float BonusLengthSilverValue = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Bonus", meta=(ClampMin="0"))
	float BonusLengthGoldValue = 1.0f;

	/** Noms des propriétés d'arme posées sur le personnage (lues par l'Auto swing) */
	static const FName YoloStat;
	static const FName YoloSizeStat;
	static const FName YoloLengthStat;
	static const FName YoloAtkSpeedStat;
	static const FName YoloContinuousStat;

	/** En plus : longueur de la chaîne (somme recalculée depuis la rareté de chaque bonus) */
	virtual void Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

	/** Bonus : en plus de la taille, la longueur de la chaîne actuelle et après ce bonus */
	virtual void GetOfferCard(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, FYSUpgradeCard& OutCard) const override;

protected:

	/** Longueur de la chaîne en plus (somme) pour les bonus déjà obtenus */
	float GetLengthTotal(const FYSUpgradeProgress& BonusProgress) const;

	float GetLengthValue(EYSUpgradeRarity Rarity) const { return Rarity == EYSUpgradeRarity::Gold ? BonusLengthGoldValue : Rarity == EYSUpgradeRarity::Silver ? BonusLengthSilverValue : 0.0f; }

	/** Taille du boulet en plus (%) */
	virtual float GetBonusDisplayValue(const AYSPlayerCharacter* Character, float BonusTotal) const override { return BonusTotal * 100.0f; }

	virtual void ApplyWeaponProperty(AYSPlayerCharacter* Character, bool bUnlocked, float BonusTotal, bool bSuper) const override;
};

/**
 *  « You're all my minions » (Cursed Captain, Ability « Allez-y mes minions ! » du Cursed Hook) :
 *  Déblocage : l'Ability des joueurs à moins de All My Minions Range (2500) du Captain est forcée aussi (priorité sur toutes
 *  leurs actions, recharge ignorée et pas déclenchée) ; Bonus « You're all my minions ++ » : dégâts des actions forcées
 *  x (1 + somme) (Silver / Gold Value) ; Super « Golden minions » : les actions forcées rendent en plus leur recharge.
 */
UCLASS(meta=(DisplayName="Upgrade d'arme : You're all my minions"))
class UYSAllMyMinionsUpgrade : public UYSWeaponUpgrade
{
	GENERATED_BODY()

public:

	UYSAllMyMinionsUpgrade();

	/** Noms des propriétés d'arme posées sur le personnage (lues par Allez-y mes minions !) */
	static const FName AllMyMinionsStat;
	static const FName ForcedDamageStat;
	static const FName ForcedCooldownResetStat;

protected:

	/** Bonus de dégâts des actions forcées (%) */
	virtual float GetBonusDisplayValue(const AYSPlayerCharacter* Character, float BonusTotal) const override { return BonusTotal * 100.0f; }

	virtual void ApplyWeaponProperty(AYSPlayerCharacter* Character, bool bUnlocked, float BonusTotal, bool bSuper) const override;
};

/**
 *  Upgrade de classe : proposée (sans hasard, sans rareté) au niveau d'équipe AYSGameMode::ClassUpgradeLevel ;
 *  les choix sont toutes les upgrades de classe proposables du pool.
 *  Normalement : change le SET D'ARME du personnage (SourceWeaponSet -> TargetWeaponSet), qui garde toutes ses upgrades.
 *  Ancien mode (si TargetWeaponSet est vide) : remplace le personnage par un TargetClass (AYSPlayerCharacter::ChangeClass).
 *  Conditions remplies seulement si renseignées : set d'arme actuel = SourceWeaponSet, classe = SourceClass / ExtraSourceClasses.
 */
/**
 *  « Atk critique » (unique, une fois, Gold uniquement) : les attaques auto deviennent régulièrement critiques.
 *  Auto swing (Melee Swing) : chaque arme compte ses attaques auto, la SwingCriticalInterval-ième (5e) est toujours critique.
 *  PewPew (Auto atk des Guns) : chaque NME compte les balles d'attaque auto reçues de chaque joueur, la BulletCriticalInterval-ième
 *  (3e) est toujours critique. Shotgun : AMMO critique sur un NME à moins de ShotgunCriticalMaxDistance (200) du joueur.
 *  Sniper : balle critique sur un NME à plus de SniperCriticalMinDistance (1000) du joueur (distances au moment du coup).
 *  Proposée si une arme tenue a une de ces attaques auto ; débloque les raretés Silver et Gold d Atk speed et Shoot system
 *  (leur Higher Rarities Upgrade : sans elle, Bronze seulement). Dégâts critiques = Critical Damage Multiplier du perso x (1 + bonus critiques d'Atk speed / Shoot system).
 */
UCLASS(meta=(DisplayName="Upgrade spéciale : Atk critique"))
class UYSCriticalAttackUpgrade : public UYSUpgrade
{
	GENERATED_BODY()

public:

	UYSCriticalAttackUpgrade();

	/** Auto swing : une attaque auto sur N est critique */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade", meta=(ClampMin="1"))
	int32 SwingCriticalInterval = 5;

	/** PewPew : une balle sur N reçue par un même NME est critique */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade", meta=(ClampMin="1"))
	int32 BulletCriticalInterval = 3;

	/** Shotgun : une AMMO est critique sur un NME à moins de cette distance du joueur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade", meta=(ClampMin="1", Units="Centimeters"))
	float ShotgunCriticalMaxDistance = 200.0f;

	/** Sniper : une balle est critique sur un NME à plus de cette distance du joueur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade", meta=(ClampMin="1", Units="Centimeters"))
	float SniperCriticalMinDistance = 1000.0f;

	/** Descriptions du bouton ({0} = l'intervalle ou la distance) selon l'attaque auto du joueur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Names")
	FText SwingDescription;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Names")
	FText BulletDescription;

	/** Shotgun ({0} = Shotgun Critical Max Distance) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Names")
	FText ShotgunDescription;

	/** Sniper ({0} = Sniper Critical Min Distance) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Names")
	FText SniperDescription;

	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

	virtual bool IsCompatibleWith(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual void GetOfferCard(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, FYSUpgradeCard& OutCard) const override;

	virtual void Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

	/** Attaque auto Swing / PewPew / Shotgun / Sniper parmi les armes tenues */
	static bool HasSwingAutoAttack(const AYSPlayerCharacter* Character);
	static bool HasPewPewAutoAttack(const AYSPlayerCharacter* Character);
	static bool HasShotgunAutoAttack(const AYSPlayerCharacter* Character);
	static bool HasSniperAutoAttack(const AYSPlayerCharacter* Character);

	/** Attaque auto de Gun avec ce shoot system parmi les armes tenues */
	static bool HasGunAutoAttack(const AYSPlayerCharacter* Character, EYSShootSystem System);
};

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

	/** Nom : DisplayName, sinon le nom du set d'arme (ou du personnage) obtenu */
	virtual FText GetVariantName(int32 Variant) const override;

	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

	virtual void GetOfferCard(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, FYSUpgradeCard& OutCard) const override;

	/** Change le set d'arme (ou remplace le personnage par TargetClass) (serveur) */
	virtual void Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;
};
