#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "Kismet/KismetMathLibrary.h"
#include "Upgrades/YSUpgrade.h"
#include "YSPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UYSFaceDecalComponent;
class UBlendSpace;
class UInputAction;
class UInputMappingContext;
class UChildActorComponent;
class AYSWeapon;
struct FInputActionValue;

/**
 *  Personnage joueur top-down, déplacé au clavier (ZQSD sur AZERTY, WASD sur QWERTY).
 *  Il regarde toujours vers le curseur (rotation de contrôle fournie par AYSPlayerController)
 *  et strafe : l'animation dépend de la direction du mouvement par rapport au regard.
 *  DA « primitives » : le corps est un cylindre (BodyMesh) avec un visage en decal (FaceDecal),
 *  portés par BodyPivot qui s'incline dans la direction du mouvement (penché autour des pieds).
 *  Une arme (AYSWeapon) par main via LeftHandWeaponClass / RightHandWeaponClass ; clic gauche = actions des deux mains
 *  (main principale = droite ; en akimbo la gauche suit après AkimboFireDelay).
 *  Déplacement et rotation sont répliqués par le CharacterMovementComponent (prédiction client incluse).
 *  Classe de base du Blueprint PlayerCharacter_mouss : les réglages se font dans le Blueprint.
 */
UCLASS(abstract, meta=(PrioritizeCategories="3C"))
class AYSPlayerCharacter : public ACharacter, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

private:

	/** Bras de caméra au-dessus du personnage (rotation absolue : ne tourne pas avec lui) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** Caméra top-down */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> TopDownCamera;

	/** Pivot visuel du corps : s'incline pendant le mouvement (corps, visage et pistolet le suivent) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> BodyPivot;

	/** Corps du personnage (cylindre par défaut) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** Visage projeté sur l'avant du corps (texture réglable via Face Texture sur le composant) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UYSFaceDecalComponent> FaceDecal;

	/** Main gauche : point d'attache (non mis à l'échelle) des armes / objets tenus à gauche */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> LeftHand;

	/** Main droite : point d'attache (non mis à l'échelle) des armes / objets tenus à droite */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> RightHand;

	/** Sphère visible de la main gauche */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> LeftHandMesh;

	/** Sphère visible de la main droite */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> RightHandMesh;

	/** Arme de la main gauche (acteur enfant de classe LeftHandWeaponClass) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> LeftHandWeapon;

	/** Arme de la main droite (acteur enfant de classe RightHandWeaponClass) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> RightHandWeapon;

	/** Action de déplacement (Axis2D), créée à l'exécution */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	/** Action de tir (clic gauche), créée à l'exécution */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FireAction;

	/** Contexte d'input clavier/souris par défaut, créé à l'exécution */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

