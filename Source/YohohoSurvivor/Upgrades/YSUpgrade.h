#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "YSUpgrade.generated.h"

class AYSPlayerCharacter;

/**
 *  Amélioration proposée au joueur à chaque montée de niveau de l'équipe.
 *  Les upgrades disponibles sont listées sur le personnage (3C > Upgrades > Upgrade Pool), instanciées
 *  et réglées dans son Blueprint (EditInlineNew : choisir le type dans la liste, régler ses paramètres).
 *  Nouveau type d'upgrade = sous-classe qui surcharge Apply.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UYSUpgrade : public UObject
{
	GENERATED_BODY()

public:

	/** Nom affiché sur le bouton */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FText DisplayName;

	/** Description affichée sous le nom */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FText Description;

	/** Applique l'upgrade au personnage (serveur uniquement) */
	virtual void Apply(AYSPlayerCharacter* Character) const {}
};

/** Placeholder : augmente les dégâts de toutes les actions des armes du personnage */
UCLASS(meta=(DisplayName="Dégâts bonus"))
class UYSDamageUpgrade : public UYSUpgrade
{
	GENERATED_BODY()

public:

	/** Dégâts ajoutés à chaque action (Coup d'Petoir, Coup d'sabre…) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	float BonusDamage = 10.0f;

	UYSDamageUpgrade();

	virtual void Apply(AYSPlayerCharacter* Character) const override;
};
