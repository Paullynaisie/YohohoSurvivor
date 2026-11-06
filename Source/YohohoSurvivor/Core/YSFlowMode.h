#pragma once

#include "CoreMinimal.h"
#include "Core/YSGameMode.h"
#include "YSFlowMode.generated.h"

class AYSIsland;
class UYSIslandData;
class UYSGameBoatData;
class AYSGameBoatLongboat;

/** « Go naviguer » : une vague en mer, comptée depuis le moment où une île commence à couler */
USTRUCT(BlueprintType)
struct FYSNavigationWave
{
	GENERATED_BODY()

	/** Secondes après le début de la coulaison */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation", meta=(ClampMin="0", Units="s"))
	float Delay = 10.0f;

	/** Chaloupes de NME (Jolly Ship Class du GameMode) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation", meta=(ClampMin="0"))
	int32 JollyShips = 0;

	/** NME Galions (Galion Class du GameMode) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation", meta=(ClampMin="0"))
	int32 Galions = 0;

	/** Nombres x Spawn Multiplier du nombre de joueurs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	bool bScaleWithPlayers = false;
};

/**
 *  Gamemode final (BP_FlowMod) : le déroulement de la partie dépend des îles. L'île de départ est générée à chaque lancement ;
 *  chaque île a son cycle (jour, nuit, coulaison), le jour global passe au suivant dès qu'une nuit se termine quelque part.
 *  Graine imposée pour les tests : -YSIslandSeed=<n>
 */
UCLASS()
class AYSFlowMode : public AYSGameMode
{
	GENERATED_BODY()

public:

	/** Île générée au démarrage de la map */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow")
	TObjectPtr<UYSIslandData> StartIsland;

	/** Le bateau des joueurs et ses étapes ; vide = pas de mise en scène de départ (bateau échoué de l'île, joueurs sur la plage) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow")
	TObjectPtr<UYSGameBoatData> GameBoatData;

	/** Îles qui apparaissent quand l'île de départ commence à couler : en triangle (cercle) autour d'elle, places tirées au hasard */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Next Islands")
	TArray<TObjectPtr<UYSIslandData>> NextIslands;

	/** Écart entre la côte de l'île de départ et celle de chaque nouvelle île (rayons moyens) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Next Islands", meta=(ClampMin="0", Units="Centimeters"))
	float NextIslandsGap = 50000.0f;

	/** Décalage au hasard de la direction de chaque île, autour de sa place régulière */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Next Islands", meta=(ClampMin="0", ClampMax="60", Units="Degrees"))
	float NextIslandsAngleJitter = 20.0f;

	/** « Go naviguer » : vagues en mer à chaque fois qu'une île commence à couler (défaut : 3 chaloupes à 10 s, 1 Galion à 15 s, 6 chaloupes à 45 s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Navigation")
	TArray<FYSNavigationWave> NavigationWaves;

	AYSFlowMode();

	virtual void InitGameState() override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/** Une île commence à couler (serveur) : « go naviguer », et les îles suivantes si c'est l'île de départ */
	void OnIslandSinkStarted(AYSIsland* SinkingIsland);

	/** Tous les objectifs principaux d'une île sont livrés (serveur) : GameBoat à l'étape suivante, et les îles suivantes si c'est l'île de départ */
	void OnIslandMainObjectivesDone(AYSIsland* DoneIsland);

	/** Îles suivantes autour de From (serveur, une seule fois) */
	void SpawnNextIslands(const AYSIsland* From);

	/** « Go naviguer » (serveur) : programme les vagues en mer */
	void StartNavigationWaves();

	AYSIsland* GetStartIslandActor() const { return Island; }

	const TArray<TObjectPtr<AYSIsland>>& GetSpawnedIslands() const { return SpawnedIslands; }

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	virtual void RestartPlayer(AController* NewPlayer) override;

	/** L'île vient de se construire (serveur) : chaloupe posée sur l'eau en face de la plage ; faux = pas de GameBoat */
	bool SpawnGameBoat(AYSIsland* ForIsland);

	/** La chaloupe a débarqué les joueurs : elle devient le point de dépôt des objectifs de l'île */
	void OnGameBoatLanded(AYSGameBoatLongboat* Boat, const FVector& Landing);

	/** GameBoat : étape suivante, sur l'eau à côté de l'étape actuelle (serveur) */
	void UpgradeGameBoat();

	virtual void OnEliteKilled(AYSFoeCharacter* Elite) override;

	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

protected:

	virtual void OnDayStarted(int32 Day) override;

	virtual void OnAllPlayersReady() override;

private:

	UPROPERTY(Transient)
	TObjectPtr<AYSIsland> Island;

	/** Îles apparues après l'île de départ */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AYSIsland>> SpawnedIslands;

	bool bNextIslandsSpawned = false;

	/** Vagues en mer à venir : temps de partie et vague */
	TArray<TPair<float, FYSNavigationWave>> PendingNavigationWaves;

	/** Rang du prochain joueur qui apparaît */
	int32 NextPlayerSpot = 0;

	/** Étape actuelle du GameBoat */
	int32 GameBoatStage = 0;

	/** Chaloupe de départ encore en route, ou nullptr */
	AYSGameBoatLongboat* GetArrivingBoat() const;
};
