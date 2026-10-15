#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YSWeapon.generated.h"

class UStaticMeshComponent;
class UYSAbilityComponent;

/**
 *  Arme tenue dans une main du joueur : un mesh, une bouche (Muzzle) et une action.
 *  L'action est un composant de capacité (UYSAbilityComponent, ex. Coup d'Petoir) ajouté dans le
 *  Blueprint de l'arme ; ses réglages (dégâts, recharge, projectile…) se font dans ce Blueprint.
 *  Créée par les ChildActorComponent des mains du personnage : spawnée par le serveur, répliquée,
 *  avec le personnage comme propriétaire (nécessaire aux RPC de l'action).
 *  Classe de base du Blueprint BP_Arme.
 */
UCLASS(meta=(PrioritizeCategories="Weapon Akimbo Recoil"))
class AYSWeapon : public AActor
{
	GENERATED_BODY()

private:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Root;

	/** Visuel de l'arme */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Point de départ des projectiles / origine des attaques */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Muzzle;

protected:

	/** Nom affiché */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(InlineEditConditionToggle))
	bool bOverrideHandLocation = false;

	/**
	 *  Position de la main qui tient cette arme, par rapport au corps du personnage (remplace 3C > Hands > Hand Location).
	 *  Exprimée pour la main droite ; symétrisée automatiquement (Y inversé) quand l'arme est dans la main gauche.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(EditCondition="bOverrideHandLocation"))
	FVector HandLocation = FVector(0.0f, 55.0f, -10.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(InlineEditConditionToggle))
	bool bOverrideHandRotation = false;

	/**
	 *  Rotation de la main qui tient cette arme, par rapport au corps (remplace 3C > Hands > Hand Rotation).
	 *  Exprimée pour la main droite ; symétrisée automatiquement (Yaw et Roll inversés) dans la main gauche.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(EditCondition="bOverrideHandRotation"))
	FRotator HandRotation = FRotator::ZeroRotator;

	/** Akimbo (la même arme dans les deux mains) : délai entre l'action de la main gauche et celle de la main droite */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Akimbo", meta=(ClampMin="0", Units="Seconds"))
	float AkimboFireDelay = 0.5f;

	/** Recul à chaque activation de l'action : la main se cabre (muzzle vers le haut) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recoil", meta=(Units="Degrees"))
	float RecoilPitch = 25.0f;

	/** Recul à chaque activation : la main recule le long de l'axe de l'arme */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="0", Units="Centimeters"))
	float RecoilKickBack = 10.0f;

	/** Durée du coup (montée rapide, courbe EaseOut) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="0.001", Units="Seconds"))
	float RecoilKickDuration = 0.05f;

	/** Durée du retour en place (courbe EaseInOut) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="0.001", Units="Seconds"))
	float RecoilRecoverDuration = 0.25f;

public:

	/** Intensité actuelle du recul (0 = au repos, 1 = pic), d'après la dernière activation de l'action */
	UFUNCTION(BlueprintPure, Category="Recoil")
	float GetRecoilAlpha() const;

	float GetRecoilPitch() const { return RecoilPitch; }

	float GetRecoilKickBack() const { return RecoilKickBack; }

	float GetAkimboFireDelay() const { return AkimboFireDelay; }

	/** Rotation de main imposée par l'arme (si bOverrideHandRotation) */
	bool GetHandRotationOverride(FRotator& OutRotation) const
	{
		OutRotation = HandRotation;
		return bOverrideHandRotation;
	}

	/** Position de main imposée par l'arme (si bOverrideHandLocation) */
	bool GetHandLocationOverride(FVector& OutLocation) const
	{
		OutLocation = HandLocation;
		return bOverrideHandLocation;
	}

	AYSWeapon();

	/** Action de l'arme (premier composant de capacité de l'acteur), ou nullptr */
	UFUNCTION(BlueprintPure, Category="Weapon")
	UYSAbilityComponent* GetAction() const;

	/** Pawn qui tient l'arme */
	UFUNCTION(BlueprintPure, Category="Weapon")
	APawn* GetHolder() const;

	FVector GetMuzzleLocation() const;

	UStaticMeshComponent* GetMesh() const { return Mesh.Get(); }
};
