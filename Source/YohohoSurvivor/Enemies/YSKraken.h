#pragma once

#include "CoreMinimal.h"
#include "Enemies/YSFoeCharacter.h"
#include "YSKraken.generated.h"

class AYSKrakenArena;
class AYSPirateShip;
class UChildActorComponent;
class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UYSGroundZoneComponent;

namespace YSKraken
{
	/** Ordre de rendu des translucides du combat : l'eau sombre de l'arène toujours sous les signes au sol (sinon, triés par la
	 *  distance de leur centre, les signes passaient parfois sous le disque de 200 m) */
	constexpr int32 WaterSortPriority = -10;
	constexpr int32 MarkerSortPriority = 10;
}

/**
 *  Morceau du boss Kraken (partie 3 du flow) : NME immobile posé en mer par l'arène (AYSKrakenArena), sans comportement de NME
 *  (ni déplacement, ni poursuite, ni pushback / bump, ni XP, ni ragdoll). Visuel = acteur enfant Visual Class (prop en primitives,
 *  local sur chaque machine) sous Visual Pivot ; hurtbox = BodyMesh (cylindre caché, à régler dans le Blueprint) accroché au pivot.
 *  Le mouvement n'est pas répliqué : chaque machine place le morceau d'après son état répliqué.
 */
UCLASS(Abstract, meta=(PrioritizeCategories="Kraken Foe"))
class AYSKrakenPart : public AYSFoeCharacter
{
	GENERATED_BODY()

public:

	AYSKrakenPart();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void FellOutOfWorld(const UDamageType& DamageType) override {}

	virtual bool IsImmovable() const override { return true; }

	/** Serveur : posé par l'arène qui le gère */
	void SetArena(AYSKrakenArena* InArena) { Arena = InArena; }

	AYSKrakenArena* GetArena() const { return Arena.Get(); }

	/** Retiré sans mort (changement d'étape : les tentacules restantes disparaissent) */
	virtual void Vanish(const TCHAR* Reason);

protected:

	/** Pivot du visuel et de la hurtbox (tourné / incliné par les morceaux) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> VisualPivot;

	/** Prop en primitives (BP_Prop_KrakenHead / BP_Prop_KrakenTentacle) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UChildActorComponent> Visual;

	/** Barre de vie au-dessus du morceau (sinon : seulement la barre du boss dans le HUD) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken")
	bool bShowOverheadHealthBar = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken", meta=(Units="Centimeters", EditCondition="bShowOverheadHealthBar"))
	float OverheadHealthBarHeight = 2900.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<AYSKrakenArena> Arena;

	/** Chaque image, sur chaque machine (le comportement de NME n'est pas joué) */
	virtual void TickKraken(float DeltaSeconds) {}

	/** Morceau touchable / ciblable ou non (sous l'eau : non), local sur chaque machine */
	void SetTargetable(bool bTargetable);

	bool IsTargetable() const { return bTargetableApplied; }

	virtual void Die() override;

	/** Bateau des joueurs (GameBoat) */
	AYSPirateShip* GetGameBoat() const;

	/** Heure serveur (toutes machines) */
	double GetServerTime() const;

	/** Niveau de la mer (sinon 0) */
	float GetSeaLevel() const;

private:

	bool bTargetableApplied = true;
	FGameplayTagContainer TargetableTags;
};

/** Tête du Kraken (étape 1) : au centre de l'arène, toujours tournée vers le bateau ; aspiration toutes les Suction Cooldown */
UCLASS(meta=(PrioritizeCategories="Kraken Foe"))
class AYSKrakenHead : public AYSKrakenPart
{
	GENERATED_BODY()

public:

	AYSKrakenHead();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	/** Rayon de contact : le bateau qui touche ce cercle (coque à moins de ce rayon du centre) touche la tête */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Head", meta=(ClampMin="0", Units="Centimeters"))
	float ContactRadius = 2000.0f;

	/** Vitesse de rotation vers le bateau (0 = immédiat) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Head", meta=(ClampMin="0"))
	float HeadTurnSpeed = 2.0f;

	/** Première aspiration ces secondes après le début du combat */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Suction", meta=(ClampMin="0", Units="s"))
	float FirstSuctionDelay = 10.0f;

