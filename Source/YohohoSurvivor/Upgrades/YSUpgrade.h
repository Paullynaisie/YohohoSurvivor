#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Abilities/YSAbilityComponent.h"
#include "UI/YSCharacterSelectWidget.h"
#include "YSUpgrade.generated.h"

class AYSPlayerCharacter;
class AYSWeapon;
class UYSAbilityComponent;

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

	/** Variante de l'upgrade (ex. Atk Bonus : 0 = main droite, 1 = main gauche) */
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

	/** Valeur de chaque bonus obtenu (pour garder les meilleurs quand deux progressions fusionnent : CD reduction en akimbo) */
	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	TArray<float> BonusValues;
};

/**
 *  Amélioration proposée au joueur à chaque montée de niveau de l'équipe.
 *  Chaque upgrade est un Data Asset (Content Browser > clic droit > Miscellaneous > Data Asset, choisir la classe),
 *  réglé une seule fois pour tous les personnages ; les personnages le référencent dans 3C > Upgrades > Upgrade Pool.
 *  Une upgrade peut avoir plusieurs variantes proposées et cumulées séparément (ex. Atk Bonus : main droite / gauche).
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

	/** Nom affiché d'une variante pour ce personnage (ex. CD reduction commune en akimbo) */
	virtual FText GetVariantNameFor(const AYSPlayerCharacter* Character, int32 Variant) const { return GetVariantName(Variant); }

	/** Après un changement de classe (RestoreUpgrades), avant de réappliquer : fusionne des progressions si besoin */
	virtual void MergeProgress(const AYSPlayerCharacter* Character, int32 PoolIndex, TArray<FYSUpgradeProgress>& Progress) const {}

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
 *  Upgrade de stat par main ET par type d'action : 4 bonus distincts cumulés séparément
 *  (variante = main + 2 × type : 0 main droite Ability, 1 main gauche Ability, 2 main droite Auto atk, 3 main gauche Auto atk).
 *  Une variante n'est proposée que si l'arme de la main a une action de ce type (Ability = action du clic, Auto atk = attaque auto).
 *  Nom affiché : « <main> <type> <BonusName> » (ex. « Left hand Auto atk Damage bonus »).
 */
UCLASS(Abstract)
class UYSHandStatUpgrade : public UYSStatUpgrade
{
	GENERATED_BODY()

public:

	UYSHandStatUpgrade();

	/** Morceaux du nom affiché. La main est nommée par l'arme qu'elle tient (« Pétoir Ability Damage bonus »),
	 *  suivie en akimbo de AkimboRightLabel / AkimboLeftLabel (« Pétoir (Droite) ... ») ; Right/LeftHandLabel si la main est vide */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Names")
	FText RightHandLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Names")
	FText LeftHandLabel;

	/** Akimbo : entre parenthèses après le nom de l'arme selon la main */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Names")
	FText AkimboRightLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Names")
	FText AkimboLeftLabel;

	/** Entre parenthèses après le nom de l'arme quand le bonus est commun aux deux mains (CD reduction en akimbo) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Names")
	FText AkimboLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Names")
	FText AbilityLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Names")
	FText AutoAtkLabel;

	/** Nom du bonus (« Damage bonus », « CD reduction ») */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Names")
	FText BonusName;

	static EYSHand GetHand(int32 Variant) { return Variant % 2 == 1 ? EYSHand::Left : EYSHand::Right; }

	static EYSActionSlot GetSlot(int32 Variant) { return Variant >= 2 ? EYSActionSlot::AutoAtk : EYSActionSlot::Ability; }

	static int32 MakeVariant(EYSHand Hand, EYSActionSlot Slot) { return (Hand == EYSHand::Left ? 1 : 0) + (Slot == EYSActionSlot::AutoAtk ? 2 : 0); }

	virtual int32 GetVariantCount() const override { return 4; }

	virtual FText GetVariantName(int32 Variant) const override;