protected:

	/** Nom affiché (menu de sélection du personnage, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C")
	FText CharacterName;

	/** Vitesse de déplacement maximale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Movement", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float MoveSpeed = 600.0f;

	/** Points de vie max de base (avant Hps bonus) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="3C|Health", meta=(ClampMin="1"))
	float MaxHealth = 20.0f;

	/** Points de vie actuels (gérés par le serveur) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="3C|Health")
	float Health = 0.0f;

	/** Hps regen : délai sans dégât subi avant que la regen démarre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Health", meta=(ClampMin="0", Units="s"))
	float HealthRegenDelay = 5.0f;

	/** Hps regen : intervalle entre deux soins */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Health", meta=(ClampMin="0.05", Units="s"))
	float HealthRegenInterval = 1.0f;

	/** Armor : dégâts minimum d'un coup après réduction par l'armure */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Health", meta=(ClampMin="0"))
	float MinDamageAfterArmor = 0.0f;

	/**
	 *  BlendSpace 2D de strafe (X = direction -180..180 relative au regard, Y = vitesse).
	 *  S'il est renseigné, il remplace l'Anim Blueprint du mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Animation")
	TObjectPtr<UBlendSpace> StrafeBlendSpace;

	/** Lissage des paramètres envoyés au BlendSpace (0 = aucun lissage) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Animation", meta=(ClampMin="0"))
	float StrafeBlendSmoothing = 10.0f;

	/** Inclinaison maximale du corps dans la direction du mouvement */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Lean", meta=(ClampMin="0", ClampMax="89", Units="Degrees"))
	float LeanMaxAngle = 30.0f;

	/** Durée de la transition vers une nouvelle inclinaison */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Lean", meta=(ClampMin="0", Units="Seconds"))
	float LeanDuration = 0.14f;

	/** Courbe de la transition */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Lean")
	TEnumAsByte<EEasingFunc::Type> LeanEasing = EEasingFunc::EaseIn;

	/** Exposant de la courbe (EaseIn / EaseOut / EaseInOut) : plus il est grand, plus le départ est lent */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Lean", meta=(ClampMin="1"))
	float LeanEaseExponent = 2.0f;

	/** Hauteur du sautillement du corps pendant le déplacement */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Hop", meta=(ClampMin="0", Units="Centimeters"))
	float HopHeight = 10.0f;

	/**
	 *  Durée d'un sautillement (montée + retour) quand le personnage avance à MoveSpeed.
	 *  Proportionnelle à la vitesse réelle : 2x plus lent à MoveSpeed / 2, 2x plus rapide à MoveSpeed x 2.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Hop", meta=(ClampMin="0.01", Units="Seconds"))
	float HopDuration = 0.2f;

	/** Courbe du sautillement */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Hop")
	TEnumAsByte<EEasingFunc::Type> HopEasing = EEasingFunc::EaseInOut;

	/** Exposant de la courbe (EaseIn / EaseOut / EaseInOut) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Hop", meta=(ClampMin="1"))
	float HopEaseExponent = 2.0f;

	/** Arme tenue dans la main gauche (clic gauche = son action) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Weapons")
	TSubclassOf<AYSWeapon> LeftHandWeaponClass;

	/** Arme tenue dans la main droite */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Weapons")
	TSubclassOf<AYSWeapon> RightHandWeaponClass;

	/**
	 *  Upgrades que ce personnage peut se voir proposer à chaque montée de niveau
	 *  (Data Assets d'upgrade, réglés une seule fois pour tous les personnages, dans /Game/Core/Upgrades).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Upgrades")
	TArray<TObjectPtr<UYSUpgrade>> UpgradePool;

	/** Progression de chaque variante d'upgrade obtenue (répliquée pour l'affichage du menu) */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Upgrades")
	TArray<FYSUpgradeProgress> UpgradeProgress;

	/** Multiplicateur des dégâts de l'arme de chaque main (Atk Bonus ; serveur) */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Upgrades")
	float RightHandDamageMultiplier = 1.0f;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Upgrades")
	float LeftHandDamageMultiplier = 1.0f;

	/** Multiplicateur de recharge de l'arme de chaque main (CD Reduction ; répliqué : le client prédit la recharge) */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Upgrades")
	float RightHandCooldownMultiplier = 1.0f;

	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Upgrades")
	float LeftHandCooldownMultiplier = 1.0f;

	/** Multiplicateur de la vitesse de déplacement (Move speed bonus ; répliqué : la prédiction de mouvement du client doit le connaître) */
	UPROPERTY(ReplicatedUsing=OnRep_MoveSpeedMultiplier, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Upgrades")
	float MoveSpeedMultiplier = 1.0f;

	/** Multiplicateur des PV max (Hps bonus) */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Upgrades")
	float MaxHealthMultiplier = 1.0f;

	/** PV rendus à chaque intervalle de regen (Hps regen ; 0 = pas de regen ; serveur) */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Upgrades")
	float HealthRegen = 0.0f;

	/** Réduction des dégâts de chaque coup subi (Armor ; serveur) */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Upgrades")
	float Armor = 0.0f;

	/** Dernier dégât subi (temps du monde, serveur) et temps accumulé vers le prochain soin */
	float LastDamageTime = -1.0e9f;
	float HealthRegenTimer = 0.0f;

	/** Regen de PV (serveur, Tick) */
	void UpdateHealthRegen(float DeltaSeconds);

	UFUNCTION()
	void OnRep_MoveSpeedMultiplier();

	/**
	 *  Position par défaut de la main droite, par rapport au corps (BodyPivot) ; la main gauche est symétrique (Y inversé).
	 *  Une arme peut la remplacer (Weapon > Hand Location).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Hands")
	FVector HandLocation = FVector(0.0f, 55.0f, -10.0f);

	/**
	 *  Rotation par défaut de la main droite, par rapport au corps ; la main gauche est symétrique (Yaw et Roll inversés).
	 *  Une arme peut la remplacer (Weapon > Hand Rotation).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Hands")
	FRotator HandRotation = FRotator::ZeroRotator;

	/** Écart maximal de l'oscillation des mains autour de leur position */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Hands", meta=(ClampMin="0", Units="Centimeters"))
	float HandSwayAmplitude = 5.0f;

	/** Vitesse de l'oscillation (bruit de Perlin) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Hands", meta=(ClampMin="0"))
	float HandSwaySpeed = 0.6f;

	/** Vitesse à laquelle une main pivote vers la cible de la visée assistée de son arme (0 = instantané) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Hands", meta=(ClampMin="0"))
	float HandAimSpeed = 15.0f;

	/** Réactivité des mains au mouvement du corps : plus petit = plus de retard (0 = aucun retard) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Hands", meta=(ClampMin="0"))
	float HandFollowSpeed = 12.0f;

	/** Les sphères des mains reprennent le matériau du corps (BodyMesh) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Hands")
	bool bHandsUseBodyMaterial = true;

	/** Tags portés par le personnage (camp, etc.), lus par les projectiles */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Tags")
	FGameplayTagContainer GameplayTags;

private:

	/** Position actuelle (lissée) dans le BlendSpace : X = direction, Y = vitesse */
	FVector2D StrafeBlendPosition = FVector2D::ZeroVector;

	/** Inclinaison en espace monde (X, Y) en degrés : direction = sens du penché, norme = angle */
	FVector2D CurrentLean = FVector2D::ZeroVector;

	/** Transition d'inclinaison en cours : départ, cible et progression (0..1) */
	FVector2D LeanStart = FVector2D::ZeroVector;
	FVector2D LeanTarget = FVector2D::ZeroVector;
	float LeanAlpha = 1.0f;

	/** Progression du sautillement en cours (0..1, 0 = au sol) */
	float HopPhase = 0.0f;

	/** Positions monde lissées des mains (retard par rapport au corps) */
	FVector LeftHandWorldLocation = FVector::ZeroVector;
	FVector RightHandWorldLocation = FVector::ZeroVector;
	bool bHandsInitialized = false;

	/** Temps écoulé pour le bruit d'oscillation des mains */
	float HandSwayTime = 0.0f;

	/** Yaw actuel (lissé, relatif au corps) de chaque main : vers la cible de visée assistée, sinon rotation par défaut */
	float LeftHandYaw = 0.0f;
	float RightHandYaw = 0.0f;

	/** Tir différé de la main droite en akimbo */
	FTimerHandle AkimboTimer;

public:

	AYSPlayerCharacter();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;

	USceneComponent* GetLeftHand() const { return LeftHand.Get(); }

	USceneComponent* GetRightHand() const { return RightHand.Get(); }

	virtual void Tick(float DeltaSeconds) override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override { TagContainer.AppendTags(GameplayTags); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Dégâts reçus (serveur) : PV, chiffre de dégâts chez tous les joueurs, barre de vie */
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category="3C|Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category="3C|Health")
	bool IsAlive() const { return Health > 0.0f; }

	UFUNCTION(BlueprintPure, Category="3C|Health")
	float GetMaxHealth() const { return MaxHealth * MaxHealthMultiplier; }

	/** PV max sans bonus */
	float GetBaseMaxHealth() const { return MaxHealth; }

	/** Vitesse de déplacement sans bonus */
	float GetBaseMoveSpeed() const { return MoveSpeed; }

	/** Move speed bonus (serveur ; répliqué) */
	void SetMoveSpeedMultiplier(float Multiplier);

	/** Hps bonus (serveur) : les PV max gagnés s'ajoutent aux PV actuels */
	void SetMaxHealthMultiplier(float Multiplier);

	/** Hps regen (serveur) */
	void SetHealthRegen(float Amount) { HealthRegen = Amount; }

	/** Armor (serveur) */
	void SetArmor(float Amount) { Armor = Amount; }

	/** Upgrades disponibles pour ce personnage */
	const TArray<TObjectPtr<UYSUpgrade>>& GetUpgradePool() const { return UpgradePool; }

	/** Progression de l'upgrade d'index PoolIndex (vide si jamais obtenue) */
	FYSUpgradeProgress GetUpgradeProgress(int32 PoolIndex, int32 Variant) const;

	/** Nombre total de bonus obtenus (toutes upgrades confondues) */
	int32 GetTotalUpgradeStacks() const;

	/**
	 *  Tire jusqu'à Count propositions différentes parmi les variantes d'upgrades encore proposables
	 *  (tirage pondéré par OfferWeight, sans remise), chacune avec sa rareté (vide si aucune).
	 */
	TArray<FYSUpgradeOffer> PickUpgradeOffers(int32 Count) const;

	/** Ajoute le bonus proposé à la progression et applique l'upgrade (serveur) */
	void ApplyUpgrade(const FYSUpgradeOffer& Offer);

	/** Multiplicateur de dégâts d'une main (Atk Bonus) */
	float GetHandDamageMultiplier(EYSHand Hand) const { return Hand == EYSHand::Left ? LeftHandDamageMultiplier : RightHandDamageMultiplier; }

	void SetHandDamageMultiplier(EYSHand Hand, float Multiplier) { (Hand == EYSHand::Left ? LeftHandDamageMultiplier : RightHandDamageMultiplier) = Multiplier; }

	void SetHandCooldownMultiplier(EYSHand Hand, float Multiplier) { (Hand == EYSHand::Left ? LeftHandCooldownMultiplier : RightHandCooldownMultiplier) = Multiplier; }

	/** Multiplicateur de recharge de la main qui tient Weapon (1 si elle n'est pas tenue) */
	float GetWeaponCooldownMultiplier(const AYSWeapon* Weapon) const;

	/** Multiplicateur de dégâts de la main qui tient Weapon (1 si elle n'est pas tenue) */
	float GetWeaponDamageMultiplier(const AYSWeapon* Weapon) const;

	/** Hurtbox : le corps (BodyMesh) */
	UPrimitiveComponent* GetHurtbox() const;

	UCameraComponent* GetTopDownCamera() const { return TopDownCamera.Get(); }

	USpringArmComponent* GetCameraBoom() const { return CameraBoom.Get(); }

	/** Arme actuellement tenue dans la main gauche / droite (nullptr si aucune) */
	UFUNCTION(BlueprintPure, Category="3C|Weapons")
	AYSWeapon* GetLeftHandWeapon() const;

	UFUNCTION(BlueprintPure, Category="3C|Weapons")
	AYSWeapon* GetRightHandWeapon() const;

	/** Nom affiché (repli sur le nom de la classe si CharacterName est vide) */
	FText GetCharacterName() const { return CharacterName.IsEmpty() ? FText::FromString(GetClass()->GetName().Replace(TEXT("_C"), TEXT(""))) : CharacterName; }

protected:

	/** Gère l'input de déplacement */
	void Move(const FInputActionValue& Value);

	/**
	 *  Clic gauche : déclenche l'action des armes des deux mains vers le curseur.
	 *  Akimbo (même arme dans les deux mains) : la gauche tout de suite, la droite après son AkimboFireDelay.
	 */
	void Fire();

	/** Point visé par le joueur local (souris) */
	FVector GetAimLocation() const;

	/** Met à jour la position du BlendSpace de strafe (sur toutes les machines) */
	void UpdateStrafeAnimation(float DeltaSeconds);

	/** Incline BodyPivot vers la direction du mouvement et le fait sautiller (visuel, sur toutes les machines) */
	void UpdateBodyPivot(float DeltaSeconds);

	/** Avance l'inclinaison lissée et retourne la rotation monde correspondante */
	FQuat UpdateLean(float DeltaSeconds, const FVector& Velocity);

	/** Avance le sautillement et retourne la hauteur actuelle */
	float UpdateHop(float DeltaSeconds, float Speed);

	/** Place les mains : position cible (défaut ou arme) + oscillation, suivie avec retard (visuel, toutes machines) */
	void UpdateHands(float DeltaSeconds);

	/** Position cible d'une main dans l'espace de BodyPivot (sans oscillation) */
	FVector GetHandTargetLocation(bool bLeft) const;

	/** Rotation cible d'une main, relative à BodyPivot */
	FRotator GetHandTargetRotation(bool bLeft) const;

	/** Cible de la visée assistée de l'arme d'une main (Coup d'Petoir), ou nullptr */
	const AActor* GetHandAssistTarget(bool bLeft) const;

	/** PV à 0 (serveur) : PlayerState mort, caméra laissée sur place, personnage détruit */
	virtual void Die();

private:

	/** Crée les actions et le contexte ZQSD/WASD + flèches + clic gauche */
	void CreateDefaultInput();
};
