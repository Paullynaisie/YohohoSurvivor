
#pragma once

#include "CoreMinimal.h"
#include "YSShipRewardTypes.generated.h"

/**
 *  Sélection d'upgrade en cours : un level up (XP), ou une SilverCoin upgrade = récompense de la jauge de doublons
 *  (niveau du bateau, AYSGameMode::DoubloonRewardCycle). Les SilverCoin upgrades proposent Silver Coin Choice Count (2)
 *  propositions, sans relance.
 */
UENUM(BlueprintType)
enum class EYSUpgradeSelectionKind : uint8
{
	/** Level up de l'équipe (XP) : upgrades de stats et Bonus (++) des upgrades d'arme et de MoveAction déjà débloquées */
	LevelUp,
	/** Upgrade de classe (changement de set d'arme) */
	Class,
	/** Déblocage d'une upgrade d'arme */
	Weapon,
	/** Super (Golden) d'une upgrade d'arme débloquée */
	GoldenWeapon,
	/** Propositions en rareté mythique parmi les upgrades possibles */
	Mythic,
	/** Déblocage d'une MoveAction (Roll, Run, Shoulder bump, Booster de moral, Camouflage) */
	MoveAction,
	/** Golden de la MoveAction débloquée (une des variantes Golden) */
	GoldenMoveAction,
};
