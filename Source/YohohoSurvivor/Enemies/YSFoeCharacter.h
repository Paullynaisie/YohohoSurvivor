#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "YSFoeCharacter.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class UYSFaceDecalComponent;
class AYSInteractiveObject;
class AYSFoeBomb;
class AYSWeapon;
class UChildActorComponent;
class UYSWeaponSet;
class UMaterialInterface;
class UYSFoeRangedAction;
class UYSAutoSwingAbility;
class UYSGroundZoneComponent;
class AYSProjectile;

/**
 *  Ennemi de base : attiré en ligne droite vers le joueur le plus proche, sans pathfinding,
 *  et toujours tourné vers lui. Meurt (détruit) quand ses points de vie tombent à 0 et donne alors de l'XP à l'équipe.
 *  Niveau (Level) : PV, dégâts de contact et XP = valeur de base + bonus par niveau (catégorie Foe|Level).
 *  DA « primitives » comme le joueur : cylindre (BodyMesh, aussi sa hurtbox) + visage en decal, sans inclinaison (perf).
 *  Contact : sa Hitbox déclenche l'attaque de son arme de mêlée (NME sword : swing du joueur) ; sans arme de mêlée, elle
 *  inflige ContactDamage à la hurtbox (corps) des joueurs, puis toutes les ContactDamageInterval secondes.
 *  C'EST L'ARME QUI DÉFINIT L'ATTAQUE : arme avec une action à distance (UYSFoeRangedAction : NME bomb, NME pistol) -> le NME
 *  court jusqu'à sa portée, s'arrête et attaque (bombe, visée + balle) ; arme avec un Auto swing -> sa hitbox déclenche le coup.
 *  La logique tourne uniquement sur le serveur ; position, rotation et vie sont répliquées aux clients.
 *  Classe de base du Blueprint FoeCharacter_Skull : les réglages se font dans le Blueprint.
 */
UCLASS(abstract, meta=(PrioritizeCategories="Foe"))
class AYSFoeCharacter : public ACharacter, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

