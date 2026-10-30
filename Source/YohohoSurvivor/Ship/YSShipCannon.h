#pragma once

#include "CoreMinimal.h"
#include "Ship/YSShipStation.h"
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> CannonBarrel;

	/** Départ des CanonAMMO (bout du fût) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Muzzle;

	/** Heure serveur où le canon est rechargé */
	UPROPERTY(Replicated)
	double ReadyServerTime = 0.0;

protected:

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

	/** Recharge du tir de canon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Shot", meta=(ClampMin="0", Units="s"))
	float Cooldown = 2.0f;

	/** Icône du tir de canon (en bas de l'écran) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Shot")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cannon|Shot")
	FLinearColor IconTint = FLinearColor::White;

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

	/** Recul joué sur toutes les machines */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFired();

	virtual const TCHAR* GetLogTag() const override { return TEXT("[YSCannon]"); }

public:

	AYSShipCannon();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void Tick(float DeltaSeconds) override;

	virtual bool HandlesAction(EYSStationAction Action) const override { return Action == EYSStationAction::Fire && IsUsable(); }

	/** Clic droit : tir de canon vers AimLocation (dans les limites du canon) */
	virtual void ServerHandleAction(AYSPlayerCharacter* Character, EYSStationAction Action, const FVector& AimLocation) override;

	/** Direction du canon (vers la visée de l'occupant, limitée) */
	virtual bool GetCameraAimDirection(FVector& OutDirection) const override;

	virtual bool GetActionIcon(UTexture2D*& OutIcon, FLinearColor& OutTint, float& OutRemaining, float& OutDuration) const override;

	/** Secondes avant que le canon soit rechargé */
	float GetCooldownRemaining() const;

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

	/** Angle (relatif, limité) vers un point du monde */
	float ComputeAimYaw(const FVector& WorldTarget) const;

	/** Angle (relatif, limité) vers le regard de l'occupant */
	float GetTargetAimYaw() const;

	/** NME au canon (serveur) : cible, visée, tir */
	void UpdateFoeFire(float DeltaSeconds);

	/** NME au canon : angle visé (serveur -> clients : il n'a pas de regard répliqué au poste) */
	UPROPERTY(Replicated)
	float FoeAimYaw = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<AActor> FoeTarget;

	float FoeTargetTimer = 0.0f;

	/** Temps local du dernier tir (recul) */
	double LastFireTime = -1.0e6;

	FVector BarrelRestLocation = FVector::ZeroVector;
};
