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
	AYSKrakenPart* SpawnPart(UClass* Class, const FVector& Location, float EmergeStagger = 0.0f);
	void UpdateBossBar();
	void ForceLevelUpAndRepair();
	AYSPirateShip* GetGameBoat() const;
	float GetSeaLevel() const;
};