private:

	/** Corps (cylindre par défaut) ; c'est aussi la hurtbox de l'ennemi */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** Visage projeté sur l'avant du corps */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UYSFaceDecalComponent> FaceDecal;

	/** Zone de contact qui blesse les joueurs (un peu plus large que le corps : les capsules empêchent les corps de se toucher) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCapsuleComponent> Hitbox;

	/** Barre de vie au-dessus de la tête (cachée tant que les PV sont au max) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UWidgetComponent> HealthBar;

	/** Mains (comme le joueur) : points d'attache des armes, position monde calculée par UpdateHands */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> LeftHand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> RightHand;

	/** Sphères visibles des mains */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> LeftHandMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> RightHandMesh;

	/** Armes des mains (acteurs enfants ; armes de NME NON répliquées : créées localement sur chaque machine) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> LeftHandWeapon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> RightHandWeapon;

	/** Skull Gun : ligne de visée posée sur le sol pendant la visée (coordonnées monde) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UYSGroundZoneComponent> AimLine;

protected:

	/** Nom affiché (barre de vie de l'élite dans le HUD) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe")
	FText FoeName;

	/** Le niveau suit le numéro du jour en cours au moment de l'apparition (+ LevelFromDayOffset : jour 1 = niveau 0) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level")
	bool bLevelFromDay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level", meta=(EditCondition="bLevelFromDay"))
	int32 LevelFromDayOffset = -1;

	/** Élite : apparaît la nuit, sa mort fait passer au jour suivant ; barre de vie dans le HUD (haut droite) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Elite")
	bool bIsElite = false;

	/** Multiplicateurs (élites) : PV, dégâts de contact, taille (appliquée au BeginPlay sur toutes les machines) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Elite", meta=(ClampMin="0"))
	float HealthMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Elite", meta=(ClampMin="0"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Elite", meta=(ClampMin="0.1"))
	float SizeMultiplier = 1.0f;

	/** Objet laissé au sol à sa mort (ex. coffre de l'élite ; vide = rien) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Loot")
	TSubclassOf<AYSInteractiveObject> DeathLootClass;

	/** Niveau du NME : augmente ses PV, ses dégâts et l'XP qu'il donne */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Foe|Level", meta=(ClampMin="0"))
	int32 Level = 0;

	/** PV au niveau 0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level", meta=(ClampMin="1"))
	float BaseHealth = 100.0f;

	/** PV ajoutés par niveau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level", meta=(ClampMin="0"))
	float HealthPerLevel = 10.0f;

	/** Dégâts de contact au niveau 0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level", meta=(ClampMin="0"))
	float BaseContactDamage = 5.0f;

	/** Dégâts de contact ajoutés par niveau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level", meta=(ClampMin="0"))
	float ContactDamagePerLevel = 3.0f;

	/** XP donnée à l'équipe à sa mort, au niveau 0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level", meta=(ClampMin="0"))
	float BaseExperience = 1.0f;

	/** XP ajoutée par niveau (XP donnée = Base + ParNiveau × niveau) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level", meta=(ClampMin="0"))
	float ExperiencePerLevel = 1.0f;

	/** PV max, calculés au spawn depuis le niveau */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Foe|Health")
	float MaxHealth = 100.0f;

	/** Points de vie actuels (gérés par le serveur) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Foe|Health")
	float Health = 0.0f;

	/** Dégâts infligés à un joueur au contact, calculés au spawn depuis le niveau */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Foe|Attack")
	float ContactDamage = 5.0f;

	/** Tant que le contact dure, dégâts répétés toutes les N secondes (par joueur touché) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foe|Attack", meta=(ClampMin="0.05", Units="s"))
	float ContactDamageInterval = 1.0f;

	/**
	 *  Cibles de la hitbox : acteurs portant un de ces tags. NME armé d'une arme de mêlée (Auto swing, ex. NME sword) :
	 *  la hitbox ne blesse plus, elle DÉCLENCHE l'attaque de l'arme (animation du swing du joueur, la hitbox de l'arme
	 *  inflige ContactDamage aux joueurs) ; sinon dégâts de contact directs toutes les ContactDamageInterval secondes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foe|Attack")
	FGameplayTagContainer ContactHitTags;

	/**
	 *  Pushback des joueurs touchés par ce NME (coup d'épée, balle), exactement comme celui des NME frappés par le joueur :
	 *  selon la part de la vie du joueur retirée par le coup ; sous Min Damage Ratio rien, à partir de Max Damage Ratio la
	 *  force max, entre les deux de Min Force à Max Force. Direction = celle du coup.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foe|Pushback", meta=(ClampMin="0", ClampMax="1"))
	float PushbackMinDamageRatio = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foe|Pushback", meta=(ClampMin="0", ClampMax="1"))
	float PushbackMaxDamageRatio = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foe|Pushback", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float PushbackMinForce = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foe|Pushback", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float PushbackMaxForce = 1000.0f;

	/** Tags portés par l'ennemi (camp, etc.), lus par les projectiles */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Tags")
	FGameplayTagContainer GameplayTags;

	/** Vitesse de déplacement maximale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Movement", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float MoveSpeed = 300.0f;

	/** Vitesse à laquelle l'ennemi se tourne vers sa cible (0 = instantané) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Movement", meta=(ClampMin="0"))
	float TurnSpeed = 10.0f;

	/**
	 *  Position par défaut de la main droite, par rapport au corps (centre de la capsule) ; la gauche est symétrique (Y inversé).
	 *  Une arme peut la remplacer (Weapon > Hand Location). Mêmes réglages que le joueur (3C|Hands).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Hands")
	FVector HandLocation = FVector(0.0f, 55.0f, -10.0f);

	/** Rotation par défaut de la main droite, par rapport au corps ; la gauche est symétrique (Yaw et Roll inversés) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Hands")
	FRotator HandRotation = FRotator::ZeroRotator;

	/** Écart maximal de l'oscillation des mains autour de leur position (0 = pas d'oscillation) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Hands", meta=(ClampMin="0", Units="Centimeters"))
	float HandSwayAmplitude = 5.0f;

	/** Vitesse de l'oscillation (bruit de Perlin) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Hands", meta=(ClampMin="0"))
	float HandSwaySpeed = 0.6f;

	/** Réactivité des mains au mouvement du corps : plus petit = plus de retard (0 = aucun retard) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Hands", meta=(ClampMin="0"))
	float HandFollowSpeed = 12.0f;

	/** Les sphères des mains reprennent le matériau du corps (BodyMesh) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Hands")
	bool bHandsUseBodyMaterial = true;

	/** Perf : les mains ne sont animées que si le NME a été affiché il y a moins de ce temps (hors écran : figées) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Hands", meta=(ClampMin="0", Units="Seconds"))
	float HandsOffscreenFreezeDelay = 0.2f;

	/** Set d'arme (Data Asset : arme de chaque main, /Game/Core/WeaponSets) ; prioritaire sur les armes par main ci-dessous */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Weapons")
	TObjectPtr<UYSWeaponSet> WeaponSet;

	/** Armes par main, utilisées seulement sans set (tests) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Weapons", meta=(EditCondition="WeaponSet == nullptr"))
	TSubclassOf<AYSWeapon> LeftHandWeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Weapons", meta=(EditCondition="WeaponSet == nullptr"))
	TSubclassOf<AYSWeapon> RightHandWeaponClass;

	/** Chance (0 à 1) d'être gaucher à l'apparition : le set est réglé droitier, un gaucher a les armes des deux mains échangées */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Weapons", meta=(ClampMin="0", ClampMax="1"))
	float LeftHandedChance = 0.0f;

	/** Gaucher (tiré par le serveur à l'apparition, répliqué) */
	UPROPERTY(ReplicatedUsing=OnRep_LeftHanded, VisibleInstanceOnly, BlueprintReadOnly, Category="Foe|Weapons")
	bool bLeftHanded = false;

	UFUNCTION()
	void OnRep_LeftHanded();

	/** Apparition dans les vagues : chance relative face aux autres NME de la règle de spawn (1 = normal ; 0.25 face à des NME à 1 = 1 sur 5) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Spawn", meta=(ClampMin="0"))
	float SpawnWeight = 1.0f;

	/** Map de test (GameMode avec Numpad Foe Spawn) : touche du pavé numérique (0 à 9) qui fait apparaître ce NME ; -1 = aucune */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Spawn", meta=(ClampMin="-1", ClampMax="9"))
	int32 NumpadSpawnKey = -1;

	/** Joueur actuellement ciblé (renseigné sur le serveur uniquement) */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Foe")
	TObjectPtr<APawn> Target;