	/** Recharge entre deux aspirations (comptée depuis la fin de la précédente) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Suction", meta=(ClampMin="0", Units="s"))
	float SuctionCooldown = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Suction", meta=(ClampMin="0.1", Units="s"))
	float SuctionDuration = 10.0f;

	/** Vitesse d'aspiration = vitesse max avant de base du bateau x ceci */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Suction", meta=(ClampMin="0"))
	float SuctionSpeedMultiplier = 0.6f;

	/** Coup quand le bateau touche la tête pendant l'aspiration : dégâts à chaque poste et au core */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Suction", meta=(ClampMin="0"))
	float SmashDamage = 2000.0f;

	/** Poussée du coup (cm/s au départ, amortie par le bateau) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Suction", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float SmashKnockbackSpeed = 3000.0f;

	/** Hors aspiration, un bateau qui touche la tête est repoussé sans dégâts (choix de Claude : il ne la traverse pas) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Suction", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float ContactKnockbackSpeed = 1500.0f;

	/** Animation du coup : tête qui s'abat vers le bateau (degrés) en Smash Anim Duration */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Suction|Anim")
	float SmashAnimPitch = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Suction|Anim", meta=(ClampMin="0.05", Units="s"))
	float SmashAnimDuration = 0.7f;

	/** VFX d'aspiration : anneaux qui se resserrent vers la tête (maillage local, M_ShipPreview) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Suction|VFX", meta=(ClampMin="0", Units="Centimeters"))
	float VfxOuterRadius = 18000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Suction|VFX", meta=(ClampMin="1"))
	int32 VfxRingCount = 4;

	/** Temps pour qu'un anneau aille du bord à la tête */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Suction|VFX", meta=(ClampMin="0.1", Units="s"))
	float VfxRingPeriod = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Suction|VFX", meta=(ClampMin="1", Units="Centimeters"))
	float VfxRingWidth = 250.0f;

	/** Bras en spirale qui tournent vers la tête */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Suction|VFX", meta=(ClampMin="0"))
	int32 VfxSpiralArms = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Suction|VFX", meta=(Units="DegreesPerSecond"))
	float VfxSpinSpeed = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Suction|VFX")
	FLinearColor VfxColor = FLinearColor(0.35f, 0.75f, 1.0f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Suction|VFX")
	TObjectPtr<UMaterialInterface> VfxMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Suction|VFX", meta=(Units="Centimeters"))
	float VfxHeightAboveSea = 15.0f;

	/** Heure serveur du début de l'aspiration en cours (0 = aucune) */
	UPROPERTY(Replicated)
	double SuctionStartServerTime = 0.0;

	/** Heure serveur du dernier coup (animation) */
	UPROPERTY(Replicated)
	double SmashServerTime = 0.0;

	virtual void BeginPlay() override;

	virtual void TickKraken(float DeltaSeconds) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Serveur : aspiration, contact, coup */
	virtual void TickHeadServer(float DeltaSeconds);

	/** Le bateau touche la tête (serveur) */
	void Smash(AYSPirateShip* Boat, bool bWithDamage);

	/** Tête tournée vers le bateau (local) */
	void UpdateFacing(float DeltaSeconds);

	/** Le bateau touche la tête : coque à moins de Contact Radius du centre */
	bool IsTouching(const AYSPirateShip* Boat) const;

	/** Lean du coup (local) */
	float GetSmashPitch() const;

	double LastContactServerTime = -1.0e6;
	float CurrentYaw = 0.0f;

private:

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> SuctionVfx;

	/** Serveur : heure (serveur) de la prochaine aspiration */
	double NextSuctionServerTime = -1.0;
	bool bYawInitialized = false;
	bool bVfxVisible = false;

	void UpdateSuctionVfx();
	void StopSuction(const TCHAR* Reason);
};

/** Un motif de tirs d'encre en cloche de la tête finale (points d'impact au sol, comme les boules des tentacules) */
UENUM(BlueprintType)
enum class EYSKrakenInkPattern : uint8
{
	/** Cercles concentriques d'impacts autour de la tête, séparés d'un écart où tient le bateau (rayon du premier au hasard) */
	Rings,
	/** Spirale d'impacts depuis la tête jusqu'au bord, tours séparés d'un écart où tient le bateau */
	Spiral,
	/** Lignes d'impacts du centre jusqu'au bord de l'arène */
	Lines
};

/** Tête finale du Kraken (étape 3) : au centre, tournée vers le bateau ; alterne attente et motif de tirs d'encre en cloche au hasard */
UCLASS(meta=(PrioritizeCategories="Kraken Foe"))
class AYSKrakenHeadFinal : public AYSKrakenHead
{
	GENERATED_BODY()

public:

	AYSKrakenHeadFinal();

protected:

	/** Attente entre deux motifs (et avant le premier), comptée depuis le dernier impact du motif précédent */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink", meta=(ClampMin="0", Units="s"))
	float PatternInterval = 3.0f;

	/** Motifs tirés au hasard (pas deux fois le même de suite s'il y en a plusieurs ; choix de Claude) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink")
	TArray<EYSKrakenInkPattern> Patterns = { EYSKrakenInkPattern::Rings, EYSKrakenInkPattern::Lines, EYSKrakenInkPattern::Spiral };

	/** Dégâts au core du bateau par tir qui tombe sur lui */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink", meta=(ClampMin="0"))
	float ShotDamage = 300.0f;

	/** Rayon de l'impact (signe au sol) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink", meta=(ClampMin="1", Units="Centimeters"))
	float ShotRadius = 600.0f;

	/** Taille de la boule (diamètre) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink", meta=(ClampMin="1", Units="Centimeters"))
	float ShotBallSize = 450.0f;

	/** Vol en cloche de chaque boule (le signe au sol est affiché pendant tout le vol, plus Marker Lead Time avant) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink", meta=(ClampMin="0.1", Units="s"))
	float ShotFlightDuration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink", meta=(ClampMin="0", Units="s"))
	float MarkerLeadTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink", meta=(ClampMin="0", Units="Centimeters"))
	float ShotApexHeight = 2500.0f;

	/** Départ des boules : au-dessus de la tête, à cette hauteur au-dessus de la mer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink", meta=(ClampMin="0", Units="Centimeters"))
	float ShotLaunchHeight = 3000.0f;

	/** Distance entre deux impacts voisins d'une ligne / d'un cercle / de la spirale (moins de 2 x Shot Radius = sans trou) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink", meta=(ClampMin="50", Units="Centimeters"))
	float ShotSpacing = 1100.0f;

	/** Écart libre entre deux cercles / deux tours de spirale (bord à bord des impacts) : de quoi placer le bateau
	 *  (galion des joueurs x0.7 = 41 m de long, 14 m de large) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink", meta=(ClampMin="0", Units="Centimeters"))
	float BoatGap = 4500.0f;

	/** Pas d'impact plus près du centre (la tête) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink", meta=(ClampMin="0", Units="Centimeters"))
	float MinPatternRadius = 3000.0f;

	/** Bord des motifs (0 = rayon de l'arène - Shot Radius) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink", meta=(ClampMin="0", Units="Centimeters"))
	float MaxPatternRadius = 0.0f;

	/** Cercles : nombre, rayon du premier tiré au hasard dans cet intervalle, écart de départ entre deux cercles (du centre vers le bord) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink|Rings", meta=(ClampMin="1"))
	int32 RingCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink|Rings", meta=(Units="Centimeters"))
	FVector2D FirstRingRadius = FVector2D(3500.0, 7000.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink|Rings", meta=(ClampMin="0", Units="s"))
	float RingInterval = 0.4f;

	/** Lignes : nombre (réparties autour, orientation au hasard), départ de chaque impact après le précédent de sa ligne (du centre vers le bord) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink|Lines", meta=(ClampMin="1"))
	int32 LineCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink|Lines", meta=(ClampMin="0", Units="s"))
	float LineStepDelay = 0.08f;

	/** Spirale : rayon de départ au hasard, durée pour tirer toute la spirale (du centre vers le bord), sens et angle de départ au hasard */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink|Spiral", meta=(Units="Centimeters"))
	FVector2D SpiralStartRadius = FVector2D(3000.0, 5000.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink|Spiral", meta=(ClampMin="0", Units="s"))
	float SpiralDuration = 4.0f;

	virtual void TickHeadServer(float DeltaSeconds) override;

private:

	double NextPatternServerTime = -1.0;
	int32 LastPattern = INDEX_NONE;

	/** Lance le motif ; renvoie sa durée (dernier impact) */
	float FirePattern(EYSKrakenInkPattern Pattern);
};

/** Phase du cycle d'une tentacule */
UENUM()
enum class EYSKrakenTentaclePhase : uint8
{
	Submerged,
	Rising,
	/** Sortie, avant l'amorce */
	Waiting,
	/** Amorce de l'attaque (signe au sol) */
	Windup,
	/** Tentacule de mêlée qui s'abat */
	Slam,
	/** Tentacule de mêlée qui retient le bateau */
	Grabbing,
	/** Attaque finie, attend avant de replonger */
	Recover,
	Sinking
};

/** État d'une tentacule, répliqué (chaque machine place la tentacule d'après lui) */
USTRUCT()
struct FYSKrakenTentacleState
{
	GENERATED_BODY()

