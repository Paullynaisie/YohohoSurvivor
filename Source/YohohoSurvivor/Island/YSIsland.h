#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "GameFramework/Actor.h"
#include "Island/YSIslandGenerator.h"
#include "YSIsland.generated.h"

class AYSFoeCharacter;
class AYSInteractiveObject;
class AYSIslandPier;
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

	/** Réglages de l'objectif (Data Asset) */
	FYSIslandObjectiveDef Def;

	bool bMain = false;
	bool bDone = false;
	/** Objet posé sur l'île (au jour Spawn Day) */
	bool bSpawned = false;
};

/**
 *  Une île en jeu, définie par son Data Asset (UYSIslandData). Le serveur calcule le plan (massifs, props, emplacements) à partir d'une graine et
 *  le réplique (props : AYSIslandPropChunk) ; chaque machine construit le terrain (mesh procédural), les pontons et pose les props localement.
 *  Le plan est en coordonnées de l'île : l'acteur peut être posé n'importe où sur la mer et tourné (Random Rotation).
 *  Un joueur y pose le pied : elle s'active, prend le jour global en cours comme référence et se considère au début du jour. Chaque île a sa propre phase
 *  (jour, nuit, coulaison), le jour global passe au suivant dès qu'une nuit se termine quelque part.
 *  Plus d'objectif principal, ou Max Days atteint : routine de coulaison, puis l'île coule (elle-même et ce qui y est accroché, sur chaque machine).
 */
UCLASS()
class AYSIsland : public AActor
{
	GENERATED_BODY()

public:

	AYSIsland();

	/** Avant FinishSpawning (serveur) : Data Asset, graine (0 = au hasard, sauf graine fixe du Data Asset), île de départ (GameBoat) */
	void InitIsland(UYSIslandData* InData, int32 InSeed, bool bInStartIsland);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/** Île la plus proche de Location (nullptr si aucune) */
	static AYSIsland* FindAt(const UWorld* World, const FVector& Location);

	/** Point dans un massif de l'île (à Margin près) : les chemins et apparitions l'évitent */
	static bool IsBlockedAt(const UWorld* World, const FVector& Location, float Margin);

	/** Une île qui coule couvre ce point (sa descente n'est pas finie) : plus de sol de nage */
	static bool IsSinkingAt(const UWorld* World, const FVector& Location);

	bool IsNight() const { return Phase == EYSIslandPhase::Night; }

	EYSIslandPhase GetPhase() const { return Phase; }

	UYSIslandData* GetData() const { return Data; }

	bool IsStartIsland() const { return bStartIsland; }

	/** L'île est en train de descendre (entre le début et la fin de la coulaison) */
	bool IsSinking() const;

	/** La coulaison a commencé (descente en cours ou finie) */
	bool HasSinkStarted() const { return SinkStartServerTime > 0.0; }

	/** Demi-côté du carré qui contient l'île et ses pentes sous l'eau (0 tant que le plan n'est pas construit sur cette machine) */
	float GetReach() const { return Terrain.GetHalfSize(); }

	/** Rayon moyen de l'île (règles du Data Asset) */
	float GetRadius() const;

	/** Coordonnées de l'île <-> monde */
	FVector LocalToWorld(const FVector2f& Local, float LocalZ) const;
	FVector2f WorldToLocal(const FVector& World) const;

	/** Point de départ de la chaloupe du GameBoat : sur la mer, Distance au large du bord de l'eau, en face de la plage */
	FVector GetGameBoatStartLocation(float Distance) const;

	/** Zone de dépôt des objectifs : Start Boat Class du Data Asset (serveur) */
	void SpawnStartBoat(const FVector& Location, float Yaw);

	/** Où apparaît le joueur Index de la partie */
	FTransform GetPlayerSpawnTransform(int32 Index) const;

	/** Le jour global vient de changer (serveur) : Max Days atteint ou plus d'objectif principal = routine de coulaison, sinon nouveau jour de l'île */
	void OnGlobalDayChanged(int32 Day);

	/** Élite apparu pour la nuit de cette île (encore suivi) */
	bool OwnsNightElite(const AYSFoeCharacter* Elite) const;

	/** Mort d'un élite de cette île (serveur) : quand il n'en reste plus, la nuit se termine et le jour global passe au suivant */
	void OnEliteKilled(AYSFoeCharacter* Elite);

	/** Cet objet est attendu par un objectif encore ouvert (toutes machines) */
	bool IsObjectExpected(const AYSInteractiveObject* Object) const;

	/** Objet reposé dans une zone de dépôt (serveur) : l'objectif correspondant est accompli */
	void NotifyObjectDelivered(AYSInteractiveObject* Object);

	/** Accroche un acteur à l'île : il coule avec elle */
	void AttachToIsland(AActor* Actor);

	/** Terrain construit sur cette machine (les props peuvent être posés) */
	bool IsBuiltLocally() const { return bBuiltLocally; }

	/** Pose des props du plan sur cette machine (locaux, accrochés à l'île) */
	void SpawnPropList(const TArray<FYSIslandPropSpawn>& Props);

