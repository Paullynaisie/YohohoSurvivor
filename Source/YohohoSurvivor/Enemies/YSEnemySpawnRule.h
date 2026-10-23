#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "YSEnemySpawnRule.generated.h"

class AYSFoeCharacter;
class AYSGameMode;
class APawn;

/** « Circle spawn » : une immense vague de NME répartis sur tout le contour de la plage de l'île */
USTRUCT(BlueprintType)
struct FYSCircleSpawn
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Circle Spawn")
	bool bEnabled = true;

	/** NME du cercle (un tiré au hasard par emplacement, selon leur Spawn Weight) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Circle Spawn")
	TArray<TSubclassOf<AYSFoeCharacter>> EnemyClasses;

	/** Nombre max de NME du cercle (répartis à intervalles réguliers sur le contour ; moins si une direction n'a pas de plage) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Circle Spawn", meta=(ClampMin="0"))
	int32 MaxEnemies = 50;
};

/**
 *  Règle de spawn des NME (serveur uniquement), instanciée dans le GameMode (liste SpawnRules, catégorie Spawn) :
 *  on choisit le type de règle dans la liste déroulante et on règle ses paramètres directement dans le BP du GameMode.
 *  Plusieurs règles peuvent tourner ensemble. Démarre quand la partie démarre (tous les joueurs ont choisi).
 *
 *  Classe de base : outils communs (joueurs en jeu, temps de partie, emplacement hors écran, spawn).
 *  Nouveau type de spawn = nouvelle sous-classe qui surcharge TickRule (et éventuellement StartRule).
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UYSEnemySpawnRule : public UObject
{
	GENERATED_BODY()

public:

	/** Désactive la règle sans la retirer de la liste */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn")
	bool bEnabled = true;

	/** NME à faire apparaître (un tiré au hasard à chaque spawn) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn")
	TArray<TSubclassOf<AYSFoeCharacter>> EnemyClasses;

	/** « Hors écran » : aucun NME n'apparaît à moins de cette distance (cm) d'un joueur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Placement", meta=(ClampMin="0", Units="cm"))
	float SafeRadius = 1500.0f;

	/** Les NME apparaissent entre SafeRadius et SafeRadius + SpawnBandWidth d'un des joueurs (sinon ils pourraient apparaître à l'autre bout de la map) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Placement", meta=(ClampMin="0", Units="cm"))
	float SpawnBandWidth = 1000.0f;

	/** Écart de hauteur max entre le sol trouvé et les pieds du joueur de référence (évite le haut des obstacles) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Placement", meta=(ClampMin="0", Units="cm"))
	float MaxHeightDifference = 150.0f;

	/** Essais d'emplacement par NME avant d'abandonner ce NME */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Placement", meta=(ClampMin="1"))
	int32 MaxPlacementAttempts = 20;

	/** Nombre max de NME vivants dans la map (0 = illimité) : les spawns au-delà sont ignorés */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn", meta=(ClampMin="0"))
	int32 MaxAliveEnemies = 0;

	/** Appelé par le GameMode au démarrage de la partie */
	void Start(AYSGameMode* InGameMode);

	/** Appelé par le GameMode au passage du jour à la nuit (bNight) ou de la nuit au jour */
	void NotifyPhaseChanged(bool bNight) { OnPhaseChanged(bNight); }

	/** Appelé par le GameMode à chaque tick tant que la partie est en cours */
	void Tick(float DeltaSeconds);

	/** Joueurs de la partie pour la taille des vagues : vivants + morts (pas ceux qui n'ont pas encore choisi) */
	int32 GetPlayersInMatchCount() const;

	/** Fait apparaître Count NME, chacun à un emplacement hors écran différent. Renvoie le nombre réellement apparus. */
	int32 SpawnEnemies(int32 Count);

	/** Un NME de la liste, tiré au hasard selon leur Spawn Weight (catégorie Foe|Spawn du NME) */
	static TSubclassOf<AYSFoeCharacter> PickEnemyClass(const TArray<TSubclassOf<AYSFoeCharacter>>& Classes);

protected:

	virtual void StartRule() {}

	virtual void TickRule(float DeltaSeconds) {}

	virtual void OnPhaseChanged(bool bNight) {}

	/** Fait apparaître un NME de Class hors écran ; nullptr si aucun emplacement trouvé */
	AYSFoeCharacter* SpawnEnemy(TSubclassOf<AYSFoeCharacter> EnemyClass);

	virtual UWorld* GetWorld() const override;

	/** Pawns des joueurs en jeu (ayant leur personnage, vivants) : servent au placement */
	TArray<APawn*> GetActivePlayers() const;

	/** Secondes écoulées depuis le démarrage de la partie */
	float GetMatchElapsedTime() const;

	/** Emplacement au sol hors de SafeRadius de tous les joueurs, dans la bande autour de l'un d'eux */
	bool FindSpawnLocation(const TArray<APawn*>& Players, float CapsuleHalfHeight, FVector& OutLocation) const;

	int32 CountAliveEnemies() const;

	UPROPERTY(Transient)
	TObjectPtr<AYSGameMode> GameMode;
};

