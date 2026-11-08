#pragma once

#include "CoreMinimal.h"
#include "YSShipRewardTypes.generated.h"

/**
 *  Sélection d'upgrade en cours : un level up (XP), ou une récompense de la jauge de doublons (niveau du bateau, AYSGameMode::DoubloonRewardCycle)
 */
UENUM(BlueprintType)
enum class EYSUpgradeSelectionKind : uint8
{
	/** Level up de l'équipe (XP) : upgrades de stats et Bonus (++) des upgrades d'arme déjà débloquées */
	LevelUp,
	/** Upgrade de classe (changement de set d'arme) */
	Class,
	/** Déblocage d'une upgrade d'arme */
	Weapon,
	/** Super (Golden) d'une upgrade d'arme débloquée */
	GoldenWeapon,
	/** 3 propositions en rareté mythique parmi les upgrades possibles */
	Mythic
};
