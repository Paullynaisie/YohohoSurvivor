#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/YSCustomization.h"
#include "Core/YSShipRewardTypes.h"
#include "Core/YSGameState.h"
#include "Upgrades/YSShardUpgrade.h"
#include "YSGameMode.generated.h"

class AYSPlayerCharacter;
class UYSEnemySpawnRule;
class AYSFoeCharacter;
class AYSFoeJollyShip;
class AYSFoeGalion;
class AYSTreasureMap;
class AYSInteractiveObject;
class AYSPlayerCorpse;
class UTexture2D;
class UYSUpStats;
class UYSUpShard;
struct FYSUpgradeOffer;

/** Overtime : circle spawn lancé Delay secondes après le début de l'overtime, NME au niveau jour en cours + Level Offset */
USTRUCT(BlueprintType)
struct FYSOvertimeWave
{
	GENERATED_BODY()

	/** Secondes après le début de l'overtime (début du jour d'overtime) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Overtime", meta=(ClampMin="0", Units="s"))
	float Delay = 0.0f;

	/** NME tirés selon leur Spawn Weight */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Overtime")
	TArray<TSubclassOf<AYSFoeCharacter>> EnemyClasses;

	/** NME pour 1 joueur (x Spawn Multiplier du nombre de joueurs, AYSGameMode::PlayerCountScaling) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Overtime", meta=(ClampMin="1"))
	int32 EnemiesPerPlayer = 25;

	/** Niveau des NME = numéro du jour + ce décalage (jour 3, +3 = niveau 6) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Overtime")
	int32 LevelOffset = 3;
};

/** Scaling des NME pour un nombre de joueurs (ligne de AYSGameMode::PlayerCountScaling) */
USTRUCT(BlueprintType)
struct FYSPlayerCountScaling
{
	GENERATED_BODY()

	/** Nombre de NME qui apparaissent (vagues, circle spawns, vagues bonus) = valeur pour 1 joueur x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Count", meta=(ClampMin="0"))
	float SpawnMultiplier = 1.0f;

	/** PV des NME (hors élites) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Count", meta=(ClampMin="0"))
	float HealthMultiplier = 1.0f;

	/** PV des élites (boss), à la place de Health Multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Count", meta=(ClampMin="0"))
	float EliteHealthMultiplier = 1.0f;
};

/** Une place du Hired crew (serveur) : son HiredMouss, et ce qu'il garde entre deux vies (set, upgrades, shards) */
USTRUCT()
struct FYSHiredCrewSlot
{
	GENERATED_BODY()

	/** Numéro affiché (« Hired Mouss N ») */
	UPROPERTY()
	int32 Number = 1;

	UPROPERTY()
	int32 ColorIndex = 0;

	UPROPERTY()
	int32 FaceIndex = 0;

	/** Personnage en vie (ou en train de mourir) ; nullptr = mort, en attente de réanimation, ou pas encore apparu */
	UPROPERTY()
	TObjectPtr<AYSPlayerCharacter> Mouss;

	/** Déjà apparu une fois : il revient au bateau (réanimation), sinon il apparaît à côté du chef */
	UPROPERTY()
	bool bSpawned = false;

	/** Mort à cette heure (temps de jeu) : réanimé après Hired Revive Delay */
	double DeathTime = -1.0;

	/** Gardé à sa mort (le set d'arme ne change jamais : pas de choix de classe pour le Hired crew) */
	UPROPERTY()
	TArray<TObjectPtr<class UYSUpgrade>> UpgradePool;

	UPROPERTY()
	TArray<FYSUpgradeProgress> UpgradeProgress;

	UPROPERTY()
	TArray<FYSShardLevel> ShardLevels;

	/** Level up passés pendant qu'il était mort : upgrades tirées et prises à sa réanimation */
	int32 MissedLevelUps = 0;
};

/**
 *  GameMode du jeu (serveur uniquement), avec sélection du personnage :
 *  les joueurs arrivent sans pawn et choisissent un personnage dans le menu (AYSPlayerController).
 *  Quand tous les joueurs connectés ont choisi, la partie démarre et chacun apparaît avec son personnage.
 *  Un joueur qui arrive en cours de partie apparaît dès qu'il a choisi.
 *  XP commune à l'équipe (AddTeamExperience) : à chaque niveau, partie en pause et choix d'une upgrade par joueur.
 *  Classe de base de BP_TestGameMode.
 */
UCLASS(meta=(PrioritizeCategories="Spawn Objective TreasureMap DayNight Level"))
class AYSGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:

	/**
	 *  Règles de spawn des NME, actives dès le démarrage de la partie (choisir le type dans la liste, régler ses paramètres).
	 *  Plusieurs règles peuvent tourner ensemble ; pour d'autres réglages, faire un BP enfant de ce GameMode.
	 */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="Spawn")
	TArray<TObjectPtr<UYSEnemySpawnRule>> SpawnRules;

	/** Personnages proposés dans le menu de sélection (dans l'ordre des boutons) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Select")
	TArray<TSubclassOf<AYSPlayerCharacter>> SelectableCharacters;

	/** Couleurs proposées à l'écran de sélection : une ligne par couleur (DT_CustomizationColors), dans l'ordre des boutons */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Select", meta=(RowType="/Script/YohohoSurvivor.YSCustomizationColor"))
	TObjectPtr<UDataTable> CustomizationColorTable;

	/** Visages proposés à l'écran de sélection : une ligne par visage (DT_CustomizationFaces), dans l'ordre des boutons */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Select", meta=(RowType="/Script/YohohoSurvivor.YSCustomizationFace"))
	TObjectPtr<UDataTable> CustomizationFaceTable;