/**
 *  Vagues jour / nuit : toutes les WaveInterval secondes (NightWaveInterval la nuit), X NME apparaissent,
 *  X = nb de joueurs de la partie (morts compris) × EnemiesPerPlayer (NightEnemiesPerPlayer la nuit) × jour en cours.
 *  Au début de chaque nuit, un élite (EliteClasses) apparaît au centre de l'île ; sa mort fait passer au jour suivant (AYSGameMode).
 *  La nuit, pas de vagues par timer (bNightTimedWaves) : seuls les events font apparaître des NME (circle spawn sur la plage
 *  avec l'élite, second circle spawn quand l'élite passe sous EliteHealthCircleThreshold de ses PV).
 *  La première vague d'une phase arrive FirstWaveDelay secondes après son début.
 */
UCLASS(meta=(DisplayName="Vagues jour / nuit (par joueur et par jour)"))
class UYSTimedWaveSpawnRule : public UYSEnemySpawnRule
{
	GENERATED_BODY()

public:

	/** Jour : secondes entre deux vagues */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Waves", meta=(ClampMin="0.1", Units="s"))
	float WaveInterval = 5.0f;

	/** Jour : NME par joueur et par jour */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Waves", meta=(ClampMin="0"))
	int32 EnemiesPerPlayer = 1;

	/** Délai avant la première vague d'une phase (jour ou nuit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Waves", meta=(ClampMin="0", Units="s"))
	float FirstWaveDelay = 0.0f;

	/** Nuit : secondes entre deux vagues */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Night", meta=(ClampMin="0.1", Units="s"))
	float NightWaveInterval = 30.0f;

	/** Nuit : NME par joueur et par jour */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Night", meta=(ClampMin="0"))
	int32 NightEnemiesPerPlayer = 5;

	/** Nuit : élite qui apparaît au début de la nuit (un tiré au hasard) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Night")
	TArray<TSubclassOf<AYSFoeCharacter>> EliteClasses;

	/** Nuit : vagues par timer (NightWaveInterval / NightEnemiesPerPlayer). Décoché = la nuit, seuls les events (élite, circle spawns) font apparaître des NME */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Night")
	bool bNightTimedWaves = false;

	/** Nuit : l'élite apparaît au centre de l'île (Island Center du GameMode) ; sinon hors écran comme les autres NME */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Night")
	bool bEliteAtIslandCenter = true;

	/** Circle spawn lancé en même temps que l'élite, au début de la nuit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Night Events")
	FYSCircleSpawn NightStartCircleSpawn;

	/** Circle spawn lancé une fois par nuit quand l'élite passe sous EliteHealthCircleThreshold de ses PV max */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Night Events")
	FYSCircleSpawn EliteHealthCircleSpawn;

	/** Part des PV max de l'élite sous laquelle EliteHealthCircleSpawn se déclenche */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Night Events", meta=(ClampMin="0", ClampMax="1"))
	float EliteHealthCircleThreshold = 0.5f;

	/** Taille de vague calculée maintenant (nb de joueurs × NME par joueur (jour ou nuit) × jour en cours) */
	int32 GetWaveSize() const;

protected:

	virtual void StartRule() override;

	virtual void TickRule(float DeltaSeconds) override;

	virtual void OnPhaseChanged(bool bNight) override;

	/** Fait apparaître l'élite ; faux si aucun emplacement trouvé (réessayé au tick suivant) */
	bool SpawnElite();

	/** Circle spawn (AYSGameMode::SpawnCircle, réglages de l'île sur le GameMode) ; renvoie le nombre apparus */
	int32 SpawnCircle(const FYSCircleSpawn& Circle, const FString& Reason);

	bool IsNight() const;

	int32 GetDayNumber() const;

	/** Temps de partie de la prochaine vague */
	float NextWaveTime = 0.0f;

	/** L'élite de la nuit en cours reste à faire apparaître */
	bool bElitePending = false;

	/** Prochain essai d'apparition de l'élite (temps de partie) et échec déjà signalé dans le log */
	float NextEliteAttemptTime = 0.0f;
	bool bEliteFailureLogged = false;

	/** Élite de la nuit en cours, et circle spawn de ses PV déjà lancé */
	TWeakObjectPtr<AYSFoeCharacter> NightElite;
	bool bEliteHealthCircleDone = false;
};
