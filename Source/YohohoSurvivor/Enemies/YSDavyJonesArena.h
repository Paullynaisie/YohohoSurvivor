#pragma once

#include "CoreMinimal.h"
#include "Enemies/YSFoeGalion.h"
#include "GameFramework/Actor.h"
#include "YSDavyJonesArena.generated.h"

class AYSFoeCharacter;
class AYSInteractiveObject;
class AYSPirateShip;

/** Un équipage invoqué par Davy Jones : groupes de NME posés sur CHAQUE Galion encore à flot (nombres pour 1 joueur, x Spawn Multiplier) */
USTRUCT(BlueprintType)
struct FYSDavyJonesCrewWave
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crew")
	TArray<FYSGalionCrewGroup> Groups;
};

UENUM()
enum class EYSDavyJonesState : uint8
{
	/** Le bateau des joueurs n'est pas encore arrivé */
	Waiting,
	/** Davy Jones sur la plateforme d'un Galion : geste du crochet, puis équipage */
	Summoning,
	/** Équipage invoqué : Davy Jones attend sur sa plateforme qu'il soit tué */
	Crew,
	/** Davy Jones saute d'un pont à l'autre (invulnérable) */
	Jumping,
	/** Davy Jones se bat sur les ponts */
	Fighting,
	Defeated
};

/**
 *  World boss « Davy Jones » (partie 2 du flow) : zone en mer posée au lancement par AYSFlowMode, à l'opposé de la zone du Kraken
 *  (Blueprint BP_DavyJonesArena). La Super Treasure Map la montre (pointillés dorés). Le bateau des joueurs qui y entre lance le combat :
 *  toutes les Super Treasure Maps disparaissent, 3 Galions de Davy Jones apparaissent en triangle autour de lui (bâbord, tribord, proue) en
 *  abordage (planches posées sur son pont), les 4 bateaux sont immobilisés, objectif « Vaincre Davy Jones ».
 *    Phase 1 : Davy Jones sur la plateforme d'un Galion lève son crochet, un équipage par Galion (Crew Waves[0]) ; équipages morts : il descend.
 *    PV sous Phase Health Thresholds[0] (65 %) : un Galion sans joueur coule, il monte sur un des autres, équipage Crew Waves[1] sur chacun.
 *    PV sous Phase Health Thresholds[1] (30 %) : le Galion où les joueurs ne sont pas coule, il monte sur le dernier, équipage Crew Waves[2].
 *    Dernier équipage mort : dernier combat. Davy Jones mort : son dernier Galion coule, le bateau des joueurs est libéré, le butin (Loot Classes
 *    + Random Crew Loot Count membres d'équipage) flotte autour de lui. Davy Jones vaincu : le Kraken a moins de tentacules.
 *  Sur sa plateforme et pendant ses sauts, Davy Jones est invulnérable et ne fait rien. Serveur pour le combat ; l'état est répliqué.
 */
UCLASS(meta=(PrioritizeCategories="World Boss"))
class AYSDavyJonesArena : public AActor
{
	GENERATED_BODY()

public:

	AYSDavyJonesArena();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void Tick(float DeltaSeconds) override;

	/** L'arène de la partie (ou nullptr) */
	static AYSDavyJonesArena* Find(const UWorld* World);

	bool IsFightStarted() const { return State != EYSDavyJonesState::Waiting; }

	bool IsDefeated() const { return State == EYSDavyJonesState::Defeated; }

	float GetTriggerRadius() const { return TriggerRadius; }

	/** Debug (serveur) : combat lancé tout de suite autour du bateau des joueurs, où qu'il soit */
	void DebugStartFight();

	/** Debug (serveur) : équipage en cours tué, sinon Davy Jones au seuil de la phase suivante (ou tué à la dernière) */
	void DebugSkip();

protected:

	virtual void BeginPlay() override;

	// ---------------------------------------------------------------- zone

	/** Le bateau des joueurs (centre de la coque) à moins de cette distance du centre lance le combat (partie 2 seulement) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Boss|Zone", meta=(ClampMin="100", Units="Centimeters"))
	float TriggerRadius = 6000.0f;

	// ---------------------------------------------------------------- combat

	/** Le boss (FoeCharacter_DavyJones) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Boss|Fight")
	TSubclassOf<AYSFoeCharacter> DavyJonesClass;

	/** Galion de Davy Jones (BP_DavyJonesGalion : sans équipage ni capitaine, grand mât remplacé par une plateforme) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Boss|Fight")
	TSubclassOf<AYSFoeGalion> GalionClass;

	/** Eau entre la coque du bateau des joueurs et celle de chaque Galion (les planches d'abordage la traversent) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Boss|Fight", meta=(ClampMin="0", Units="Centimeters"))
	float GalionGap = 300.0f;

	/** Équipages invoqués, dans l'ordre des phases (un par Galion encore à flot) ; défaut BP : 25 / 5 / 3, 35 / 10 / 5, 2 élites + 20 Skull Gun */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Boss|Fight")
	TArray<FYSDavyJonesCrewWave> CrewWaves;

