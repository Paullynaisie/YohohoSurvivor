#pragma once

#include "CoreMinimal.h"
#include "Ship/YSShipStation.h"
#include "Weapons/YSPlayerBomb.h"
#include "YSShipCannon.generated.h"

class AYSFoeCharacter;
class AYSProjectile;
class UStaticMeshComponent;
class UTexture2D;

/**
 *  Module de combat « Cannonerie » (acteurs enfants CannonFront / CannonLeft / CannonRight du bateau ; +X = direction d'origine :
 *  avant, bâbord, tribord) : E -> le joueur se met derrière le canon, ne bouge plus, ni attaque auto ni Ability ; E le quitte.
 *  Un seul joueur à la fois. Le canon (Cannon Pivot) suit le regard du joueur (curseur), à +/- Max Aim Angle de son alignement
 *  d'origine. Clic droit = « Tir de canon » : comme le tir du Shotgun (Ammo Per Shot CanonAMMO réparties à angles égaux de -Spread à +Spread
 *  autour de la direction du canon, tout part d'un coup), Damage par AMMO, échelle Projectile Scale, recharge Cooldown (icône en
 *  bas de l'écran à la place de celles des armes). Caméra : dézoomée comme à la barre, toujours décalée au maximum vers la visée
 *  du canon (butée à l'angle max).
 */
UCLASS(meta=(PrioritizeCategories="Cannon Station Interaction"))
class AYSShipCannon : public AYSShipStation
{
	GENERATED_BODY()

private:

	/** Affût (bloquant) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> CannonBase;

	/** Pivot du canon (tourne en yaw) : porte le fût, la bouche et les points des mains */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> CannonPivot;

	/** Racine du fût (sous le pivot, recule au tir) : porte CannonBarrel et les formes ajoutées dans le Blueprint */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> BarrelRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> CannonBarrel;

	/** Départ des CanonAMMO (bout du fût) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Muzzle;

	/** Heure serveur où le canon est rechargé */
	UPROPERTY(Replicated)
	double ReadyServerTime = 0.0;

protected:

	/** Prévisus du poste (joueur au poste seulement) ; vide = /Game/Core/Ship/DA_CombatPostPreview */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Preview")
	TObjectPtr<class UYSCombatPostPreviewData> PreviewData;

	/** Composants du Blueprint cachés (et sans collision, enfants compris) : formes héritées d'un poste parent
	 *  (ex. formes du canon dans la Gatling / le Grappeshot, enfants de BP_ShipCannon) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Visual")
	TArray<FName> HiddenComponentNames;

	/** Angle max du canon de chaque côté de son alignement d'origine */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon", meta=(ClampMin="0", ClampMax="180", Units="Degrees"))
	float MaxAimAngle = 45.0f;

	/** Vitesse à laquelle le canon suit la visée (0 = immédiat) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon", meta=(ClampMin="0"))
	float AimFollowSpeed = 12.0f;

	/** AMMO tirée (CanonAMMO) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Shot")
	TSubclassOf<AYSProjectile> ProjectileClass;

	/** Échelle des AMMO (taille de la balle et de sa collision) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Shot", meta=(ClampMin="0.1"))
	float ProjectileScale = 5.0f;

	/** Dégâts de chaque AMMO */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Shot", meta=(ClampMin="0"))
	float Damage = 250.0f;