	UPROPERTY()
	EYSKrakenTentaclePhase Phase = EYSKrakenTentaclePhase::Submerged;

	/** Point de sortie (niveau de la mer) */
	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	/** Tournée vers le bateau à la sortie */
	UPROPERTY()
	float Yaw = 0.0f;

	/** Heure serveur du début de la phase */
	UPROPERTY()
	double PhaseStartServerTime = 0.0;

	/** A retenu le bateau pendant cette sortie (tentacule de mêlée : replonge abattue) */
	UPROPERTY()
	bool bGrabbed = false;
};

/** Tentacule du Kraken (base) : sort de l'eau près du bateau, attaque, replonge, recommence */
UCLASS(Abstract, meta=(PrioritizeCategories="Kraken Foe"))
class AYSKrakenTentacle : public AYSKrakenPart
{
	GENERATED_BODY()

public:

	AYSKrakenTentacle();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	virtual void Vanish(const TCHAR* Reason) override;

	/** Serveur, avant FinishSpawning : secondes ajoutées à la première sortie (l'arène décale les tentacules d'une même étape) */
	void SetEmergeStagger(float Seconds) { EmergeStagger = FMath::Max(Seconds, 0.0f); }

protected:

	float EmergeStagger = 0.0f;

	/** Sous l'eau au début, puis sort après ce délai */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Tentacle", meta=(ClampMin="0", Units="s"))
	float FirstEmergeDelay = 10.0f;

	/** Sous l'eau entre deux sorties : durée au hasard entre min et max */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Tentacle", meta=(Units="s"))
	FVector2D SubmergedDelay = FVector2D(5.0, 15.0);

	/** Distance au bord de la coque du bateau où elle sort (au hasard autour de lui) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Tentacle", meta=(ClampMin="0", Units="Centimeters"))
	float EmergeDistance = 3500.0f;

	/** Sortie : à moins de cette distance du centre de l'arène (le bateau au bord : sortie ramenée vers l'intérieur) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Tentacle", meta=(ClampMin="0", Units="Centimeters"))
	float MaxDistanceFromArenaCenter = 19000.0f;

	/** Et jamais dans la tête (à plus de cette distance du centre) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Tentacle", meta=(ClampMin="0", Units="Centimeters"))
	float MinDistanceFromArenaCenter = 2800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Tentacle", meta=(ClampMin="0.05", Units="s"))
	float RiseDuration = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Tentacle", meta=(ClampMin="0.05", Units="s"))
	float SinkDuration = 1.2f;

	/** Sous l'eau : profondeur (cachée et intouchable) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Tentacle", meta=(ClampMin="0", Units="Centimeters"))
	float SubmergedDepth = 3200.0f;

	/** Sortie : attente avant l'amorce */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Tentacle", meta=(ClampMin="0", Units="s"))
	float AttackDelay = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Tentacle", meta=(ClampMin="0", Units="s"))
	float TentacleWindupDuration = 1.0f;

