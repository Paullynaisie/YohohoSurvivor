#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "Kismet/KismetMathLibrary.h"
#include "Upgrades/YSUpgrade.h"
#include "Upgrades/YSShardUpgrade.h"
#include "Core/YSBodyAnimTypes.h"
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
class UYSPassive;
class AYSInteractiveObject;
class AYSShipStation;
class AYSPlayerCorpse;
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

	/** Action de clic (Ability, clic droit), créée à l'exécution */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FireAction;

	/** Attaque auto (clic gauche maintenu), créée à l'exécution */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AutoAttackAction;

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

	/** Personnage de test : proposé à la sélection dans l'éditeur (PIE, -game) mais retiré des builds packagées (ex. JohnnyPlaytest) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C")
	bool bEditorOnlyCharacter = false;

	/** Taille du personnage : échelle de tout l'acteur (capsule, corps, mains, armes) ; 1.2 = 20 % plus grand ; x Character Scale Multiplier du set d'arme */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C", meta=(ClampMin="0.1"))
	float CharacterScale = 1.0f;

	/** Vitesse de déplacement maximale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Movement", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float MoveSpeed = 600.0f;

	/** Manette : zone morte du stick gauche (déplacement) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Gamepad", meta=(ClampMin="0", ClampMax="1"))
	float GamepadMoveDeadZone = 0.2f;

	/** Manette : zone morte des gâchettes (gauche = Ability, droite = attaque auto) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Gamepad", meta=(ClampMin="0", ClampMax="1"))
	float GamepadTriggerDeadZone = 0.2f;

	/** Points de vie max de base (avant Hps bonus) ; remplacés par ceux du set d'arme s'il en fixe (Weapon Set > Health) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="3C|Health", meta=(ClampMin="1"))
	float MaxHealth = 20.0f;

	/** Points de vie actuels (gérés par le serveur) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="3C|Health")
	float Health = 0.0f;

	/** Hps regen de base : PV rendus à chaque intervalle de regen, sans upgrade (les upgrades Hps regen s'y ajoutent) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Health", meta=(ClampMin="0"))
	float BaseHealthRegen = 0.0f;

	/** Hps regen : délai sans dégât subi avant que la regen démarre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Health", meta=(ClampMin="0", Units="s"))
	float HealthRegenDelay = 5.0f;

	/** Hps regen : intervalle entre deux soins */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Health", meta=(ClampMin="0.05", Units="s"))
	float HealthRegenInterval = 1.0f;

	/** Armor : dégâts minimum d'un coup après réduction par l'armure (l'armure ne ramène jamais un coup à 0) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Health", meta=(ClampMin="0"))
	float MinDamageAfterArmor = 1.0f;

	/**
	 *  Armor : part minimum du coup gardée après l'armure (0.1 : 100 d'armure contre un coup de 60 -> 6, contre 108 -> 10.8 arrondi à 11) ;
	 *  comptée sur le coup qui arrive à l'armure (après le %Armor du passif)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Health", meta=(ClampMin="0", ClampMax="1"))
	float MinDamageRatioAfterArmor = 0.1f;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Upgrades", meta=(ClampMin="0"))
	float MythicChance = 0.0f;

	/** Ligne de DT_Chara_Upgrades des upgrades exclusives de ce personnage (vide = son Character Name) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Upgrades")
	FName UpgradeTableRow;

	/** Niveau de chaque shard obtenue (système de shards ; répliqué pour l'affichage du menu) */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Upgrades")
	TArray<FYSShardLevel> ShardLevels;

	/** Shard Luck : points de % ajoutés aux chances Bronze, Silver, Gold, Mythique (répliqué : affichage) */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, Category="3C|Upgrades")
	float RarityChanceOffsets[4];

	/** Coups critiques dès le début de la partie, comme avec l'ancienne upgrade Atk critique (réglages ci-dessous) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Combat")
	bool bCriticalsFromStart = true;

	/** Auto swing : une attaque auto sur N est critique */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Combat", meta=(ClampMin="1", EditCondition="bCriticalsFromStart"))
	int32 StartSwingCriticalInterval = 5;

	/** PewPew : une balle d'attaque auto sur N reçue par un même NME est critique */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Combat", meta=(ClampMin="1", EditCondition="bCriticalsFromStart"))
	int32 StartBulletCriticalInterval = 3;

	/** Shotgun : AMMO critique sur un NME à moins de cette distance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Combat", meta=(ClampMin="1", Units="Centimeters", EditCondition="bCriticalsFromStart"))
	float StartShotgunCriticalMaxDistance = 200.0f;

	/** Sniper : balle critique sur un NME à plus de cette distance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Combat", meta=(ClampMin="1", Units="Centimeters", EditCondition="bCriticalsFromStart"))
	float StartSniperCriticalMinDistance = 1000.0f;

	/** Composant qui reçoit la couleur choisie à l'écran de sélection (avec tous ses sous-composants) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Customization")
	FName CustomizationColorComponentName = TEXT("Head Feature");

	/** Nage (eau profonde, sur le sol de nage de l'océan) : vitesse x ce multiplicateur (passif Larbin : le sien) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Swim", meta=(ClampMin="0", ClampMax="1"))
	float SwimSpeedMultiplier = 0.3f;

	/** Part de la hauteur du corps sous la surface pendant la nage (2/3) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Swim", meta=(ClampMin="0", ClampMax="1"))
	float SwimImmersion = 0.667f;

	/** Nage dès que les pieds sont à (profondeur du sol de nage - cette marge) sous la surface */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Swim", meta=(ClampMin="0", Units="Centimeters"))
	float SwimEnterTolerance = 10.0f;

	/** Hauteur des mains par rapport à la surface de l'eau pendant la nage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Swim", meta=(Units="Centimeters"))
	float SwimHandHeight = 0.0f;

	/** Vitesse de passage marche <-> nage (corps qui s'enfonce, mains qui descendent à la surface) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Swim", meta=(ClampMin="0"))
	float SwimBlendSpeed = 6.0f;

	/** Aiming (Harquebusier) : critiques prêts = le corps s'allonge à plat ventre, basculé vers l'avant de cet angle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Aiming", meta=(ClampMin="0", ClampMax="90", Units="Degrees"))
	float ProneAngle = 90.0f;

	/** Allongé : hauteur du centre du corps au-dessus des pieds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Aiming", meta=(Units="Centimeters"))
	float ProneBodyHeight = 30.0f;

	/** Allongé : hauteur des mains au-dessus des pieds, et décalage des mains vers l'avant */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Aiming", meta=(Units="Centimeters"))
	float ProneHandHeight = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Aiming", meta=(Units="Centimeters"))
	float ProneHandForward = 40.0f;

	/** Vitesse de passage debout <-> allongé */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Aiming", meta=(ClampMin="0"))
	float ProneBlendSpeed = 8.0f;

	/** Armes colorées selon la main (principale / autre / set d'upgrade de classe) ; faux = matériaux de base de leur Blueprint (minions) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Customization")
	bool bWeaponColoration = true;

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

	/** Caméra décalée vers le curseur (joueur local) : curseur au centre de l'écran = caméra sur le perso,
	 *  au bord de l'écran = CursorCameraMaxOffset devant le perso, proportionnel entre les deux */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Camera")
	bool bCursorCameraOffset = true;

	/** Décalage maximal de la caméra (curseur au bord de l'écran) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Camera", meta=(ClampMin="0", Units="cm", EditCondition="bCursorCameraOffset"))
	float CursorCameraMaxOffset = 500.0f;

	/** Vitesse de lissage du décalage (plus petit = plus doux ; 0 = pas de lissage) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Camera", meta=(ClampMin="0", EditCondition="bCursorCameraOffset"))
	float CursorCameraInterpSpeed = 3.0f;

	/** Décalage actuel de la caméra (monde, lissé) */
	FVector CursorCameraOffset = FVector::ZeroVector;

	/** Déplace la caméra vers le curseur (joueur local, Tick) */
	void UpdateCursorCamera(float DeltaSeconds);

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

	/** Passif du personnage (Data Asset de /Game/Core/Passives) : jamais amélioré, seulement remplacé (SetPassive) */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category="3C|Passive")
	TObjectPtr<UYSPassive> Passive;

	/** Coup critique : dégâts x ce multiplicateur (Golden bloodlust, Atk critique), augmenté par les bonus critiques d'Atk speed / Shoot system ; un passif peut le remplacer (Marksman) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="3C|Combat", meta=(ClampMin="1"))
	float CriticalDamageMultiplier = 1.5f;

	/** Attaque qui touche plusieurs NME (swing, balle qui traverse, explosion) : chaque NME au-delà du premier prend cette part
	 *  de dégâts en moins (cumulé : 2e x 0.9, 3e x 0.8...), le plus proche en premier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="3C|Combat", meta=(ClampMin="0", ClampMax="1"))
	float MultiHitDamageFalloff = 0.1f;

	/** Attaque qui touche plusieurs NME : multiplicateur de dégâts minimal (atteint au 6e NME avec 0.1 par NME) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="3C|Combat", meta=(ClampMin="0", ClampMax="1"))
	float MultiHitMinDamageMultiplier = 0.5f;

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

	/** Aiming (upgrade d'arme du Harquebusier ; serveur, répliqué pour la pose) : immobile depuis ce temps = balles critiques ; 0 = pas débloqué */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Aiming")
	float AimingDelay = 0.0f;

	/** Golden aiming : les balles critiques d'Aiming traversent tous les NME */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Aiming")
	bool bGoldenAiming = false;

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

	/** Shard Melee Range : échelle des armes de mêlée tenues (répliqué : appliquée sur chaque machine, UpdateWeaponRangeScale) */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, Category="3C|Upgrades")
	float MeleeRangeMultiplier = 1.0f;

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

	/**
	 *  Poste de bateau occupé (barre, nid d'pie, canonnerie, chaloupe) : gardé sur le poste, armes rangées ; barre : ZQSD pilote le
	 *  bateau ; clics envoyés au poste (tir de canon, ping)
	 */
	UPROPERTY(ReplicatedUsing=OnRep_CurrentStation, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="3C|Interaction")
	TObjectPtr<AYSShipStation> CurrentStation;

	UFUNCTION()
	void OnRep_CurrentStation(AYSShipStation* PreviousStation);

	/** Tags portés par le personnage (camp, etc.), lus par les projectiles */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Tags")
	FGameplayTagContainer GameplayTags;

	/**
	 *  Apparition en sortant du sol, comme les NME (minions du Captain ; faux pour les joueurs) : pendant Spawn Rise Duration,
	 *  ni déplacement ni attaque ; un coup qui pousse le sort tout de suite
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Spawn Anim")
	bool bSpawnRise = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Spawn Anim", meta=(ClampMin="0.05", Units="s", EditCondition="bSpawnRise"))
	float SpawnRiseDuration = 2.0f;

	/** Profondeur de départ en plus de la hauteur du personnage, x sa taille */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Spawn Anim", meta=(ClampMin="0", Units="Centimeters", EditCondition="bSpawnRise"))
	float SpawnRiseExtraDepth = 120.0f;

	/** Courbe : rapide au début, ralentit en arrivant au sol */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Spawn Anim", meta=(ClampMin="0.1", EditCondition="bSpawnRise"))
	float SpawnRiseEaseExponent = 2.0f;

	/**
	 *  Mort en ragdoll, comme les NME : le corps (mains, armes, tête comprises) est lancé par le pushback maximal du NME qui l'a tué
	 *  (x Death Impulse Multiplier), reste Death Scale Delay au sol puis rétrécit en Death Scale Duration ; ensuite le joueur
	 *  passe en spectateur (minion : détruit)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Death")
	bool bDeathRagdoll = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Death", meta=(ClampMin="0", EditCondition="bDeathRagdoll"))
	float DeathImpulseMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Death", meta=(Units="CentimetersPerSecond", EditCondition="bDeathRagdoll"))
	float DeathExtraUpImpulse = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Death", meta=(ClampMin="0", Units="DegreesPerSecond", EditCondition="bDeathRagdoll"))
	float DeathAngularImpulse = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Death", meta=(ClampMin="0", Units="s", EditCondition="bDeathRagdoll"))
	float DeathScaleDelay = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Death", meta=(ClampMin="0.01", Units="s", EditCondition="bDeathRagdoll && !bKeepCorpse"))
	float DeathScaleDuration = 1.0f;

	/**
	 *  Joueurs : le corps reste en jeu (pas de rétrécissement) ; après Death Scale Delay il devient un cadavre (Corpse Class) que
	 *  les joueurs peuvent porter et poser comme un coffre. Faux pour les minions (ils rétrécissent et disparaissent)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Death", meta=(EditCondition="bDeathRagdoll"))
	bool bKeepCorpse = true;

	/** Cadavre : objet portable qui porte le corps (Blueprint enfant de YSPlayerCorpse possible) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Death", meta=(EditCondition="bDeathRagdoll && bKeepCorpse"))
	TSubclassOf<AYSPlayerCorpse> CorpseClass;

	/** Cadavre plus lourd : masse du ragdoll x ce multiplicateur, donc même poussée = vitesse divisée d'autant (va moins loin) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Death", meta=(ClampMin="0.1", EditCondition="bDeathRagdoll && bKeepCorpse"))
	float CorpseMassMultiplier = 2.0f;

	/**
	 *  Ragdoll simulé par le serveur seulement, pose répliquée : les clients la suivent (le corps est au même endroit sur tous les PC).
	 *  Faux : chaque machine simule le sien (minions)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Death", meta=(EditCondition="bDeathRagdoll && bKeepCorpse"))
	bool bReplicateRagdoll = true;

	/** Clients : vitesse de rattrapage de la pose répliquée du ragdoll (lissage entre deux mises à jour) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Death", meta=(ClampMin="1", EditCondition="bDeathRagdoll && bKeepCorpse && bReplicateRagdoll"))
	float RagdollFollowSpeed = 15.0f;

	/** Le corps flotte sur la mer (ragdoll poussé vers la surface) et le cadavre reste à la surface, au lieu de couler */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Death|Float", meta=(EditCondition="bDeathRagdoll && bKeepCorpse"))
	bool bCorpseFloats = true;

	/** Profondeur du centre du corps sous la surface quand il flotte (équilibre) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Death|Float", meta=(ClampMin="0", Units="cm", EditCondition="bCorpseFloats"))
	float CorpseFloatDepth = 20.0f;

	/** Poussée de l'eau : accélération vers le haut par cm d'enfoncement au-delà de Corpse Float Depth (en plus de compenser la gravité) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Death|Float", meta=(ClampMin="0", EditCondition="bCorpseFloats"))
	float CorpseBuoyancy = 40.0f;

	/** Freinage du corps dans l'eau (amortissement linéaire et angulaire) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Death|Float", meta=(ClampMin="0", EditCondition="bCorpseFloats"))
	float CorpseWaterDamping = 3.0f;

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

	/** Aiming : dernier instant (temps local) où le personnage bougeait */
	double LastMovingTime = 0.0;

	/** Aiming : 0 = debout, 1 = allongé à plat ventre (lissé) */
	float ProneBlend = 0.0f;

	/** Corps sans la pose allongée (lean, sautillement, nage) : repère des mains, qui restent à l'endroit */
	FTransform HandsPivotTransform = FTransform::Identity;

	/** Yaw actuel (lissé, relatif au corps) de chaque main : vers la cible de visée assistée, sinon rotation par défaut */
	float LeftHandYaw = 0.0f;
	float RightHandYaw = 0.0f;

	/** Tir différé de la main droite en akimbo */
	FTimerHandle AkimboTimer;

	/** Objet dont l'invite « E » est affichée (joueur local) */
	TWeakObjectPtr<AYSInteractiveObject> FocusedInteractable;

	/** Sortie du sol (serveur -> toutes les machines) */
	UPROPERTY(ReplicatedUsing=OnRep_Rise)
	FYSFoeRise Rise;

	UFUNCTION()
	void OnRep_Rise();

	void StartRise();
	void BeginRiseLocal();
	void UpdateRise();
	void EndRiseLocal();

	bool bLocalRiseActive = false;

	/** Mort en ragdoll (serveur -> toutes les machines) */
	UPROPERTY(ReplicatedUsing=OnRep_Death)
	FYSFoeDeath Death;

	UFUNCTION()
	void OnRep_Death();

	void BeginDeathLocal();

	/** Rétrécissement après Death Scale Delay ; à 0 : FinishDeath (serveur) ; cadavre : ragdoll (flottaison, pose répliquée) puis cadavre */
	void UpdateDeath(float DeltaSeconds);

	/**
	 *  Corps disparu ou devenu cadavre (serveur) : caméra de secours du joueur, puis personnage détruit, ou seulement
	 *  dépossédé s'il reste en cadavre (le joueur passe en spectateur)
	 */
	void FinishDeath(bool bKeepActor = false);

	/** Fait du corps immobile un cadavre portable posé au sol sous lui (serveur) ; faux si impossible (dans l'eau, pas de sol) */
	bool SpawnCorpse();

	/** Le cadavre a été créé, ou il n'y en aura pas (serveur) */
	bool bCorpseHandled = false;

	/** Pose du ragdoll simulé par le serveur (bReplicateRagdoll), suivie par les clients tant que le corps n'est pas sur son cadavre */
	UPROPERTY(Replicated)
	FYSRagdollPose RagdollPose;

	/** Ce ragdoll suit la pose du serveur au lieu d'être simulé sur cette machine */
	bool UsesReplicatedRagdoll() const { return bKeepCorpse && bReplicateRagdoll && !HasAuthority(); }

	/** Le corps suit son cadavre (AttachBodyTo) : plus de ragdoll */
	bool bBodyOnCorpse = false;

	/** Serveur : poussée de la mer sur le ragdoll (bCorpseFloats) */
	void ApplyCorpseBuoyancy(float DeltaSeconds);

	/** Client : le corps rejoint la pose répliquée du ragdoll */
	void FollowRagdollPose(float DeltaSeconds);

	/** Joueur mort (cadavre gardé) : armes cachées (local, toutes machines) */
	void HideCorpseWeapons();

	/** Vitesse du ragdoll : pushback maximal du NME tueur dans la direction du coup, + vertical */
	FVector ComputeDeathImpulse(const FDamageEvent& DamageEvent, const AController* EventInstigator, const AActor* DamageCauser) const;

	bool bLocalDeathStarted = false;
	double DeathStartTime = 0.0;
	FVector DeathBodyScale = FVector::OneVector;
	FVector PendingDeathImpulse = FVector::ZeroVector;
	bool bSkipDeathAnim = false;

