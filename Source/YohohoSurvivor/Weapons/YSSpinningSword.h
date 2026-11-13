#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YSSpinningSword.generated.h"

class AYSPlayerCharacter;
class AYSWeapon;
class UBoxComponent;

/**
 *  « Spinning Sword » : des épées (visuel d'une arme du joueur, BP_Epee par défaut) qui tournent à plat autour d'un point,
 *  Height au-dessus du sol. Level 1 à Max Level = une épée par level, réparties à angles égaux.
 *  Self : accrochée au joueur qui la possède (suit son déplacement, pas sa rotation) ; sinon reste où elle a été posée.
 *  Perd un level toutes les Level Duration secondes (disparaît après le level 1) ; gagner un level relance ce délai.
 *  Dégâts (serveur) : chaque épée a la hitbox (box) de son arme ; un NME qui y entre prend Damage x Damage bonus de l'Auto atk
 *  x Damage bonus de l'Ability du propriétaire ; aucune collision (on la traverse).
 *  Taille des épées x Melee Range du propriétaire ; vitesse de rotation / Atk speed du propriétaire (recharge x0.5 = 2x plus vite).
 *  Couleur : tout le visuel prend la couleur du propriétaire, le composant Coloration de l'arme son émissif.
 *  Répliquée (owner = le joueur) ; les visuels des épées sont créés localement sur chaque machine.
 *  Classe utilisée : /Game/Placeable/Weapons/Player/BP_SpinningSword (réglages), sinon cette classe C++.
 */
UCLASS(Blueprintable, meta=(PrioritizeCategories="SpinningSword"))
class AYSSpinningSword : public AActor
{
	GENERATED_BODY()

public:

	AYSSpinningSword();

	/**
	 *  Serveur : un level de Spinning Sword pour Character. Self : celle accrochée au joueur (créée au besoin) ; sinon celle du
	 *  joueur posée à moins de Merge Radius de Location, sinon une nouvelle posée au sol sous Location.
	 */
	static AYSSpinningSword* GiveLevel(AYSPlayerCharacter* Character, bool bSelf, const FVector& Location, int32 Levels = 1, bool bAllowMerge = true);

	/** Serveur : +Count levels (sans dépasser Max Level) et délai de perte de level relancé */
	void AddLevel(int32 Count = 1);

	int32 GetLevel() const { return Level; }

	int32 GetMaxLevel() const { return MaxLevel; }

	bool IsSelf() const { return bSelf; }

	/** Self Spinning Sword du joueur (nullptr : aucune) */
	static AYSSpinningSword* FindSelf(const AYSPlayerCharacter* Character);

	/** Level Duration de la classe utilisée (BP_SpinningSword, sinon C++) : affichage de BladeMaster ++ */
	static float GetDefaultLevelDuration();

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Coup critique sur un NME à cet endroit (Lesser Spinning Blade : dans la zone de croisement) */
	virtual bool IsCriticalAt(const FVector& TargetLocation) const { return false; }

	/** Perd un level toutes les Level Duration (faux : Lesser Spinning Blade, qui disparaît en fin de trajet) */
	bool bLevelDecay = true;

	UPROPERTY(Replicated)
	bool bSelf = false;

	/** Heure serveur de création : angle de départ (et trajet des Lesser Spinning Blades) identiques sur toutes les machines */
	UPROPERTY(Replicated)
	double SpawnServerTime = 0.0;

	/** Angle de départ et sens de rotation (+1 / -1), tirés au hasard à la création (serveur, répliqués) */
	UPROPERTY(Replicated)
	float StartAngleOffset = 0.0f;

	UPROPERTY(Replicated)
	int8 SpinDirection = 1;

	/** Serveur, avant FinishSpawning : heure de création, angle de départ et sens au hasard */
	void InitSpawn(const UWorld* World);

	/** Classe Blueprint à charger (sinon cette classe C++) */
	static UClass* LoadSpawnClass(const TCHAR* BlueprintPath, UClass* Fallback);

	AYSPlayerCharacter* GetOwnerCharacter() const;

	double GetServerTime() const;

	/** Hauteur du sol sous Location (rien dessous : Fallback Z) */
	float FindGroundZ(const FVector& Location, float FallbackZ) const;

	/** Visuel de chaque épée (ses capacités sont retirées ; sa première box sert de hitbox) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SpinningSword")
	TSoftClassPtr<AYSWeapon> SwordVisualClass;

	/** Dégâts de chaque épée, avant les Damage bonus de l'Auto atk et de l'Ability du propriétaire */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SpinningSword", meta=(ClampMin="0"))
	float Damage = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SpinningSword", meta=(ClampMin="1"))
	int32 MaxLevel = 5;