	/** Debug : état de l'île dans le log, temps de phase avancé, objet de l'objectif Index, bateau, coulaison tout de suite */
	void LogStatus() const;
	void DebugSkipTime(float Seconds);
	AYSInteractiveObject* GetObjectiveItem(int32 Index) const;
	AYSStartBoat* GetStartBoat() const { return StartBoat; }
	void DebugSinkNow();

	/** Milieu du morceau de ponton Index, dans le monde (faux s'il n'existe pas) */
	bool GetPierSegmentLocation(int32 Index, FVector& OutLocation) const;

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

	/** Heure serveur du début de la descente (0 = pas encore) */
	UPROPERTY(Replicated)
	double SinkStartServerTime = 0.0;

private:

	/** Plan prêt (serveur) : construction, parts répliquées, objectifs */
	void OnPlanReady(const FString& Report);

	/** Terrain, pontons et props locaux, une fois le plan connu */
	void BuildLocally();
	void BuildTerrain();

	/** Props en attente, posés par paquets à chaque image (une grande île en a des milliers) */
	void TickPendingProps();
	TArray<FYSIslandPropSpawn> PendingProps;
	int32 PendingPropIndex = 0;

	/** Génération du plan sur un autre fil (îles qui apparaissent en cours de partie) */
	struct FAsyncPlanResult
	{
		FYSIslandPlan Plan;
		FString Report;
	};
	TFuture<FAsyncPlanResult> AsyncPlan;
	double AsyncPlanStartTime = 0.0;

	/** Props du plan répartis en parts répliquées (serveur, partie en réseau) */
	void SpawnPropChunks();

	/** Bateau et objectifs (serveur) */
	void SpawnGameplayActors();
	void SpawnObjective(FYSIslandObjectiveState& State);
	void SpawnDueObjectives();
	bool FindFreeSpot(float MinDistanceFromBoat, float MaxDistanceFromBoat, bool bInTown, FVector2f& OutSpot);
	bool IsSpotFree(const FVector2f& Spot, float Margin) const;

	/** Phases (serveur) */
	void TickActivation();
	void Activate();
	void StartDayPhase();
	void StartNightPhase();
	void StartSinkRoutine(const TCHAR* Reason);
	void StartSinking();
	void StartPhaseEvents(const TArray<FYSIslandSpawnEvent>* Events);
	void TickPhaseEvents();
	int32 CountOpenMainObjectives() const;
	void RefreshOpenDeliveryClasses();

	/** Jour de l'île (1 = jour de l'activation) */
	int32 GetIslandDay() const;

	/** Descente de l'île sur cette machine */
	void UpdateSink();

	/** Vagues (serveur) */
	void FireEvent(const FYSIslandSpawnEvent& Event, int32 Repeat);
	int32 SpawnOffScreen(const TArray<UClass*>& Classes, int32 ForcedLevel, int32 LevelBonus);
	int32 SpawnCircle(const TArray<UClass*>& Classes, int32 ForcedLevel, int32 LevelBonus);
	bool FindCirclePoint(float Angle, FVector& OutGround) const;

	/** Au milieu de l'île, ou autour si la place est prise */
	AYSFoeCharacter* TrySpawnAtCenter(UClass* Class, int32 ForcedLevel, int32 LevelBonus);
	void TickPendingCenterSpawns(float DeltaSeconds);

	/** Élite de la nuit apparu : suivi jusqu'à sa mort */
	void TrackNightElite(AYSFoeCharacter* Foe);

	int32 RequestedSeed = 0;
	bool bStartIsland = false;
	bool bBuiltLocally = false;
	FYSIslandTerrain Terrain;

	UPROPERTY(Transient)
	TObjectPtr<AYSIslandPier> Pier;

	/** Jour global à l'activation */
	int32 ReferenceDay = 0;

	/** Temps écoulé dans la phase, vagues de la phase et répétitions faites */
	float PhaseElapsed = 0.0f;
	const TArray<FYSIslandSpawnEvent>* PhaseEvents = nullptr;
	TArray<int32> EventsFired;

	bool bSinkStarted = false;

	/** Descente sur cette machine : composants rendus mobiles, position de départ */
	bool bSinkGathered = false;
	FVector SinkOrigin = FVector::ZeroVector;

	/** Apparitions au centre (élite) qui n'ont pas trouvé de place : réessayées, sinon la nuit ne finirait jamais */
	struct FPendingCenterSpawn
	{
		UClass* Class = nullptr;
		int32 ForcedLevel = INDEX_NONE;
		int32 LevelBonus = 0;
	};
	TArray<FPendingCenterSpawn> PendingCenterSpawns;
	float PendingRetryTimer = 0.0f;

	/** Élites apparus pour la nuit en cours : la nuit se termine quand ils sont tous morts */
	TArray<TWeakObjectPtr<AYSFoeCharacter>> NightElites;

	UPROPERTY()
	TArray<FYSIslandObjectiveState> Objectives;

	FRandomStream ServerRng;
};