	/** Attaque finie : attente avant de replonger */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Tentacle", meta=(ClampMin="0", Units="s"))
	float RecoverDuration = 10.0f;

	/** Amorce : la tentacule se cabre en arrière (degrés) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Tentacle|Anim")
	float WindupPitch = -15.0f;

	/** Oscillation de la tentacule sortie */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Tentacle|Anim", meta=(ClampMin="0"))
	float SwayAngle = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Tentacle|Anim", meta=(ClampMin="0"))
	float SwaySpeed = 1.3f;

	UPROPERTY(ReplicatedUsing=OnRep_State)
	FYSKrakenTentacleState State;

	UFUNCTION()
	void OnRep_State();

	virtual void TickKraken(float DeltaSeconds) override;

	/** Serveur : déroulé des phases */
	virtual void TickTentacleServer(float DeltaSeconds);

	/** Serveur : nouvelle phase */
	void SetPhase(EYSKrakenTentaclePhase Phase);

	float GetPhaseElapsed() const;

	/** Serveur : début de l'amorce / de l'attaque (sous-classes) */
	virtual void OnWindupStarted() {}
	virtual void OnWindupFinished() { SetPhase(EYSKrakenTentaclePhase::Recover); }

	/** Inclinaison du visuel vers l'avant (degrés, local) */
	virtual float GetVisualPitch() const;

	/** Serveur : point de sortie au hasard à Emerge Distance du bord de la coque du bateau ; faux sans bateau */
	bool PickEmergeLocation(FVector& OutLocation, float& OutYaw) const;

	/** Point du bord de la coque du bateau dans la direction (depuis son centre) */
	static FVector GetHullEdgePoint(const AYSPirateShip* Boat, const FVector& Direction);

	/** Haut de la tentacule (départ des tirs) */
	FVector GetTipLocation() const;

private:

	/** Serveur : heure serveur de la prochaine sortie */
	double NextEmergeServerTime = -1.0;

	EYSKrakenTentaclePhase AppliedPhase = EYSKrakenTentaclePhase::Submerged;

	/** Position locale d'après l'état (toutes machines) */
	void ApplyStateLocally();
};

/** Tentacule à distance : sort à 35 m du bateau, tire une grosse boule d'encre (signe au sol), replonge */
UCLASS(meta=(PrioritizeCategories="Kraken Foe"))
class AYSKrakenTentacleRange : public AYSKrakenTentacle
{
	GENERATED_BODY()

public:

	AYSKrakenTentacleRange();

protected:

	/** Dégâts au core du bateau par boule qui le touche */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink Ball", meta=(ClampMin="0"))
	float InkDamage = 500.0f;

	/** Boules tirées par attaque (chacune visant un point au hasard autour du bateau) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink Ball", meta=(ClampMin="1"))
	int32 InkBallsPerAttack = 3;

	/** Départ de chaque boule après la précédente (choix de Claude) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink Ball", meta=(ClampMin="0", Units="s"))
	float InkBallInterval = 0.25f;

	/** Imprécision : point visé au hasard dans ce rayon autour du centre de la coque (répartition uniforme dans le disque) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink Ball", meta=(ClampMin="0", Units="Centimeters"))
	float InkScatterRadius = 3500.0f;

	/** Rayon de l'impact (signe au sol) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink Ball", meta=(ClampMin="1", Units="Centimeters"))
	float InkRadius = 600.0f;

	/** Vol de la boule après l'amorce */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink Ball", meta=(ClampMin="0.1", Units="s"))
	float InkFlightDuration = 4.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink Ball", meta=(ClampMin="0", Units="Centimeters"))
	float InkApexHeight = 1500.0f;