	/** Nom avec l'arme tenue à la place de la main */
	virtual FText GetVariantNameFor(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

protected:

	/** Arme tenue par la main de la variante */
	static AYSWeapon* GetHandWeapon(const AYSPlayerCharacter* Character, int32 Variant);

	/** Action de l'arme de la main pour le type de la variante (nullptr si l'arme n'en a pas) */
	static UYSAbilityComponent* GetHandAction(const AYSPlayerCharacter* Character, int32 Variant);

	/** « <main ou Akimbo> <type> <BonusName> » */
	FText MakeName(const FText& HandText, int32 Variant) const;
};

/** Damage bonus (par main et par type d'action) : dégâts de l'action x (1 + somme des bonus) */
UCLASS(meta=(DisplayName="Upgrade de stat : Damage bonus (par main et type d'action)"))
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
 *  CD reduction (par main et par type d'action ; en akimbo, une seule commune aux deux mains par type :
 *  « Akimbo Ability CD reduction » / « Akimbo Auto atk CD reduction ») : recharge de l'action x (1 - somme des bonus),
 *  jamais sous MinCooldownMultiplier. Coup d'Petoir : Cooldown ; Coup d'sabre : Auto Atk Cooldown (pas la durée de l'anim).
 */
UCLASS(meta=(DisplayName="Upgrade de stat : CD reduction (par main et type d'action)"))
class UYSHandCooldownReductionUpgrade : public UYSHandStatUpgrade
{
	GENERATED_BODY()

public:

	UYSHandCooldownReductionUpgrade();

	/** Multiplicateur minimum de la recharge (garde-fou) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade", meta=(ClampMin="0", ClampMax="1"))
	float MinCooldownMultiplier = 0.1f;

	/** Akimbo : seules les variantes de la main droite sont proposées (« Akimbo ... ») et s'appliquent aux deux mains */
	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

	virtual FText GetVariantNameFor(const AYSPlayerCharacter* Character, int32 Variant) const override;

	/** Passage en akimbo : pour chaque type d'action, les bonus des deux mains fusionnent, les MaxStacks meilleurs sont gardés */
	virtual void MergeProgress(const AYSPlayerCharacter* Character, int32 PoolIndex, TArray<FYSUpgradeProgress>& Progress) const override;

protected:

	/** Recharge de base de l'action */
	virtual float GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual float GetStatValue(float BaseValue, float TotalBonus) const override { return BaseValue * GetMultiplier(TotalBonus); }

	virtual void ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const override;

	float GetMultiplier(float TotalBonus) const { return FMath::Max(MinCooldownMultiplier, 1.0f - TotalBonus); }
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
 *  Upgrade d'arme, par main, en 3 paliers (variante = main x 3 + palier : 0-2 main droite, 3-5 main gauche) :
 *  - Déblocage (Silver uniquement, une fois) : donne la propriété à l'arme de la main ; débloque les 2 autres paliers ;
 *  - Bonus (Silver / Gold, MaxBonusStacks fois) : améliore la propriété, comme une upgrade de stat ;
 *  - Super (Gold uniquement, une fois) : comportement prédéfini.
 *  Proposée seulement si l'action de l'arme de la main est une RequiredActionClass (ex. Coup d'Petoir).
 *  Nouvelle upgrade d'arme = sous-classe qui surcharge ApplyWeaponProperty (et GetBonusBaseValue pour l'affichage).
 */
UCLASS(Abstract)
class UYSWeaponUpgrade : public UYSUpgrade
{
	GENERATED_BODY()

public:

	UYSWeaponUpgrade();

	/** Action d'arme requise dans la main (classe ou sous-classe) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	TSubclassOf<UYSAbilityComponent> RequiredActionClass;

	/** Type d'action concerné : Ability (clic) ou Auto atk (ex. le Ricochet ne touche que le Coup d'Petoir en Ability, pas l'Auto shot) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	EYSActionSlot RequiredSlot = EYSActionSlot::Ability;

	/** Préfixe des noms : nom de l'arme de la main (« Pétoir Ricochet »), suivi en akimbo de AkimboRightLabel / AkimboLeftLabel
	 *  (« Pétoir (Droite) Ricochet ») ; Right/LeftHandLabel si la main est vide */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FText RightHandLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FText LeftHandLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FText AkimboRightLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FText AkimboLeftLabel;

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

	static EYSHand GetHand(int32 Variant) { return Variant >= 3 ? EYSHand::Left : EYSHand::Right; }

	static EYSWeaponUpgradeTier GetTier(int32 Variant) { return static_cast<EYSWeaponUpgradeTier>(Variant % 3); }

	static int32 MakeVariant(EYSHand Hand, EYSWeaponUpgradeTier Tier) { return (Hand == EYSHand::Left ? 3 : 0) + static_cast<int32>(Tier); }

	virtual int32 GetVariantCount() const override { return 6; }

	virtual FText GetVariantName(int32 Variant) const override;

	virtual FText GetVariantNameFor(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

	virtual float GetOfferWeight(int32 Variant) const override;

	virtual bool SupportsRarity(int32 Variant, EYSUpgradeRarity Rarity) const override;

	virtual float GetBonusValue(int32 Variant, EYSUpgradeRarity Rarity) const override;

	virtual void GetOfferLines(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, TArray<FYSChoiceLine>& OutLines) const override;

	/** Recalcule la propriété de la main à partir des 3 paliers obtenus (ApplyWeaponProperty) */
	virtual void Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

protected:

	/** Valeur affichée pour le palier Bonus sans bonus (ex. dégâts de base de l'arme) ; valeur = base x (1 + somme) */
	virtual float GetBonusBaseValue(const AYSPlayerCharacter* Character, EYSHand Hand) const { return 1.0f; }

	/** Applique la propriété à la main : débloquée ?, somme des bonus, super obtenu ? (serveur) */
	virtual void ApplyWeaponProperty(AYSPlayerCharacter* Character, EYSHand Hand, bool bUnlocked, float BonusTotal, bool bSuper) const {}

	/** Action de l'arme de la main si elle est de la classe requise */
	UYSAbilityComponent* GetRequiredAction(const AYSPlayerCharacter* Character, EYSHand Hand) const;
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

	/** Noms des propriétés d'arme posées sur la main (lues par le Coup d'Petoir) */
	static const FName RicochetCountStat;
	static const FName RicochetDamageBonusStat;

protected:

	/** Dégâts de base du Coup d'Petoir de la main */
	virtual float GetBonusBaseValue(const AYSPlayerCharacter* Character, EYSHand Hand) const override;

	virtual void ApplyWeaponProperty(AYSPlayerCharacter* Character, EYSHand Hand, bool bUnlocked, float BonusTotal, bool bSuper) const override;
};

/**
 *  Upgrade de classe : proposée (sans hasard, sans rareté) au niveau d'équipe AYSGameMode::ClassUpgradeLevel,
 *  aux personnages de la classe SourceClass ; les choix sont toutes les upgrades de classe proposables du pool.
 *  Le personnage est remplacé par un TargetClass qui garde toutes les upgrades obtenues (AYSPlayerCharacter::ChangeClass).
 */
UCLASS(meta=(DisplayName="Upgrade de classe"))
class UYSClassUpgrade : public UYSUpgrade
{
	GENERATED_BODY()

public:

	UYSClassUpgrade();

	/** Personnage qui peut prendre cette classe (classe exacte) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Class")
	TSubclassOf<AYSPlayerCharacter> SourceClass;

	/** Personnage obtenu */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Class")
	TSubclassOf<AYSPlayerCharacter> TargetClass;

	/** Description sous le nom */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Class", meta=(MultiLine="true"))
	FText Description;

	/** Nom : DisplayName, sinon le nom du personnage obtenu */
	virtual FText GetVariantName(int32 Variant) const override;

	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

	virtual void GetOfferLines(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, TArray<FYSChoiceLine>& OutLines) const override;

	/** Remplace le personnage par TargetClass (serveur) */
	virtual void Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;
};