public:

	/** En train de sortir du sol (pas d'attaque ni de déplacement) */
	bool IsRising() const { return bLocalRiseActive; }

	/** Mort, corps en ragdoll en attendant sa disparition (joueur : ou resté en cadavre) */
	bool IsDying() const { return Death.bDead; }

	/** Cadavre (toutes machines) : le corps (et tout ce qui le suit) arrête la physique et suit Parent avec cette pose relative */
	void AttachBodyTo(USceneComponent* Parent, const FVector& RelativeLocation, const FRotator& RelativeRotation);

	/** Sortie du sol terminée tout de suite (coup qui pousse ; serveur) */
	void FinishRiseNow();

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

	/** Hps regen de base ramenée à la seconde (fiche du personnage) */
	float GetBaseHealthRegenPerSecond() const { return HealthRegenInterval > 0.0f ? BaseHealthRegen / HealthRegenInterval : BaseHealthRegen; }

	/** Taille du personnage : Character Scale x Character Scale Multiplier du set d'arme (1 = normale) */
	float GetCharacterSize() const;

	/** Armor (serveur) */
	void SetArmor(float Amount) { Armor = Amount; }

	float GetArmor() const { return Armor; }

	float GetMaxHealthMultiplier() const { return MaxHealthMultiplier; }

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

	/**
	 *  Résurrection (serveur, personnage mort resté en cadavre) : un personnage de la même classe, avec le même set d'arme, passif,
	 *  main, upgrades et shards, apparaît debout sur le sol Ground, possédé par NewController, à HealthRatio de ses PV max ;
	 *  ce corps disparaît. Renvoie le nouveau personnage.
	 */
	AYSPlayerCharacter* Revive(AController* NewController, const FVector& Ground, float HealthRatio);

	/** Applique la personnalisation de son joueur (couleur, visage ; AYSPlayerState) ; minion : couleur de son Captain */
	virtual void RefreshCustomization();

	/** Applique un matériau au composant CustomizationColorComponentName et à ses sous-composants, et un visage (nullptr = inchangé) */
	void ApplyCustomization(UMaterialInterface* ColorMaterial, UTexture2D* FaceTexture);

	/** Reprend la progression d'upgrades d'un autre personnage (OldPool = son pool) et la réapplique (serveur) */
	void RestoreUpgrades(const TArray<TObjectPtr<UYSUpgrade>>& OldPool, const TArray<FYSUpgradeProgress>& OldProgress);

	/** Ajoute le bonus proposé à la progression et applique l'upgrade (serveur) ; système de shards : +1 niveau par shard donnée */
	void ApplyUpgrade(const FYSUpgradeOffer& Offer);

	/** Niveau d'une shard (0 = jamais obtenue) */
	int32 GetShardLevel(const UYSUpShard* Shard) const;

	const TArray<FYSShardLevel>& GetShardLevels() const { return ShardLevels; }

	/** Ajoute des niveaux à une shard (au plus son maximum) et applique son effet (serveur) */
	void AddShardLevels(UYSUpShard* Shard, int32 Levels);

	/** Reprend les shards d'un autre personnage (changement de classe ; jumelles converties) et applique leurs effets (serveur) */
	void RestoreShards(const TArray<FYSShardLevel>& OldLevels);

	/** Shard Luck : points de % ajoutés aux chances de chaque rareté (serveur) */
	void SetRarityChanceOffsets(float Bronze, float Silver, float Gold, float Mythic);

	/** Ligne de DT_Chara_Upgrades de ce personnage */
	FName GetUpgradeTableRow() const { return UpgradeTableRow.IsNone() ? FName(*GetCharacterName().ToString()) : UpgradeTableRow; }

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

	/** Melee Range : échelle des armes de mêlée (serveur, répliqué) */
	void SetMeleeRangeMultiplier(float Multiplier) { MeleeRangeMultiplier = Multiplier; ForceNetUpdate(); }

	float GetMeleeRangeMultiplier() const { return MeleeRangeMultiplier; }

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
	virtual bool HasForcedCriticalAgainst(const AActor* Target) const;

	/** Marquage Critique : plus grande valeur de la propriété d'arme Stat parmi les Captains dont Target est le NME marqué (0 sinon ; serveur) */
	static float GetMarkStat(const AActor* Target, FName Stat);

	/**
	 *  « Allez-y mes minions ! » (serveur) : force l'Ability du clic (actions de clic des deux mains, comme le clic gauche), vers le
	 *  curseur de la machine qui contrôle le personnage (RPC au client) ; priorité sur tout, recharge ignorée (Forced).
	 */
	void ForceAbility(const FYSForcedActivation& Forced);

	/** Level up (serveur) : tous les PV rendus et recharge de l'Ability (actions de clic des deux mains) terminée */
	void RestoreOnLevelUp();

	/** Bonus Shoot system pour Weapon (0 si elle n'est pas tenue) */
	float GetWeaponShootSystemBonus(const AYSWeapon* Weapon, EYSActionSlot Slot) const;

	/** Propriété des armes tenues (upgrades d'arme ; serveur) */
	void SetWeaponStat(FName Stat, float Value) { WeaponStats.Add(Stat, Value); }

	/** Propriété d'arme de Weapon (Default si absente ou arme pas tenue) */
	float GetWeaponStat(const AYSWeapon* Weapon, FName Stat, float Default) const;

	/** Propriété d'arme posée sur ce personnage, sans condition d'arme (Default si absente ; serveur) */
	float GetCharacterWeaponStat(FName Stat, float Default) const { const float* Value = WeaponStats.Find(Stat); return Value ? *Value : Default; }

	/** Retire Seconds à la recharge des Abilities (actions de clic) des armes tenues (serveur ; Golden Marquage) */
	void ReduceAbilityCooldown(float Seconds);

	/** Multiplicateurs d'un type d'action pour Weapon (1 si elle n'est pas tenue) */
	float GetWeaponCooldownMultiplier(const AYSWeapon* Weapon, EYSActionSlot Slot) const;

	/** Bloodlust réglé par l'upgrade (serveur) : MaxStacks 0 = désactivé (stacks remises à 0) */
	void SetBloodlust(int32 MaxStacks, float Duration, float AtkSpeedPerStack, bool bGolden);

	/** Aiming (serveur, upgrade) : délai immobile avant les critiques (0 = pas débloqué), Golden aiming */
	void SetAiming(float Delay, bool bGolden);

	/** Aiming : immobile depuis Aiming Delay, toutes ses balles sont critiques (calculé sur chaque machine depuis la vitesse) */
	bool IsAimingCritical() const;

	bool HasGoldenAiming() const { return bGoldenAiming; }

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
	/** Multiplicateur de dégâts du HitIndex-ième NME touché par une même attaque (0 = le premier : x1) */
	float GetMultiHitDamageMultiplier(int32 HitIndex) const { return FMath::Max(MultiHitMinDamageMultiplier, 1.0f - MultiHitDamageFalloff * FMath::Max(0, HitIndex)); }

	virtual float GetCriticalDamageMultiplier() const { return (GetBaseCriticalDamageMultiplier() + CriticalDamageAdd) * (1.0f + CriticalDamageBonus); }

	/** Upgrade Dégât critique : somme de ses bonus (serveur) */
	void SetCriticalDamageAdd(float Value);
	float GetCriticalDamageAdd() const { return CriticalDamageAdd; }

	/** Le personnage peut faire des coups critiques (Atk critique ou Golden bloodlust) */
	bool CanCriticalHit() const { return CriticalSwingInterval > 0 || CriticalBulletInterval > 0 || CriticalShotgunMaxDistance > 0.0f || CriticalSniperMinDistance > 0.0f || bGoldenBloodlust || BloodRageMaxStacks > 0; }

	/** Atk critique : une attaque auto Swing de plus pour le personnage (serveur), renvoie le total */
	int32 RegisterAutoSwing() { return ++AutoSwingCounter; }

	/** Atk critique : attaques auto Swing déjà comptées (la suivante sera la n° GetAutoSwingCounter() + 1) */
	int32 GetAutoSwingCounter() const { return AutoSwingCounter; }

	/** Multiplicateur de base des critiques : celui du passif s'il en fixe un (Marksman), sinon Critical Damage Multiplier */
	float GetBaseCriticalDamageMultiplier() const;

	UYSPassive* GetPassive() const { return Passive; }

	/** Remplace le passif (serveur ; futures upgrades) */
	void SetPassive(UYSPassive* NewPassive);

	/** %Armor : part des dégâts subis ignorée avant l'armure (passif Tough Guy) */
	float GetArmorPercent() const;

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

	/** Traverse les pawns en ce moment (capsule qui ignore les Pawns : roulade, dash, saut, apparition d'un minion) : les projectiles NME le traversent aussi */
	bool IsPassingThroughPawns() const;

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

	/** Déclenche les actions de clic des deux mains (comme le clic droit ; debug YSFire) */
	void TriggerFire() { Fire(); }

	/** Attaque auto autorisée : joueur = clic gauche maintenu (serveur : dernier état reçu) ; minions : toujours */
	bool WantsAutoAttack() const { return !UsesHeldAutoAttack() || bAutoAttackHeld || bDebugAutoAttack; }

	/** Distance (à plat) du curseur pendant le clic gauche, connue du serveur ; < 0 = inconnue */
	float GetAutoAttackAimDistance() const { return AutoAttackAimDistance; }

	/** Le personnage n'attaque automatiquement qu'en maintenant le clic gauche, et l'attaque auto le ralentit (joueurs ; pas les minions) */
	virtual bool UsesHeldAutoAttack() const { return true; }

	/** Debug (YSHoldAutoAttack) : clic gauche considéré comme maintenu */
	void SetDebugAutoAttack(bool bHeld);

	/** Comme la touche E (debug YSInteract) */
	void TriggerInteract() { Interact(); }

	USceneComponent* GetBodyPivot() const { return BodyPivot.Get(); }

	/** Objet porté à deux mains (nullptr si aucun) */
	AYSInteractiveObject* GetCarriedObject() const { return CarriedObject; }

	bool IsCarrying() const { return CarriedObject != nullptr; }

	/** Nage (eau profonde) : calculé sur chaque machine depuis la position (pieds sous la surface) ; plus ciblé par les NME */
	bool IsSwimming() const { return bSwimming; }

	/** Armes rangées (objet porté, nage, poste de bateau) : cachées, ni Ability ni attaque auto */
	bool AreWeaponsStowed() const { return IsCarrying() || bSwimming || IsAtStation(); }

	/** À un poste de bateau (barre, nid d'pie, canonnerie, chaloupe) */
	bool IsAtStation() const { return CurrentStation != nullptr; }

	AYSShipStation* GetCurrentStation() const { return CurrentStation; }

	/** S'installe au poste (serveur) : actions interrompues, armes rangées, placé sur le poste */
	void EnterStation(AYSShipStation* Station);

	/** Quitte le poste (serveur) ; bForce : même un poste qu'on ne quitte pas avec E (chaloupe éjectée, mort) */
	void LeaveStation(const TCHAR* Reason = TEXT(""), bool bForce = false);

	/** Téléporte le personnage (pieds sur FeetLocation, tourné vers Facing) : serveur et machine qui le contrôle */
	void TeleportToFeetLocation(const FVector& FeetLocation, const FVector& Facing);

	/** Debug (YSTakeHelm) : prend la barre du bateau pilotable le plus proche (Skip : le suivant par distance) */
	void DebugTakeHelm(int32 Skip = 0);

	/** Debug (YSUseStation) : utilise le poste libre le plus proche de ce type (Helm, Nest, Cannon, Longboat, Seat), où qu'il soit */
	void DebugUseStation(const FString& Kind, int32 Skip = 0);

	/** Debug (YSStationAction) : action au poste (0 = clic droit / tir, 1 = clic gauche / ping) vers un point du monde */
	void DebugStationAction(uint8 Action, const FVector& AimLocation);

	/** Debug (YSLeaveStation) : quitte le poste comme avec E */
	void DebugLeaveStation();

	/** Debug (YSHelmInput) : commandes de la barre tenues (X = Q / D, Y = Z / S) quand aucune touche n'est enfoncée */
	void SetDebugHelmInput(const FVector2D& Input) { DebugHelmInput = Input; }

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

	bool IsEditorOnlyCharacter() const { return bEditorOnlyCharacter; }

