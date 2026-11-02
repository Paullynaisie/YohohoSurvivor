#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "YSPassive.generated.h"

/**
 *  Passif d'un personnage (3C > Passive) : un Data Asset par passif dans /Game/Core/Passives, jamais amélioré, seulement
 *  remplacé par un autre (AYSPlayerCharacter::SetPassive, serveur, répliqué). Ses effets sont lus là où ils s'appliquent
 *  (stamina, port d'un objet, dégâts critiques, dégâts subis), sur toutes les machines.
 *  Classe de base = passif sans effet (ex. « Toi ta gueule » du Captain, qui servira plus tard).
 */
UCLASS(BlueprintType, meta=(DisplayName="Passif sans effet"))
class UYSPassive : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Nom affiché (écran de sélection) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Passive")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Passive", meta=(MultiLine="true"))
	FText Description;

	/** Stamina consommée en portant un objet ou en nageant x ce multiplicateur */
	virtual float GetPassiveStaminaCostMultiplier() const { return 1.0f; }

	/** Le personnage garde ses armes en portant un objet ; l'objet est porté par l'autre main, placé par GetCarryPlacement */
	virtual bool CanAttackWhileCarrying() const { return false; }

	/** Can Attack While Carrying : position et rotation de l'objet porté (relatives au corps), pour un droitier */
	virtual void GetCarryPlacement(FVector& OutLocation, FRotator& OutRotation) const {}

	/** Multiplicateur de base des coups critiques à la place de celui du personnage (0 = celui du personnage) */
	virtual float GetCriticalDamageMultiplierOverride() const { return 0.0f; }

	/** %Armor : part des dégâts subis ignorée, appliquée avant l'armure (0.2 = 20 %) */
	virtual float GetArmorPercent() const { return 0.0f; }
};

/** « Larbin » (Mouss) : consomme moins de stamina en portant un objet ou en nageant (le ralentissement arrive donc plus tard) */
UCLASS(meta=(DisplayName="Passif : stamina des actions passives"))
class UYSPassiveStaminaPassive : public UYSPassive
{
	GENERATED_BODY()

public:

	/** Stamina consommée en portant un objet ou en nageant x ce multiplicateur (0.5 = 2x moins) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Passive", meta=(ClampMin="0", ClampMax="1"))
	float PassiveStaminaCostMultiplier = 0.5f;

	virtual float GetPassiveStaminaCostMultiplier() const override { return PassiveStaminaCostMultiplier; }
};

/** « Tough Guy » (Quartermaster) : porte un objet sur son flanc avec l'autre main (elle quitte l'arme à 2 mains) et continue d'attaquer */
UCLASS(meta=(DisplayName="Passif : porter et attaquer"))
class UYSCarryAttackPassive : public UYSPassive
{
	GENERATED_BODY()

public:

	/** Position de l'objet porté (son origine = son bas), relative au corps ; pour un droitier (flanc gauche), en miroir pour un gaucher */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Passive")
	FVector CarryFlankLocation = FVector(0.0f, -100.0f, -30.0f);

	/** Rotation de l'objet porté, relative au corps ; pour un droitier */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Passive")
	FRotator CarryFlankRotation = FRotator::ZeroRotator;

	virtual bool CanAttackWhileCarrying() const override { return true; }

	virtual void GetCarryPlacement(FVector& OutLocation, FRotator& OutRotation) const override
	{
		OutLocation = CarryFlankLocation;
		OutRotation = CarryFlankRotation;
	}
};

/** « Marksman » (Cannoneer) : les critiques font Critical Damage Multiplier (200 %) au lieu de 150 % */
UCLASS(meta=(DisplayName="Passif : dégâts critiques de base"))
class UYSCriticalDamagePassive : public UYSPassive
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Passive", meta=(ClampMin="1"))
	float CriticalDamageMultiplier = 2.0f;

	virtual float GetCriticalDamageMultiplierOverride() const override { return CriticalDamageMultiplier; }
};

/** Ignore Armor Percent (20 %) des dégâts subis, avant l'armure (DA_Passive_ToughGuy : ancien passif du Quartermaster, plus porté par personne) */
UCLASS(meta=(DisplayName="Passif : %Armor"))
class UYSArmorPercentPassive : public UYSPassive
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Passive", meta=(ClampMin="0", ClampMax="1"))
	float ArmorPercent = 0.2f;

	virtual float GetArmorPercent() const override { return ArmorPercent; }
};
