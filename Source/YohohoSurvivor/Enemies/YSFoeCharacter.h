#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "Abilities/YSAutoSwingAbility.h"
#include "Core/YSBodyAnimTypes.h"
#include "YSFoeCharacter.generated.h"

class APlayerState;
class UCapsuleComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class UYSFaceDecalComponent;
class AYSInteractiveObject;
class AYSFoeBomb;
class AYSWeapon;
class AYSPlayerCharacter;
class UChildActorComponent;
class UYSWeaponSet;
class UMaterialInterface;
class UYSFoeRangedAction;
class UYSAutoSwingAbility;
class UYSGroundZoneComponent;
class AYSProjectile;

/** Bump en cours (Bumping Slash) : vol en cloche répliqué, recalculé à l'identique sur chaque machine */
USTRUCT()
struct FYSFoeBump
{
	GENERATED_BODY()

	UPROPERTY()
	bool bActive = false;

	UPROPERTY()
	FVector_NetQuantize Start = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantize Target = FVector::ZeroVector;

	UPROPERTY()
	float Duration = 1.0f;

	UPROPERTY()
	float ApexHeight = 300.0f;

	UPROPERTY()
	float SpinTurns = 1.0f;

	/** Yaw du NME au début du vol (la rotation se fait autour de cet angle) */
	UPROPERTY()
	float StartYaw = 0.0f;

	UPROPERTY()
	double StartServerTime = 0.0;

	/** Incrémenté à chaque bump : un nouveau bump pendant le vol est vu par les clients */
	UPROPERTY()
	uint8 Counter = 0;
};

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

	/** Marque de la Captain AMMO : disque posé sur le relief sous le NME, visible tant qu'il est marqué */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UYSGroundZoneComponent> MarkZone;

