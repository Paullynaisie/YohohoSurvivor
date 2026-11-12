#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "GameFramework/Actor.h"
#include "Island/YSIslandGenerator.h"
#include "YSIsland.generated.h"

class AYSFoeCharacter;
class AYSInteractiveObject;
class AYSIslandPier;
class AYSObjectiveTreasureMap;
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
	Sinking,
	/** Flow : « Quittez l'île ! », en attendant que tout le monde soit à bord (ou la fin du temps) */
	Evacuation
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
	/** Boussole d'une île des zones d'intérêt : pas posée, une carte au trésor d'objectif apparaît à l'accostage et la pose à la fin de ses tracés */
	bool bWaitsObjectiveMap = false;
	bool bObjectiveMapSpawned = false;
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

	/** L'île la plus proche de Location qui attend cet objet pour un de ses objectifs, ou nullptr */
	static AYSIsland* FindExpecting(const UWorld* World, const AYSInteractiveObject* Object, const FVector& Location);

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

	/** Tous les objectifs principaux livrés (répliqué : icônes du nid d'pie) */
	bool AreMainObjectivesDone() const { return bMainObjectivesDone; }

	/**
	 *  Île prégénérée sous l'eau (serveur, avant FinishSpawning) : l'acteur est posé Depth sous sa place, construite mais inactive
	 *  (ni objectifs ni activation) jusqu'à StartRise
	 */
	void SetSubmerged(float Depth) { SubmergedDepth = Depth; }

	/** Remontée à la surface en Duration (serveur) ; objectifs et activation une fois arrivée */
	void StartRise(float Duration);

	/** Sous l'eau en attente de remonter */
	bool IsSubmergedWaiting() const { return SubmergedDepth > 0.0f && RiseStartServerTime <= 0.0; }

	/** Hors de l'eau (remontée finie, ou jamais immergée) */
	bool IsEmerged() const;

	/** Demi-côté du carré qui contient l'île et ses pentes sous l'eau (0 tant que le plan n'est pas construit sur cette machine) */
	float GetReach() const { return Terrain.GetHalfSize(); }

	/** Rayon moyen de l'île (règles du Data Asset) */
	float GetRadius() const;

	/** Coordonnées de l'île <-> monde */
	FVector LocalToWorld(const FVector2f& Local, float LocalZ) const;

	/** Debug : point sur la mer face au milieu de la plage, à Distance du bord de l'eau ; OutYaw = tourné vers l'île */
	FVector GetBeachApproachPoint(float Distance, float& OutYaw) const;
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

	/** Coule tout de suite, sans routine de coulaison (serveur ; partie 3 du flow : toutes les îles coulent) */
	void ForceSinkNow(const TCHAR* Reason);

	/** Milieu du morceau de ponton Index, dans le monde (faux s'il n'existe pas) */
	bool GetPierSegmentLocation(int32 Index, FVector& OutLocation) const;

	/** Île du flow à objectifs affichés (Objective Flow du Data Asset) */
	bool UsesObjectiveFlow() const;

	/** Place du bateau entre les pontons de la plage : milieu au niveau de la mer, tourné vers le sud (faux sans pontons) */
	bool GetBerthTransform(FTransform& OutTransform) const;

	/** Debug : évacuation finie tout de suite (victoire) */
	void DebugFinishEvacuation();

	/**
	 *  Objectif en plus de ceux du Data Asset (serveur, avant que le plan soit prêt) : objets des zones d'intérêt répartis par le flow
	 *  (boussole, CoreBoat, membre d'équipage...)
	 */
	void AddExtraObjective(const FYSIslandObjectiveDef& Def, bool bMain);

	/** La coulaison est finie (serveur : l'île est sous l'eau) */
	bool HasSunk() const { return bSinkFinished; }

	/**
	 *  Point au hasard sur le plateau de l'île (serveur, monde, au sol, hors massifs) : avec AwayFrom, dans la moitié de l'île opposée à ce point
	 *  et à au moins Min Distance de lui ; avec Max Distance From Center > 0, à moins de ça du centre de l'île. Faux si rien trouvé
	 */
	bool FindObjectiveMapSpot(const FVector* AwayFrom, float MinDistance, float MaxDistanceFromCenter, FVector& OutWorld);

	/** Fin des tracés d'une carte au trésor d'objectif (serveur) : l'objet de l'objectif (boussole) est posé à Location */
	void OnObjectiveMapCompleted(AYSObjectiveTreasureMap* Map, int32 ObjectiveIndex, const FVector& Location);

	/** Debug : carte au trésor d'objectif encore en jeu, ou nullptr */
	AYSObjectiveTreasureMap* GetObjectiveMap() const;

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

	/** Prégénérée sous l'eau : profondeur sous sa place (0 = jamais immergée), remontée (heure serveur du début, durée) */
	UPROPERTY(Replicated)
	float SubmergedDepth = 0.0f;

	UPROPERTY(Replicated)
	double RiseStartServerTime = 0.0;

	UPROPERTY(Replicated)
	float RiseDuration = 15.0f;

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

	/** Remontée de l'île sur cette machine ; finie : composants remis statiques, objectifs posés (serveur) */
	void UpdateRise();
	bool bRiseGathered = false;
	bool bRiseDone = false;
	FVector RiseOrigin = FVector::ZeroVector;
	TArray<TWeakObjectPtr<USceneComponent>> RiseStaticComponents;

	/** Objectifs et zone de dépôt en attente de la remontée (serveur) */
	bool bGameplayActorsPending = false;

	/** Vagues d'une liste depuis Elapsed secondes (Fired = répétitions faites de chaque vague) */
	void TickEventList(const TArray<FYSIslandSpawnEvent>& Events, TArray<int32>& Fired, float Elapsed);

	/** Flow à objectifs (serveur) : état du HUD, overtime, spawn constant, évacuation */
	void UpdateFlowStatus();
	void StartOvertime();
	void StartEvacuation();
	void TickEvacuation();
	void FinishEvacuation(const TCHAR* Reason);
	void TickConstantSpawn(float DeltaSeconds);

	/** Le bord de l'île (hors plage) le plus loin des joueurs : Classes côte à côte, Spacing entre eux */
	int32 SpawnFarSide(const TArray<UClass*>& Classes, float Spacing);

	/** Objectif « le plus au nord » : le point du plan, ou le point libre le plus au nord autour */
	bool FindNorthSpot(FVector2f& OutSpot) const;

	/** Îles de la partie 2 : bateau des joueurs échoué devant la plage -> bloqué à quai jusqu'à la fin de l'évacuation (serveur) */
	void TickDocking();

	/** L'objet de cet objectif est remplacé par une carte au trésor d'objectif (boussole d'une île des zones d'intérêt) */
	bool UsesObjectiveMap(const FYSIslandObjectiveState& State) const;

	/** Cartes au trésor d'objectif (serveur, une fois par objectif) : sur la plage à côté de Landing (point d'accostage, monde) */
	void SpawnObjectiveMaps(const FVector& Landing, const TCHAR* Reason);

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AYSObjectiveTreasureMap>> ObjectiveMaps;

