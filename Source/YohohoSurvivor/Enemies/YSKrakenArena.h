#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YSKrakenArena.generated.h"

class AYSKrakenPart;
class AYSKrakenHead;
class AYSKrakenHeadFinal;
class AYSKrakenTentacleRange;
class AYSKrakenTentacleMelee;
class AYSPirateShip;
class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;

/**
 *  « Boss fight final » (partie 3 du flow) : zone d'eau sombre en mer (Radius 200 m), posée au lancement par AYSFlowMode à 750 m du centre.
 *  Pointillés géants du bateau des joueurs à son centre une fois toutes les boussoles ramenées (« Trouvez la créature »).
 *  Le bateau des joueurs qui y entre lance la partie 3 (toutes les îles coulent, « Affrontez le Kraken ») et le combat en 3 étapes :
 *    1) tête + tentacules (barre du boss = PV de la tête ; tête morte : tentacules restantes retirées),
 *    2) tentacules seules (barre = PV cumulés de toutes),
 *    3) tête finale (barre = ses PV) ; morte : « BRAVO » puis retour au menu.
 *  Chaque changement d'étape : level up forcé et bateau régénéré (core et postes). Serveur pour le combat, toutes machines pour le visuel.
 */
UCLASS(meta=(PrioritizeCategories="Kraken"))
class AYSKrakenArena : public AActor
{
	GENERATED_BODY()

public:

	AYSKrakenArena();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void Tick(float DeltaSeconds) override;

	float GetRadius() const { return Radius; }

	/** Serveur : un morceau est mort (la barre et l'étape suivent au Tick) */
	void OnPartKilled(AYSKrakenPart* Part);

	/** Serveur : une tentacule de mêlée tient / lâche le bateau (immobilisé tant qu'au moins une le tient) */
	void AddBoatGrab();
	void RemoveBoatGrab();

	/** Serveur, debug : combat lancé tout de suite (partie 3), à l'étape Stage (1 à 3) */
	void DebugStartStage(int32 Stage);

	/** Serveur, debug : morceaux de l'étape en cours tués */
	void DebugKillStage();

	bool IsFightStarted() const { return CurrentStage > 0; }

	/** L'arène de la partie (ou nullptr) */
	static AYSKrakenArena* Find(const UWorld* World);

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---------------------------------------------------------------- zone

	/** Rayon de la zone (le bateau qui y entre lance la partie 3) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Arena", meta=(ClampMin="100", Units="Centimeters"))
	float Radius = 20000.0f;

	/** Eau sombre : opaque jusqu'à Inner Radius, puis s'estompe jusqu'au bord (Radius) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Arena|Water", meta=(ClampMin="0", Units="Centimeters"))
	float InnerRadius = 15000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Arena|Water")
	FLinearColor WaterColor = FLinearColor(0.004f, 0.015f, 0.06f, 1.0f);

	/** Hauteur du disque au-dessus de la surface de la mer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Arena|Water", meta=(Units="Centimeters"))
	float WaterHeightAboveSea = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Arena|Water", meta=(ClampMin="8"))
	int32 WaterSegments = 128;

	/** Anneaux du dégradé (Inner Radius -> Radius) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Arena|Water", meta=(ClampMin="1"))
	int32 WaterFadeRings = 12;

	/** Translucide, couleur et alpha des sommets (défaut : /Game/Core/Ship/M_ShipPreview) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Arena|Water")
	TObjectPtr<UMaterialInterface> WaterMaterial;

	/** Partie 3 seulement quand toutes les boussoles sont ramenées au bateau (décoché : dès que le bateau entre dans la zone) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Arena")
	bool bRequireAllCompasses = true;

	// ---------------------------------------------------------------- pointillés « Trouvez la créature »

	/** Pointillés du bateau au centre de la zone (même rendu que la carte au trésor, 3x plus gros) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Guide Line")
	bool bShowGuideLine = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Guide Line", meta=(ClampMin="1", Units="Centimeters"))
	float DashSpacing = 210.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Guide Line", meta=(ClampMin="1", Units="Centimeters"))
	float DashLength = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Guide Line", meta=(ClampMin="1", Units="Centimeters"))
	float DashWidth = 42.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Guide Line", meta=(ClampMin="0.1", Units="Centimeters"))
	float DashThickness = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Guide Line", meta=(Units="Centimeters"))
	float DashHeightAboveSea = 20.0f;

	/** Pas de pointillés à moins de cette distance du bateau (sous la coque) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Guide Line", meta=(ClampMin="0", Units="Centimeters"))
	float DashStartOffset = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Guide Line")
	TObjectPtr<UStaticMesh> DashMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Guide Line")
	TObjectPtr<UMaterialInterface> DashMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Guide Line")
	FLinearColor DashColor = FLinearColor(1.0f, 0.85f, 0.2f);

	UPROPERTY(EditAnywhere, Category="Kraken|Guide Line")
	FName DashColorParameterName = TEXT("Color");

	// ---------------------------------------------------------------- combat

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Fight")
	TSubclassOf<AYSKrakenHead> HeadClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Fight")
	TSubclassOf<AYSKrakenTentacleRange> TentacleRangeClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Fight")
	TSubclassOf<AYSKrakenTentacleMelee> TentacleMeleeClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Fight")
	TSubclassOf<AYSKrakenHeadFinal> HeadFinalClass;

	/** Étape 1 : tentacules avec la tête */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Fight", meta=(ClampMin="0"))
	int32 Stage1RangeTentacles = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Fight", meta=(ClampMin="0"))
	int32 Stage1MeleeTentacles = 2;