	/** Sans table de couleurs : couleurs proposées (ancienne liste, vidée une fois reprise dans la table) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Select|Without Table")
	TArray<FYSCustomizationColor> CustomizationColors;

	/** Sans table de visages : visages proposés (ancienne liste, vidée une fois reprise dans la table) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Select|Without Table")
	TArray<TObjectPtr<UTexture2D>> CustomizationFaces;

	/** Longueur max d'un pseudo */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Select", meta=(ClampMin="1"))
	int32 MaxNicknameLength = 16;

	/** XP nécessaire pour passer du niveau 0 au niveau 1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level", meta=(ClampMin="1"))
	float LevelUpBaseExperience = 50.0f;

	/** XP supplémentaire nécessaire par niveau déjà atteint (XP requise = Base + ParNiveau × niveau) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level", meta=(ClampMin="0"))
	float LevelUpExperiencePerLevel = 70.0f;

	/** Nombre d'upgrades proposées à chaque montée de niveau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level", meta=(ClampMin="1"))
	int32 UpgradeChoiceCount = 3;

	/** Bouton « Relance » du menu d'upgrade : chaque joueur peut retirer ses propositions autant qu'il veut (jamais les upgrades de classe) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level")
	bool bAllowUpgradeReroll = true;

	/** Relances par joueur et par sélection d'upgrade (-1 = illimité) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level", meta=(ClampMin="-1", EditCondition="bAllowUpgradeReroll"))
	int32 MaxUpgradeRerolls = -1;

	/**
	 *  Réseau : pendant un choix d'upgrade, un client qui n'a pas encore choisi est vérifié toutes ces secondes (temps réel) : son état
	 *  lui est renvoyé, et s'il n'affiche pas ses propositions il en reçoit de nouvelles (0 = jamais)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level|Network", meta=(ClampMin="0", Units="Seconds"))
	float UpgradeResyncInterval = 5.0f;

	/** Réseau : un joueur prêt et vivant sans personnage (apparition ratée au lancement) est cherché toutes ces secondes (0 = jamais) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level|Network", meta=(ClampMin="0", Units="Seconds"))
	float MissingPawnCheckInterval = 5.0f;

	float MissingPawnTimer = 0.0f;

	/** Niveau d'équipe où chaque joueur choisit sa classe (upgrades de classe de son personnage, sans hasard) ; ignoré avec Doubloon Rewards */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level", meta=(ClampMin="1", EditCondition="!bDoubloonRewards"))
	int32 ClassUpgradeLevel = 5;

	/**
	 *  Récompenses de la jauge de doublons (demande de Paul, 2026-11-08) : les level up ne proposent plus d'upgrade de classe ni le Déblocage /
	 *  Golden des upgrades d'arme (le ++ d'une upgrade d'arme déjà débloquée reste proposé). À chaque jauge de doublons remplie (niveau du
	 *  bateau), une sélection d'upgrade pour tous : Doubloon Reward Cycle dans l'ordre, puis Doubloon Reward After Cycle
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level|Doubloon Rewards")
	bool bDoubloonRewards = true;

	/**
	 *  SilverCoin upgrades (récompenses des premières jauges remplies), dans l'ordre (demande de Paul du 2026-11-13) : MoveAction,
	 *  classe, upgrade d'arme, Golden MoveAction, Golden d'arme
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level|Doubloon Rewards", meta=(EditCondition="bDoubloonRewards"))
	TArray<EYSUpgradeSelectionKind> DoubloonRewardCycle = { EYSUpgradeSelectionKind::MoveAction, EYSUpgradeSelectionKind::Class, EYSUpgradeSelectionKind::Weapon,
		EYSUpgradeSelectionKind::GoldenMoveAction, EYSUpgradeSelectionKind::GoldenWeapon };

	/** SilverCoin upgrade de toutes les jauges suivantes (défaut : propositions mythiques) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level|Doubloon Rewards", meta=(EditCondition="bDoubloonRewards"))
	EYSUpgradeSelectionKind DoubloonRewardAfterCycle = EYSUpgradeSelectionKind::Mythic;

	/** Nombre de propositions d'une SilverCoin upgrade (classe et mythique comprises ; tirées au hasard s'il y en a plus) ; jamais de relance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level|Doubloon Rewards", meta=(ClampMin="1", EditCondition="bDoubloonRewards"))
	int32 SilverCoinChoiceCount = 2;

	/** À chaque level up, chaque joueur vivant récupère tous ses PV et la recharge de son Ability */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level")
	bool bRestoreOnLevelUp = true;

