#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Island/YSIslandGenerator.h"
#include "YSIsland.generated.h"

class AYSFoeCharacter;
class AYSInteractiveObject;
class AYSPlayerCharacter;
class AYSStartBoat;
class UProceduralMeshComponent;
class UYSIslandData;

/** Phase d'une île */
UENUM()
enum class EYSIslandPhase : uint8
{
	/** Personne n'y a encore posé le pied */
	Inactive,
	Day,
	Night,
	/** Routine de coulaison : plus d'objectif principal, ou temps écoulé */
	Sinking
};

/** État d'un objectif (serveur) */
USTRUCT()
struct FYSIslandObjectiveState
{
	GENERATED_BODY()

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	TSubclassOf<AYSInteractiveObject> DeliveryClass;

	UPROPERTY()
	TObjectPtr<AYSInteractiveObject> Item;

	bool bMain = false;
	bool bDone = false;
};

/**
 *  Une île en jeu, définie par son Data Asset (UYSIslandData). Le serveur calcule le plan (massifs, props, emplacements) à partir d'une graine et
 *  le réplique ; chaque machine construit le terrain (mesh procédural) et pose les props localement.
 *  Un joueur y pose le pied : elle s'active, prend le jour global en cours comme référence et se considère au début du jour. Chaque île a sa propre phase
 *  (jour, nuit, coulaison), le jour global passe au suivant dès qu'une nuit se termine quelque part.
 *  Plus d'objectif principal, ou Max Days atteint : routine de coulaison.
 */
UCLASS()
class AYSIsland : public AActor
{
	GENERATED_BODY()

public:

	AYSIsland();

	/** Avant FinishSpawning (serveur) : Data Asset et graine (0 = au hasard, sauf graine fixe du Data Asset) */
	void InitIsland(UYSIslandData* InData, int32 InSeed);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/** Île la plus proche de Location (nullptr si aucune) */
	static AYSIsland* FindAt(const UWorld* World, const FVector& Location);

	/** Point dans un massif de l'île (à Margin près) : les chemins et apparitions l'évitent */
	static bool IsBlockedAt(const UWorld* World, const FVector& Location, float Margin);

	bool IsNight() const { return Phase == EYSIslandPhase::Night; }

	EYSIslandPhase GetPhase() const { return Phase; }

	/** Où apparaît le joueur Index de la partie */
	FTransform GetPlayerSpawnTransform(int32 Index) const;

	/** Le jour global vient de changer (serveur) : Max Days atteint ou plus d'objectif principal = routine de coulaison, sinon nouveau jour de l'île */
	void OnGlobalDayChanged(int32 Day);

	/** Mort de l'élite de cette île (serveur) : la nuit se termine, le jour global passe au suivant */
	void OnEliteKilled(AYSFoeCharacter* Elite);

	/** Cet objet est attendu par un objectif encore ouvert (toutes machines) */
	bool IsObjectExpected(const AYSInteractiveObject* Object) const;

	/** Objet reposé dans une zone de dépôt (serveur) : l'objectif correspondant est accompli */
	void NotifyObjectDelivered(AYSInteractiveObject* Object);

	/** Accroche un acteur à l'île : il coule avec elle */
	void AttachToIsland(AActor* Actor);

	/** Debug : état de l'île dans le log, temps de phase avancé, objet de l'objectif Index, bateau */
	void LogStatus() const;
	void DebugSkipTime(float Seconds);
	AYSInteractiveObject* GetObjectiveItem(int32 Index) const;
	AYSStartBoat* GetStartBoat() const { return StartBoat; }

protected:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Island")
	TObjectPtr<UYSIslandData> Data;

	/** Plan calculé par le serveur */
	UPROPERTY(ReplicatedUsing=OnRep_Plan)
	FYSIslandPlan Plan;

	UFUNCTION()
	void OnRep_Plan();

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Island")
	EYSIslandPhase Phase = EYSIslandPhase::Inactive;

	/** Objets attendus par les objectifs encore ouverts (répliqué : bulle de la zone de dépôt chez les clients) */
	UPROPERTY(Replicated)
	TArray<TSubclassOf<AYSInteractiveObject>> OpenDeliveryClasses;

	UPROPERTY(Replicated)
	TObjectPtr<AYSStartBoat> StartBoat;

private:

	/** Terrain et props locaux, une fois le plan connu */
	void BuildLocally();
	void BuildTerrain();
	void SpawnProps();

	/** Bateau et objectifs (serveur) */
	void SpawnGameplayActors();
	void SpawnObjective(const FYSIslandObjectiveDef& Def, bool bMain);
	bool FindFreeSpot(float MinDistanceFromBoat, float MaxDistanceFromBoat, FVector2f& OutSpot);
	bool IsSpotFree(const FVector2f& Spot, float Margin) const;

	/** Phases (serveur) */
	void TickActivation();
	void Activate();
	void StartDayPhase();
	void StartNightPhase();
	void StartSinkRoutine(const TCHAR* Reason);
	void StartPhaseEvents(const TArray<FYSIslandSpawnEvent>* Events);
	void TickPhaseEvents();
	int32 CountOpenMainObjectives() const;
	void RefreshOpenDeliveryClasses();

	/** Vagues (serveur) */
	void FireEvent(const FYSIslandSpawnEvent& Event, int32 Repeat);
	int32 SpawnOffScreen(const TArray<UClass*>& Classes, int32 ForcedLevel, int32 LevelBonus);
	int32 SpawnCircle(const TArray<UClass*>& Classes, int32 ForcedLevel, int32 LevelBonus);
	bool FindCirclePoint(float Angle, FVector& OutGround) const;

	/** Au milieu de l'île, ou autour si la place est prise */
	bool TrySpawnAtCenter(UClass* Class, int32 ForcedLevel, int32 LevelBonus);
	void TickPendingCenterSpawns(float DeltaSeconds);

	int32 RequestedSeed = 0;
	bool bBuiltLocally = false;
	FYSIslandTerrain Terrain;

	/** Jour global à l'activation */
	int32 ReferenceDay = 0;

	/** Temps écoulé dans la phase, vagues de la phase et répétitions faites */
	float PhaseElapsed = 0.0f;
	const TArray<FYSIslandSpawnEvent>* PhaseEvents = nullptr;
	TArray<int32> EventsFired;

	bool bSinkStarted = false;

	/** Apparitions au centre (élite) qui n'ont pas trouvé de place : réessayées, sinon la nuit ne finirait jamais */
	struct FPendingCenterSpawn
	{
		UClass* Class = nullptr;
		int32 ForcedLevel = INDEX_NONE;
		int32 LevelBonus = 0;
	};
	TArray<FPendingCenterSpawn> PendingCenterSpawns;
	float PendingRetryTimer = 0.0f;

	UPROPERTY()
	TArray<FYSIslandObjectiveState> Objectives;

	FRandomStream ServerRng;
};