	/** Étape 2 : tentacules seules */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Fight", meta=(ClampMin="0"))
	int32 Stage2RangeTentacles = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Fight", meta=(ClampMin="0"))
	int32 Stage2MeleeTentacles = 2;

	/**
	 *  Bonus : Davy Jones vaincu (world boss) = tentacules des étapes 1 et 2 x ce multiplicateur, à distance et de mêlée chacune de leur côté,
	 *  arrondi au-dessus (0.5 : 2 + 2 -> 1 + 1, 6 + 2 -> 3 + 1)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Fight", meta=(ClampMin="0", ClampMax="1"))
	float DavyJonesTentacleMultiplier = 0.5f;

	/**
	 *  Première sortie des tentacules d'une étape décalée de ces secondes de l'une à l'autre (0 = toutes ensemble, 10 s après le début) ;
	 *  choix de Claude : sinon les 6 tentacules à distance de l'étape 2 tiraient ensemble (6 x 1000 sur le sloop de 5000 PV)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Fight", meta=(ClampMin="0", Units="s"))
	float TentacleStagger = 1.5f;

	/** Étape 2 : toutes les tentacules sortent de l'eau dès le début de l'étape (sans First Emerge Delay ni Tentacle Stagger) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Fight")
	bool bStage2EmergeImmediately = true;

	// ---------------------------------------------------------------- courant (étapes 1 à 3)

	/**
	 *  Pendant tout le combat (étapes 1 à 3), le courant tire le bateau des joueurs vers le centre de l'arène, d'autant plus fort
	 *  qu'il est loin du centre. Vitesses en part de la vitesse max avant du bateau (avec ses bonus) : sortir de l'arène est
	 *  quasiment impossible, le centre reste calme
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Current")
	bool bArenaCurrent = true;

	/** Zone calme : jusqu'à cette part du rayon, le courant monte doucement de 0 à Calm Speed Ratio */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Current", meta=(ClampMin="0", ClampMax="0.95"))
	float CurrentCalmRadiusRatio = 0.35f;