	/**
	 *  Upgrades du système de shards (DT_Chara_Upgrades) à la place des anciennes upgrades de l'Upgrade Pool des personnages
	 *  (dont seules les upgrades de classe restent proposées, au niveau de classe)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level|Shards")
	bool bShardUpgrades = true;

	/** Upgrades proposables : ligne AllChara (tous les personnages) + ligne de chaque personnage (exclusives) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level|Shards", meta=(RowType="/Script/YohohoSurvivor.YSCharaUpgradesRow", EditCondition="bShardUpgrades"))
	TObjectPtr<UDataTable> CharaUpgradesTable;

	/** Upgrade interdite (sa Blocking Shard au maximum) : une de celles-ci à la place (DA_Up_Stats_Roadmap, CatchUp, FullRandom) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level|Shards", meta=(EditCondition="bShardUpgrades"))
	TArray<TObjectPtr<UYSUpStats>> ReplacementUpStats;

	/**
	 *  Scaling selon le nombre de joueurs de la partie (morts compris) : ligne 1 = 1 joueur, ligne 2 = 2 joueurs...
	 *  (au-delà de la dernière ligne : la dernière). Les « NME par joueur » des vagues / circle spawns / vagues bonus
	 *  sont les valeurs pour 1 joueur, multipliées par Spawn Multiplier ; PV des NME x Health Multiplier, élites x Elite Health Multiplier.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Player Count")
	TArray<FYSPlayerCountScaling> PlayerCountScaling;

	/** Gains de stats des NME par niveau (= par jour : PV, dégâts de contact, dégâts des attaques à distance) x ce multiplicateur ; valeurs de base inchangées (demande de Paul, 2026-11-08 : 1.5) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Foe Level", meta=(ClampMin="0"))
	float FoeLevelGainMultiplier = 1.5f;

	/** Cycle jour / nuit (désactivé : toujours le jour 1, pas de nuit ; la mort d'un élite ne change pas de jour) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DayNight")
	bool bDayNightCycle = true;

	/** Durée du jour ; ensuite la nuit commence et dure tant que l'élite est en vie */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DayNight", meta=(ClampMin="1", Units="s", EditCondition="bDayNightCycle"))
	float DayDuration = 30.0f;

	/**
	 *  Map de test : les touches du pavé numérique (0 à 9) font apparaître un NME devant le personnage du joueur qui appuie.
	 *  Touche de chaque NME = son Numpad Spawn Key (Foe|Spawn) ; NME proposés = Numpad Foe Classes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Test")
	bool bNumpadFoeSpawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Test", meta=(EditCondition="bNumpadFoeSpawn"))
	TArray<TSubclassOf<AYSFoeCharacter>> NumpadFoeClasses;

	/** Distance devant le personnage où apparaît le NME */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Test", meta=(EditCondition="bNumpadFoeSpawn", ClampMin="0", Units="Centimeters"))
	float NumpadSpawnDistance = 900.0f;

	/** Ctrl gauche + touche du pavé numérique : circle spawn de ce nombre de NME de la touche sur le contour de la plage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Test", meta=(EditCondition="bNumpadFoeSpawn", ClampMin="1"))
	int32 NumpadCircleCount = 50;

	/** Touche du pavé numérique qui fait apparaître un NME Jolly Ship (Jolly Ship Class) sur l'eau ; -1 = aucune */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Test", meta=(EditCondition="bNumpadFoeSpawn", ClampMin="-1", ClampMax="9"))
	int32 NumpadJollyShipKey = -1;

	/** Touche du pavé numérique qui fait apparaître un NME Galion sur l'eau (Galion Class ; -1 = aucune) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Test", meta=(EditCondition="bNumpadFoeSpawn", ClampMin="-1", ClampMax="9"))
	int32 NumpadGalionKey = -1;

	/** NME Galion (BP_NME_Galion) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Galion")
	TSubclassOf<AYSFoeGalion> GalionClass;

	/**
	 *  Dès que le bateau est là : un Galion apparaît régulièrement, et il n'y a plus de nuit (une nuit en cours se termine, son élite reste un
	 *  NME normal ; plus d'overtime s'il n'a pas commencé). Décoché sur TestMap (pavé numérique seulement).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Galion")
	bool bGalionWaves = true;

	/** Premier Galion : ce délai après l'arrivée du bateau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Galion", meta=(ClampMin="0", Units="s", EditCondition="bGalionWaves"))
	float GalionFirstDelay = 10.0f;

	/** Secondes entre deux Galions */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Galion", meta=(ClampMin="1", Units="s", EditCondition="bGalionWaves"))
	float GalionWaveInterval = 120.0f;

	/** Pas d'emplacement libre sur l'eau : nouvel essai après ce délai */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Galion", meta=(ClampMin="0.1", Units="s", EditCondition="bGalionWaves"))
	float GalionRetryDelay = 2.0f;

	/** Chaque Galion apparu fait passer au jour suivant (nouvelle carte au trésor, niveau des NME) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Galion", meta=(EditCondition="bGalionWaves"))
	bool bGalionAdvancesDay = true;

	/** Apparition sur l'eau, à au moins cette distance de tous les joueurs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Galion", meta=(ClampMin="0", Units="Centimeters"))
	float GalionMinSpawnDistance = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Galion", meta=(ClampMin="0", Units="Centimeters"))
	float GalionMaxSpawnDistance = 25000.0f;

	/** NME Jolly Ship : vagues sur l'eau à partir du moment où le bateau est là (objectif de doublons atteint) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Jolly Ship")
	bool bJollyShipWaves = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Jolly Ship")
	TSubclassOf<AYSFoeJollyShip> JollyShipClass;

	/** Secondes entre deux vagues (la première part dès que le bateau arrive) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Jolly Ship", meta=(ClampMin="0.1", Units="s", EditCondition="bJollyShipWaves"))
	float JollyShipWaveInterval = 20.0f;

	/** Jolly Ships par vague pour 1 joueur (x Spawn Multiplier du nombre de joueurs, comme tous les NME) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Jolly Ship", meta=(ClampMin="0", EditCondition="bJollyShipWaves"))
	int32 JollyShipsPerWave = 3;

	/** Nombre max de Jolly Ships à flot (0 = illimité) : les apparitions au-delà sont ignorées */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Jolly Ship", meta=(ClampMin="0", EditCondition="bJollyShipWaves"))
	int32 JollyShipMaxAlive = 0;