public:

	AYSFoeCharacter();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/**
	 *  Pushback (serveur) : poussé dans la direction du coup (FPointDamageEvent::ShotDirection), avec une force selon la part
	 *  de sa vie retirée (réglages du personnage qui frappe, catégorie 3C|Pushback ; AYSPlayerCharacter::ComputePushbackForce)
	 */
	void ApplyPushback(float Damage, const FDamageEvent& DamageEvent, const AController* EventInstigator);

	/** Force du pushback que ce NME inflige à un joueur (réglages Foe|Pushback) */
	float ComputePushbackForce(float Damage, float TargetMaxHealth) const;

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override { TagContainer.AppendTags(GameplayTags); }

	UFUNCTION(BlueprintPure, Category="Foe|Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category="Foe|Level")
	int32 GetFoeLevel() const { return Level; }

	bool IsElite() const { return bIsElite; }

	float GetMaxHealth() const { return MaxHealth; }

	float GetSizeMultiplier() const { return SizeMultiplier; }

	float GetSpawnWeight() const { return SpawnWeight; }

	int32 GetNumpadSpawnKey() const { return NumpadSpawnKey; }

	float GetDamageMultiplier() const { return DamageMultiplier; }

	/** Attaque à distance de l'arme tenue (la première trouvée, main droite d'abord), ou nullptr : le NME est alors au corps à corps */
	UYSFoeRangedAction* GetRangedAction() const;

	/** Tir (NME shot) : début de la visée dans la direction de sa cible décalée de YawOffset degrés (serveur) ; tir à la fin de AimDuration */
	bool StartAim(const APawn* AimTarget, float AimDuration, float YawOffset);

	/** Point de départ d'une attaque à distance : Muzzle de l'arme qui la porte, sinon au-dessus du NME */
	FVector GetAttackOrigin(const UYSFoeRangedAction* Action) const;

	/** Moment visible d'une attaque à distance (serveur) : animations de l'arme ici et sur les clients (lancer, recul) */
	void NotifyRangedAttack(UYSFoeRangedAction* Action);

	virtual void OnConstruction(const FTransform& Transform) override;

	AYSWeapon* GetLeftHandWeapon() const;

	AYSWeapon* GetRightHandWeapon() const;

	bool IsLeftHanded() const { return bLeftHanded; }

	/** Nom affiché (repli sur le nom de la classe) */
	FText GetFoeName() const { return FoeName.IsEmpty() ? FText::FromString(GetClass()->GetName().Replace(TEXT("_C"), TEXT(""))) : FoeName; }

	/** XP donnée à l'équipe quand ce NME meurt */
	UFUNCTION(BlueprintPure, Category="Foe|Level")
	float GetExperienceReward() const { return BaseExperience + ExperiencePerLevel * Level; }

	/** Hurtbox : le corps */
	UPrimitiveComponent* GetHurtbox() const;

protected:

	/** Mort de l'ennemi (serveur) */
	virtual void Die();

	/** Deadzone : tombé sous le KillZ de la map (World Settings) -> mort (serveur ; rien chez les clients) */
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

	UFUNCTION()
	void OnRep_Health();

	/** Rafraîchit la barre de vie avec Health / MaxHealth */
	void UpdateHealthBar();

	/** Retourne le pawn joueur le plus proche (sur le plan horizontal), ou nullptr */
	APawn* FindNearestPlayer() const;

	/** Blesse les joueurs dont la hurtbox touche la Hitbox (serveur) */
	void ApplyContactDamage();

	/** Actions de mêlée (Auto swing) des armes tenues */
	void GetMeleeActions(TArray<UYSAutoSwingAbility*>& OutActions) const;

	/** NME armé : un joueur touche la hitbox -> l'attaque de l'arme part si elle est prête (serveur) */
	void TriggerMeleeAttack();

	/** Une attaque de mêlée est en cours sur cette machine (animation) */
	bool IsMeleeAttacking() const;

	/** Animation de l'attaque de mêlée sur toutes les machines (armes de NME locales, non répliquées) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastMeleeAttack();

	/** Skull Gun : tire la balle dans la direction de la visée (serveur) */
	void FireShot();

	/** Ligne de visée au sol sur toutes les machines (direction figée) */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastAimStarted(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastAimEnded();

	/** Animation de l'attaque à distance de l'arme sur les clients (armes de NME locales) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRangedAttack();

	/** Affiche (ou cache) la ligne de visée sur cette machine */
	void ShowAimLine(bool bShow, const FVector& Start, const FVector& Direction);


	/** Armes des mains selon le set (ou les armes par main) et gaucher / droitier */
	void ApplyWeaponClasses();

	/** Mains : position cible + oscillation, suivie avec retard ; rotation ; arme pouvant imposer sa pose (visuel, toutes machines) */
	void UpdateHands(float DeltaSeconds);

	FVector GetHandTargetLocation(bool bLeft) const;

	FRotator GetHandTargetRotation(bool bLeft) const;

private:

	/** Mains : position monde lissée, temps de l'oscillation, première image */
	FVector LeftHandWorldLocation = FVector::ZeroVector;
	FVector RightHandWorldLocation = FVector::ZeroVector;
	float HandSwayTime = 0.0f;
	bool bHandsInitialized = false;


	/** Recharge de la bombe depuis la dernière (serveur, plafonnée à ThrowInterval) */
	float ThrowTimer = 0.0f;

	/** Skull Gun : visée en cours (serveur), direction figée et temps restant */
	bool bAiming = false;
	FVector AimDirection = FVector::ForwardVector;
	float AimTimeLeft = 0.0f;


	/** Prochain instant (temps du monde) où chaque joueur touché peut reprendre des dégâts de contact */
	TMap<TWeakObjectPtr<AActor>, double> NextContactDamageTime;
};
