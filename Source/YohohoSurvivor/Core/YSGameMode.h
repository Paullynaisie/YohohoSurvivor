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
	float LevelUpBaseExperience = 5.0f;

	/** XP supplémentaire nécessaire par niveau déjà atteint (XP requise = Base + ParNiveau × niveau) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level", meta=(ClampMin="0"))
	float LevelUpExperiencePerLevel = 5.0f;

	/** Nombre d'upgrades proposées à chaque montée de niveau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level", meta=(ClampMin="1"))
	int32 UpgradeChoiceCount = 3;

	/** Bouton « Relance » du menu d'upgrade : chaque joueur peut retirer ses propositions autant qu'il veut (jamais les upgrades de classe) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level")
	bool bAllowUpgradeReroll = true;

	/** Niveau d'équipe où chaque joueur choisit sa classe (upgrades de classe de son personnage, sans hasard) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level", meta=(ClampMin="1"))
	int32 ClassUpgradeLevel = 5;

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
	float ShoreSearchRadius = 6300.0f;

	/** Circle spawn : pas de la recherche de la plage, du large vers le centre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Island", meta=(ClampMin="10", Units="Centimeters"))
	float ShoreSearchStep = 50.0f;

	/** Circle spawn : les NME sont posés à cette distance à l'intérieur de la limite de l'eau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Island", meta=(ClampMin="0", Units="Centimeters"))
	float ShoreInset = 200.0f;

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

public:

	AYSGameMode();

	virtual void InitGameState() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

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
	int32 SpawnCircle(const TArray<TSubclassOf<AYSFoeCharacter>>& Classes, int32 Count, const FString& Reason);

	/** NME de la classe posé sur le sol Ground (taille de l'élite comprise), tourné vers FacingTarget ; nullptr si la place est prise */
	AYSFoeCharacter* SpawnFoeOnGround(UClass* FoeClass, const FVector& Ground, const FVector& FacingTarget);

	/** Sol de l'île (Landscape seulement : ponton, bateau, palmiers traversés) à la verticale de Point, hors de l'eau */
	bool FindIslandGround(const FVector& Point, FVector& OutGround) const;

	/** Point de plage dans la direction Angle (radians) depuis IslandCenter : premier sol hors de l'eau en venant du large, puis ShoreInset vers l'intérieur */
	bool FindShorePoint(float Angle, FVector& OutGround) const;

	const FVector& GetIslandCenter() const { return IslandCenter; }

	/** Doublons gagnés par l'équipe (vente au Brooker ; serveur) */
	void AddDoubloons(int32 Amount);

	/** Vague supplémentaire hors écran : EnemiesPerPlayer × joueurs de la partie (NME de la première règle de spawn active) */
	int32 SpawnBonusWave(int32 EnemiesPerPlayer, const TCHAR* Reason);

	/** Carte au trésor au hasard dans la zone (début de jour) ; nullptr si aucun emplacement n'a de chemin complet */
	AYSTreasureMap* SpawnTreasureMap();

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

	/** Prochain numéro de joueur (nom par défaut « Player N », couleur par défaut) */
	int32 NextPlayerNumber = 1;

	/** Démarre la partie si tous les joueurs connectés ont choisi */
	void TryStartMatch();

	/** Le joueur a-t-il déjà son personnage ? */
	static bool HasCharacter(const AController* Controller);
};
