#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "YSWeaponSet.generated.h"

class AYSWeapon;

/**
 *  Set d'arme : le duo d'armes prédéfini d'un personnage (ex. Sabre et Pistolet, Akimbo Battle Petoir, Akimbo Sabre).
 *  Un Data Asset par set dans /Game/Core/WeaponSets, référencé par les personnages (3C > Weapons > Weapon Set).
 *  Les armes restent des Blueprints individuels (/Game/Placeable/Weapons) ; le set dit juste qui tient quoi.
 *  Exprimé pour un droitier (main principale = droite) : un gaucher en est le miroir.
 *  Les upgrades ne dépendent plus des mains : Ability et Auto atk s'appliquent à tout le set.
 */
UCLASS(BlueprintType)
class UYSWeaponSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Nom affiché */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon Set")
	FText DisplayName;

	/** Arme de la main principale (droite) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon Set")
	TSubclassOf<AYSWeapon> RightHandWeapon;

	/** Arme de l'autre main (gauche) ; la même que la droite = akimbo */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon Set")
	TSubclassOf<AYSWeapon> LeftHandWeapon;
};
