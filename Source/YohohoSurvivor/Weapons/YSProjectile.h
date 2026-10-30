#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Weapons/YSPlayerBomb.h"
#include "YSProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 *  Éventail tiré d'un coup (Shotgun) : partagé par ses AMMO (serveur). La N-ième AMMO qui touche une même cible fait
 *  x max(Min Falloff Multiplier, 1 - Damage Falloff Per Hit x (N - 1)) (dégressivité naturelle) x (1 + Damage Bonus Per Hit x (N - 1))
 *  (Gros sel) ; à partir de Critical Hit Count AMMO sur la même cible (0 = jamais, Golden gros sel), critique.
 */
struct FYSShotVolley
{
	float DamageBonusPerHit = 0.0f;
	float DamageFalloffPerHit = 0.0f;
	float MinFalloffMultiplier = 0.2f;
	int32 CriticalHitCount = 0;
	TMap<TWeakObjectPtr<AActor>, int32> HitCounts;
};

/** Explosion d'une balle (shoot system Explosif), donnée par l'action au tir (serveur) */
struct FYSProjectileExplosion
{
	/** Explosion à l'impact (NME ou décor) */
	TSubclassOf<AYSPlayerBomb> ExplosionClass;
	float Damage = 0.0f;
	float RadiusMultiplier = 1.0f;

	/** MineShooter : sur un NME, mine collée à la place de l'explosion (le décor : explosion normale) */
	TSubclassOf<AYSPlayerBomb> StickyMineClass;
	float StickyFuse = 5.0f;
	/** Dégâts en plus par mine collée sur le NME au moment de l'explosion (MineShooter ++) */
	float StickyBonusPerMine = 0.0f;
};

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="AMMO", meta=(ClampMin="0", Units="Seconds"))
	float LifeTime = 5.0f;

	/** Le projectile touche les acteurs qui portent au moins un de ces tags ; il traverse les autres */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Collision")
	FGameplayTagContainer HitTags;

	/** Détruit le projectile quand il heurte le décor (murs, obstacles) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Collision")
	bool bDestroyOnWorldHit = true;

	/** Relief : la balle garde pendant tout son vol sa hauteur au-dessus du sol prise au tir (au-dessus de la mer s'il y a un océan) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="AMMO|Ground")
	bool bFollowGround = true;

	/** Montée max du sol suivie d'une image à l'autre ; au-delà c'est un mur (tronc, falaise) : la balle le heurte */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Ground", meta=(ClampMin="0", Units="Centimeters", EditCondition="bFollowGround"))
	float GroundFollowMaxStep = 100.0f;

	/**
	 *  Pente max suivie à la lettre (dénivelé / distance parcourue, 1 = 45°) : les pentes de l'île (jusqu'à ~0.42) sont suivies exactement,
	 *  en montée comme en descente ; une descente plus raide (bord d'un rocher, d'un prop) est adoucie (Ground Follow Interp Speed)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Ground", meta=(ClampMin="0", EditCondition="bFollowGround"))
	float GroundFollowMaxSlope = 1.0f;

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

	/** « Roll, bomb and shot » : la balle passe à moins de cette distance (à plat) d'une bombe / mine de son tireur -> explosion critique */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Bomb", meta=(ClampMin="0", Units="Centimeters"))
	float BombHitRadius = 60.0f;

	/** Cette balle fait exploser les bombes de son tireur (donné par l'action au tir) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="AMMO|Bomb")
	bool bDetonatesBombs = false;

	/** La balle traverse tous les NME (aucun maximum ; ex. DragonBreathAMMO) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Pierce")
	bool bPierceAllTargets = false;

	/** Chaque NME touché (qui survit) est bumpé : lancé en cloche, sans comportement pendant le vol (ex. DragonBreathAMMO) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Bump")
	bool bBumpsTargets = false;

	/** Bump : distance (à plat, dans le sens de vol de la balle) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Bump", meta=(ClampMin="0", Units="Centimeters", EditCondition="bBumpsTargets"))
	float BumpDistance = 800.0f;

	/** Bump : sommet de la cloche au-dessus du plus haut des deux points */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Bump", meta=(ClampMin="1", Units="Centimeters", EditCondition="bBumpsTargets"))
	float BumpApexHeight = 300.0f;

	/** Bump : durée du vol */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Bump", meta=(ClampMin="0.05", Units="Seconds", EditCondition="bBumpsTargets"))
	float BumpDuration = 1.0f;

	/** Bump : tours sur lui-même pendant le vol */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Bump", meta=(ClampMin="0", EditCondition="bBumpsTargets"))
	float BumpSpinTurns = 1.0f;

	/** Bump : les élites sont bumpés aussi (sinon pushback normal) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Bump", meta=(EditCondition="bBumpsTargets"))
	bool bBumpElites = true;

private:

	/** Shoot system Pierce : critique sur un NME en train d'être bumpé (fixé au tir) */
	bool bCriticalOnBumped = false;

	/** Toujours critique (Aiming du Harquebusier ; fixé au tir) */
	bool bAlwaysCritical = false;

	/** Gros sel : éventail partagé avec les autres AMMO du même tir (serveur) */
	TSharedPtr<FYSShotVolley> Volley;

	/** Shoot system Explosif (serveur) */
	FYSProjectileExplosion Explosion;

	/** Explosion (ou mine collée) à la position de la balle : sur Target (NME touché) ou le décor (Target nul) ; serveur */
	void SpawnExplosion(AActor* Target, bool bCritical);

	/** Dégâts à une cible touchée (chevauchement, ou NME bumpé survolé ; serveur) */
	void HitTarget(AActor* OtherActor, const FHitResult& SweepResult);

	/** NME bumpés (en vol, au-dessus de la balle) sur le segment parcouru depuis la dernière image : touchés (serveur) */
	void CheckBumpedFoeHits();

	/** Position à la dernière recherche de NME bumpés */
	FVector PreviousBumpCheckLocation = FVector::ZeroVector;

	/**
	 *  Pawns traversés depuis le dernier appel (balayage de la sphère de la balle, serveur) : les NME n'entretiennent pas
	 *  d'overlaps (perf des foules), la balle les trouve elle-même ; les joueurs sont aussi trouvés par chevauchement
	 */
	void SweepPawnHits();

	/** Position au dernier balayage des pawns */
	FVector LastPawnSweepLocation = FVector::ZeroVector;
	bool bHasPawnSweepLocation = false;

	/** Position à la frame précédente (balles qui font exploser les bombes : segment parcouru) */
	FVector PreviousLocation = FVector::ZeroVector;

	/** Bombes du tireur sur le segment parcouru : explosion critique, la balle disparaît (serveur) */
	void CheckBombHits();

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

	/** NME touchés depuis le tir ou le dernier ricochet (perte de dégâts multi-cibles du tireur) */
	int32 PierceHitIndex = 0;

	/** Atk critique à la distance (0 = non ; fixé au tir) : critique sur un NME à moins de Within (Shotgun) / à plus de Beyond (Sniper) du tireur */
	float CriticalWithinDistance = 0.0f;
	float CriticalBeyondDistance = 0.0f;

	/** Cibles déjà touchées (un ricochet ne les retouche pas) */
	TArray<TWeakObjectPtr<AActor>> HitActors;

	/** Suivi du sol : hauteur voulue au-dessus du sol (prise au tir) et dernier sol trouvé ; négatif = pas de suivi */
	float HeightAboveGround = -1.0f;
	float LastGroundZ = 0.0f;

	/** Traversé en vol (canon : le bateau qui tire et ses postes) ; répliqué à l'apparition */
	UPROPERTY(Replicated)
	TObjectPtr<AActor> FlightIgnoredActor;

	/** Hauteur au-dessus du sol imposée (canon) ; négatif = prise au tir ; répliquée à l'apparition */
	UPROPERTY(Replicated)
	float ForcedHeightAboveGround = -1.0f;

