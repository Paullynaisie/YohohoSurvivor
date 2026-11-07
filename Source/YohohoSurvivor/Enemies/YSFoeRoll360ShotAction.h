#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "YSFoeRoll360ShotAction.generated.h"

class AActor;
class AYSFoeCharacter;
class AYSProjectile;
class UMaterialInterface;
class UYSFoeShotAction;

/**
 *  « Roll360shot » : attaque en plus d'un boss armé d'un pistolet (BP_NME_BossPistol, Boss Gun) tant que son bouclier de boss
 *  est actif (AYSFoeCharacter, Foe|Boss), comme le Boss Big Slash du Boss Skull. Lancée par le NME (serveur) quand sa cible est
 *  à moins de Trigger Range et que l'action est rechargée :
 *  1. roulade de Roll Distance en Roll Duration (à l'opposé de la cible si Roll Away From Target), arrivée jamais dans l'eau ;
 *     le boss reste touchable (PAS d'invulnérabilité), pas de pushback pendant toute l'attaque ;
 *  2. immobile, il tire Shot Count balles une par une toutes les Shot Interval, séparées de Angle Step : la première droit vers
 *     la cible (vue depuis l'arrivée de la roulade), puis tout autour dans un sens tiré au hasard (droite ou gauche) ;
 *     akimbo : un pistolet sur deux tire chaque balle (recul de ce pistolet, hauteur de balle de son NME shot).
 *  Chaque balle : dégâts du NME shot de l'arme x Damage Multiplier, échelle x Bullet Scale. Prévisualisation au sol : une bande
 *  par balle, de la largeur de la balle, qui apparaît Preview Lead Time avant son tir (donc dans l'ordre des tirs) et disparaît au tir.
 *  Composant d'arme de NME (local sur chaque machine) ; le déroulé est porté par le NME (FYSFoeRoll360Shot répliqué).
 *  Recharge = Cooldown, comptée du début d'une attaque au début de la suivante.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Roll360shot", PrioritizeCategories="Roll360shot"))
class UYSFoeRoll360ShotAction : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Lancée quand la cible du NME est à moins de cette distance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot", meta=(ClampMin="0", Units="Centimeters"))
	float TriggerRange = 1500.0f;

	/** Recharge, comptée du début d'une attaque au début de la suivante */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot", meta=(ClampMin="0", Units="Seconds"))
	float Cooldown = 10.0f;

	/** Distance de la roulade (raccourcie si l'arrivée est dans l'eau) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Roll", meta=(ClampMin="0", Units="Centimeters"))
	float RollDistance = 600.0f;

	/** Durée de la roulade */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Roll", meta=(ClampMin="0.05", Units="Seconds"))
	float RollDuration = 0.4f;

	/** Vrai : roulade à l'opposé de la cible ; faux : vers elle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Roll")
	bool bRollAwayFromTarget = true;

	/** Courbe de la roulade : 1 = vitesse constante, plus grand = départ rapide qui ralentit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Roll", meta=(ClampMin="1"))
	float RollEaseExponent = 2.0f;

	/** Tours sur lui-même pendant la roulade (le corps roule vers l'avant, autour de son centre) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Roll", meta=(ClampMin="0"))
	float RollTurns = 1.0f;

	/** Arrivée dans l'eau : reculée vers le départ par pas de cette distance jusqu'à un sol hors de l'eau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Roll", meta=(ClampMin="10", Units="Centimeters"))
	float WaterRetreatStep = 50.0f;

	/** Pause entre la fin de la roulade et la première balle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Shots", meta=(ClampMin="0", Units="Seconds"))
	float FirstShotDelay = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Shots", meta=(ClampMin="1"))
	int32 ShotCount = 24;

	/** Angle entre deux balles successives */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Shots", meta=(Units="Degrees"))
	float AngleStep = 15.0f;

	/** Décalage de la première balle par rapport à la cible (+ ou - au hasard) ; les suivantes tournent vers la cible */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Shots", meta=(Units="Degrees"))
	float FirstShotAngleOffset = 90.0f;

	/** Délai entre deux balles */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Shots", meta=(ClampMin="0", Units="Seconds"))
	float ShotInterval = 0.05f;

	/** Dégâts de chaque balle = dégâts du NME shot de l'arme (niveau, Damage Multiplier du NME) x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Shots", meta=(ClampMin="0"))
	float DamageMultiplier = 1.5f;

	/** Échelle des balles (taille et collision) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Shots", meta=(ClampMin="0.1"))
	float BulletScale = 2.0f;

	/** Balle tirée ; vide = celle du NME shot de l'arme */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Shots")
	TSubclassOf<AYSProjectile> ProjectileClass;

	/** Chaque bande apparaît ce temps avant le tir de sa balle (dans l'ordre des tirs) et disparaît au tir */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Preview", meta=(ClampMin="0", Units="Seconds"))
	float PreviewLeadTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Preview", meta=(ClampMin="0", Units="Centimeters"))
	float PreviewLength = 2000.0f;

	/** Largeur de la bande = diamètre de la balle (rayon de collision x Bullet Scale) x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Preview", meta=(ClampMin="0.1"))
	float PreviewWidthMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Preview")
	FLinearColor PreviewColor = FLinearColor(1.0f, 0.1f, 0.05f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Preview", meta=(ClampMin="0", ClampMax="1"))
	float PreviewOpacity = 0.35f;

	/** Matériau des bandes (M_BrookerZone : Color, Glow, FillOpacity, EdgeOpacity, Radius, EdgeWidth) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Roll360shot|Preview")
	TObjectPtr<UMaterialInterface> PreviewMaterial;

	UYSFoeRoll360ShotAction();

	/** Rechargée (serveur) */
	bool IsReady() const;

	/** Début d'une attaque (serveur) : recharge relancée */
	void NotifyStarted();

	/** NME shot de la même arme (dégâts de base, balle par défaut, recul), ou nullptr */
	UYSFoeShotAction* GetShotAction() const;

	/** Balle tirée : Projectile Class, sinon celle du NME shot */
	TSubclassOf<AYSProjectile> GetProjectileClass() const;

	/** Largeur des bandes de prévisualisation (diamètre de la balle à l'échelle) */
	float GetPreviewWidth() const;

	/** Dégâts d'une balle pour ce NME */
	float GetBulletDamage(const AYSFoeCharacter* Foe) const;

	/** Part de la roulade parcourue à Alpha (0 à 1) */
	float GetRollFraction(float Alpha) const;

	/** Arrivée de la roulade (serveur) : Roll Distance dans Direction, reculée tant qu'elle est dans l'eau ; pieds au sol + HalfHeight */
	FVector FindRollEnd(const FVector& Start, const FVector& Direction, float HalfHeight, const AActor* IgnoredActor) const;

	/** Tire une balle (serveur) depuis Origin dans Direction (à plat) */
	void FireBullet(AYSFoeCharacter* Foe, const FVector& Origin, const FVector& Direction) const;

private:

	/** Heure (serveur) où l'attaque est de nouveau prête */
	double ReadyTime = 0.0;
};