	/** Part des PV max sous laquelle Davy Jones remonte sur un Galion (un Galion coule) : une valeur par phase suivante */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Boss|Fight")
	TArray<float> PhaseHealthThresholds = { 0.65f, 0.30f };

	/** Équipage invoqué : Davy Jones descend dès que cette part de l'équipage est tuée... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Boss|Fight", meta=(ClampMin="0", ClampMax="1"))
	float DescendCrewKilledRatio = 0.75f;

	/** ... ou au bout de ce temps après l'apparition de l'équipage (0 = jamais) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Boss|Fight", meta=(ClampMin="0", Units="s"))
	float DescendMaxCrewTime = 60.0f;

	/** Geste du crochet avant l'apparition de l'équipage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Boss|Fight", meta=(ClampMin="0", Units="s"))
	float SummonAnimDuration = 1.5f;

	/** Sauts de Davy Jones (plateforme <-> pont) : durée du vol */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Boss|Fight", meta=(ClampMin="0.2", Units="s"))
	float JumpDuration = 1.2f;

	/** Descente : il atterrit sur le pont du bateau des joueurs, à cette distance du bord de la coque côté Galion */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Boss|Fight", meta=(ClampMin="0", Units="Centimeters"))
	float DescendInset = 600.0f;

	/** Nom de la barre de vie du boss */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Boss|Fight")
	FText BossName = NSLOCTEXT("YS", "DavyJonesName", "Davy Jones");

	// ---------------------------------------------------------------- butin

	/** Davy Jones vaincu : objets qui flottent autour du bateau des joueurs (défaut BP : 3 Elite Chest, 2 Boss Chest) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Boss|Loot")
	TArray<TSubclassOf<AYSInteractiveObject>> LootClasses;

	/**
	 *  Plus ce nombre de membres d'équipage (crewboat) tirés au hasard dans Crew Item Classes du FlowMod, parmi ceux qu'on ne trouve pas
	 *  ailleurs (ni posés dans les zones d'intérêt, ni déjà en jeu) ; le core du bateau des joueurs les prend (ils rejoignent l'équipage)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Boss|Loot", meta=(ClampMin="0"))
	int32 RandomCrewLootCount = 2;

	/** Distance entre le bord de la coque du bateau des joueurs et le butin */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Boss|Loot", meta=(ClampMin="0", Units="Centimeters"))
	float LootDistanceFromHull = 700.0f;

	UPROPERTY(ReplicatedUsing=OnRep_State)
	EYSDavyJonesState State = EYSDavyJonesState::Waiting;

	UFUNCTION()
	void OnRep_State();

private:

	UPROPERTY(Transient)
	TArray<TObjectPtr<AYSFoeGalion>> Galions;

	UPROPERTY(Transient)
	TObjectPtr<AYSFoeCharacter> Davy;

	/** Galion dont Davy Jones occupe la plateforme (nullptr sur les ponts) */
	UPROPERTY(Transient)
	TObjectPtr<AYSFoeGalion> DavyGalion;

	UPROPERTY(Transient)
	TObjectPtr<AYSPirateShip> PlayerBoat;

	TArray<TWeakObjectPtr<AYSFoeCharacter>> Crew;

	/** NME de l'équipage en cours à son apparition */
	int32 CrewSpawnedCount = 0;

	/** Phase en cours (0 = première), heure (monde) du début de l'état */
	int32 Phase = 0;
	double StateStartTime = 0.0;

	/** Saut en cours : point visé (centre de la capsule), vers la plateforme (sinon vers le pont), heure d'arrivée */
	FVector JumpTarget = FVector::ZeroVector;
	bool bJumpToPlatform = false;
	double JumpEndTime = 0.0;

	void SetState(EYSDavyJonesState NewState);
	void TickFight();
	void StartFight(const TCHAR* Reason);
	void RemoveSuperTreasureMaps();
	bool SpawnGalions();
	void SpawnDavy();

	/** Geste du crochet puis équipage de la phase (sur la plateforme) */
	void BeginSummon();
	void SpawnCrewWave();

	/** PV sous le seuil : un Galion coule, Davy Jones remonte sur un autre */
	void StartNextPhase();

	/** Saut de Davy Jones vers TargetFeet (pieds) */
	void StartJump(const FVector& TargetFeet, bool bToPlatform);
	void FinishJump();

	/** Point du pont du bateau des joueurs où Davy Jones descend (pieds) */
	FVector FindDescendPoint() const;

	/** Davy Jones mort : Galions restants coulés, bateau libéré, butin */
	void Victory();
	void SpawnLoot();

	/** Galions encore à flot */
	TArray<AYSFoeGalion*> GetLiveGalions() const;

	/** Joueurs vivants debout sur ce bateau */
	int32 CountPlayersOn(const AYSPirateShip* Ship) const;

	void SinkGalion(AYSFoeGalion* Galion, const TCHAR* Reason);

	float GetSeaLevel() const;

	bool IsDavyAlive() const;
};
