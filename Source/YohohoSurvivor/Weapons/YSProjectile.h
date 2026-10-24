#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "YSProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 *  Projectile en ligne droite (à hauteur constante au-dessus du relief si bFollowGround). Touche uniquement les acteurs portant un des HitTags
 *  (via IGameplayTagAssetInterface), traverse les autres pawns, est détruit contre le décor.
 *  Les dégâts sont fixés par la capacité qui le tire (SetDamage) et appliqués sur le serveur.
 *  Classe de base du Blueprint PetoirAMMO.
 */
UCLASS(abstract, meta=(PrioritizeCategories="AMMO"))
class AYSProjectile : public AActor
{
	GENERATED_BODY()

private:

	/** Collision (racine) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USphereComponent> Collision;

	/** Apparence */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

protected:

	/** Vitesse de déplacement */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float Speed = 1200.0f;

	/** Durée de vie avant destruction automatique */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO", meta=(ClampMin="0", Units="Seconds"))
	float LifeTime = 5.0f;

	/** Le projectile touche les acteurs qui portent au moins un de ces tags ; il traverse les autres */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Collision")
	FGameplayTagContainer HitTags;

	/** Détruit le projectile quand il heurte le décor (murs, obstacles) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Collision")
	bool bDestroyOnWorldHit = true;

	/** Relief : la balle garde pendant tout son vol sa hauteur au-dessus du sol prise au tir (au-dessus de la mer s'il y a un océan) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Ground")
	bool bFollowGround = true;

	/** Montée max du sol suivie d'une image à l'autre ; au-delà c'est un mur (tronc, falaise) : la balle le heurte */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Ground", meta=(ClampMin="0", Units="Centimeters", EditCondition="bFollowGround"))
	float GroundFollowMaxStep = 100.0f;

	/** La balle regarde le sol ce temps devant elle pour monter avant la pente (sinon elle la heurte) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Ground", meta=(ClampMin="0", Units="Seconds", EditCondition="bFollowGround"))
	float GroundFollowLookAhead = 0.1f;

	/** Descente : vitesse à laquelle la balle redescend à la hauteur voulue (0 = immédiat ; plus petit = plus doux). Les montées sont immédiates */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Ground", meta=(ClampMin="0", EditCondition="bFollowGround"))
	float GroundFollowInterpSpeed = 15.0f;

	/** Couleur appliquée au mesh */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Appearance")
	FLinearColor Color = FLinearColor::Black;

	/** Paramètre vectoriel du matériau du mesh qui reçoit Color */
	UPROPERTY(EditAnywhere, Category="AMMO|Appearance")
	FName ColorParameterName = TEXT("Color");

	/** Dégâts infligés, fixés par la capacité au moment du tir (serveur) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="AMMO")
	float Damage = 0.0f;

	/** Captain AMMO : le NME touché (s'il survit) devient le NME marqué du tireur (un seul à la fois ; marque au sol, minions attirés) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Mark")
	bool bMarksTarget = false;

	/** Marque : les minions du tireur font des critiques sur le NME marqué */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Mark", meta=(EditCondition="bMarksTarget"))
	bool bMarkCritsMinions = false;

	/** Marque (Cursed Captain AMMO) : à la mort du NME marqué, un minion du tireur apparaît à sa place s'il reste de la place */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Mark", meta=(EditCondition="bMarksTarget"))
	bool bMarkSpawnsMinion = false;

	/** Ricochets restants : après un dégât, la balle repart vers le NME le plus proche pas encore touché (fixé au tir, upgrade Ricochet) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="AMMO|Ricochet")
	int32 RemainingRicochets = 0;

	/** Multiplicateur des dégâts appliqué à chaque ricochet (Ricochet bonus) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="AMMO|Ricochet")
	float RicochetDamageMultiplier = 1.0f;

	/** Distance max (depuis l'impact) pour trouver la cible d'un ricochet ; sans cible, la balle disparaît */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Ricochet", meta=(ClampMin="0", Units="Centimeters"))
	float RicochetRange = 1500.0f;

