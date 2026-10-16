#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
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

/** Une proposition du menu de montée de niveau */
USTRUCT(BlueprintType)
struct FYSUpgradeOffer
{
	GENERATED_BODY()

	/** Index dans l'UpgradePool du personnage */
	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	int32 PoolIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	EYSUpgradeRarity Rarity = EYSUpgradeRarity::None;

	bool operator==(const FYSUpgradeOffer& Other) const { return PoolIndex == Other.PoolIndex && Rarity == Other.Rarity; }
};

/** Progression d'une upgrade du pool sur un personnage (répliquée) */
USTRUCT(BlueprintType)
struct FYSUpgradeProgress
{
	GENERATED_BODY()

	/** Nombre de bonus déjà obtenus */
	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	int32 Stacks = 0;

	/** Somme des valeurs des bonus obtenus */
	UPROPERTY(BlueprintReadOnly, Category="Upgrades")
	float TotalValue = 0.0f;
};

/**
 *  Amélioration proposée au joueur à chaque montée de niveau de l'équipe.
 *  Les upgrades disponibles sont listées sur le personnage (3C > Upgrades > Upgrade Pool), instanciées
 *  et réglées dans son Blueprint (EditInlineNew : choisir le type dans la liste, régler ses paramètres).
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UYSUpgrade : public UObject
{
	GENERATED_BODY()

public:

	/** Nom affiché sur le bouton */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FText DisplayName;

	/** Catégorie (fixée par la classe) */
	UFUNCTION(BlueprintPure, Category="Upgrade")
	EYSUpgradeType GetUpgradeType() const { return UpgradeType; }

	/** Peut encore être proposée à ce personnage (Progress = ce qu'il a déjà obtenu de cette upgrade) */
	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, const FYSUpgradeProgress& Progress) const { return true; }

	/** Rareté tirée pour une nouvelle proposition (None si l'upgrade n'en a pas) */
	virtual EYSUpgradeRarity RollRarity() const { return EYSUpgradeRarity::None; }

	/** Valeur ajoutée à Progress.TotalValue par un bonus de cette rareté */
	virtual float GetBonusValue(EYSUpgradeRarity Rarity) const { return 0.0f; }

	/** Lignes du bouton pour une proposition */
	virtual void GetOfferLines(const AYSPlayerCharacter* Character, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, TArray<FYSChoiceLine>& OutLines) const;

	/** Applique l'état de l'upgrade au personnage après un nouveau bonus (serveur ; Progress déjà mis à jour) */
	virtual void Apply(AYSPlayerCharacter* Character, const FYSUpgradeProgress& Progress) const {}

protected:

	UPROPERTY(VisibleDefaultsOnly, Category="Upgrade")
	EYSUpgradeType UpgradeType = EYSUpgradeType::Stat;
};

/**
 *  Upgrade de stat : modifie une statistique pure du joueur. Cumule jusqu'à MaxStacks bonus
 *  (ensuite elle n'est plus proposée) ; chaque bonus a une rareté tirée au hasard qui fixe sa valeur.
 *  Nouvelle stat = sous-classe qui surcharge Apply (et GetStatLabel / GetStatValue pour l'affichage).
 */
UCLASS(Abstract)
class UYSStatUpgrade : public UYSUpgrade
{
	GENERATED_BODY()

public:

	UYSStatUpgrade();

	/** Nombre maximum de bonus cumulables */
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

	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, const FYSUpgradeProgress& Progress) const override;

	virtual EYSUpgradeRarity RollRarity() const override;

	virtual float GetBonusValue(EYSUpgradeRarity Rarity) const override;

	virtual void GetOfferLines(const AYSPlayerCharacter* Character, const FYSUpgradeProgress& Progress, EYSUpgradeRarity Rarity, TArray<FYSChoiceLine>& OutLines) const override;

protected:

	/** Nom de la stat affichée (« Dégât ») */
	virtual FText GetStatLabel() const { return FText::GetEmpty(); }

	/** Valeur affichée de la stat pour une somme de bonus donnée (ex. multiplicateur 1 + somme) */
	virtual float GetStatValue(float TotalBonus) const { return TotalBonus; }

	FLinearColor GetRarityColor(EYSUpgradeRarity Rarity) const;
};

UENUM(BlueprintType)
enum class EYSHand : uint8
{
	Right,
	Left,
};

/**
 *  Atk Bonus : multiplie les dégâts de l'arme d'une main par (1 + somme des bonus).
 *  Proposée seulement si la main tient une arme.
 */
UCLASS(meta=(DisplayName="Stat : Atk Bonus (main)"))
class UYSHandAtkBonusUpgrade : public UYSStatUpgrade
{
	GENERATED_BODY()

public:

	UYSHandAtkBonusUpgrade();

	/** Main concernée */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	EYSHand Hand = EYSHand::Right;

	virtual bool CanBeOffered(const AYSPlayerCharacter* Character, const FYSUpgradeProgress& Progress) const override;

	virtual void Apply(AYSPlayerCharacter* Character, const FYSUpgradeProgress& Progress) const override;

protected:

	virtual FText GetStatLabel() const override;

	virtual float GetStatValue(float TotalBonus) const override { return 1.0f + TotalBonus; }
};