	/** Apparition : à au moins cette distance de tous les joueurs (hors écran), jusqu'à Max si la mer est plus loin, par pas de Step */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Jolly Ship", meta=(ClampMin="0", Units="Centimeters"))
	float JollyShipMinSpawnDistance = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Jolly Ship", meta=(ClampMin="0", Units="Centimeters"))
	float JollyShipMaxSpawnDistance = 20000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Jolly Ship", meta=(ClampMin="10", Units="Centimeters"))
	float JollyShipSpawnStep = 500.0f;

	/** Directions essayées par Jolly Ship */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Jolly Ship", meta=(ClampMin="1"))
	int32 JollyShipSpawnAttempts = 10;

	/** Centre de l'île (apparition de l'élite la nuit, centre des circle spawns) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Island")
	FVector IslandCenter = FVector::ZeroVector;

	/** Circle spawn : distance au centre d'où l'on part du large pour chercher la plage (au-delà de la côte la plus lointaine) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Island", meta=(ClampMin="0", Units="Centimeters"))
	float ShoreSearchRadius = 9000.0f;

	/** Circle spawn : pas de la recherche de la plage, du large vers le centre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Island", meta=(ClampMin="10", Units="Centimeters"))
	float ShoreSearchStep = 50.0f;

	/** Circle spawn : les NME sont posés à cette distance à l'intérieur de la limite de l'eau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Island", meta=(ClampMin="0", Units="Centimeters"))
	float ShoreInset = 200.0f;

	/**
	 *  Port placé au hasard sur la plage au début de la partie : les acteurs de la map tagués « Harbor » (ponton, Brooker, bateau)
	 *  tournent ensemble autour du ponton (tag « HarborPivot ») qui est posé au bord de l'eau dans une direction tirée au hasard,
	 *  en gardant leurs positions relatives. Un acteur tagué « HarborLand » doit rester sur la terre ferme (Brooker),
	 *  « HarborSea » entièrement en mer (bateau), sinon une autre direction est tirée.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Island")
	bool bRandomHarbor = true;

	/** Directions essayées pour le port avant d'abandonner (le port reste alors à sa place dans la map) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Island", meta=(EditCondition="bRandomHarbor", ClampMin="1"))
	int32 HarborPlacementAttempts = 40;

	/** Le bateau n'existe pas au début de la partie : il apparaît quand l'objectif de doublons est atteint (GameMap ; décoché = toujours là) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship")
	bool bShipArrivesAtDoubloonGoal = true;

	/** Fin de jeu temporaire : « X/Y sur le bateau » en bas de l'écran, et quand tous les joueurs vivants sont à bord, écran « BRAVO » puis retour au menu */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship")
	bool bShipBoardingVictory = true;

	/** Secondes entre l'écran « BRAVO » et le retour au menu principal */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship", meta=(ClampMin="0", Units="s", EditCondition="bShipBoardingVictory"))
	float VictoryReturnDelay = 5.0f;

	/** Game over : le bateau des joueurs (GameBoat) coule ; « GAME OVER » puis retour au menu après ce délai (demande de Paul, 2026-11-08) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship", meta=(ClampMin="0", Units="s"))
	float GameOverReturnDelay = 8.0f;

	/** Overtime de l'île (GameMap) : la nuit d'Overtime Day, pas d'élite : circle spawns plus forts (Overtime Waves), puis l'île coule */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Overtime")
	bool bOvertime = true;

	/** Jour dont la NUIT lance l'overtime (ce jour-là reste normal) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Overtime", meta=(ClampMin="1", EditCondition="bOvertime"))
	int32 OvertimeDay = 3;

	/** Circle spawns de l'overtime (île de départ : Skull niveau jour + 3 tout de suite, puis niveau jour + 5 30 s plus tard) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Overtime", meta=(EditCondition="bOvertime"))
	TArray<FYSOvertimeWave> OvertimeWaves;

	/** Overtime : après les vagues, l'île (Landscape et ce qui y est accroché) descend sous l'eau, jusque sous la deadzone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Overtime", meta=(ClampMin="1", Units="s", EditCondition="bOvertime"))
	float IslandSinkDuration = 30.0f;

	/** Profondeur de la descente (île jusqu'à +421 et deadzone à -300 : au-delà de 750 tout passe sous la deadzone) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Overtime", meta=(ClampMin="0", Units="Centimeters", EditCondition="bOvertime"))
	float IslandSinkDepth = 1200.0f;

	/** Courbe de la descente (1 = vitesse constante, 2 = lente au début puis de plus en plus vite) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Overtime", meta=(ClampMin="0.1", EditCondition="bOvertime"))
	float IslandSinkEaseExponent = 1.5f;

	/** L'île commence à couler ce délai après le spawn de la dernière vague d'overtime */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Overtime", meta=(ClampMin="0", Units="s", EditCondition="bOvertime"))
	float IslandSinkDelayAfterLastWave = 30.0f;

