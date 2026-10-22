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

	/** Moment visible de l'attaque (début du lancer, tir) sur cette machine : animations locales (NetMulticast du NME) */
	void PlayAttackLocally();

	/** Secondes depuis PlayAttackLocally sur cette machine (très grand si jamais) */
	float GetTimeSinceAttack() const;

	/**
	 *  Pose animée de la main qui tient l'arme (main droite, relative au corps), comme l'Auto swing : OutIdleWeight = part de
	 *  la pose normale (1 = normale). Faux hors animation.
	 */
	virtual bool EvaluateHandPose(float& OutIdleWeight, FVector& OutLocation, FQuat& OutRotation) const { return false; }

	/** Recul après un tir : cabrage (degrés) et recul le long de l'axe de l'arme (cm) ; faux au repos */
	virtual bool GetRecoil(float& OutPitch, float& OutKickBack) const { return false; }

	/** L'arme est cachée en ce moment (ex. bombe partie, pas encore revenue dans la main) */
	virtual bool IsWeaponHidden() const { return false; }

	/** Animation de l'attaque en cours sur cette machine (les mains du serveur ne sont alors jamais figées) */
	virtual bool IsAnimating() const { return false; }

private:

	/** Instant (temps local) du dernier PlayAttackLocally */
	double LastAttackTime = -1.0e6;
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

	/** La bombe vise un point au hasard autour de la cible, à une distance entre 0 et ce rayon (0 = pile sur la cible) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Bomb", meta=(ClampMin="0", Units="Centimeters"))
	float TargetScatterRadius = 350.0f;

	/** Lancer, phase 1 : la main recule et monte (pose de préparation) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Throw Anim", meta=(ClampMin="0.01", Units="Seconds"))
	float ThrowWindupDuration = 0.3f;

	/** Phase 2 : la main part vers l'avant ; la bombe quitte la main à la fin de cette phase */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Throw Anim", meta=(ClampMin="0.01", Units="Seconds"))
	float ThrowDuration = 0.12f;

	/** Phase 3 : retour à la pose normale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Throw Anim", meta=(ClampMin="0.01", Units="Seconds"))
	float ThrowRecoverDuration = 0.35f;

	/** Pose de préparation (main droite, relative au corps ; X avant, Y droite, Z haut) : derrière l'épaule */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Throw Anim")
	FVector WindupHandLocation = FVector(-40.0f, 45.0f, 50.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Throw Anim")
	FRotator WindupHandRotation = FRotator(-30.0f, 0.0f, 0.0f);

	/** Pose au lâcher : bras tendu devant, en hauteur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Throw Anim")
	FVector ReleaseHandLocation = FVector(70.0f, 30.0f, 40.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Throw Anim")
	FRotator ReleaseHandRotation = FRotator(20.0f, 0.0f, 0.0f);

	/** Courbes : préparation EaseOut, lancer EaseOut marqué, retour EaseInOut */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Throw Anim", meta=(ClampMin="1"))
	float ThrowEaseExponent = 3.0f;

	/** La bombe réapparaît dans la main ce temps après l'avoir quittée */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Throw Anim", meta=(ClampMin="0", Units="Seconds"))
	float BombReappearDelay = 1.0f;

	UYSFoeBombAction();

	/** Démarre l'animation de lancer (serveur) ; la bombe part au lâcher (fin de ThrowDuration), vers le point visé au début */
	virtual bool StartAttack(AYSFoeCharacter* Foe, const APawn* Target, const FVector& Origin) override;

	virtual bool EvaluateHandPose(float& OutIdleWeight, FVector& OutLocation, FQuat& OutRotation) const override;

	virtual bool IsWeaponHidden() const override;

	virtual bool IsAnimating() const override { return GetTimeSinceAttack() < ThrowWindupDuration + ThrowDuration + ThrowRecoverDuration; }

private:

	/** Lâcher (serveur) : la bombe part du Muzzle de l'arme (dans la main) vers Target */
	void ReleaseBomb(TWeakObjectPtr<AYSFoeCharacter> Foe, FVector Target);

	FTimerHandle ReleaseTimer;
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

	/** Imprécision : la direction de visée est décalée au hasard de +/- cet angle (ligne de visée comprise) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Shot", meta=(ClampMin="0", ClampMax="90", Units="Degrees"))
	float AimSpread = 5.0f;

	/** Recul après le tir (mêmes réglages et même animation que le pistolet du joueur, catégorie Recoil) : cabrage de la main */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Recoil", meta=(Units="Degrees"))
	float RecoilPitch = 25.0f;

	/** Recul de la main le long de l'axe de l'arme */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Recoil", meta=(ClampMin="0", Units="Centimeters"))
	float RecoilKickBack = 10.0f;

	/** Durée du coup (EaseOut) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Recoil", meta=(ClampMin="0.001", Units="Seconds"))
	float RecoilKickDuration = 0.05f;

	/** Durée du retour (EaseInOut) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NME Attack|Recoil", meta=(ClampMin="0.001", Units="Seconds"))
	float RecoilRecoverDuration = 0.25f;

	virtual bool GetRecoil(float& OutPitch, float& OutKickBack) const override;

	virtual bool IsAnimating() const override { return GetTimeSinceAttack() < RecoilKickDuration + RecoilRecoverDuration; }

	/** Démarre la visée du NME (le tir part à la fin, AYSFoeCharacter::FireShot) */
	virtual bool StartAttack(AYSFoeCharacter* Foe, const APawn* Target, const FVector& Origin) override;

	/** Tire la balle (serveur) */
	void Fire(AYSFoeCharacter* Foe, const FVector& Origin, const FVector& Direction) const;
};