	/** Taille de la boule (diamètre) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Ink Ball", meta=(ClampMin="1", Units="Centimeters"))
	float InkBallSize = 450.0f;

	virtual void OnWindupStarted() override;

	virtual void OnWindupFinished() override;
};

/** Tentacule de mêlée : sort à 5 m du bateau, s'abat sur lui ; touché = bateau immobilisé et Skull qui en sortent jusqu'à ce qu'elle lâche */
UCLASS(meta=(PrioritizeCategories="Kraken Foe"))
class AYSKrakenTentacleMelee : public AYSKrakenTentacle
{
	GENERATED_BODY()

public:

	AYSKrakenTentacleMelee();

	virtual void Vanish(const TCHAR* Reason) override;

protected:

	/** Zone frappée : bande depuis le pied de la tentacule vers le bateau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Slam", meta=(ClampMin="0", Units="Centimeters"))
	float SlamLength = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Slam", meta=(ClampMin="0", Units="Centimeters"))
	float SlamWidth = 700.0f;

	/** Durée de la chute de la tentacule */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Slam", meta=(ClampMin="0.05", Units="s"))
	float SlamDuration = 0.3f;

	/** Inclinaison une fois abattue (degrés vers l'avant) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Slam")
	float SlamPitch = 70.0f;

	/** Lâche le bateau après avoir perdu cette part de ses PV max depuis la prise (0.5 : la 1re prise lâche sous 50 % ; choix de Claude), ou en mourant */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Slam", meta=(ClampMin="0.01", ClampMax="1"))
	float ReleaseDamageRatio = 0.5f;

	/** NME qui sortent de la tentacule tant qu'elle tient le bateau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Slam")
	TSubclassOf<AYSFoeCharacter> GrabSpawnClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Slam", meta=(ClampMin="0.01"))
	float GrabSpawnsPerSecond = 2.0f;

	/** Les NME sortent du corps de la tentacule posé sur le pont (points au-dessus du pont, à cette hauteur), et tombent d'elle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Slam", meta=(ClampMin="0", Units="Centimeters"))
	float GrabSpawnHeight = 250.0f;

	/** Taille du visuel (prop) par rapport à la zone frappée, qui ne change pas (1.25 : la tentacule dépasse sa hitbox d'attaque) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Slam", meta=(ClampMin="0.1"))
	float VisualScale = 1.25f;

	/** Signe au sol de l'amorce (zone frappée) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Slam|Warning")
	TObjectPtr<UMaterialInterface> WarningMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Slam|Warning")
	FLinearColor WarningColor = FLinearColor(1.0f, 0.1f, 0.05f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Slam|Warning", meta=(ClampMin="0", ClampMax="1"))
	float WarningOpacity = 0.45f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UYSGroundZoneComponent> SlamWarning;

	virtual void TickTentacleServer(float DeltaSeconds) override;

	virtual void TickKraken(float DeltaSeconds) override;

	virtual void OnWindupFinished() override;

	virtual float GetVisualPitch() const override;

	virtual void Die() override;

private:

	/** Point du pont où sortent les NME (prise) */
	FVector GrabPoint = FVector::ZeroVector;
	TArray<FVector> GrabSpawnPoints;
	float GrabStartHealth = 0.0f;
	float GrabSpawnAccumulator = 0.0f;
	bool bHoldingBoat = false;
	bool bWarningShown = false;

	/** Bande frappée : premier point dans la coque du bateau (faux s'il est raté) */
	bool FindSlamHit(FVector& OutPoint) const;

	void ReleaseBoat(const TCHAR* Reason);
	void SpawnGrabFoe();
};

/** Boule d'encre d'une tentacule à distance : signe au sol au point visé, puis vol en cloche ; touche le bateau dans le rayon (serveur) */
USTRUCT()
struct FYSKrakenInkBallFlight
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Start = FVector::ZeroVector;

	UPROPERTY()
	FVector Target = FVector::ZeroVector;

	/** Heure serveur du départ de la boule (le signe au sol est affiché avant) */
	UPROPERTY()
	double LaunchServerTime = 0.0;

	UPROPERTY()
	float FlightDuration = 1.5f;

	UPROPERTY()
	float ApexHeight = 1500.0f;

	UPROPERTY()
	float Radius = 600.0f;