	/**
	 *  Objectif commun : doublons à réunir en revendant des coffres au Brooker (jauge d'objectif du HUD) = XP du BATEAU : le premier
	 *  seuil le fait arriver (niveau 1), puis chaque seuil le fait monter d'un niveau (la jauge repart de 0, le surplus est gardé).
	 *  Seuil du niveau N -> N + 1 = Doubloon Goal + Doubloon Goal Per Ship Level x N (comme l'XP des joueurs)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective", meta=(ClampMin="1"))
	int32 DoubloonGoal = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective", meta=(ClampMin="0"))
	int32 DoubloonGoalPerShipLevel = 50;

	/** Hardcore : jour des NME (leur niveau) = jour en cours x ceci (jour 1 -> 2, jour 3 -> 6) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Play Mode", meta=(ClampMin="1"))
	int32 HardcoreFoeDayMultiplier = 2;

	/** Peaceful : les vagues répétées des îles (Repeat Interval > 0, hors écran) continuent tant que dure le jour (sinon : Repeat Count fois) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Play Mode")
	bool bPeacefulEndlessWaves = true;

	/** Plumes de phoenix dans le stock de l'équipe au début de la partie */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Phoenix", meta=(ClampMin="0"))
	int32 StartingPhoenixFeathers = 1;

	/** Part des PV max rendue par une plume */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Phoenix", meta=(ClampMin="0.01", ClampMax="1"))
	float PhoenixReviveHealthRatio = 1.0f;

	/** Le joueur réapparaît à cette distance du Brooker (plusieurs à la fois : tout autour) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Phoenix", meta=(ClampMin="0", Units="Centimeters"))
	float PhoenixReviveDistance = 200.0f;

	/** Hired crew : personnage piloté par IA qui prend les places vides (Blueprint enfant de Mouss avec un composant YSHiredCrewComponent) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew")
	TSoftClassPtr<AYSPlayerCharacter> HiredMoussClass;

	/** Hired crew : places comblées jusqu'à ce nombre de personnages (joueurs + HiredMouss) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew", meta=(ClampMin="1"))
	int32 HiredCrewTeamSize = 4;

	/** Un HiredMouss mort revient au bateau (à côté du core, sinon du Brooker de l'île) ce délai après la disparition de son corps, sans plume de phoenix */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew", meta=(ClampMin="0", Units="s"))
	float HiredReviveDelay = 1.0f;

	/** Part des PV max à la réanimation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew", meta=(ClampMin="0.01", ClampMax="1"))
	float HiredReviveHealthRatio = 1.0f;

	/** Apparition (début de partie) : à cette distance du chef, tout autour */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew", meta=(ClampMin="0", Units="Centimeters"))
	float HiredSpawnDistance = 250.0f;

	/** Carte au trésor qui apparaît au hasard au début de chaque jour (vide = aucune) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap")
	TSubclassOf<AYSTreasureMap> TreasureMapClass;

	/** Tous les « poste de combat [item] » : un objectif (ou une carte au trésor) qui en crée un le tire au hasard dans cette liste */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat Posts")
	TArray<TSubclassOf<AYSInteractiveObject>> CombatPostItemClasses;

	/** Zone d'apparition de la carte : centre (Z = hauteur du sol attendue) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap")
	FVector TreasureMapAreaCenter = FVector::ZeroVector;

	/** Zone d'apparition de la carte : demi-largeur en X et Y autour du centre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap", meta=(Units="Centimeters"))
	FVector2D TreasureMapAreaExtent = FVector2D(4500.0f, 4500.0f);

	/** Emplacements essayés (sol + chemin complet) avant d'abandonner la carte du jour */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap", meta=(ClampMin="1"))
	int32 TreasureMapSpawnAttempts = 30;

	/** Une seule carte au trésor active à la fois sur l'île : si celle d'un jour n'est pas finie, aucune nouvelle carte le jour suivant */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap")
	bool bOneTreasureMapAtATime = true;

public:

	/** Un poste de combat ramassable au hasard (Combat Post Item Classes), nullptr si la liste est vide */
	TSubclassOf<AYSInteractiveObject> PickRandomCombatPostItem() const;

	const TArray<TSubclassOf<AYSInteractiveObject>>& GetCombatPostItemClasses() const { return CombatPostItemClasses; }

	AYSGameMode();

	virtual void InitGameState() override;

	/** Port au hasard (Random Harbor) : les traces du sol marchent maintenant */
	virtual void BeginPlay() override;

	/** Tire une direction de plage pour le port (Random Harbor) et la donne au GameState, qui déplace le port sur toutes les machines */
	void PlaceHarbor();

	virtual void Tick(float DeltaSeconds) override;

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/** Accepte aussi un joueur dont l'identifiant en ligne n'est pas du type de l'hôte (partie par IP : Steam chez l'un, pas chez l'autre) */
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	virtual void Logout(AController* Exiting) override;

	/** Choix d'un joueur (reçu par RPC du PlayerController) */
	void PlayerSelectedCharacter(APlayerController* Player, int32 CharacterIndex);

	/** Bouton « Prêt » d'un joueur (reçu par RPC) : la partie démarre quand tous les joueurs sont prêts */
	void PlayerSetReady(APlayerController* Player, bool bReady);

	/** Couleur déjà prise par un autre joueur (les couleurs sont uniques) */
	bool IsColorTaken(int32 ColorIndex, const APlayerState* Except) const;

	/** Personnage unique (Unique Character) déjà choisi par un autre joueur */
	bool IsCharacterTaken(int32 CharacterIndex, const APlayerState* Except) const;

	/** Personnalisation d'un joueur (reçue par RPC ; index invalides ignorés) */
	void PlayerSetCustomization(APlayerController* Player, int32 ColorIndex, int32 FaceIndex);

	/** Pseudo d'un joueur (reçu par RPC ; vide = inchangé, tronqué à MaxNicknameLength) */
	void PlayerSetNickname(APlayerController* Player, const FString& Nickname);