	/** Courant au bord de la zone calme (x vitesse max du bateau) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Current", meta=(ClampMin="0"))
	float CurrentCalmSpeedRatio = 0.08f;

	/** Courant au bord de l'arène (x vitesse max du bateau ; 1 = impossible d'en sortir) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Current", meta=(ClampMin="0"))
	float CurrentEdgeSpeedRatio = 0.95f;

	/** Au-delà du bord : courant encore plus fort (x vitesse max du bateau, atteint à Radius x 1.1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Current", meta=(ClampMin="0"))
	float CurrentOutsideSpeedRatio = 1.3f;

	/** Courbe entre la zone calme et le bord (1 = linéaire ; plus grand = faible longtemps puis très fort près du bord) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Current", meta=(ClampMin="0.1"))
	float CurrentExponent = 2.0f;

	/** VFX permanent du courant (comme l'aspiration de la tête, en très léger) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Current|VFX")
	FLinearColor CurrentVfxColor = FLinearColor(0.35f, 0.75f, 1.0f, 0.12f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Current|VFX", meta=(ClampMin="0", Units="Centimeters"))
	float CurrentVfxInnerRadius = 2000.0f;

	/** 0 = rayon de l'arène */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Current|VFX", meta=(ClampMin="0", Units="Centimeters"))
	float CurrentVfxOuterRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Current|VFX", meta=(ClampMin="1"))
	int32 CurrentVfxRingCount = 5;

	/** Temps pour qu'un anneau aille du bord au centre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Current|VFX", meta=(ClampMin="0.1", Units="s"))
	float CurrentVfxRingPeriod = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Current|VFX", meta=(ClampMin="1", Units="Centimeters"))
	float CurrentVfxRingWidth = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Current|VFX", meta=(ClampMin="0"))
	int32 CurrentVfxSpiralArms = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Current|VFX", meta=(Units="DegreesPerSecond"))
	float CurrentVfxSpinSpeed = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Current|VFX", meta=(Units="Centimeters"))
	float CurrentVfxHeightAboveSea = 12.0f;

	/** Défaut : /Game/Core/Ship/M_ShipPreview (comme l'aspiration) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Current|VFX")
	TObjectPtr<UMaterialInterface> CurrentVfxMaterial;

	/** Noms des barres du boss */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Fight")
	FText HeadBarName = NSLOCTEXT("YS", "KrakenHeadBar", "Kraken");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Fight")
	FText TentaclesBarName = NSLOCTEXT("YS", "KrakenTentaclesBar", "Tentacules du Kraken");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Fight")
	FText FinalHeadBarName = NSLOCTEXT("YS", "KrakenFinalBar", "Kraken");

	/** Changement d'étape : level up forcé (l'XP qui manque) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Fight")
	bool bLevelUpOnStageChange = true;

	/** Changement d'étape : core et postes du bateau des joueurs remis au max */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Fight")
	bool bRepairBoatOnStageChange = true;

	/** Tête finale morte : « BRAVO » puis retour au menu après ce délai */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Kraken|Fight", meta=(ClampMin="0", Units="s"))
	float VictoryReturnDelay = 10.0f;

	/** Étape en cours (0 = pas commencé, 4 = vaincu) */
	UPROPERTY(Replicated)
	int32 CurrentStage = 0;

private:

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> WaterDisc;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> GuideDashes;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> CurrentVfx;

	/** Heure serveur du début du combat (VFX du courant, toutes machines) */
	UPROPERTY(Replicated)
	double FightStartServerTime = 0.0;

	bool bCurrentVfxVisible = false;
	bool bCurrentApplied = false;

	void UpdateCurrentVfx();

	/** Serveur : courant posé sur le bateau des joueurs pendant le combat, retiré sinon */
	void UpdateBoatCurrent();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DashMaterialInstance;

	/** Morceaux de l'étape en cours (serveur) */
	TArray<TWeakObjectPtr<AYSKrakenPart>> Parts;

	/** Étape 2 : PV max cumulés des tentacules */
	float StageMaxHealth = 0.0f;

	int32 BoatGrabs = 0;
	float GuideRefreshTimer = 0.0f;
	bool bGuideVisible = false;

	void BuildWaterDisc();
	void UpdateGuideLine(float DeltaSeconds);

	/** Serveur */
	void TickFight();
	void StartStage(int32 Stage, const TCHAR* Reason);
	void SpawnStageParts(int32 Stage);
	AYSKrakenPart* SpawnPart(UClass* Class, const FVector& Location, float EmergeStagger = 0.0f, bool bEmergeNow = false);
	void UpdateBossBar();
	void ForceLevelUpAndRepair();
	AYSPirateShip* GetGameBoat() const;
	float GetSeaLevel() const;
};
