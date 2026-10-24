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
class UYSWeaponSet;
class AYSInteractiveObject;
class UTexture2D;
class UWidgetComponent;
class UMaterialInterface;
class AYSFoeCharacter;
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

	/** Pseudo et PV au-dessus du personnage, visibles pour les autres joueurs (le joueur local les a dans le HUD) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UWidgetComponent> Nameplate;

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

	/** Action d'interaction (touche E), créée à l'exécution */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;

	/** Contexte d'input clavier/souris par défaut, créé à l'exécution */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

protected:

	/** Nom affiché (menu de sélection du personnage, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C")
	FText CharacterName;

	/** Personnage unique : un seul joueur de la partie peut le choisir (ex. Quartermaster, Captain) ; grisé pour les autres */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C")
	bool bUniqueCharacter = false;

	/** Taille du personnage : échelle de tout l'acteur (capsule, corps, mains, armes) ; 1.2 = 20 % plus grand ; x Character Scale Multiplier du set d'arme */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C", meta=(ClampMin="0.1"))
	float CharacterScale = 1.0f;

	/** Vitesse de déplacement maximale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Movement", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float MoveSpeed = 600.0f;

	/** Points de vie max de base (avant Hps bonus) ; remplacés par ceux du set d'arme s'il en fixe (Weapon Set > Health) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="3C|Health", meta=(ClampMin="1"))
	float MaxHealth = 20.0f;

	/** Points de vie actuels (gérés par le serveur) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="3C|Health")
	float Health = 0.0f;

	/** Hps regen de base : PV rendus à chaque intervalle de regen, sans upgrade (les upgrades Hps regen s'y ajoutent) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Health", meta=(ClampMin="0"))
	float BaseHealthRegen = 1.0f;

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

	/** Set d'arme du personnage (Data Asset : arme de chaque main, /Game/Core/WeaponSets) ; prioritaire sur les armes par main ci-dessous */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_WeaponSet, Category="3C|Weapons")
	TObjectPtr<UYSWeaponSet> WeaponSet;

	/** Clients : la taille dépend du set (Character Scale Multiplier) */
	UFUNCTION()
	void OnRep_WeaponSet();

	/** Échelle ABSOLUE de l'acteur = échelle de la classe x Character Scale x Character Scale Multiplier du set */
	void ApplyCharacterScale();

	/** Sans set d'arme (tests) : arme tenue dans la main gauche */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Weapons", meta=(EditCondition="WeaponSet == nullptr"))
	TSubclassOf<AYSWeapon> LeftHandWeaponClass;

	/** Sans set d'arme (tests) : arme tenue dans la main droite */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Weapons", meta=(EditCondition="WeaponSet == nullptr"))
	TSubclassOf<AYSWeapon> RightHandWeaponClass;

	/**
	 *  Upgrades que ce personnage peut se voir proposer à chaque montée de niveau
	 *  (Data Assets d'upgrade, réglés une seule fois pour tous les personnages, dans /Game/Core/Upgrades).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Upgrades")
	TArray<TObjectPtr<UYSUpgrade>> UpgradePool;

	/** Chances (%) de chaque rareté sans Luck (la rareté de chaque proposition est tirée en premier) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Upgrades", meta=(ClampMin="0"))
	float BronzeChance = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Upgrades", meta=(ClampMin="0"))
	float SilverChance = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Upgrades", meta=(ClampMin="0"))
	float GoldChance = 10.0f;

	/** Composant qui reçoit la couleur choisie à l'écran de sélection (avec tous ses sous-composants) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Customization")
	FName CustomizationColorComponentName = TEXT("Head Feature");

	/** Gaucher (choisi à l'écran de sélection) : miroir du personnage, réglé en droitier dans son Blueprint */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Customization")
	bool bLeftHanded = false;

	/** Personnalisation appliquée (couleur et visage trouvés) */
	bool bCustomizationApplied = false;

	/** Post-process de la nuit (appliqué à la caméra du joueur local, mélangé selon le jour / la nuit) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|DayNight")
	FPostProcessSettings NightPostProcess;

	/** Durée de la transition jour <-> nuit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|DayNight", meta=(ClampMin="0", Units="s"))
	float DayNightBlendDuration = 2.0f;

	/** Poids actuel du post-process de nuit (0 jour, 1 nuit) */
	float NightBlend = 0.0f;

	/** Progression de chaque variante d'upgrade obtenue (répliquée pour l'affichage du menu) */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Upgrades")
	TArray<FYSUpgradeProgress> UpgradeProgress;

	/**
	 *  Multiplicateurs par type d'action, pour tout le set d'arme (index = GetSlotIndex : 0 Ability, 1 Auto atk) :
	 *  dégâts (Damage bonus ; serveur) et recharge (CD reduction ; répliqués : le client prédit la recharge)
	 */
	UPROPERTY(Transient, VisibleInstanceOnly, Category="3C|Upgrades")
	float DamageMultipliers[2];

	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, Category="3C|Upgrades")
	float CooldownMultipliers[2];

	/** Propriétés données aux armes tenues par les upgrades d'arme (ex. Ricochets ; serveur) */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Upgrades")
	TMap<FName, float> WeaponStats;

	/** Coup critique : dégâts x ce multiplicateur (Golden bloodlust, Atk critique), augmenté par les bonus critiques d'Atk speed / Shoot system */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="3C|Combat", meta=(ClampMin="1"))
	float CriticalDamageMultiplier = 1.5f;

	/** Somme des bonus aux dégâts critiques des upgrades (Atk speed / Shoot system ; serveur, répliquée pour l'affichage des upgrades) */
	UPROPERTY(Transient, Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Combat")
	float CriticalDamageBonus = 0.0f;

	/** Somme des bonus de l'upgrade Dégât critique, ajoutée au multiplicateur de base (1.5 + 0.9 = x2.4 ; serveur, répliquée pour l'affichage) */
	UPROPERTY(Transient, Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Combat")
	float CriticalDamageAdd = 0.0f;

	/** Atk critique : attaques auto Swing lancées par le personnage, toutes armes confondues (épées akimbo comptées ensemble ; serveur) */
	int32 AutoSwingCounter = 0;

	/** Atk critique : une attaque auto Swing sur N est critique (0 = pas l'upgrade ; serveur) */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Combat")
	int32 CriticalSwingInterval = 0;

	/** Atk critique : une balle PewPew sur N reçue par un même NME est critique (0 = pas l'upgrade ; serveur) */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Combat")
	int32 CriticalBulletInterval = 0;

	/** Atk critique : une AMMO de Shotgun est critique sur un NME à moins de cette distance du personnage (0 = pas l'upgrade ; serveur) */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Combat")
	float CriticalShotgunMaxDistance = 0.0f;

	/** Atk critique : une balle de Sniper est critique sur un NME à plus de cette distance du personnage (0 = pas l'upgrade ; serveur) */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Combat")
	float CriticalSniperMinDistance = 0.0f;

	/**
	 *  Pushback des NME touchés par ce personnage, proportionnel à la part de leur vie retirée par le coup
	 *  (dégâts / PV max de la cible) : sous Min Damage Ratio rien, à partir de Max Damage Ratio la force max,
	 *  entre les deux la force passe linéairement de Min Force à Max Force. Direction = celle du coup (hitbox, projectile).
	 *  Remplacés par ceux du set d'arme s'il en fixe (Weapon Set > Pushback).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="3C|Pushback", meta=(ClampMin="0", ClampMax="1"))
	float PushbackMinDamageRatio = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="3C|Pushback", meta=(ClampMin="0", ClampMax="1"))
	float PushbackMaxDamageRatio = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="3C|Pushback", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float PushbackMinForce = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="3C|Pushback", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float PushbackMaxForce = 500.0f;

	/** Vitesse verticale donnée en plus à chaque pushback (le NME décolle un peu du sol : moins de friction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="3C|Pushback", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float PushbackVerticalForce = 50.0f;

	/**
	 *  Bloodlust (upgrade d'arme des épées ; serveur, répliqué pour l'UI et la prédiction de la recharge) :
	 *  BL stacks gagnées par les Auto swing qui touchent, perdues toutes d'un coup après BloodlustDuration sans toucher.
	 *  BloodlustMaxStacks 0 = Bloodlust pas débloqué.
	 */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Bloodlust")
	int32 BloodlustStacks = 0;

	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Bloodlust")
	int32 BloodlustMaxStacks = 0;

	/** Durée sans toucher avant de perdre les stacks (reprise à chaque stack gagnée ou coup au maximum) */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Bloodlust")
	float BloodlustDuration = 3.0f;

	/** Vitesse d'atk par stack (0.05 = 5 % ; + Bloodlust Atk speed) */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Bloodlust")
	float BloodlustAtkSpeedPerStack = 0.05f;

	/** Golden bloodlust : Auto swing critiques au maximum de stacks */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Bloodlust")
	bool bGoldenBloodlust = false;

	/** Temps serveur où les stacks sont perdues */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Bloodlust")
	double BloodlustExpireServerTime = 0.0;

	/**
	 *  BloodRage (upgrade d'arme de la GreatAxe ; serveur, répliqué pour l'UI) : stacks gagnées à chaque coup reçu, toutes perdues
	 *  après BloodRageDuration sans subir de coup (jamais avec Golden BloodRage) ; au maximum, la prochaine attaque est critique
	 *  et consomme les stacks. Armure + BloodRageArmorPerStack par stack. BloodRageMaxStacks 0 = pas débloquée.
	 */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|BloodRage")
	int32 BloodRageStacks = 0;

	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|BloodRage")
	int32 BloodRageMaxStacks = 0;

	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|BloodRage")
	float BloodRageDuration = 10.0f;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|BloodRage")
	float BloodRageArmorPerStack = 0.0f;

	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|BloodRage")
	bool bGoldenBloodRage = false;

	/** Temps serveur où les stacks sont perdues */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|BloodRage")
	double BloodRageExpireServerTime = 0.0;

	/** Upgrade Shoot system par type d'action (Guns ; serveur) : PewPew = balles ajoutées à chaque rafale */
	UPROPERTY(Transient, VisibleInstanceOnly, Category="3C|Upgrades")
	float ShootSystemBonus[2];

	/** Upgrade Summon system (armes d'invocation, ex. Crochet du capitaine) : minions max en plus (répliqué : le client vérifie s'il peut invoquer) */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, Category="3C|Upgrades")
	float SummonSystemBonus = 0.0f;

	/** NME marqué par les balles de ce joueur (Captain AMMO) : un seul à la fois, marquage au sol sous lui sur toutes les machines */
	UPROPERTY(ReplicatedUsing=OnRep_MarkedFoe, Transient, VisibleInstanceOnly, Category="3C|Combat")
	TObjectPtr<AYSFoeCharacter> MarkedFoe;

	UFUNCTION()
	void OnRep_MarkedFoe();

	/** NME dont cette machine affiche la marque (pour l'enlever quand elle change) */
	TWeakObjectPtr<AYSFoeCharacter> DisplayedMarkedFoe;

	/** Affiche la marque sur MarkedFoe (et l'enlève de l'ancien) sur cette machine */
	void UpdateMarkDisplay();

	/** Effets de la marque actuelle (serveur) : voir SetMarkedFoe */
	bool bMarkMinionsCritical = false;
	bool bMarkSpawnsMinion = false;

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

	/** Luck : points de % de chance déplacés du Bronze vers le Gold (répliquée : affichage et debug côté client) */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Upgrades")
	float Luck = 0.0f;

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

	/** Interaction (E) : distance max (au sol) d'un objet devant le personnage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Interaction", meta=(ClampMin="0", Units="Centimeters"))
	float InteractionDistance = 200.0f;

	/** Interaction : écart d'angle max (de chaque côté) entre le regard et l'objet */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Interaction", meta=(ClampMin="0", ClampMax="180", Units="Degrees"))
	float InteractionAngle = 70.0f;

	/** Objet porté à deux mains (coffre) : armes désactivées et cachées, mains sur l'objet */
	UPROPERTY(ReplicatedUsing=OnRep_CarriedObject, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Interaction")
	TObjectPtr<AYSInteractiveObject> CarriedObject;

	UFUNCTION()
	void OnRep_CarriedObject();

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

	/** Dernière image où le joueur a appuyé sur une direction (déplacement forcé de la roulade sinon) */
	uint64 LastMoveInputFrame = 0;

	/** Direction de l'animation de roulade (dernière direction de mouvement pendant la roulade) */
	FVector RollAnimDirection = FVector::ForwardVector;

	/** Yaw actuel (lissé, relatif au corps) de chaque main : vers la cible de visée assistée, sinon rotation par défaut */
	float LeftHandYaw = 0.0f;
	float RightHandYaw = 0.0f;

	/** Tir différé de la main droite en akimbo */
	FTimerHandle AkimboTimer;

	/** Objet dont l'invite « E » est affichée (joueur local) */
	TWeakObjectPtr<AYSInteractiveObject> FocusedInteractable;

public:

	AYSPlayerCharacter();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void PossessedBy(AController* NewController) override;

	virtual void OnRep_PlayerState() override;

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
	float GetMaxHealth() const { return GetBaseMaxHealth() * MaxHealthMultiplier; }

	/** PV max sans bonus : ceux du set d'arme s'il les fixe, sinon Max Health du personnage */
	float GetBaseMaxHealth() const;

	/** Vitesse de déplacement sans bonus */
	float GetBaseMoveSpeed() const { return MoveSpeed; }

	/** Move speed bonus (serveur ; répliqué) */
	void SetMoveSpeedMultiplier(float Multiplier);

	/** Hps bonus (serveur) : les PV max gagnés s'ajoutent aux PV actuels */
	void SetMaxHealthMultiplier(float Multiplier);

	/** Hps regen (serveur) */
	void SetHealthRegen(float Amount) { HealthRegen = Amount; }

	float GetBaseHealthRegen() const { return BaseHealthRegen; }

	/** Armor (serveur) */
	void SetArmor(float Amount) { Armor = Amount; }

	/** Luck (serveur ; répliquée) : points de % de chance déplacés du Bronze vers le Gold */
	void SetLuck(float Amount) { Luck = Amount; }

	float GetLuck() const { return Luck; }

	/** Chance (%) de tirer une rareté sans Luck */
	float GetBaseRarityChance(EYSUpgradeRarity Rarity) const;

	/** Chance (%) de tirer une rareté avec la Luck actuelle : Bronze - Luck (min 0), Silver inchangé, Gold + ce que perd le Bronze */
	float GetRarityChance(EYSUpgradeRarity Rarity) const;

	/** Upgrades disponibles pour ce personnage */
	const TArray<TObjectPtr<UYSUpgrade>>& GetUpgradePool() const { return UpgradePool; }

	/** Progression de l'upgrade d'index PoolIndex (vide si jamais obtenue) */
	FYSUpgradeProgress GetUpgradeProgress(int32 PoolIndex, int32 Variant) const;

	/** Nombre total de bonus obtenus (toutes upgrades confondues) */
	int32 GetTotalUpgradeStacks() const;

	/**
	 *  Tire jusqu'à Count propositions différentes parmi les variantes d'upgrades encore proposables
	 *  (sans remise) : rareté tirée d'abord (GetRarityChance, Luck), puis upgrade de cette rareté pondérée par OfferWeight (vide si aucune).
	 */
	TArray<FYSUpgradeOffer> PickUpgradeOffers(int32 Count) const;

	/** Propositions de classe : toutes les upgrades de classe proposables du pool (vide si aucune) */
	TArray<FYSUpgradeOffer> PickClassOffers() const;

	/**
	 *  Remplace ce personnage par un NewClass au même endroit (serveur) : même contrôleur, mêmes upgrades
	 *  (réappliquées sur le nouveau personnage, y compris sur ses nouvelles armes), même proportion de PV.
	 */
	void ChangeClass(TSubclassOf<AYSPlayerCharacter> NewClass, UYSWeaponSet* NewWeaponSet = nullptr);

	/** Upgrade de classe : même personnage avec un autre set d'arme (serveur ; respawn comme ChangeClass, upgrades et PV gardés) */
	void ChangeWeaponSet(UYSWeaponSet* NewWeaponSet) { ChangeClass(GetClass(), NewWeaponSet); }

	/** Applique la personnalisation de son joueur (couleur, visage ; AYSPlayerState) ; minion : couleur de son Captain */
	virtual void RefreshCustomization();

	/** Applique un matériau au composant CustomizationColorComponentName et à ses sous-composants, et un visage (nullptr = inchangé) */
	void ApplyCustomization(UMaterialInterface* ColorMaterial, UTexture2D* FaceTexture);

	/** Reprend la progression d'upgrades d'un autre personnage (OldPool = son pool) et la réapplique (serveur) */
	void RestoreUpgrades(const TArray<TObjectPtr<UYSUpgrade>>& OldPool, const TArray<FYSUpgradeProgress>& OldProgress);

	/** Ajoute le bonus proposé à la progression et applique l'upgrade (serveur) */
	void ApplyUpgrade(const FYSUpgradeOffer& Offer);

	/** Index des multiplicateurs pour un type d'action */
	static int32 GetSlotIndex(EYSActionSlot Slot) { return Slot == EYSActionSlot::AutoAtk ? 1 : 0; }

	/** Damage bonus / CD reduction d'un type d'action, pour toutes les armes du set (serveur) */
	void SetDamageMultiplier(EYSActionSlot Slot, float Multiplier) { DamageMultipliers[GetSlotIndex(Slot)] = Multiplier; }

	void SetCooldownMultiplier(EYSActionSlot Slot, float Multiplier) { CooldownMultipliers[GetSlotIndex(Slot)] = Multiplier; ForceNetUpdate(); }

	/** Shoot system d'un type d'action, pour tous les Guns du set (serveur) */
	void SetShootSystemBonus(EYSActionSlot Slot, float Bonus) { ShootSystemBonus[GetSlotIndex(Slot)] = Bonus; }

	/** Damage bonus du type d'action pour tout le set (1 sans bonus) ; ex. minions du Captain : celui de son Ability */
	float GetSlotDamageMultiplier(EYSActionSlot Slot) const { return DamageMultipliers[GetSlotIndex(Slot)]; }

	/** Summon system : minions max en plus (serveur, répliqué) */
	void SetSummonSystemBonus(float Bonus) { SummonSystemBonus = Bonus; ForceNetUpdate(); }

	float GetSummonSystemBonus() const { return SummonSystemBonus; }

	/**
	 *  Captain AMMO / Captain Sword : marque ce NME (la marque quitte l'ancien ; serveur). Effets de la marque (ceux de la dernière arme
	 *  qui a marqué) : bMinionsCritical = les minions font des critiques sur lui (Captain Sword) ; bSpawnsMinion = à sa mort, un minion
	 *  apparaît là où il meurt s'il reste de la place (Cursed Captain Gun).
	 */
	void SetMarkedFoe(AYSFoeCharacter* Foe, bool bMinionsCritical = false, bool bSpawnsMinion = false);

	AYSFoeCharacter* GetMarkedFoe() const { return MarkedFoe; }

	/** Les minions de ce Captain font des critiques sur son NME marqué (serveur) */
	bool IsMarkCriticalForMinions() const { return bMarkMinionsCritical; }

	/** Le NME marqué meurt (serveur, AYSFoeCharacter::Die) : Cursed Captain Gun = un minion apparaît à sa place */
	void OnMarkedFoeKilled(const AYSFoeCharacter* Foe);

	/** Un minion apparaît là où est ce NME, s'il reste de la place (serveur ; Cursed Captain AMMO : NME marqué mort, NME tué par la balle) */
	void SpawnMinionAtFoe(const AYSFoeCharacter* Foe, const TCHAR* Reason);

	/** Coups toujours critiques sur cette cible (ex. minion du Captain sur le NME marqué par la Captain Sword ; serveur) */
	virtual bool HasForcedCriticalAgainst(const AActor* Target) const { return false; }

	/**
	 *  « Allez-y mes minions ! » (serveur) : force l'Ability du clic (actions de clic des deux mains, comme le clic gauche), vers le
	 *  curseur de la machine qui contrôle le personnage (RPC au client) ; priorité sur tout, recharge ignorée (Forced).
	 */
	void ForceAbility(const FYSForcedActivation& Forced);

	/** Bonus Shoot system pour Weapon (0 si elle n'est pas tenue) */
	float GetWeaponShootSystemBonus(const AYSWeapon* Weapon, EYSActionSlot Slot) const;

	/** Propriété des armes tenues (upgrades d'arme ; serveur) */
	void SetWeaponStat(FName Stat, float Value) { WeaponStats.Add(Stat, Value); }

	/** Propriété d'arme de Weapon (Default si absente ou arme pas tenue) */
	float GetWeaponStat(const AYSWeapon* Weapon, FName Stat, float Default) const;

	/** Multiplicateurs d'un type d'action pour Weapon (1 si elle n'est pas tenue) */
	float GetWeaponCooldownMultiplier(const AYSWeapon* Weapon, EYSActionSlot Slot) const;

	/** Bloodlust réglé par l'upgrade (serveur) : MaxStacks 0 = désactivé (stacks remises à 0) */
	void SetBloodlust(int32 MaxStacks, float Duration, float AtkSpeedPerStack, bool bGolden);

	/** Une Auto swing a touché (serveur) : +1 stack (max BloodlustMaxStacks) et le délai repart */
	void AddBloodlustStack();

	/** Debug (serveur) : stacks fixées (bornées au maximum) et délai relancé */
	void DebugSetBloodlustStacks(int32 Stacks);

	int32 GetBloodlustStacks() const { return BloodlustStacks; }
	int32 GetBloodlustMaxStacks() const { return BloodlustMaxStacks; }

	/** Part restante du délai avant la perte des stacks (0..1 ; 0 sans stack) */
	float GetBloodlustTimeRatio() const;

	/** Recharge et retour de l'Auto swing x ce facteur (appliqué après l'Atk speed) : 1 - stacks x vitesse par stack */
	float GetBloodlustCooldownFactor() const;

	/** Golden bloodlust et toutes les stacks : les Auto swing sont critiques */
	bool IsBloodlustCritical() const { return bGoldenBloodlust && BloodlustMaxStacks > 0 && BloodlustStacks >= BloodlustMaxStacks; }

	/** BloodRage réglée par l'upgrade (serveur) : MaxStacks 0 = désactivée (stacks remises à 0) */
	void SetBloodRage(int32 MaxStacks, float Duration, float ArmorPerStack, bool bGolden);

	/** Coup reçu (serveur) : +1 stack de rage (max BloodRageMaxStacks) et le délai repart */
	void AddBloodRageStack();

	/** Au maximum de rage : vrai (attaque critique) et les stacks sont consommées (serveur, au début d'une attaque) */
	bool ConsumeBloodRageCritical();

	int32 GetBloodRageStacks() const { return BloodRageStacks; }
	int32 GetBloodRageMaxStacks() const { return BloodRageMaxStacks; }

	/** Part du délai restant avant de perdre la rage (1 = vient de prendre un coup ; Golden BloodRage : toujours 1) */
	float GetBloodRageTimeRatio() const;

	/** Prochaine attaque critique (rage au maximum) */
	bool IsBloodRageReady() const { return BloodRageMaxStacks > 0 && BloodRageStacks >= BloodRageMaxStacks; }

	/** Debug : fixe les stacks de rage (serveur) */
	void DebugSetBloodRageStacks(int32 Stacks);

	/** Dégâts des coups critiques : (Critical Damage Multiplier + Dégât critique) x (1 + bonus critiques des upgrades Atk speed / Shoot system) */
	virtual float GetCriticalDamageMultiplier() const { return (CriticalDamageMultiplier + CriticalDamageAdd) * (1.0f + CriticalDamageBonus); }

	/** Upgrade Dégât critique : somme de ses bonus (serveur) */
	void SetCriticalDamageAdd(float Value);
	float GetCriticalDamageAdd() const { return CriticalDamageAdd; }

	/** Le personnage peut faire des coups critiques (Atk critique ou Golden bloodlust) */
	bool CanCriticalHit() const { return CriticalSwingInterval > 0 || CriticalBulletInterval > 0 || CriticalShotgunMaxDistance > 0.0f || CriticalSniperMinDistance > 0.0f || bGoldenBloodlust || BloodRageMaxStacks > 0; }

	/** Atk critique : une attaque auto Swing de plus pour le personnage (serveur), renvoie le total */
	int32 RegisterAutoSwing() { return ++AutoSwingCounter; }

	float GetBaseCriticalDamageMultiplier() const { return CriticalDamageMultiplier; }

	float GetCriticalDamageBonus() const { return CriticalDamageBonus; }

	/** Somme des bonus critiques de toutes les upgrades de stat obtenues (chaque bonus selon sa rareté ; serveur) */
	void RecomputeCriticalDamageBonus();

	/**
	 *  Atk critique : une attaque auto Swing sur SwingInterval, une balle PewPew sur BulletInterval par NME,
	 *  Shotgun sur les NME à moins de ShotgunMaxDistance, Sniper sur les NME à plus de SniperMinDistance (0 = non ; serveur)
	 */
	void SetAutoAttackCriticals(int32 SwingInterval, int32 BulletInterval, float ShotgunMaxDistance, float SniperMinDistance)
	{
		CriticalSwingInterval = SwingInterval;
		CriticalBulletInterval = BulletInterval;
		CriticalShotgunMaxDistance = ShotgunMaxDistance;
		CriticalSniperMinDistance = SniperMinDistance;
	}

	int32 GetCriticalSwingInterval() const { return CriticalSwingInterval; }

	int32 GetCriticalBulletInterval() const { return CriticalBulletInterval; }

	float GetCriticalShotgunMaxDistance() const { return CriticalShotgunMaxDistance; }

	float GetCriticalSniperMinDistance() const { return CriticalSniperMinDistance; }

	/** Force du pushback d'un coup de Damage sur une cible de TargetMaxHealth PV max (0 = aucun) */
	float ComputePushbackForce(float Damage, float TargetMaxHealth) const;

	/** Vitesse verticale ajoutée aux pushbacks de ce personnage (set d'arme s'il fixe le pushback) */
	float GetPushbackVerticalForce() const;

	/**
	 *  Pushback reçu d'un NME (serveur), comme celui des NME frappés par le joueur : direction du coup (FPointDamageEvent),
	 *  force selon la part de vie retirée, réglée sur le NME qui frappe (Foe|Pushback)
	 */
	void ApplyPushback(float Damage, const FDamageEvent& DamageEvent, const AController* EventInstigator, const AActor* DamageCauser);

	/** Pushback : même impulsion sur le client qui contrôle ce personnage (sinon sa prédiction l'annule) */
	UFUNCTION(Client, Reliable)
	void ClientLaunch(FVector_NetQuantize10 LaunchVelocity);

	/** A déjà au moins un bonus de cette upgrade (n'importe quelle variante) */
	bool HasUpgrade(const UYSUpgrade* Upgrade) const;

	/** Damage bonus des actions de cette arme (minion : Damage bonus de l'Ability de son Captain) */
	virtual float GetWeaponDamageMultiplier(const AYSWeapon* Weapon, EYSActionSlot Slot) const;

	/** Hurtbox : le corps (BodyMesh) */
	UPrimitiveComponent* GetHurtbox() const;

	UCameraComponent* GetTopDownCamera() const { return TopDownCamera.Get(); }

	USpringArmComponent* GetCameraBoom() const { return CameraBoom.Get(); }

	/** Arme actuellement tenue dans la main gauche / droite (nullptr si aucune) */
	UFUNCTION(BlueprintPure, Category="3C|Weapons")
	AYSWeapon* GetLeftHandWeapon() const;

	UFUNCTION(BlueprintPure, Category="3C|Weapons")
	AYSWeapon* GetRightHandWeapon() const;

	/** Armes tenues (main droite puis gauche, mains vides ignorées) */
	TArray<AYSWeapon*> GetHeldWeapons() const;

	UYSWeaponSet* GetWeaponSet() const { return WeaponSet; }

	/** Invulnérable en ce moment (action en cours : roulade du Rolling shot…) */
	bool IsInvulnerable() const;

	/** Vitesse de marche sans les multiplicateurs des actions (MoveSpeed x Move speed bonus x objet porté) */
	float GetUnmodifiedWalkSpeed() const;

	/** Une action impose sa trajectoire (Dashing Slash) : ZQSD ignoré */
	bool IsMoveInputOverridden() const;

	/** Une action fige le regard du personnage (Dashing Slash) : le curseur n'est plus suivi */
	bool GetLockedAimDirection(FVector& OutDirection) const;

	/** Appelle Function pour chaque action des armes tenues */
	void ForEachHeldAction(TFunctionRef<void(const UYSAbilityComponent*)> Function) const;

	/** Akimbo : la même arme dans les deux mains (recharge unique) */
	bool IsAkimbo() const;

	/** Debug (serveur) : change l'arme d'une main en cours de partie */
	void DebugEquipWeapon(EYSHand Hand, UClass* WeaponClass) { (Hand == EYSHand::Left ? LeftHandWeapon : RightHandWeapon)->SetChildActorClass(WeaponClass); }

	/** Gaucher : armes échangées entre les mains (miroir), main principale = gauche. À régler avant FinishSpawning (OnConstruction) */
	void SetLeftHanded(bool bNewLeftHanded) { bLeftHanded = bNewLeftHanded; }
	bool IsLeftHanded() const { return bLeftHanded; }

	/** Arme de la main principale (droite, gauche pour un gaucher) et de l'autre main */
	AYSWeapon* GetMainHandWeapon() const { return bLeftHanded ? GetLeftHandWeapon() : GetRightHandWeapon(); }
	AYSWeapon* GetOffHandWeapon() const { return bLeftHanded ? GetRightHandWeapon() : GetLeftHandWeapon(); }

	/** Déclenche les actions de clic des deux mains (comme le clic gauche ; debug YSFire) */
	void TriggerFire() { Fire(); }

	/** Comme la touche E (debug YSInteract) */
	void TriggerInteract() { Interact(); }

	USceneComponent* GetBodyPivot() const { return BodyPivot.Get(); }

	/** Objet porté à deux mains (nullptr si aucun) */
	AYSInteractiveObject* GetCarriedObject() const { return CarriedObject; }

	bool IsCarrying() const { return CarriedObject != nullptr; }

	/** Prend l'objet dans ses mains (serveur) : actions en cours interrompues, armes cachées, plus lent */
	void StartCarrying(AYSInteractiveObject* Object);

	/** Repose l'objet porté au sol devant lui (serveur) ; près d'un Brooker, il est racheté */
	void DropCarriedObject(const TCHAR* Reason = TEXT(""));

	/** Choix de cible des NME : distance divisée par ce nombre (objet porté ; 1 sinon) */
	float GetFoeAttractionMultiplier() const;

	/** Objet interactif le plus proche devant le personnage, avec lequel il peut interagir (nullptr si aucun) */
	AYSInteractiveObject* FindInteractTarget() const;

	/** Nom affiché (repli sur le nom de la classe si CharacterName est vide) */
	FText GetCharacterName() const { return CharacterName.IsEmpty() ? FText::FromString(GetClass()->GetName().Replace(TEXT("_C"), TEXT(""))) : CharacterName; }

	bool IsUniqueCharacter() const { return bUniqueCharacter; }

protected:

	/** Gère l'input de déplacement */
	void Move(const FInputActionValue& Value);

	/**
	 *  Clic gauche : déclenche l'action des armes des deux mains vers le curseur.
	 *  Akimbo (même arme dans les deux mains) : la gauche tout de suite, la droite après son AkimboFireDelay.
	 */
	void Fire();

	/** Comme Fire, mais en activation forcée accordée par ForceAbility (machine qui contrôle le personnage) */
	void ForceFire();

	/** Activation forcée accordée par le serveur : le client la lance vers son curseur */
	UFUNCTION(Client, Reliable)
	void ClientForceAbility(FYSForcedActivation Forced);

	/** Touche E : repose l'objet porté, sinon interagit avec l'objet devant le personnage */
	void Interact();

	UFUNCTION(Server, Reliable)
	void ServerInteract();

	/** Interaction côté serveur */
	void InteractOnServer();

	/** MaxWalkSpeed = MoveSpeed x Move speed bonus x objet porté (serveur et client : prédiction) */
	void UpdateMaxWalkSpeed();

	/** Armes cachées pendant le portage (toutes machines) */
	void ApplyCarryVisuals();

	/** Couleur des armes (composant Coloration) : main principale, autre main, ou set d'upgrade de classe ; local */
	void UpdateWeaponColoration();

	/** Invite « E » sur l'objet devant le joueur local */
	void UpdateInteractPrompt();

	/** Point visé par le joueur local (souris) ; minion : sa cible */
	virtual FVector GetAimLocation() const;

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

	/** Mélange le post-process de nuit sur la caméra (joueur local) */
	void UpdateDayNight(float DeltaSeconds);

	/** Plaque pseudo + PV : cachée pour le joueur local et l'aperçu de l'écran de sélection */
	void UpdateNameplate();

	/** Position cible d'une main dans l'espace de BodyPivot (sans oscillation) */
	FVector GetHandTargetLocation(bool bLeft) const;

	/** Rotation cible d'une main, relative à BodyPivot */
	FRotator GetHandTargetRotation(bool bLeft) const;

	/** Cible de la visée assistée de l'arme d'une main (Coup d'Petoir), ou nullptr */
	const AActor* GetHandAssistTarget(bool bLeft) const;

	/** PV à 0 (serveur) : PlayerState mort, caméra laissée sur place, personnage détruit */
	virtual void Die();

	/** Deadzone : tombé sous le KillZ de la map (World Settings) -> mort (serveur ; rien chez les clients) */
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

private:

	/** Crée les actions et le contexte ZQSD/WASD + flèches + clic gauche */
	void CreateDefaultInput();
};
