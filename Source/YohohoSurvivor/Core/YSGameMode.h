#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/YSCustomization.h"
#include "YSGameMode.generated.h"

class AYSPlayerCharacter;
class UYSEnemySpawnRule;
class AYSFoeCharacter;
class AYSTreasureMap;
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

	/** Niveau d'équipe où chaque joueur choisit sa classe (upgrades de classe de son personnage, sans hasard) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level", meta=(ClampMin="1"))
	int32 ClassUpgradeLevel = 5;

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

	/** Objectif commun : doublons à réunir en revendant des coffres au Brooker (jauge d'objectif du HUD) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective", meta=(ClampMin="1"))
	int32 DoubloonGoal = 100;

	/** Carte au trésor qui apparaît au hasard au début de chaque jour (vide = aucune) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TreasureMap")
	TSubclassOf<AYSTreasureMap> TreasureMapClass;

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

	bool IsUpgradeRerollAllowed() const { return bAllowUpgradeReroll; }

	/** Propositions d'un level up (pas de classe) : système de shards (bShardUpgrades) ou anciennes upgrades du pool */
	TArray<FYSUpgradeOffer> PickRandomUpgradeOffers(const AYSPlayerCharacter* Character, int32 Count) const;

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

	/** Mort de l'élite : la nuit se termine, jour suivant */
	void OnEliteKilled(AYSFoeCharacter* Elite);

	/** Pavé numérique (map de test) : NME dont le Numpad Spawn Key vaut Key, devant le personnage du joueur ; nullptr si rien */
	AYSFoeCharacter* SpawnNumpadFoe(const APlayerController* Player, int32 Key);

	/** NME de la classe posé au sol à Distance devant Pawn, tourné vers lui (debug, pavé numérique) */
	AYSFoeCharacter* SpawnFoeInFront(UClass* FoeClass, const APawn* Pawn, float Distance);

	/** Ctrl gauche + pavé numérique (map de test) : circle spawn de NumpadCircleCount NME de la touche ; nombre apparus */
	int32 SpawnNumpadCircle(int32 Key);

	/** Circle spawn : Count NME (tirés dans Classes selon leur Spawn Weight) répartis à intervalles réguliers sur le contour de la plage, tournés vers le centre */
	int32 SpawnCircle(const TArray<TSubclassOf<AYSFoeCharacter>>& Classes, int32 Count, const FString& Reason, int32 ForcedLevel = INDEX_NONE);

	/** NME de la classe posé sur le sol Ground (taille de l'élite comprise), tourné vers FacingTarget ; nullptr si la place est prise ; ForcedLevel = niveau imposé */
	AYSFoeCharacter* SpawnFoeOnGround(UClass* FoeClass, const FVector& Ground, const FVector& FacingTarget, int32 ForcedLevel = INDEX_NONE);

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

	/** Vague supplémentaire hors écran : EnemiesPerPlayer (1 joueur) × Spawn Multiplier du nombre de joueurs (NME de la première règle de spawn active) */
	int32 SpawnBonusWave(int32 EnemiesPerPlayer, const TCHAR* Reason);

	/** Joueurs de la partie : ceux qui ont leur personnage + les morts */
	int32 GetPlayersInMatchCount() const;

	/** Ligne de PlayerCountScaling pour le nombre de joueurs de la partie */
	FYSPlayerCountScaling GetPlayerCountScaling() const;

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

	/** Début d'un jour (partie, mort de l'élite) : overtime à partir de Overtime Day */
	void OnDayStarted(int32 Day);

	/** Overtime : circle spawns dus (Overtime Waves) */
	void TickOvertime();

	/** Joueurs à bord du bateau (« X/Y ») et victoire quand ils y sont tous */
	void TickShipBoarding();

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