	/** Ricochet : la balle s'arrête ce temps sur l'impact avant de repartir vers la cible suivante (0 = repart tout de suite) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Ricochet", meta=(ClampMin="0", Units="Seconds"))
	float RicochetPauseDuration = 0.1f;

private:

	/** Plus de dégâts possibles (balle en cours de destruction) */
	bool bHasHit = false;

	/** Arrêtée sur l'impact avant un ricochet : ne touche rien (serveur) */
	bool bRicochetPaused = false;

	/** Cible du ricochet en attente (serveur) */
	TWeakObjectPtr<AActor> RicochetTarget;

	FTimerHandle RicochetPauseTimer;

	/** Atk critique : une balle sur N reçue par un même NME est critique (0 = non ; fixé au tir) */
	int32 CriticalBulletInterval = 0;

	/** Sniper : NME que la balle peut toucher avant de disparaître (1 = s'arrête au premier ; fixé au tir) */
	int32 MaxHits = 1;

	/** NME déjà touchés (traversée du Sniper) */
	int32 HitCount = 0;

	/** Atk critique à la distance (0 = non ; fixé au tir) : critique sur un NME à moins de Within (Shotgun) / à plus de Beyond (Sniper) du tireur */
	float CriticalWithinDistance = 0.0f;
	float CriticalBeyondDistance = 0.0f;

	/** Cibles déjà touchées (un ricochet ne les retouche pas) */
	TArray<TWeakObjectPtr<AActor>> HitActors;

	/** Suivi du sol : hauteur voulue au-dessus du sol (prise au tir) et dernier sol trouvé ; négatif = pas de suivi */
	float HeightAboveGround = -1.0f;
	float LastGroundZ = 0.0f;

public:

	AYSProjectile();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void PostInitializeComponents() override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void LifeSpanExpired() override;

	void SetDamage(float NewDamage) { Damage = NewDamage; }

	/** Ricochets (upgrade Ricochet) : nombre et multiplicateur de dégâts à chaque ricochet */
	void SetRicochets(int32 Count, float DamageMultiplier) { RemainingRicochets = Count; RicochetDamageMultiplier = DamageMultiplier; }

	/** Atk critique (balles d'attaque auto PewPew) : une balle sur N reçue par un même NME (de ce tireur) est critique ; 0 = non */
	void SetCriticalBulletInterval(int32 Interval) { CriticalBulletInterval = Interval; }

	/** Sniper : la balle traverse les NME et en touche Count avant de disparaître (1 = s'arrête au premier) */
	void SetMaxHits(int32 Count) { MaxHits = FMath::Max(1, Count); }

	/** Atk critique à la distance : critique si le point d'impact est à moins de Within / à plus de Beyond du tireur (0 = non) */
	void SetCriticalDistances(float Within, float Beyond) { CriticalWithinDistance = Within; CriticalBeyondDistance = Beyond; }

	/** Tags des acteurs que ce projectile peut toucher */
	const FGameplayTagContainer& GetHitTags() const { return HitTags; }

	/** Vrai si l'acteur porte un des HitTags */
	bool CanHit(const AActor* Target) const;

protected:

	/** Ricochet vers le NME le plus proche pas encore touché (après l'arrêt RicochetPauseDuration) ; faux si aucun à portée */
	bool Ricochet();

	/** NME le plus proche de l'impact pas encore touché, à moins de RicochetRange, ou nullptr */
	AActor* FindRicochetTarget() const;

	/** Repart vers la cible du ricochet (une autre si elle a disparu pendant l'arrêt, sinon la balle disparaît) */
	void LaunchRicochet();

	/** Sol sous (X, Y) cherché entre ReferenceZ + MaxStepUp et ReferenceZ - MaxStepDown, ramené au niveau de la mer ; faux sans sol */
	bool FindGroundZ(const FVector2D& Point, float ReferenceZ, float MaxStepUp, float MaxStepDown, float& OutGroundZ) const;

protected:

	UFUNCTION()
	void OnCollisionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Vrai si ce choc est contre le sol suivi (pente praticable) et non contre un mur */
	bool IsGroundHit(const FHitResult& Hit) const;

	/** Le composant de mouvement s'est arrêté sur un choc : contre une pente, la balle reprend sa hauteur et repart */
	UFUNCTION()
	void OnProjectileStopped(const FHitResult& Hit);

	UFUNCTION()
	void OnCollisionHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};