	/** Droitier / gaucher d'un joueur (reçu par RPC ; seulement avant d'avoir son personnage) */
	void PlayerSetLeftHanded(APlayerController* Player, bool bLeftHanded);

	/** Le personnage est réglé droitier / gaucher selon le joueur avant sa construction (armes échangées pour un gaucher) */
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

	/** XP gagnée par l'équipe (NME tué, debug) : gère les montées de niveau et la sélection d'upgrade */
	void AddTeamExperience(float Amount);

	/**
	 *  Réseau : renvoie l'état d'un joueur à son client. PlayerController, personnage, PlayerState, GameState et pause en réplication
	 *  forcée (pendant la pause du choix d'upgrade, le temps du serveur est figé : rien n'est répliqué sans ForceNetUpdate), et
	 *  possession renvoyée (ClientRestart) si le client n'a pas confirmé son personnage actuel (la relance automatique du moteur ne
	 *  tourne pas pendant la pause).
	 */
	void ResyncPlayer(APlayerController* Player, const TCHAR* Reason);

	/** Sélection d'upgrade : le client dit ne pas afficher ses propositions -> nouvelles propositions pour lui, renvoyées */
	void PlayerRequestedUpgradeResync(APlayerController* Player);

	/**
	 *  Joueurs prêts, pas morts, sans personnage alors que la partie a commencé (apparition ratée au lancement) : ils apparaissent ;
	 *  les autres sont resynchronisés (bResyncOthers). Appelé toutes les Missing Pawn Check Interval et au débarquement de la chaloupe.
	 */
	void RespawnMissingPlayers(const TCHAR* Reason, bool bResyncOthers);

	/** Réseau : un joueur qui n'a pas encore choisi son upgrade est vérifié toutes ces secondes (temps réel) */
	float GetUpgradeResyncInterval() const { return UpgradeResyncInterval; }

	bool IsUpgradeRerollAllowed() const { return bAllowUpgradeReroll; }

	/** Propositions d'un level up (pas de classe) : système de shards (bShardUpgrades) ou anciennes upgrades du pool */
	TArray<FYSUpgradeOffer> PickRandomUpgradeOffers(const AYSPlayerCharacter* Character, int32 Count) const;

	/**
	 *  Propositions d'une sélection de ce type (level up, récompense de doublons) ; sans proposition possible, le type suivant :
	 *  Golden -> Déblocage -> mythique, classe -> mythique, mythique -> level up. OutKind = type réellement proposé
	 */
	TArray<FYSUpgradeOffer> PickOffersForKind(const AYSPlayerCharacter* Character, EYSUpgradeSelectionKind Kind, EYSUpgradeSelectionKind& OutKind) const;

	/** Shard d'un type parmi les upgrades disponibles pour ce personnage (cheats, debug) ; nullptr si aucune */
	UYSUpShard* FindShard(const AYSPlayerCharacter* Character, uint8 Stat) const;

	/** Shard par nom d'asset (avec ou sans le préfixe DA_Up_Shard_) parmi les upgrades disponibles (debug) */
	UYSUpShard* FindShardByName(const AYSPlayerCharacter* Character, const FString& Name) const;

	int32 GetUpgradeChoiceCount() const { return UpgradeChoiceCount; }

	bool IsShardUpgrades() const { return bShardUpgrades; }

	/** Choix d'upgrade d'un joueur (reçu par RPC du PlayerController ; index dans ses propositions) */
	void PlayerChoseUpgrade(APlayerController* Player, int32 OfferIndex);

	/** « Relance » d'un joueur : nouvelles propositions pour lui seul (pas pour un choix de classe), sans attendre les autres */
	void PlayerRerolledUpgrades(APlayerController* Player);

	/** Fin du jour : la nuit commence (élite et vagues de nuit via les règles de spawn) */
	void StartNight();

	/** Jour suivant (serveur) : nouvelle carte au trésor, règles de spawn prévenues */
	void AdvanceDay();

	/** Mort de l'élite : la nuit se termine, jour suivant */
	virtual void OnEliteKilled(AYSFoeCharacter* Elite);

	/** Pavé numérique (map de test) : NME dont le Numpad Spawn Key vaut Key, devant le personnage du joueur ; nullptr si rien */
	AYSFoeCharacter* SpawnNumpadFoe(const APlayerController* Player, int32 Key);

	/** NME de la classe posé au sol à Distance devant Pawn, tourné vers lui (debug, pavé numérique) */
	AYSFoeCharacter* SpawnFoeInFront(UClass* FoeClass, const APawn* Pawn, float Distance);

	/** Ctrl gauche + pavé numérique (map de test) : circle spawn de NumpadCircleCount NME de la touche ; nombre apparus */
	int32 SpawnNumpadCircle(int32 Key);

	/** Classes des bateaux de NME (batailles navales : défaut quand la ligne n'en donne pas) */
	UClass* GetGalionClass() const;
	UClass* GetJollyShipClass() const;

	/** Un NME Jolly Ship sur l'eau, hors écran (serveur) ; nullptr si pas de classe ou pas d'emplacement */
	AYSFoeJollyShip* SpawnJollyShip(const TCHAR* Reason);

	/** Un NME Galion sur l'eau, loin des joueurs (serveur) ; nullptr si pas de classe ou pas d'emplacement */
	AYSFoeGalion* SpawnGalion(const TCHAR* Reason);