protected:

	/** Nom affiché (barre de vie de l'élite dans le HUD) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe")
	FText FoeName;

	/** Le niveau suit le numéro du jour en cours au moment de l'apparition (+ LevelFromDayOffset : jour 1 = niveau 0) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level")
	bool bLevelFromDay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level", meta=(EditCondition="bLevelFromDay"))
	int32 LevelFromDayOffset = -1;

	/** Niveau imposé par l'apparition (SetForcedLevel ; INDEX_NONE = niveau normal) */
	int32 ForcedLevel = INDEX_NONE;

	/** Niveaux ajoutés au niveau normal par l'apparition (SetLevelBonus ; circle spawns répétés de la nuit) */
	int32 LevelBonus = 0;

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

	/** PV x ce multiplicateur pour le nombre de joueurs de la partie (fixé à l'apparition, serveur) ; l'XP donnée est divisée par lui */
	float PlayerCountHealthMultiplier = 1.0f;

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
	float BaseExperience = 10.0f;

	/** XP ajoutée par niveau (XP donnée = Base + ParNiveau × niveau) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level", meta=(ClampMin="0"))
	float ExperiencePerLevel = 10.0f;

	/** Mort dans l'eau = pas d'XP : pieds à plus de cette profondeur sous la surface de la mer (ou tombé dans la deadzone) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level", meta=(ClampMin="0", Units="Centimeters"))
	float NoExperienceWaterDepth = 30.0f;

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
	float PushbackMinForce = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foe|Pushback", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float PushbackMaxForce = 500.0f;

	/** Vitesse verticale donnée en plus à chaque pushback d'un joueur (décolle un peu du sol : moins de friction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foe|Pushback", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float PushbackVerticalForce = 50.0f;

	/**
	 *  Choix de la cible : un minion (ex. minions du Captain) compte ce nombre de fois plus loin qu'un joueur à la même distance
	 *  (le NME préfère les joueurs, mais attaque un minion beaucoup plus proche)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foe|Movement", meta=(ClampMin="1"))
	float MinionTargetDistanceMultiplier = 2.0f;

	/** Tags portés par l'ennemi (camp, etc.), lus par les projectiles */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Tags")
	FGameplayTagContainer GameplayTags;

	/** Vitesse de déplacement maximale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Movement", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float MoveSpeed = 300.0f;

	/** Vitesse à laquelle l'ennemi se tourne vers sa cible (0 = instantané) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Movement", meta=(ClampMin="0"))
	float TurnSpeed = 10.0f;

	/** NME à distance qui vise un joueur en train de nager : s'arrête quand la mer est à cette distance devant lui (pas dans l'eau) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Movement", meta=(ClampMin="0", Units="Centimeters"))
	float WaterStopDistance = 50.0f;

	/**
	 *  Les capsules des NME se bloquent entre elles (ancien comportement : foule compacte, mais chaque NME bute sur ses voisins,
	 *  coûteux). Décoché : elles se traversent (canal Foe ignoré entre NME) et une séparation douce les écarte.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Movement|Crowd")
	bool bFoesBlockEachOther = true;

	/**
	 *  Séparation douce (serveur) : deux NME s'écartent dès que leurs centres sont à moins de (rayon + rayon voisin) x ce
	 *  facteur ; plus ils se chevauchent, plus ils se poussent
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Movement|Crowd", meta=(ClampMin="0", EditCondition="!bFoesBlockEachOther"))
	float SeparationRadiusScale = 1.15f;

	/** Force de la séparation, ajoutée à l'input de déplacement (1 = autant que la poursuite à plein chevauchement) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Movement|Crowd", meta=(ClampMin="0", EditCondition="!bFoesBlockEachOther"))
	float SeparationStrength = 1.5f;

	/**
	 *  Position par défaut de la main droite, par rapport au corps (centre de la capsule) ; la gauche est symétrique (Y inversé).
	 *  Une arme peut la remplacer (Weapon > Hand Location). Mêmes réglages que le joueur (3C|Hands).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Hands")
	FVector HandLocation = FVector(0.0f, 55.0f, -10.0f);

	/** Rotation par défaut de la main droite, par rapport au corps ; la gauche est symétrique (Yaw et Roll inversés) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Hands")
	FRotator HandRotation = FRotator::ZeroRotator;

	/**
	 *  Oscillation des mains au repos. Perf : décochée, une main au repos ne bouge plus d'elle-même ; elle n'est recalculée que
	 *  quand le NME bouge ou attaque (foules : grosse économie)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Hands")
	bool bHandSway = false;

	/** Écart maximal de l'oscillation des mains autour de leur position (0 = pas d'oscillation) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Hands", meta=(ClampMin="0", Units="Centimeters", EditCondition="bHandSway"))
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

	/** Perf (ombres) : les sphères des mains projettent une ombre (seul le corps en projette sinon) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Performance")
	bool bHandsCastShadow = false;

	/** Perf (ombres) : les armes tenues projettent une ombre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Performance")
	bool bWeaponsCastShadow = false;

	/**
	 *  Perf : un NME pas affiché à l'écran et à plus de Far Tick Distance de tout joueur (minions compris) ne calcule son
	 *  déplacement, sa barre de vie et les actions de ses armes que toutes les Far Tick Interval secondes (0 = jamais réduit)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Performance", meta=(ClampMin="0", Units="Seconds"))
	float FarTickInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Performance", meta=(ClampMin="0", Units="Centimeters"))
	float FarTickDistance = 2500.0f;

	/** Fréquence de la vérification « loin » (secondes) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Performance", meta=(ClampMin="0.05", Units="Seconds"))
	float FarTickCheckInterval = 0.5f;

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

	/** Apparition : le NME sort du sol (parti de sous la map) pendant Spawn Rise Duration ; touchable mais sans comportement
	 *  ni attaque ; un coup qui le pousse (pushback, bump) le sort tout de suite et il démarre son comportement */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Spawn Anim")
	bool bSpawnRise = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Spawn Anim", meta=(ClampMin="0.05", Units="s", EditCondition="bSpawnRise"))
	float SpawnRiseDuration = 2.0f;

	/** Profondeur de départ en plus de la hauteur du NME, x sa taille (complètement caché sous le sol au début, arme comprise) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Spawn Anim", meta=(ClampMin="0", Units="Centimeters", EditCondition="bSpawnRise"))
	float SpawnRiseExtraDepth = 120.0f;

	/** Courbe de la sortie (1 = vitesse constante, 2 = rapide au début puis ralentit en arrivant au sol) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Spawn Anim", meta=(ClampMin="0.1", EditCondition="bSpawnRise"))
	float SpawnRiseEaseExponent = 2.0f;

	/** Mort : corps lancé en ragdoll (physique) à la force MAXIMALE du pushback de celui qui a porté le coup (même direction), puis réduit à 0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Death")
	bool bDeathRagdoll = true;

	/** Multiplicateur de toute la vitesse donnée au corps (1 = pushback maximal du tueur ; 2 demandé par Paul) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Death", meta=(ClampMin="0", EditCondition="bDeathRagdoll"))
	float DeathImpulseMultiplier = 2.0f;

	/** Vitesse verticale en plus (le corps décolle ; s'ajoute à la verticale du pushback du tueur) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Death", meta=(Units="CentimetersPerSecond", EditCondition="bDeathRagdoll"))
	float DeathExtraUpImpulse = 200.0f;

	/** Rotation donnée au corps (axe au hasard) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Death", meta=(ClampMin="0", Units="DegreesPerSecond", EditCondition="bDeathRagdoll"))
	float DeathAngularImpulse = 360.0f;

	/** Le corps commence à rétrécir après ce délai, puis atteint 0 en Death Scale Duration (ensuite le NME est détruit) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Death", meta=(ClampMin="0", Units="s", EditCondition="bDeathRagdoll"))
	float DeathScaleDelay = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Death", meta=(ClampMin="0.01", Units="s", EditCondition="bDeathRagdoll"))
	float DeathScaleDuration = 1.0f;

	/** Map de test (GameMode avec Numpad Foe Spawn) : touche du pavé numérique (0 à 9) qui fait apparaître ce NME ; -1 = aucune */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Spawn", meta=(ClampMin="-1", ClampMax="9"))
	int32 NumpadSpawnKey = -1;

	/** Marque (Captain AMMO) : rayon = rayon de la capsule x ce multiplicateur (1.25 = 25 % plus large que le NME) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Mark", meta=(ClampMin="0.1"))
	float MarkRadiusScale = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Mark")
	FLinearColor MarkColor = FLinearColor(1.0f, 0.75f, 0.05f, 1.0f);

	/** Matériau du disque (M_BrookerZone : paramètres Color, Glow, FillOpacity, EdgeOpacity, Radius, EdgeWidth) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Mark")
	TObjectPtr<UMaterialInterface> MarkMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Mark", meta=(ClampMin="0"))
	float MarkGlow = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Mark", meta=(ClampMin="0", ClampMax="1"))
	float MarkFillOpacity = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Mark", meta=(ClampMin="0", ClampMax="1"))
	float MarkEdgeOpacity = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Mark", meta=(ClampMin="0", Units="Centimeters"))
	float MarkEdgeWidth = 10.0f;

	/** Le disque est reposé sur le relief quand le NME s'est déplacé de plus que cette distance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Mark", meta=(ClampMin="1", Units="Centimeters"))
	float MarkRebuildDistance = 15.0f;

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

	float GetPushbackVerticalForce() const { return PushbackVerticalForce; }

	/**
	 *  Bump (serveur ; Bumping Slash) : plus de comportement, lancé en cloche à Distance dans la direction Direction (à plat),
	 *  vers le sol à l'arrivée (rien dessous, ex. la mer : il tombe à la fin du vol), en faisant SpinTurns tours sur lui-même
	 *  (basculé dans le sens du vol). Répliqué : chaque machine calcule le même vol.
	 */
	void StartBump(const FVector& Direction, float Distance, float ApexHeight, float Duration, float SpinTurns);

	/** En vol après un bump (comportement coupé) */
	bool IsBumped() const { return Bump.bActive; }

	/** Sort encore du sol (apparition : pas de comportement ni d'attaque) */
	bool IsRising() const { return bLocalRiseActive; }

	/** Mort, corps en ragdoll avant sa disparition (plus de gameplay) */
	bool IsDying() const { return Death.bDead; }

	/** Affiche ou cache la marque au sol (Captain AMMO ; local, appelé par le personnage qui marque sur chaque machine) */
	void SetMarked(bool bMarked);

	bool IsMarked() const { return bIsMarked; }

	/** Atk critique : compte une balle d'attaque auto reçue de Shooter ; vrai si c'est la N-ième (critique ; serveur) */
	bool RegisterCriticalBullet(const AActor* Shooter, int32 Interval);

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override { TagContainer.AppendTags(GameplayTags); }

	UFUNCTION(BlueprintPure, Category="Foe|Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category="Foe|Level")
	int32 GetFoeLevel() const { return Level; }

	/** Niveau imposé à l'apparition, à la place de celui du jour (serveur, avant FinishSpawning ; overtime) */
	void SetForcedLevel(int32 NewLevel) { ForcedLevel = NewLevel; }

	/** Niveaux en plus du niveau normal (serveur, avant FinishSpawning ; circle spawns répétés de la nuit) */
	void SetLevelBonus(int32 Bonus) { LevelBonus = Bonus; }

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

	/**
	 *  XP donnée à l'équipe quand ce NME meurt : divisée par son multiplicateur de PV pour le nombre de joueurs (Health / Elite Health
	 *  Multiplier du GameMode) : plus de joueurs = NME plus solides mais autant de niveaux gagnés
	 */
	UFUNCTION(BlueprintPure, Category="Foe|Level")
	float GetExperienceReward() const { return (BaseExperience + ExperiencePerLevel * Level) / FMath::Max(PlayerCountHealthMultiplier, 0.01f); }

	/** Hurtbox : le corps */
	UPrimitiveComponent* GetHurtbox() const;

protected:

	/** Mort de l'ennemi (serveur) */
	virtual void Die();

	/** Deadzone : tombé sous le KillZ de la map (World Settings) -> mort (serveur ; rien chez les clients) */
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

	UFUNCTION()
	void OnRep_Health();

	/** Atk critique : joueurs dont la prochaine balle d'attaque auto sur ce NME sera critique (serveur, répliqué : point critique de la barre de vie) */
	UPROPERTY(ReplicatedUsing=OnRep_CriticalReadyShooters)
	TArray<TObjectPtr<APlayerState>> CriticalReadyShooters;

	UFUNCTION()
	void OnRep_CriticalReadyShooters();

	/** Rafraîchit la barre de vie avec Health / MaxHealth (élite : pas de barre, seulement le point critique) et le point critique du joueur local */
	void UpdateHealthBar();

	/** Retourne le pawn joueur le plus proche (sur le plan horizontal), ou nullptr ; ceux qui nagent seulement si bIncludeSwimmers (NME à distance) */
	APawn* FindNearestPlayer(bool bIncludeSwimmers = false) const;

	/** Le sol devant le NME (Distance dans Direction) est la mer : il s'arrête au bord au lieu d'y entrer (cible qui nage) */
	bool IsWaterAhead(const FVector& Direction, float Distance) const;

	/** Blesse les joueurs dont la hurtbox touche la Hitbox (serveur) */
	void ApplyContactDamage();

	/**
	 *  Joueurs vivants (Contact Hit Tags) dont la hurtbox touche la Hitbox : test direct contre chaque joueur (la Hitbox n'a
	 *  ni collision ni overlaps : dans une foule, les overlaps NME <-> NME coûtaient cher pour rien)
	 */
	void GetPlayersInHitbox(TArray<AYSPlayerCharacter*>& OutPlayers) const;

	/** Perf : déplacement / barre de vie / actions des armes ralentis si le NME est loin de tous les joueurs et hors écran */
	void UpdateFarTicking();

	/** Séparation douce des NME qui se traversent : input qui l'écarte de ses voisins (serveur, grille de voisinage) */
	FVector ComputeSeparation() const;

	/** Perf : armes tenues sans collision ni overlaps, mesh fusionné, ombres (une fois par arme créée) */
	void OptimizeWeapons();

	/** Actions de mêlée (Auto swing) des armes tenues */
	void GetMeleeActions(TArray<UYSAutoSwingAbility*>& OutActions) const;

	/** NME armé : un joueur touche la hitbox -> l'attaque de l'arme part si elle est prête (serveur) */
	void TriggerMeleeAttack();

	/**
	 *  Joueur qui a déclenché l'attaque de mêlée en cours (serveur) : le NME reste tourné vers lui pendant l'attaque, même si sa
	 *  cible de déplacement est un autre joueur (porteur d'objet attiré, joueur plus petit dont le centre est plus proche)
	 */
	TWeakObjectPtr<APawn> MeleeVictim;

	/** Une attaque de mêlée est en cours sur cette machine (animation) */
	bool IsMeleeAttacking() const;

	/** Animation de l'attaque de mêlée sur toutes les machines (armes de NME locales, non répliquées ; Anim = swing principal / second swing */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastMeleeAttack(EYSSwingAnim Anim);

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

	/** Bump en cours (serveur, répliqué) */
	UPROPERTY(ReplicatedUsing=OnRep_Bump)
	FYSFoeBump Bump;

	UFUNCTION()
	void OnRep_Bump();

	/** Début du vol sur cette machine : mouvement du personnage coupé (la position est calculée par UpdateBump), attaque en cours arrêtée */
	void BeginBumpLocal();

	/** Attaques des armes en cours arrêtées sur cette machine (coup d'épée, lancer de bombe ; la visée du Skull Gun est arrêtée par StartBump) */
	void CancelWeaponAttacks();

	/** Position et rotation du vol (toutes machines) ; fin du vol à la durée écoulée */
	void UpdateBump();

	/** Fin du vol sur cette machine : posé à l'arrivée, mouvement rétabli (serveur : il retombe s'il n'y a pas de sol) */
	void EndBumpLocal();

	/** Sortie du sol (serveur, répliquée) */
	UPROPERTY(ReplicatedUsing=OnRep_Rise)
	FYSFoeRise Rise;

	UFUNCTION()
	void OnRep_Rise();

	/** Apparition (serveur, BeginPlay) : placé sous le sol, sort jusqu'à sa position */
	void StartRise();

	/** Début / position / fin de la sortie sur cette machine (comme le bump : mouvement du personnage coupé) */
	void BeginRiseLocal();
	void UpdateRise();
	void EndRiseLocal();

	/** Sortie interrompue (serveur, coup qui pousse) : tout de suite au sol, comportement démarré */
	void FinishRiseNow();

	bool bLocalRiseActive = false;

	/** Mort (serveur, répliquée) : ragdoll sur chaque machine */
	UPROPERTY(ReplicatedUsing=OnRep_Death)
	FYSFoeDeath Death;

	UFUNCTION()
	void OnRep_Death();

	/** Corps en ragdoll sur cette machine : plus de collision de gameplay ni de tags (plus ciblé), armes arrêtées */
	void BeginDeathLocal();

	/** Rétrécissement après Death Scale Delay ; à 0 : détruit (serveur) */
	void UpdateDeath();

	/** Vitesse du corps à la mort : pushback maximal du tueur dans la direction du coup (serveur, TakeDamage) */
	FVector ComputeDeathImpulse(const FDamageEvent& DamageEvent, const AController* EventInstigator) const;

	bool bLocalDeathStarted = false;
	double DeathStartTime = 0.0;
	FVector DeathBodyScale = FVector::OneVector;

	/** Vitesse du corps pour la prochaine mort (renseignée par le coup fatal) ; tombé dans la deadzone : pas de ragdoll */
	FVector PendingDeathImpulse = FVector::ZeroVector;
	bool bSkipDeathAnim = false;

	/** Marque affichée : disque reposé sur le relief sous le NME quand il s'est assez déplacé */
	void UpdateMark();

	bool bIsMarked = false;
	FVector LastMarkLocation = FVector::ZeroVector;

	/** Compteur du bump déjà commencé sur cette machine */
	uint8 LocalBumpCounter = 0;
	bool bLocalBumpActive = false;

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

	/** Perf : ticks ralentis (loin et hors écran), prochaine vérification, armes déjà optimisées */
	bool bFarTicking = false;
	float FarTickCheckTimer = 0.0f;
	TWeakObjectPtr<AYSWeapon> OptimizedWeapons[2];


	/** Recharge de la bombe depuis la dernière (serveur, plafonnée à ThrowInterval) */
	float ThrowTimer = 0.0f;

	/** Skull Gun : visée en cours (serveur), direction figée et temps restant */
	bool bAiming = false;
	FVector AimDirection = FVector::ForwardVector;
	float AimTimeLeft = 0.0f;


	/** Atk critique : balles d'attaque auto reçues de chaque tireur (serveur) */
	TMap<TWeakObjectPtr<const AActor>, int32> CriticalBulletCounts;

	/** Prochain instant (temps du monde) où chaque joueur touché peut reprendre des dégâts de contact */
	TMap<TWeakObjectPtr<AActor>, double> NextContactDamageTime;
};
