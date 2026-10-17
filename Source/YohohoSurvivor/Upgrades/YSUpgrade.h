#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
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

	/** Nombre de variantes (chacune proposée et cumulée séparément) */
	virtual int32 GetVariantCount() const { return 1; }

	/** Nom affiché d'une variante */
	virtual FText GetVariantName(int32 Variant) const { return DisplayName; }

	/** Peut encore être proposée à ce personnage (Progress = ce qu'il a déjà obtenu de cette variante) */
	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const { return true; }

	/** Rareté tirée pour une nouvelle proposition (None si l'upgrade n'en a pas) */
	virtual EYSUpgradeRarity RollRarity() const { return EYSUpgradeRarity::None; }

	/** Valeur ajoutée à Progress.TotalValue par un bonus de cette rareté */
	virtual float GetBonusValue(EYSUpgradeRarity Rarity) const { return 0.0f; }

	/** Lignes du bouton pour une proposition */
	virtual void GetOfferLines(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, TArray<FYSChoiceLine>& OutLines) const;

	/** Applique l'état de la variante au personnage après un nouveau bonus (serveur ; Progress déjà mis à jour) */
	virtual void Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const {}

protected:

	UPROPERTY(VisibleDefaultsOnly, Category="Upgrade")
	EYSUpgradeType UpgradeType = EYSUpgradeType::Stat;
};
/**
 *  Upgrade de stat : modifie une statistique pure du joueur, stat = valeur de base x (1 + somme des bonus)
 *  (ou base + somme pour les stats additives : regen, armure).
 *  Chaque variante cumule jusqu'à MaxStacks bonus (ensuite elle n'est plus proposée) ;
 *  chaque bonus a une rareté tirée au hasard qui fixe sa valeur.
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

	/** Poids du tirage de chaque rareté (égaux = équiprobables) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Rarity", meta=(ClampMin="0"))
	float BronzeWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Rarity", meta=(ClampMin="0"))
	float SilverWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Rarity", meta=(ClampMin="0"))
	float GoldWeight = 1.0f;

	/** Couleur du nom selon la rareté */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Rarity")
	FLinearColor BronzeColor = FLinearColor(0.85f, 0.5f, 0.22f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Rarity")
	FLinearColor SilverColor = FLinearColor(0.8f, 0.85f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Rarity")
	FLinearColor GoldColor = FLinearColor(1.0f, 0.8f, 0.1f);

	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

	virtual EYSUpgradeRarity RollRarity() const override;

	virtual float GetBonusValue(EYSUpgradeRarity Rarity) const override;


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

	FLinearColor GetRarityColor(EYSUpgradeRarity Rarity) const;
};

/**
 *  Upgrade de stat par main : deux bonus distincts (variante 0 = main droite, 1 = main gauche), cumulés séparément.
 *  Une main n'est proposée que si elle tient une arme.
 */
UCLASS(Abstract)
class UYSHandStatUpgrade : public UYSStatUpgrade
{
	GENERATED_BODY()

public:

	/** Nom du bonus de chaque main */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FText RightHandName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FText LeftHandName;

	static EYSHand GetHand(int32 Variant) { return Variant == 1 ? EYSHand::Left : EYSHand::Right; }

	virtual int32 GetVariantCount() const override { return 2; }

	virtual FText GetVariantName(int32 Variant) const override { return GetHand(Variant) == EYSHand::Left ? LeftHandName : RightHandName; }

	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

protected:

	/** Arme tenue par la main de la variante */
	static AYSWeapon* GetHandWeapon(const AYSPlayerCharacter* Character, int32 Variant);

	/** Action de l'arme de la main qui sert de référence : celle du clic, sinon l'attaque auto */
	static UYSAbilityComponent* GetHandAction(const AYSPlayerCharacter* Character, int32 Variant);
};

/** Atk Bonus (par main) : dégâts de l'arme de la main x (1 + somme des bonus de cette main) */
UCLASS(meta=(DisplayName="Upgrade de stat : Atk Bonus (par main)"))
class UYSHandAtkBonusUpgrade : public UYSHandStatUpgrade
{
	GENERATED_BODY()

public:

	UYSHandAtkBonusUpgrade();

protected:

	/** Dégâts de base de l'action de la main */
	virtual float GetBaseStatValue(const AYSPlayerCharacter* Character, int32 Variant) const override;

	virtual void ApplyBonus(AYSPlayerCharacter* Character, int32 Variant, float TotalBonus) const override;
};

/**
 *  CD Reduction (par main) : temps de recharge de l'arme de la main x (1 - somme des bonus de cette main),
 *  jamais sous MinCooldownMultiplier. Coup d'Petoir : Cooldown ; Coup d'sabre : Auto Atk Cooldown (pas la durée de l'anim).
 */
UCLASS(meta=(DisplayName="Upgrade de stat : CD Reduction (par main)"))
class UYSHandCooldownReductionUpgrade : public UYSHandStatUpgrade
{
	GENERATED_BODY()

public:

	UYSHandCooldownReductionUpgrade();

	/** Multiplicateur minimum de la recharge (garde-fou) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade", meta=(ClampMin="0", ClampMax="1"))
	float MinCooldownMultiplier = 0.1f;

protected:

	/** Recharge de base de l'action de la main */
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