	/** Un level est perdu après cette durée sans gain de level (level 1 : la Spinning Sword disparaît) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SpinningSword", meta=(ClampMin="0.1", Units="Seconds"))
	float LevelDuration = 5.0f;

	/** Vitesse de rotation sans Atk speed (divisée par le multiplicateur de recharge de l'Auto atk du propriétaire) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SpinningSword", meta=(Units="DegreesPerSecond"))
	float RotationSpeed = 360.0f;

	/** Hauteur du plan de rotation au-dessus du sol */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SpinningSword", meta=(Units="Centimeters"))
	float Height = 100.0f;

	/**
	 *  Position de l'origine de l'arme (la poignée) par rapport au centre de rotation, pour l'épée à l'angle 0 (X = rayon).
	 *  Défaut : poignée à 1 m du centre.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SpinningSword|Pose")
	FVector SwordLocation = FVector(100.0f, 0.0f, 0.0f);

	/**
	 *  Rotation de l'arme pour l'épée à l'angle 0 (X / Y / Z = Roll / Pitch / Yaw). Défaut Roll -90, Yaw 90 : la lame (axe Z de l'arme)
	 *  couchée à plat, pointée vers l'extérieur du cercle, son axe X (tranchant) dans le sens de la rotation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SpinningSword|Pose")
	FRotator SwordRotation = FRotator(0.0f, 90.0f, -90.0f);

	/** Échelle des épées sans Melee Range (multipliée par le Melee Range du propriétaire) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SpinningSword|Pose", meta=(ClampMin="0.01"))
	float SwordScale = 1.0f;

	/** Un NME resté dans la hitbox d'une épée est retouché après cette durée (sinon : une fois par passage de l'épée) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SpinningSword|Hits", meta=(ClampMin="0.05", Units="Seconds"))
	float RehitInterval = 0.5f;

	/** Les hitbox sont testées tous les Max Hit Step Angle degrés au plus (épées rapides : pas de NME sauté entre deux images) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SpinningSword|Hits", meta=(ClampMin="1", Units="Degrees"))
	float MaxHitStepAngle = 10.0f;

	/** Pas Self : un nouveau level donné à moins de cette distance d'une Spinning Sword du même joueur la renforce au lieu d'en créer une */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SpinningSword", meta=(ClampMin="0", Units="Centimeters"))
	float MergeRadius = 300.0f;

	/** Bladékinésis : vitesse de la Self Spinning Sword = vitesse actuelle du joueur x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SpinningSword", meta=(ClampMin="0"))
	float KinesisSpeedMultiplier = 1.5f;

private:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Root;

	/** Centre de rotation (Height au-dessus de Root, tourne autour de Z) */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY(ReplicatedUsing=OnRep_Level)
	int32 Level = 0;

	UFUNCTION()
	void OnRep_Level();

	/** Bladékinésis (Golden de BladeMaster) : la Self Spinning Sword suit le curseur à la vitesse du joueur (serveur, répliqué) */
	UPROPERTY(Replicated)
	bool bKinesis = false;

	/** Point visé par le joueur (serveur ; client : envoyé par ServerSetKinesisTarget) */
	FVector KinesisTarget = FVector::ZeroVector;
	bool bHasKinesisTarget = false;
	float KinesisSendTimer = 0.0f;

	UFUNCTION(Server, Unreliable)
	void ServerSetKinesisTarget(FVector_NetQuantize Target);

	/** Self : accrochée au joueur, ou qui suit le curseur (Bladékinésis) */
	void UpdateSelfMovement(float DeltaSeconds);

	/** Serveur : -1 level (détruite sous 1) */
	void LoseLevel();

	/** Recrée les visuels (un par level), répartis à angles égaux */
	void RebuildSwords();

	void DestroySwords();

	/** Couleur et émissif du propriétaire sur les visuels (dès qu'ils sont connus) */
	void ApplyColors();

	/** Serveur : dégâts des hitbox balayées de PreviousAngle à CurrentAngle */
	void ApplyHits(float PreviousAngle, float CurrentAngle);

	float GetCurrentRotationSpeed() const;

	/** Rotation de l'épée à l'angle 0 : Sword Rotation, retournée sur l'axe de la lame quand le sens de rotation est inversé (tranchant devant) */
	FQuat GetSwordLocalRotation() const;

	struct FSword
	{
		TWeakObjectPtr<AYSWeapon> Visual;
		TWeakObjectPtr<UBoxComponent> Hitbox;
		/** Dernier coup par NME, et NME dans la hitbox au dernier test */
		TMap<TWeakObjectPtr<AActor>, float> LastHitTimes;
		TSet<TWeakObjectPtr<AActor>> Overlapping;
	};

	TArray<FSword> Swords;

	float Angle = 0.0f;

	bool bColorsApplied = false;

	FTimerHandle LevelTimer;
};
