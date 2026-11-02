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
 *  Le set peut aussi fixer les PV max de base et le pushback du personnage (Health, Pushback ; sinon ceux du personnage).
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

	/** Taille du personnage qui a ce set : multiplie son Character Scale (ex. Quartermaster : GreatAxe x1.25, Anchor Bearer x1.5) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon Set", meta=(ClampMin="0.1"))
	float CharacterScaleMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health", meta=(InlineEditConditionToggle))
	bool bOverrideMaxHealth = false;

	/** PV max de base du personnage qui a ce set (avant Hps bonus) ; décoché = Max Health du personnage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health", meta=(ClampMin="1", EditCondition="bOverrideMaxHealth"))
	float MaxHealth = 200.0f;

	/** Le pushback des NME touchés est réglé par ce set (sinon par le personnage, 3C > Pushback) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pushback")
	bool bOverridePushback = false;

	/**
	 *  Pushback des NME touchés, proportionnel à la part de leur vie retirée par le coup (dégâts / PV max de la cible) :
	 *  sous Min Damage Ratio rien, à partir de Max Damage Ratio la force max, entre les deux de Min Force à Max Force
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pushback", meta=(ClampMin="0", ClampMax="1", EditCondition="bOverridePushback"))
	float PushbackMinDamageRatio = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pushback", meta=(ClampMin="0", ClampMax="1", EditCondition="bOverridePushback"))
	float PushbackMaxDamageRatio = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pushback", meta=(ClampMin="0", Units="CentimetersPerSecond", EditCondition="bOverridePushback"))
	float PushbackMinForce = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pushback", meta=(ClampMin="0", Units="CentimetersPerSecond", EditCondition="bOverridePushback"))
	float PushbackMaxForce = 500.0f;

	/** Vitesse verticale donnée en plus à chaque pushback (le NME décolle un peu du sol : moins de friction) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pushback", meta=(ClampMin="0", Units="CentimetersPerSecond", EditCondition="bOverridePushback"))
	float PushbackVerticalForce = 50.0f;

	// ---- Showcase : mise en scène du personnage dans les aperçus (écran de sélection, upgrades de classe) ----
	// Réglée pour un droitier, un gaucher en est le miroir. Rotations : X / Y / Z = Roll / Pitch / Yaw.

	/** Voir la mise en scène sur le personnage dans l'éditeur (Blueprint, map), mise à jour à chaque changement ; sans effet en jeu */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Showcase")
	bool bForceShowcaseParameter = false;

	/** Position de la main droite dans le repère du corps (comme Hand Location) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Showcase", meta=(Units="cm"))
	FVector ShowcaseRightHandLocation = FVector(0.0f, 55.0f, -10.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Showcase")
	FRotator ShowcaseRightHandRotation = FRotator::ZeroRotator;

	/** Position de la main gauche dans le repère du corps (pas de symétrie automatique avec la droite) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Showcase", meta=(Units="cm"))
	FVector ShowcaseLeftHandLocation = FVector(0.0f, -55.0f, -10.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Showcase")
	FRotator ShowcaseLeftHandRotation = FRotator::ZeroRotator;

	/** Arme à deux mains : l'autre main reste sur l'Off Hand Socket de l'arme (sa position et sa rotation de showcase sont ignorées) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Showcase")
	bool bShowcaseOffHandOnSocket = true;

	/** Position du corps (Body Pivot) dans le repère du personnage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Showcase", meta=(Units="cm"))
	FVector ShowcaseBodyLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Showcase")
	FRotator ShowcaseBodyRotation = FRotator::ZeroRotator;

	/** Position de la caméra de l'aperçu dans le repère du personnage (il regarde vers +X ; suit sa taille) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Showcase", meta=(Units="cm"))
	FVector ShowcaseCameraLocation = FVector(450.0f, 0.0f, 40.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Showcase")
	FRotator ShowcaseCameraRotation = FRotator(-5.08f, 180.0f, 0.0f);

#if WITH_EDITOR
	/** Force Showcase Parameter : les personnages de l'éditeur qui ont ce set suivent les réglages en direct */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
