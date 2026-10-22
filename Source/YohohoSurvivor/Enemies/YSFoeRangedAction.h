#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "YSFoeRangedAction.generated.h"

class AYSFoeBomb;
class AYSFoeCharacter;
class AYSProjectile;
class UMaterialInterface;

/**
 *  Attaque à distance d'une ARME DE NME (composant ajouté dans le BP de l'arme : BP_NME_Bomb, BP_NME_Pistol).
 *  C'est l'arme qui définit l'attaque : un NME qui tient une arme avec une action à distance fonce vers sa cible jusqu'à
 *  AttackRange, s'y arrête (pas de dégâts de contact), recharge son attaque en continu (AttackInterval, même hors de portée)
 *  et la lance dès que la cible est à portée. Logique pilotée par le NME sur le serveur (AYSFoeCharacter) ; l'arme de NME
 *  n'étant pas répliquée, chaque machine a la sienne (réglages lus localement pour les effets).
 *  Dégâts = (BaseDamage + DamagePerLevel x niveau du NME) x Damage Multiplier du NME (élites). Pas d'upgrade pour les NME.
 */
UCLASS(Abstract, ClassGroup=(YS), meta=(PrioritizeCategories="NME Attack"))
class UYSFoeRangedAction : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Le NME s'arrête à cette distance de sa cible (au-delà, il fonce vers elle) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack", meta=(ClampMin="0", Units="Centimeters"))
	float AttackRange = 1000.0f;

	/** Recharge entre deux attaques (en continu, même hors de portée) ; rechargée, l'attaque part dès que la cible est à portée */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack", meta=(ClampMin="0.1", Units="Seconds"))
	float AttackInterval = 5.0f;

	/** Dégâts au niveau 0 (x Damage Multiplier du NME) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack", meta=(ClampMin="0"))
	float BaseDamage = 20.0f;

	/** Dégâts ajoutés par niveau du NME */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack", meta=(ClampMin="0"))
	float DamagePerLevel = 0.0f;

	/** Prévisualisation au sol (disque d'impact, ligne de visée) : matériau M_BrookerZone (Color, Glow, FillOpacity, EdgeOpacity, Radius, EdgeWidth) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Preview")
	TObjectPtr<UMaterialInterface> PreviewMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Preview")
	FLinearColor PreviewColor = FLinearColor(1.0f, 0.1f, 0.05f);

	UYSFoeRangedAction();

	/** Dégâts de l'attaque pour ce NME (niveau, multiplicateur) */
	float GetDamage(const AYSFoeCharacter* Foe) const;

	/** Lance l'attaque vers la cible (serveur) ; vrai si elle part. Origin = Muzzle de l'arme */
	virtual bool StartAttack(AYSFoeCharacter* Foe, const APawn* Target, const FVector& Origin) { return false; }
};

/**
 *  « NME bomb » : bombe lancée en cloche vers le sol sous la cible (AYSFoeBomb : vol, mèche, prévisualisation au sol, explosion).
 *  Action de BP_NME_Bomb (Skull Bomb).
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="NME bomb throw"))
class UYSFoeBombAction : public UYSFoeRangedAction
{
	GENERATED_BODY()

public:

	/** Sommet de la cloche : hauteur au-dessus du plus haut entre le départ et l'impact */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Bomb", meta=(ClampMin="0", Units="Centimeters"))
	float BombApexHeight = 300.0f;

	/** Durée du vol (la cloche est calculée pour atterrir pile à ce moment) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Bomb", meta=(ClampMin="0.05", Units="Seconds"))
	float BombFlightDuration = 1.5f;

	/** Posée au sol, la bombe attend ce temps avant d'exploser, en passant de BombColor à FuseColor lumineux */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Bomb", meta=(ClampMin="0", Units="Seconds"))
	float BombFuseDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Bomb")
	FLinearColor BombColor = FLinearColor(0.02f, 0.02f, 0.02f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Bomb")
	FLinearColor FuseColor = FLinearColor(1.0f, 0.05f, 0.02f);

	/** Intensité de la lueur (émissif) à la fin de la mèche */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Bomb", meta=(ClampMin="0"))
	float FuseGlow = 20.0f;

	/** Matériau de la bombe lancée (M_FoeBomb : paramètres Color et Emissive) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Bomb")
	TObjectPtr<UMaterialInterface> BombMaterial;

	/** Rayon de l'explosion (joueurs dont le corps touche la sphère) et du disque de prévisualisation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Bomb", meta=(ClampMin="1", Units="Centimeters"))
	float ExplosionRadius = 500.0f;

	/** La sphère de l'explosion grossit de 0 au rayon en ce temps, puis s'efface en ExplosionFadeDuration */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Bomb", meta=(ClampMin="0", Units="Seconds"))
	float ExplosionGrowDuration = 0.14f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Bomb", meta=(ClampMin="0", Units="Seconds"))
	float ExplosionFadeDuration = 0.2f;

	/** Couleur de la sphère d'explosion (alpha = opacité) ; matériau M_FoeExplosion (Color, Opacity) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Bomb")
	FLinearColor ExplosionColor = FLinearColor(1.0f, 0.0f, 0.0f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Bomb")
	TObjectPtr<UMaterialInterface> ExplosionMaterial;

	/** Disque d'impact (Preview Material / Preview Color) : opacités de l'intérieur et du bord */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Preview", meta=(ClampMin="0", ClampMax="1"))
	float PreviewFillOpacity = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Preview", meta=(ClampMin="0", ClampMax="1"))
	float PreviewEdgeOpacity = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Bomb")
	TSubclassOf<AYSFoeBomb> BombClass;

	UYSFoeBombAction();

	virtual bool StartAttack(AYSFoeCharacter* Foe, const APawn* Target, const FVector& Origin) override;
};

/**
 *  « NME shot » : le NME vise pendant AimDuration (ligne au sol dans la direction de sa cible AU DÉBUT de la visée, il ne
 *  réajuste pas et reste immobile), puis tire une balle (ProjectileClass, HitTags Team.Player) dans cette direction.
 *  Action de BP_NME_Pistol (Skull Gun). La visée est tenue par le NME (StartAim / FireShot).
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="NME shot"))
class UYSFoeShotAction : public UYSFoeRangedAction
{
	GENERATED_BODY()

public:

	/** Durée de la visée avant le tir */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Shot", meta=(ClampMin="0", Units="Seconds"))
	float AimDuration = 1.0f;

	/** Balle tirée (BP enfant de PetoirAMMO qui touche les joueurs : NME_GunAMMO) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Shot")
	TSubclassOf<AYSProjectile> ProjectileClass;

	/** Ligne de visée au sol : longueur et largeur (à peu près celle de la balle) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Preview", meta=(ClampMin="0", Units="Centimeters"))
	float AimLineLength = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Preview", meta=(ClampMin="1", Units="Centimeters"))
	float AimLineWidth = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Preview", meta=(ClampMin="0", ClampMax="1"))
	float AimLineOpacity = 0.35f;

	/** Démarre la visée du NME (le tir part à la fin, AYSFoeCharacter::FireShot) */
	virtual bool StartAttack(AYSFoeCharacter* Foe, const APawn* Target, const FVector& Origin) override;

	/** Tire la balle (serveur) */
	void Fire(AYSFoeCharacter* Foe, const FVector& Origin, const FVector& Direction) const;
};