	/** AMMO par tir (comme le Shotgun) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Shot", meta=(ClampMin="1"))
	int32 AmmoPerShot = 5;

	/** Dispersion : AMMO réparties à angles égaux entre -cet angle et +cet angle autour de la direction du canon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Shot", meta=(ClampMin="0", ClampMax="90", Units="Degrees"))
	float Spread = 20.0f;

	/** Recharge du tir de canon (comptée depuis le premier tir de la rafale) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Shot", meta=(ClampMin="0", Units="s"))
	float Cooldown = 2.0f;

	/** Rafale : tirs (de Ammo Per Shot AMMO chacun) par activation, un toutes les Burst Interval, vers la visée du moment (Gatling : 10 x 0.1 s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Shot", meta=(ClampMin="1"))
	int32 ShotsPerBurst = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Shot", meta=(ClampMin="0.01", Units="s", EditCondition="ShotsPerBurst > 1"))
	float BurstInterval = 0.1f;

	/**
	 *  Explosion à l'impact (canon : 750 de dégâts sur 7.5 m ; vide = pas d'explosion) : sur tout ce que l'AMMO touche et au bout de sa course ;
	 *  joueurs et minions touchés quand le tireur est un NME
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Explosion")
	TSubclassOf<class AYSPlayerBomb> ExplosionClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Explosion", meta=(ClampMin="0", EditCondition="ExplosionClass != nullptr"))
	float ExplosionDamage = 750.0f;

	/** Rayon de l'explosion (remplace celui de la bombe) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Explosion", meta=(ClampMin="1", Units="Centimeters", EditCondition="ExplosionClass != nullptr"))
	float ExplosionRadius = 750.0f;

	/**
	 *  Joueur ou minion au poste : dégâts (AMMO et explosion) x (1 + ceci x niveau de l'équipe) ; un passif peut le remplacer (Maître canonnier :
	 *  0.1). NME au poste : le Galion donne son multiplicateur (Post Damage Per Day)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Scaling", meta=(ClampMin="0"))
	float DamagePerPlayerLevel = 0.05f;

	/** Recharge x ce multiplicateur quand le tireur n'est pas un joueur (NME, minion du Captain) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Shot", meta=(ClampMin="0"))
	float NonPlayerCooldownMultiplier = 2.0f;

	/** Icône du tir de canon (en bas de l'écran) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Shot")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Shot")
	FLinearColor IconTint = FLinearColor::White;

	/**
	 *  Hauteur de vol des AMMO au-dessus de la mer / du sol (assez bas pour toucher les chaloupes de NME ; au moins le rayon de l'AMMO) ;
	 *  ex-constante 100
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Shot", meta=(ClampMin="0", Units="Centimeters"))
	float ShotHeightAboveWater = 40.0f;

	/** Nom de l'action sous l'icône (clic gauche) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Shot")
	FText FireActionName = NSLOCTEXT("YS", "CannonFire", "Tirer");

	/** Recul visuel du fût au tir */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Shot", meta=(ClampMin="0", Units="Centimeters"))
	float RecoilDistance = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Shot", meta=(ClampMin="0.01", Units="s"))
	float RecoilDuration = 0.4f;

	/** Minion au canon : il vise le NME le plus proche à moins de cette distance et dans l'angle du canon, et tire dès que le canon est rechargé. 0 = portée de l'AMMO (vitesse x durée de vie) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Minion", meta=(ClampMin="0", Units="Centimeters"))
	float AutoFireRange = 0.0f;

	/** Le minion tire quand le canon est à moins de cet angle de sa cible */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Minion", meta=(ClampMin="0", Units="Degrees"))
	float AutoFireAimTolerance = 5.0f;

	/** NME au canon : recherche de sa cible toutes les ... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Foe", meta=(ClampMin="0", Units="s"))
	float FoeTargetInterval = 0.25f;

	/** Canon à 0 PV : le fût penche de cet angle (visuel) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Broken", meta=(Units="Degrees"))
	float BrokenBarrelPitch = -25.0f;

	virtual void OnBrokenChanged(bool bBroken) override;

	/** Réparation / démontage arrêtés : le canon se met en recharge (on ne répare pas sans perdre le tir) */
	virtual void OnWorkStopped(EYSStationWork StoppedWork) override;

	/** Recul joué sur toutes les machines */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFired();

	virtual const TCHAR* GetLogTag() const override { return TEXT("[YSCannon]"); }

