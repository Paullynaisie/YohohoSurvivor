#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "YSPassive.generated.h"

/**
 *  Passif d'un personnage (3C > Passive) : un Data Asset par passif dans /Game/Core/Passives, jamais amélioré, seulement
 *  remplacé par un autre (AYSPlayerCharacter::SetPassive, serveur, répliqué). Ses effets sont lus là où ils s'appliquent
 *  (vitesse de port, dégâts critiques, dégâts subis), sur toutes les machines.
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

	/** Pas de ralentissement quand le personnage porte un objet (coffre, carte au trésor…) */
	virtual bool IgnoresCarrySlowdown() const { return false; }

	/** Multiplicateur de base des coups critiques à la place de celui du personnage (0 = celui du personnage) */
	virtual float GetCriticalDamageMultiplierOverride() const { return 0.0f; }

	/** %Armor : part des dégâts subis ignorée, appliquée avant l'armure (0.2 = 20 %) */
	virtual float GetArmorPercent() const { return 0.0f; }

	/** Vitesse de nage à la place de Swim Speed Multiplier du personnage (0 = celle du personnage) */
	virtual float GetSwimSpeedMultiplierOverride() const { return 0.0f; }
};

/** « Larbin » (Mouss) : aucune perte de vitesse en portant un objet, et nage un peu moins ralentie (50 % au lieu de 30 %) */
UCLASS(meta=(DisplayName="Passif : pas de ralentissement en portant un objet"))
class UYSCarrySpeedPassive : public UYSPassive
{
	GENERATED_BODY()

public:

	/** Vitesse de nage (x MoveSpeed) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Passive", meta=(ClampMin="0", ClampMax="1"))
	float SwimSpeedMultiplier = 0.5f;

	virtual bool IgnoresCarrySlowdown() const override { return true; }

	virtual float GetSwimSpeedMultiplierOverride() const override { return SwimSpeedMultiplier; }
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

/** « Tough Guy » (Quartermaster) : ignore Armor Percent (20 %) des dégâts subis, avant l'armure */
UCLASS(meta=(DisplayName="Passif : %Armor"))
class UYSArmorPercentPassive : public UYSPassive
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Passive", meta=(ClampMin="0", ClampMax="1"))
	float ArmorPercent = 0.2f;

	virtual float GetArmorPercent() const override { return ArmorPercent; }
};