public:

	/** L'objet a été posé par un des objectifs de l'île (serveur) */
	bool IsObjectiveItem(const AYSInteractiveObject* Object) const
	{
		return Object && Objectives.ContainsByPredicate([Object](const FYSIslandObjectiveState& State) { return State.Item == Object; });
	}

private:

	/** Bord de l'eau (coordonnées de l'île) en allant de FromLocal vers le centre de l'île */
	FVector2f FindShorePointToward(const FVector2f& FromLocal) const;

	/** Texte de l'objectif « ramène » : objets des objectifs principaux restants (Find Objective Format), sinon Find Text */
	FText BuildFindText() const;

	bool bOvertime = false;
	bool bFlowVictory = false;
	bool bBoatDocked = false;
	float ConstantSpawnTimer = 0.0f;
	TArray<int32> OvertimeEventsFired;
	int32 FlowBoarded = INDEX_NONE;
	int32 FlowBoardingTotal = INDEX_NONE;
	float FlowFinalMatchTime = -1.0f;

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

	/** Coulée par ForceSinkNow (partie 3) : plus de cycle ; répliqué : chaque machine la fait disparaître d un coup (UpdateSink) */
	UPROPERTY(Replicated)
	bool bForceSunk = false;

	/** Coulaison finie, flow prévenu (serveur) */
	bool bSinkFinished = false;

	/** Objectifs ajoutés par le flow (zones d'intérêt) : objectif, principal */
	TArray<TPair<FYSIslandObjectiveDef, bool>> ExtraObjectives;

	/** Nuit en cours imposée par la séquence de nuits du flow : les vagues « au centre » de Night Events sont remplacées */
	bool bNightSetupOverride = false;

	UPROPERTY(Replicated)
	bool bMainObjectivesDone = false;

	/** Descente sur cette machine : composants rendus mobiles, position de départ */
	bool bSinkGathered = false;

	/** Coulée de force : île et acteurs accrochés déjà cachés sur cette machine */
	bool bForceSunkHidden = false;
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