	/** Un tir : Ammo Per Shot AMMO en éventail autour de BaseYaw, depuis la bouche (serveur) ; mortier : obus en cloche vers Last Aim Location */
	virtual void SpawnVolley(APawn* Shooter, float BaseYaw, bool bFoeShooter);

	/** De quoi tirer (AMMO ; mortier : obus) */
	virtual bool HasShotClass() const;

	/** Portée du tir automatique (minion, NME) */
	virtual float GetAutoFireRange() const;

	/** Prévisu de la visée (section 1, repère du poste) : un trait par AMMO ; mortier : cercle de l'explosion */
	virtual void BuildAimPreview(TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FLinearColor>& Colors, const FVector2D& Origin, float Z,
		const class UYSCombatPostPreviewData* Data, float AimYaw) const;

	/** Clé de reconstruction de la prévisu de visée (angle ; mortier : angle et distance) */
	virtual float GetAimPreviewKey() const;

	/** Point visé du dernier tir (Fire) */
	FVector LastAimLocation = FVector::ZeroVector;

	/** Angle (relatif, limité) vers un point du monde */
	float ComputeAimYaw(const FVector& WorldTarget) const;

	/** Angle (relatif, limité) vers le regard de l'occupant */
	float GetTargetAimYaw() const;

	USceneComponent* GetMuzzle() const { return Muzzle; }
	USceneComponent* GetCannonPivot() const { return CannonPivot; }

public:

	virtual bool IsCombatPost() const override { return true; }

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Dégâts de ses tirs x ce multiplicateur pour ce tireur (niveau de l'équipe pour un joueur / minion, jours écoulés pour un NME) */
	float GetShotDamageMultiplier(const APawn* Shooter, bool bFoeShooter) const;

	AYSShipCannon();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void Tick(float DeltaSeconds) override;

	virtual bool HandlesAction(EYSStationAction Action) const override { return Action == EYSStationAction::Primary && IsUsable() && !IsWorking(); }

	/** Clic gauche : tir de canon vers AimLocation (dans les limites du canon) ; pas pendant une réparation / un démontage */
	virtual void ServerHandleAction(AYSPlayerCharacter* Character, EYSStationAction Action, const FVector& AimLocation) override;

	/** Direction du canon (vers la visée de l'occupant, limitée) */
	virtual bool GetCameraAimDirection(FVector& OutDirection) const override;

	virtual bool GetActionIcon(UTexture2D*& OutIcon, FLinearColor& OutTint, float& OutRemaining, float& OutDuration) const override;

	/** Tirer (clic gauche), Démonter (clic droit, poste de combat posé), Réparer (Espace) */
	virtual void GetStationActionIcons(FYSStationActionIcon& OutLeft, FYSStationActionIcon& OutRight, FYSStationActionIcon& OutSpace) const override;

	/** Secondes avant que le canon soit rechargé */
	float GetCooldownRemaining() const;

	/** Recharge après un tir : Cooldown x bonus du Flibustier (bateau des joueurs) */
	float GetEffectiveCooldown() const;

	/**
	 *  Tir automatique, le plus proche à portée et dans l'angle du canon, ou nullptr : modules des bateaux du camp adverse, NME (minion du
	 *  Captain) ou joueurs (NME au canon). Ceux qui sont sur un bateau ne sont visés que si ce bateau est hors d'usage.
	 */
	AActor* FindAutoFireTarget(bool bFoeShooter) const;

	/** Tir de canon par Shooter vers AimLocation (serveur) : minion, joueur ou NME */
	void Fire(APawn* Shooter, const FVector& AimLocation, bool bFoeShooter);

	/** Le canon pointe sur ce NME, à Auto Fire Aim Tolerance près */
	bool IsAimedAt(const AActor* Target) const;

private:

	/** Angle du canon par rapport à son alignement d'origine (affiché), et celui visé */
	float CurrentAimYaw = 0.0f;

	/** NME au canon (serveur) : cible, visée, tir */
	void UpdateFoeFire(float DeltaSeconds);

	/** Tir suivant de la rafale (serveur, timer) : vers la visée actuelle du canon */
	void FireNextBurstShot();

	FTimerHandle BurstTimer;
	TWeakObjectPtr<APawn> BurstShooter;
	bool bBurstFoeShooter = false;
	int32 BurstShotsLeft = 0;

	/** NME au canon : angle visé (serveur -> clients : il n'a pas de regard répliqué au poste) */
	UPROPERTY(Replicated)
	float FoeAimYaw = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<AActor> FoeTarget;

	float FoeTargetTimer = 0.0f;

	/** Temps local du dernier tir (recul) */
	double LastFireTime = -1.0e6;

	/** Prévisus (traits des limites d'angle, remplissage à la couleur du joueur, trait de visée) : seulement chez le joueur au poste */
	UPROPERTY(Transient)
	TObjectPtr<class UProceduralMeshComponent> PreviewMesh;

	float BuiltPreviewAimYaw = 1.0e6f;

protected:

	/** Couleur (émissive du joueur au poste) déjà construite (ne reconstruit qu'aux changements) ; lue par la prévisu du mortier */
	FLinearColor BuiltPreviewColor = FLinearColor::Transparent;

private:

	/** Joueur local au poste : prévisus construits / mis à jour ; sinon cachés */
	void UpdatePreview();

	/** Data Asset des prévisus (Preview Data, sinon /Game/Core/Ship/DA_CombatPostPreview) */
	const class UYSCombatPostPreviewData* GetPreviewData() const;

	FVector BarrelRestLocation = FVector::ZeroVector;
	bool bBarrelRestCaptured = false;
};

/**
 *  Poste de combat « Mortier » : comme le canon (visée au curseur dans l'angle du poste), mais tire un obus (Shell Class, AYSPlayerBomb lancée
 *  en cloche) qui atterrit au point visé en Flight Duration et explose (rayon de la bombe, ex. 15 m ; Damage x scaling du poste). Portée
 *  Min Range..Max Range (35 m). Prévisu : cercle de l'explosion sous le curseur.
 */
UCLASS(meta=(PrioritizeCategories="Mortar Cannon Station Interaction"))
class AYSShipMortar : public AYSShipCannon
{
	GENERATED_BODY()

protected:

	/** Obus (bombe lancée, explose en touchant le sol / l'eau) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mortar")
	TSubclassOf<class AYSPlayerBomb> ShellClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mortar", meta=(ClampMin="0.1", Units="s"))
	float FlightDuration = 4.0f;

	/** Sommet de la cloche au-dessus du plus haut des deux points */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mortar", meta=(ClampMin="0", Units="Centimeters"))
	float ApexHeight = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mortar", meta=(ClampMin="0", Units="Centimeters"))
	float MinRange = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mortar", meta=(ClampMin="0", Units="Centimeters"))
	float MaxRange = 3500.0f;

	virtual void SpawnVolley(APawn* Shooter, float BaseYaw, bool bFoeShooter) override;

	virtual bool HasShotClass() const override { return ShellClass != nullptr; }

	virtual float GetAutoFireRange() const override { return MaxRange; }

	virtual void BuildAimPreview(TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FLinearColor>& Colors, const FVector2D& Origin, float Z,
		const class UYSCombatPostPreviewData* Data, float AimYaw) const override;

	virtual float GetAimPreviewKey() const override;

	virtual const TCHAR* GetLogTag() const override { return TEXT("[YSMortar]"); }

public:

	/** Point d'impact pour un point visé : angle du poste, distance entre Min Range et Max Range, à la surface (sol ou mer) */
	FVector ComputeShellTarget(const FVector& Desired) const;

	/** Rayon de l'explosion de l'obus */
	float GetShellRadius() const;

private:

	/** Point visé par le joueur local au poste (prévisu) */
	FVector GetLocalAimTarget() const;
};
