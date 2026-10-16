#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "YSEnemySpawnRule.generated.h"

class AYSFoeCharacter;
class AYSGameMode;
class APawn;

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

	/** Appelé par le GameMode à chaque tick tant que la partie est en cours */
	void Tick(float DeltaSeconds);

protected:

	virtual void StartRule() {}

	virtual void TickRule(float DeltaSeconds) {}

	virtual UWorld* GetWorld() const override;

	/** Pawns des joueurs en jeu (ayant leur personnage, vivants) : servent au placement */
	TArray<APawn*> GetActivePlayers() const;

	/** Joueurs de la partie pour la taille des vagues : vivants + morts (pas ceux qui n'ont pas encore choisi) */
	int32 GetPlayersInMatchCount() const;

	/** Secondes écoulées depuis le démarrage de la partie */
	float GetMatchElapsedTime() const;

	/** Fait apparaître Count NME, chacun à un emplacement hors écran différent. Renvoie le nombre réellement apparus. */
	int32 SpawnEnemies(int32 Count);

	/** Emplacement au sol hors de SafeRadius de tous les joueurs, dans la bande autour de l'un d'eux */
	bool FindSpawnLocation(const TArray<APawn*>& Players, float CapsuleHalfHeight, FVector& OutLocation) const;

	int32 CountAliveEnemies() const;

	UPROPERTY(Transient)
	TObjectPtr<AYSGameMode> GameMode;
};

/**
 *  Vagues régulières : toutes les WaveInterval secondes, X NME apparaissent,
 *  X = nb de joueurs de la partie (morts compris) × EnemiesPerPlayer × minute en cours (minute 1 de 0 à 60 s, minute 2 de 60 à 120 s, etc.).
 */
UCLASS(meta=(DisplayName="Vagues par joueur et par minute"))
class UYSTimedWaveSpawnRule : public UYSEnemySpawnRule
{
	GENERATED_BODY()

public:

	/** Secondes entre deux vagues */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Waves", meta=(ClampMin="0.1", Units="s"))
	float WaveInterval = 5.0f;

	/** Délai avant la première vague après le démarrage de la partie */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Waves", meta=(ClampMin="0", Units="s"))
	float FirstWaveDelay = 0.0f;

	/** NME par joueur et par minute en cours */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Waves", meta=(ClampMin="0"))
	int32 EnemiesPerPlayer = 1;

	/** Durée d'une « minute » de progression (raccourcir pour tester la montée en difficulté) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn|Waves", meta=(ClampMin="1", Units="s"))
	float MinuteDuration = 60.0f;

	/** Taille de vague calculée maintenant (nb de joueurs × EnemiesPerPlayer × minute en cours) */
	int32 GetWaveSize() const;

protected:

	virtual void StartRule() override;

	virtual void TickRule(float DeltaSeconds) override;

	float NextWaveTime = 0.0f;
};
