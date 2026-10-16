#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UI/YSCharacterSelectWidget.h"
#include "YSUpgrade.generated.h"

class AYSPlayerCharacter;

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
 *  Upgrade de stat : modifie une statistique pure du joueur. Chaque variante cumule jusqu'à MaxStacks bonus
 *  (ensuite elle n'est plus proposée) ; chaque bonus a une rareté tirée au hasard qui fixe sa valeur.
 *  Nouvelle stat = sous-classe qui surcharge Apply (et GetStatLabel / GetStatValue pour l'affichage).
 */
UCLASS(Abstract)
class UYSStatUpgrade : public UYSUpgrade
{
	GENERATED_BODY()

public:

	UYSStatUpgrade();

	/** Nombre maximum de bonus cumulables (par variante) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade", meta=(ClampMin="1"))
	int32 MaxStacks = 10;

	/** Valeur d'un bonus selon sa rareté */
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

protected:

	/** Nom de la stat affichée (« Dégât ») */
	virtual FText GetStatLabel() const { return FText::GetEmpty(); }

	/** Valeur affichée de la stat pour une somme de bonus donnée (ex. multiplicateur 1 + somme) */
	virtual float GetStatValue(float TotalBonus) const { return TotalBonus; }

	FLinearColor GetRarityColor(EYSUpgradeRarity Rarity) const;
};

/**
 *  Atk Bonus : deux bonus distincts, « Right hand Atk bonus » (variante 0) et « Left hand Atk bonus » (variante 1),
 *  cumulés séparément. Multiplie les dégâts de l'arme de la main par (1 + somme des bonus de cette main).
 *  Une main n'est proposée que si elle tient une arme.
 */
UCLASS(meta=(DisplayName="Upgrade de stat : Atk Bonus (par main)"))
class UYSHandAtkBonusUpgrade : public UYSStatUpgrade
{
	GENERATED_BODY()

public:

	UYSHandAtkBonusUpgrade();

	/** Nom du bonus de chaque main */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FText RightHandName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FText LeftHandName;

	/** Nom de la stat sur le bouton (« Dégât x 1.9 ») */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FText StatLabel;

	static EYSHand GetHand(int32 Variant) { return Variant == 1 ? EYSHand::Left : EYSHand::Right; }

	virtual int32 GetVariantCount() const override { return 2; }

	virtual FText GetVariantName(int32 Variant) const override { return GetHand(Variant) == EYSHand::Left ? LeftHandName : RightHandName; }

	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

	virtual void Apply(AYSPlayerCharacter* Character, int32 Variant, const FYSUpgradeProgress& Progress) const override;

protected:

	virtual FText GetStatLabel() const override { return StatLabel; }

	virtual float GetStatValue(float TotalBonus) const override { return 1.0f + TotalBonus; }
};