	/** Circle spawn : Count NME (tirés dans Classes selon leur Spawn Weight) répartis à intervalles réguliers sur le contour de la plage, tournés vers le centre */
	int32 SpawnCircle(const TArray<TSubclassOf<AYSFoeCharacter>>& Classes, int32 Count, const FString& Reason, int32 ForcedLevel = INDEX_NONE, int32 LevelBonus = 0);

	/**
	 *  NME de la classe posé sur le sol Ground (taille de l'élite comprise), tourné vers FacingTarget ; nullptr si la place est prise ;
	 *  ForcedLevel = niveau imposé, LevelBonus = niveaux en plus du niveau normal
	 */
	AYSFoeCharacter* SpawnFoeOnGround(UClass* FoeClass, const FVector& Ground, const FVector& FacingTarget, int32 ForcedLevel = INDEX_NONE, int32 LevelBonus = 0);

	/** Le bateau apparaît (objectif de doublons atteint, debug) */
	void ShipArrives(const TCHAR* Reason);

	/** L'île passe en overtime maintenant (début du jour d'overtime, debug) */
	void StartOvertime();

	/** L'île commence à couler (nuit de l'overtime, debug) */
	void StartIslandSink();

	/** Sol de l'île (Landscape seulement : ponton, bateau, palmiers traversés) à la verticale de Point, hors de l'eau */
	bool FindIslandGround(const FVector& Point, FVector& OutGround) const;

	/** Point de plage dans la direction Angle (radians) depuis IslandCenter : premier sol hors de l'eau en venant du large, puis ShoreInset vers l'intérieur */
	bool FindShorePoint(float Angle, FVector& OutGround) const { return FindShorePoint(Angle, OutGround, ShoreInset); }

	/** Idem avec un autre retrait vers l'intérieur (0 = le bord de l'eau lui-même) */
	bool FindShorePoint(float Angle, FVector& OutGround, float Inset) const;

	const FVector& GetIslandCenter() const { return IslandCenter; }

	/** Doublons gagnés par l'équipe (vente au Brooker ; serveur) */
	void AddDoubloons(int32 Amount);

	/** Doublons pour passer du niveau de bateau Level au suivant (première jauge = Doubloon Goal, que le bateau soit là dès le début ou non) */
	int32 GetDoubloonGoalForShipLevel(int32 Level) const { return DoubloonGoal + DoubloonGoalPerShipLevel * FMath::Max(0, Level - (bShipArrivesAtDoubloonGoal ? 0 : 1)); }

	/** Debug (serveur) : une récompense de doublons maintenant, comme si la jauge venait d'être remplie */
	void QueueDoubloonReward(EYSUpgradeSelectionKind Kind, const TCHAR* Reason);

	/** Game over (serveur, une fois) : « GAME OVER » chez tous les joueurs puis retour au menu */
	void GameOver(const TCHAR* Reason);

	/** Fin de jeu temporaire (serveur) : « BRAVO » pour tous, puis retour au menu après ReturnDelay (négatif = Victory Return Delay) */
	void TriggerVictory(const TCHAR* Reason, float ReturnDelay = -1.0f);

	/** Récompense de la N-ième jauge de doublons remplie (0 = la première) */
	EYSUpgradeSelectionKind GetDoubloonReward(int32 FillIndex) const;

	/**
	 *  Cadavre reposé chez le Medic (serveur) : son joueur (encore dans la partie) ressuscite debout sur Ground, même personnage,
	 *  set d'arme et upgrades, à HealthRatio de ses PV ; le cadavre disparaît. Faux si personne à ressusciter.
	 */
	bool RevivePlayer(AYSPlayerCorpse* Corpse, const FVector& Ground, float HealthRatio);

	/** Ajoute des plumes de phoenix au stock de l'équipe (serveur) */
	void AddPhoenixFeathers(int32 Amount);

	/** Mode de jeu (serveur, hôte sur l'écran de sélection) : refusé une fois la partie commencée */
	void SetPlayMode(EYSPlayMode Mode);

	/** Hired crew (serveur, hôte sur l'écran de sélection) : refusé une fois la partie commencée */
	void SetHiredCrew(bool bEnabled);

	/** Un HiredMouss meurt (serveur, avant son ragdoll) : son set, ses upgrades et ses shards sont gardés pour sa réanimation */
	void OnHiredMoussDied(AYSPlayerCharacter* Mouss);

	/** Personnage que les HiredMouss suivent : celui de l'hôte, sinon un joueur vivant (nullptr si aucun) */
	AYSPlayerCharacter* GetHiredCrewLeader() const;

	/** Debug : état du Hired crew dans le log (YSHiredStatus) ; tue un HiredMouss (YSKillHired, 0 = tous) */
	void DebugHiredStatus() const;
	void DebugKillHired(int32 Number);

	/** Peaceful : les vagues répétées des îles (Repeat Interval > 0, hors écran, ex. 7 Skull toutes les 20 s) ne s'arrêtent plus après Repeat Count */
	bool UsesPeacefulEndlessWaves() const { return bPeacefulEndlessWaves; }

	/**
	 *  Consomme une plume de phoenix pour ressusciter ce joueur mort (serveur) : même personnage, set d'arme et upgrades, au Brooker du
	 *  bateau (core), sans bateau au Brooker de l'île ; Index = rang dans une réanimation groupée. Faux si rien n'a été consommé.
	 */
	bool PhoenixRevivePlayer(AController* Controller, int32 Index = 0);

	/** Vague supplémentaire hors écran : EnemiesPerPlayer (1 joueur) × Spawn Multiplier du nombre de joueurs (NME de la première règle de spawn active) */
	int32 SpawnBonusWave(int32 EnemiesPerPlayer, const TCHAR* Reason);