	UPROPERTY()
	float BallSize = 450.0f;
};

UCLASS()
class AYSKrakenInkBall : public AActor
{
	GENERATED_BODY()

public:

	AYSKrakenInkBall();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void Tick(float DeltaSeconds) override;

	/** Serveur, avant FinishSpawning */
	void Init(const FYSKrakenInkBallFlight& InFlight, float InDamage) { Flight = InFlight; Damage = InDamage; }

protected:

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Ball;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UYSGroundZoneComponent> Marker;

	UPROPERTY(EditAnywhere, Category="Ink Ball")
	TObjectPtr<UMaterialInterface> MarkerMaterial;

	/** Violet lumineux : visible sur la mer ET sur l eau sombre de l arène (le violet très sombre d avant y disparaissait) */
	UPROPERTY(EditAnywhere, Category="Ink Ball")
	FLinearColor MarkerColor = FLinearColor(0.55f, 0.1f, 1.0f);

	UPROPERTY(EditAnywhere, Category="Ink Ball", meta=(ClampMin="0"))
	float MarkerGlow = 0.8f;

	UPROPERTY(ReplicatedUsing=OnRep_Flight)
	FYSKrakenInkBallFlight Flight;

	UFUNCTION()
	void OnRep_Flight();

private:

	float Damage = 0.0f;
	bool bLanded = false;
	bool bMarkerBuilt = false;

	void BuildMarker();
};

/** Un tir en cloche d'un motif de la tête finale : point d'impact (niveau de la mer) et départ après le lancement du motif */
USTRUCT()
struct FYSKrakenInkShot
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize Target = FVector::ZeroVector;

	UPROPERTY()
	float Delay = 0.0f;
};

/** Tous les tirs en cloche d'un motif de la tête finale (une seule entité, copie du rendu des boules d'encre des tentacules) :
 *  chaque machine affiche les signes au sol (Marker Lead Time avant le départ, jusqu'à l'impact) et les boules en vol,
 *  le serveur applique les dégâts au bateau des joueurs à l'impact */
UCLASS()
class AYSKrakenInkBarrage : public AActor
{
	GENERATED_BODY()

public:

	AYSKrakenInkBarrage();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void Tick(float DeltaSeconds) override;

	/** Serveur, avant FinishSpawning */
	void Init(const FVector& InStart, float InFlightDuration, float InApexHeight, float InRadius, float InBallSize, float InMarkerLead, float InDamage,
		const TArray<FYSKrakenInkShot>& InShots);

protected:

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UInstancedStaticMeshComponent> BallMeshes;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UProceduralMeshComponent> Markers;

	/** Signes au sol : même violet lumineux que ceux des boules d encre des tentacules */
	UPROPERTY(EditAnywhere, Category="Ink Barrage")
	FLinearColor MarkerColor = FLinearColor(0.55f, 0.1f, 1.0f, 0.45f);

	UPROPERTY(EditAnywhere, Category="Ink Barrage")
	FLinearColor MarkerEdgeColor = FLinearColor(0.85f, 0.5f, 1.0f, 0.95f);

	UPROPERTY(EditAnywhere, Category="Ink Barrage")
	TObjectPtr<UMaterialInterface> MarkerMaterial;

	UPROPERTY(Replicated)
	FVector Start = FVector::ZeroVector;

	UPROPERTY(Replicated)
	float FlightDuration = 3.0f;

	UPROPERTY(Replicated)
	float ApexHeight = 2500.0f;

	UPROPERTY(Replicated)
	float Radius = 600.0f;

	UPROPERTY(Replicated)
	float BallSize = 450.0f;

	UPROPERTY(Replicated)
	float MarkerLead = 1.0f;

	UPROPERTY(Replicated)
	double LaunchServerTime = 0.0;

	UPROPERTY(Replicated)
	TArray<FYSKrakenInkShot> Shots;

private:

	float Damage = 0.0f;

	/** Tirs déjà tombés (local) */
	TBitArray<> Landed;

	/** Signes au sol affichés (local) : maillage reconstruit quand ça change */
	TBitArray<> MarkerShown;

	void RebuildMarkers();
};