protected:

	/** Gère l'input de déplacement */
	void Move(const FInputActionValue& Value);

	/**
	 *  Clic droit : déclenche l'action des armes des deux mains vers le curseur.
	 *  Akimbo (même arme dans les deux mains) : la gauche tout de suite, la droite après son AkimboFireDelay.
	 */
	void Fire();

	/** Clic gauche maintenu (machine qui contrôle, Tick) : envoyé au serveur quand il change */
	void UpdateAutoAttackInput();

	UFUNCTION(Server, Reliable)
	void ServerSetAutoAttackHeld(bool bHeld);

	UFUNCTION(Server, Reliable)
	void ServerSetDebugAutoAttack(bool bHeld);

	UFUNCTION(Server, Unreliable)
	void ServerSetAutoAttackAimDistance(float Distance);

	/** Clic gauche maintenu (machine qui contrôle ; serveur : dernier état reçu) */
	bool bAutoAttackHeld = false;

	/** Distance (à plat) du curseur au personnage pendant le clic gauche (machine qui contrôle ; serveur : dernière reçue) ; < 0 = inconnue */
	float AutoAttackAimDistance = -1.0f;

	/** Écart (cm) au-delà duquel la distance du curseur est renvoyée au serveur */
	static constexpr float AutoAttackAimDistanceThreshold = 25.0f;

	/** Debug : clic gauche considéré comme maintenu */
	bool bDebugAutoAttack = false;

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

	/** Commandes de la barre (machine qui contrôle) : Z / S et D / Q reçus par Move cette image */
	FVector2D HelmInput = FVector2D::ZeroVector;
	uint64 LastHelmInputFrame = 0;
	FVector2D LastSentHelmInput = FVector2D::ZeroVector;
	FVector2D DebugHelmInput = FVector2D::ZeroVector;

	UFUNCTION(Server, Reliable)
	void ServerDebugUseStation(const FString& Kind, int32 Skip);

	/** Commandes de la barre envoyées au bateau (au changement) */
	UFUNCTION(Server, Reliable)
	void ServerSetHelmInput(FVector2D Input);

	/** À un poste : commandes de la barre envoyées, joueur gardé sur le poste (serveur et machine qui contrôle) */
	void UpdateStation();

	/** Clic au poste (machine qui contrôle) : vrai = pris par le poste (envoyé au serveur), les armes ne font rien */
	bool SendStationAction(uint8 Action);

	/** Action au poste (EYSStationAction) vers un point du monde */
	UFUNCTION(Server, Reliable)
	void ServerStationAction(uint8 Action, FVector_NetQuantize AimLocation);

	/** Téléportation (sortie d'un poste, éjection de la chaloupe) sur la machine qui contrôle */
	UFUNCTION(Client, Reliable)
	void ClientTeleportToFeet(FVector_NetQuantize FeetLocation, FVector_NetQuantizeNormal Facing);

	/** Point du monde sous le curseur (sol, pont, mer), pour le ping du nid d'pie ; sinon la visée */
	FVector GetCursorGroundLocation() const;

	/** Caméra : longueur du bras du Blueprint (à pied), part de la caméra du poste (0 à 1, lissée) */
	float DefaultCameraArmLength = 0.0f;
	float HelmCameraBlend = 0.0f;
	float HelmCameraArmLength = 0.0f;
	FVector HelmCameraOffset = FVector::ZeroVector;
	float HelmCameraInterpSpeed = 2.0f;

	/** MaxWalkSpeed = MoveSpeed x Move speed bonus x objet porté (serveur et client : prédiction) */
	void UpdateMaxWalkSpeed();

	/** Armes cachées pendant le portage et la nage (toutes machines) */
	void ApplyCarryVisuals();

	/** Toutes les actions des armes interrompues (serveur : prise d'un objet, entrée dans l'eau) */
	void CancelAllActions();

	/** État de nage depuis la position (toutes machines), sol de nage coupé pendant que l'île coule, passage marche <-> nage */
	void UpdateSwimming(float DeltaSeconds);

	/** Décalage vertical du corps pendant la nage : Swim Immersion de sa hauteur sous la surface (0 hors de l'eau) */
	float GetSwimBodyOffset() const;

	bool bSwimming = false;

	/** 0 = marche, 1 = nage (lissé) */
	float SwimBlend = 0.0f;

	/** Hauteur de la surface de la mer (dernière connue) */
	float SwimSurfaceZ = 0.0f;

	/** Le capsule bloque le sol de nage (coupé pendant que l'île coule : on coule avec elle) */
	bool bSwimFloorBlocked = false;

	/** Couleur des armes (composant Coloration) : main principale, autre main, ou set d'upgrade de classe ; local */
	void UpdateWeaponColoration();

	/** Melee Range : échelle des armes de mêlée (composant d'arme de la main), local d'après MeleeRangeMultiplier répliqué */
	void UpdateWeaponRangeScale();

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

	/** PV à 0 (serveur) : PlayerState mort, corps en ragdoll (FinishDeath à la fin : personnage détruit, spectateur) */
	virtual void Die();

	/** Deadzone : tombé sous le KillZ de la map (World Settings) -> mort (serveur ; rien chez les clients) */
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

private:

	/** Crée les actions et le contexte ZQSD/WASD + flèches + clic gauche */
	void CreateDefaultInput();
};