	/** Joueurs de la partie : ceux qui ont leur personnage + les morts */
	int32 GetPlayersInMatchCount() const;

	/** Ligne de PlayerCountScaling pour le nombre de joueurs de la partie */
	FYSPlayerCountScaling GetPlayerCountScaling() const;

	/** Gains de stats des NME par niveau x ce multiplicateur (1 sans GameMode du jeu, ex. client) */
	static float GetFoeLevelGainMultiplier(const UWorld* World);

	/** Nombre de NME pour 1 joueur -> pour les joueurs de la partie (arrondi, au moins 1 si BaseCount > 0) */
	int32 ScaleEnemyCount(int32 BaseCount) const;

	/** Carte au trésor au hasard dans la zone (début de jour) ; nullptr si aucun emplacement n'a de chemin complet */
	AYSTreasureMap* SpawnTreasureMap();

	/** Carte du début de jour : rien si une carte est encore active sur l'île (One Treasure Map At A Time) */
	void SpawnDailyTreasureMap();

	/** Carte au trésor posée à Location si un chemin complet en part (debug) */
	AYSTreasureMap* SpawnTreasureMapAt(const FVector& Location);

protected:

	/** XP nécessaire pour quitter le niveau donné */
	float GetExperienceForLevel(int32 Level) const { return LevelUpBaseExperience + LevelUpExperiencePerLevel * Level; }

	/** Met la partie en pause et propose des upgrades à chaque joueur vivant */
	void StartUpgradeSelection();

	/** Termine la sélection si tout le monde a choisi (puis enchaîne une autre si plusieurs niveaux ont été gagnés) */
	void TryFinishUpgradeSelection();

	/** Montées de niveau pas encore traitées par une sélection d'upgrade */
	int32 PendingLevelUps = 0;

	/** Récompenses de doublons pas encore traitées (passent avant les level up) */
	TArray<EYSUpgradeSelectionKind> PendingDoubloonRewards;

	/** Jauges de doublons remplies depuis le début de la partie */
	int32 DoubloonFillCount = 0;

	/** Type de la sélection en cours (relance des propositions) */
	EYSUpgradeSelectionKind CurrentSelectionKind = EYSUpgradeSelectionKind::LevelUp;

	/** Début d'un jour (partie, mort de l'élite) : overtime à partir de Overtime Day */
	virtual void OnDayStarted(int32 Day);

	/** Tout le monde est prêt, les personnages viennent d'apparaître */
	virtual void OnAllPlayersReady() {}

	/** Overtime : circle spawns dus (Overtime Waves) */
	void TickOvertime();

	/** Joueurs à bord du bateau (« X/Y ») et victoire quand ils y sont tous */
	void TickShipBoarding();

	/** Vagues de NME Jolly Ship, le bateau arrivé */
	void TickJollyShips();

	/** Temps de partie de la prochaine vague de Jolly Ships (< 0 = pas encore commencé) */
	float NextJollyShipWaveTime = -1.0f;

	/** Temps de partie du prochain Galion (< 0 = pas encore commencé) */
	float NextGalionTime = -1.0f;

	/** Galions réguliers actifs : le bateau est là et Galion Waves est coché (plus de nuit) */
	bool IsGalionPhase() const;

	void TickGalions();

	/** Tous les joueurs morts : les plumes en stock les ressuscitent automatiquement */
	void TickPhoenixAutoRevive();

	/** Hired crew : places vides comblées (ou rendues quand un joueur arrive), apparition, réanimation au bateau */
	void TickHiredCrew();

	/** Fait apparaître le HiredMouss d'une place (début de partie ou réanimation) ; faux si pas encore possible */
	bool SpawnHiredMouss(FYSHiredCrewSlot& Slot, int32 SlotIndex);

	/** Level up : chaque HiredMouss vivant prend une upgrade tirée comme pour un joueur (les morts la prendront à leur réanimation) */
	void GiveHiredCrewLevelUp();

	/** Une upgrade tirée au hasard parmi les propositions d'un level up, prise tout de suite */
	void ApplyRandomHiredUpgrade(AYSPlayerCharacter* Mouss);

	UPROPERTY(Transient)
	TArray<FYSHiredCrewSlot> HiredCrew;

	int32 NextHiredNumber = 1;

	/** Place d'apparition libre sur la chaloupe de l'intro (FlowMod) : poste où l'installer, sinon nullptr */
	virtual class AYSShipStation* FindHiredIntroSeat(FTransform& OutFeet) const { return nullptr; }

	/** Game over : tous les joueurs sont morts, sans plume de phoenix ni réanimation gratuite (demande de Paul du 2026-11-12) */
	void TickAllPlayersDead();

	/** Sol où ressusciter : à côté du Brooker du bateau, sinon de celui de l'île */
	bool FindPhoenixRevivePoint(const AActor* Body, int32 Index, FVector& OutGround) const;

	/** Temps de partie du début de l'overtime (< 0 = pas d'overtime) et prochaine vague d'overtime */
	float OvertimeStartTime = -1.0f;
	float LastOvertimeWaveTime = -1.0f;
	int32 NextOvertimeWave = 0;

	/** Prochain numéro de joueur (nom par défaut « Player N », couleur par défaut) */
	int32 NextPlayerNumber = 1;

	/** Démarre la partie si tous les joueurs connectés ont choisi */
	void TryStartMatch();

	/** Le joueur a-t-il déjà son personnage ? */
	static bool HasCharacter(const AController* Controller);
};