public:

	AYSProjectile();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void PostInitializeComponents() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void LifeSpanExpired() override;

	/** Avant FinishSpawning : acteur traversé (et ce qui y est accroché), hauteur au-dessus du sol imposée (négatif = prise au tir) */
	void SetFlightOverrides(AActor* IgnoredActor, float HeightAboveGroundOverride) { FlightIgnoredActor = IgnoredActor; ForcedHeightAboveGround = HeightAboveGroundOverride; }

	/** Avant FinishSpawning : ligne droite dans la direction du tir, sans suivre le relief (NME à bord d'un Jolly Ship) */
	void SetStraightFlight() { bFollowGround = false; }

	/** Avant FinishSpawning : durée de vie multipliée (NME à bord d'un Jolly Ship) */
	void MultiplyLifeTime(float Multiplier) { LifeTime *= Multiplier; }

	void SetDamage(float NewDamage) { Damage = NewDamage; }

	/** Ricochets (upgrade Ricochet) : nombre et multiplicateur de dégâts à chaque ricochet */
	void SetRicochets(int32 Count, float DamageMultiplier) { RemainingRicochets = Count; RicochetDamageMultiplier = DamageMultiplier; }

	/** Atk critique (balles d'attaque auto PewPew) : une balle sur N reçue par un même NME (de ce tireur) est critique ; 0 = non */
	void SetCriticalBulletInterval(int32 Interval) { CriticalBulletInterval = Interval; }

	/** Sniper : la balle traverse les NME et en touche Count avant de disparaître (1 = s'arrête au premier) */
	void SetMaxHits(int32 Count) { MaxHits = FMath::Max(1, Count); }

	/** « Roll, bomb and shot » : la balle fait exploser (critique) les bombes de son tireur qu'elle touche */
	void SetDetonatesBombs(bool bDetonates) { bDetonatesBombs = bDetonates; }

	/** Atk critique à la distance : critique si le point d'impact est à moins de Within / à plus de Beyond du tireur (0 = non) */
	void SetCriticalDistances(float Within, float Beyond) { CriticalWithinDistance = Within; CriticalBeyondDistance = Beyond; }

	/** Shoot system Pierce : critique sur un NME en train d'être bumpé */
	void SetCriticalOnBumped(bool bEnabled) { bCriticalOnBumped = bEnabled; }

	/** Toujours critique (Aiming) */
	void SetAlwaysCritical(bool bEnabled) { bAlwaysCritical = bEnabled; }

	/** Traverse tous les NME */
	void SetPierceAllTargets(bool bEnabled) { bPierceAllTargets = bEnabled; }

	/** Gros sel : éventail partagé par les AMMO d'un même tir */
	void SetVolley(const TSharedPtr<FYSShotVolley>& InVolley) { Volley = InVolley; }

	/** Shoot system Explosif : explosion (ou mine collée) à l'impact */
	void SetExplosion(const FYSProjectileExplosion& InExplosion) { Explosion = InExplosion; }

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
