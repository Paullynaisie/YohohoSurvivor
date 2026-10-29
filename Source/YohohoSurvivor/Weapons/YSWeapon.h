#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YSWeapon.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UYSAbilityComponent;

/** Couleur d'une arme de joueur selon la main qui la tient et la classe du set */
UENUM(BlueprintType)
enum class EYSWeaponColoration : uint8
{
	/** Matériaux du Blueprint (armes de NME) */
	None,
	MainHand,
	OffHand,
	/** Set obtenu par une upgrade de classe */
	ClassUpgrade
};

/**
 *  Arme tenue dans une main du joueur : un mesh, une bouche (Muzzle) et une action.
 *  L'action est un composant de capacité (UYSAbilityComponent, ex. Coup d'Petoir) ajouté dans le
 *  Blueprint de l'arme ; ses réglages (dégâts, recharge, projectile…) se font dans ce Blueprint.
 *  Créée par les ChildActorComponent des mains du personnage : spawnée par le serveur, répliquée,
 *  avec le personnage comme propriétaire (nécessaire aux RPC de l'action).
 *  Classe de base du Blueprint BP_Arme.
 */
UCLASS(meta=(PrioritizeCategories="Weapon"))
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

	/** Arme à deux mains : position et rotation de l'AUTRE main du porteur (à placer sur la poignée dans le Blueprint) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> OffHandSocket;

protected:

	/** Nom affiché */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
	FText DisplayName;

	/**
	 *  Arme à deux mains : tenue par une main (la pose et les attaques, ex. swing, ne concernent que cette main), l'autre main
	 *  se place TOUJOURS sur le socket OffHandSocket de l'arme, sans retard, même pendant les attaques. L'autre main ne
	 *  doit pas avoir d'arme (set : une seule main). NME et joueurs (ex. Shotgun du Cannoneer).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
	bool bTwoHanded = false;

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

	/** Composant du Blueprint dont tous les sous-composants (meshes) prennent la couleur de l'arme (nom de variable, espaces ignorés) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Coloration")
	FName ColorationComponentName = TEXT("Coloration");

	/** Arme dans la main principale du joueur (droite, gauche pour un gaucher) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Coloration")
	TSoftObjectPtr<UMaterialInterface> MainHandMaterial;

	/** Arme dans l'autre main */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Coloration")
	TSoftObjectPtr<UMaterialInterface> OffHandMaterial;

	/** Arme d'un set obtenu par une upgrade de classe (les deux mains) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Coloration")
	TSoftObjectPtr<UMaterialInterface> ClassUpgradeMaterial;

	/**
	 *  Perf (armes des NME) : toutes les formes du Blueprint fusionnées en un seul mesh (Tools/EditorScripts/merge_foe_weapons.py,
	 *  à relancer après avoir retouché les formes). En jeu, ce mesh remplace les formes : une primitive à déplacer et à dessiner
	 *  au lieu d'une dizaine. Vide = formes du Blueprint. Appliqué par OptimizeForCrowd (NME seulement).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Optimization")
	TObjectPtr<UStaticMesh> MergedMesh;

public:

	/**
	 *  Perf, arme d'un NME (local, sur chaque machine) : Merged Mesh à la place des formes, aucune primitive avec collision ni
	 *  overlaps (la Hitbox de l'Auto swing ne sert que de forme pour sa requête), ombres selon bCastShadow. Une seule fois.
	 */
	void OptimizeForCrowd(bool bCastShadow);

	/** Colore les sous-composants du composant Coloration (rien si déjà appliquée ou None) ; local, sur chaque machine */
	void SetColoration(EYSWeaponColoration NewColoration);

	/**
	 *  Recul actuel de l'arme (réglages Recoil de ses actions de tir : Coup d'Petoir, Auto shot), d'après la plus récente
	 *  de ces actions : cabrage (degrés) et recul le long de l'axe de l'arme (cm). Faux si l'arme est au repos.
	 */
	bool GetRecoil(float& OutPitch, float& OutKickBack) const;

	/** Rotation actuelle de l'animation de rechargement (Reload Anim des actions de tir ; nulle au repos), à ajouter à la main */
	FRotator GetReloadSpin() const;

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

	/** Nom affiché (DisplayName, sinon le nom de la classe sans BP_ ni _C) : utilisé dans les noms d'upgrades */
	FText GetWeaponName() const;

	/** Attaque active (déclenchée par le joueur, ex. Coup d'Petoir), ou nullptr */
	UFUNCTION(BlueprintPure, Category="Weapon")
	UYSAbilityComponent* GetActiveAction() const;

	/** Attaque automatique (déclenchée par le serveur, ex. Auto swing), ou nullptr */
	UFUNCTION(BlueprintPure, Category="Weapon")
	UYSAbilityComponent* GetAutoAction() const;

	/** Arme de mêlée = son attaque auto est Melee (ex. épées, GreatAxe ; concernée par la shard Melee Range) */
	bool IsMeleeWeapon() const;

	/** Pawn qui tient l'arme */
	UFUNCTION(BlueprintPure, Category="Weapon")
	APawn* GetHolder() const;

	FVector GetMuzzleLocation() const;

	bool IsTwoHanded() const { return bTwoHanded; }

	/** Socket de l'autre main (armes à deux mains) */
	USceneComponent* GetOffHandSocket() const { return OffHandSocket.Get(); }

	UStaticMeshComponent* GetMesh() const { return Mesh.Get(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 *  Bout des chaînes de l'arme (serveur, répliqué ; upgrade Yoloooooo++ de la MorningStar), pour les chaînes à composants au bout
	 *  (BallChain) : taille de ces composants (Ball) et de leurs box (Hitbox) x (1 + SizeBonus), longueur des segments x (1 + LengthBonus)
	 */
	void SetChainSizeBonus(float SizeBonus, float LengthBonus);

private:

	EYSWeaponColoration AppliedColoration = EYSWeaponColoration::None;

	bool bCrowdOptimized = false;

	UPROPERTY(ReplicatedUsing=OnRep_ChainSizeBonus)
	float ChainSizeBonus = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_ChainSizeBonus)
	float ChainLengthBonus = 0.0f;

	UFUNCTION()
	void OnRep_ChainSizeBonus();
};
