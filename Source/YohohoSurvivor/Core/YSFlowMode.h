#pragma once

#include "CoreMinimal.h"
#include "Core/YSGameMode.h"
#include "Island/YSIslandData.h"
#include "YSFlowMode.generated.h"

class AYSIsland;
class AYSFoeGalion;
class AYSInteractiveObject;
class AYSPlayerCharacter;
class UDataTable;
class UYSIslandData;
class UYSGameBoatData;
class AYSGameBoatLongboat;
class AYSPirateShip;

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

/** Une nuit de la séquence du flow : les élites / boss qui apparaissent au centre de l'île */
USTRUCT(BlueprintType)
struct FYSFlowNightSetup
{
	GENERATED_BODY()

	/** Count NME par entrée (pas multiplié par le nombre de joueurs) ; Random Foe Classes = tiré au hasard pour chacun */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night")
	TArray<FYSIslandFoeEntry> Foes;
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

	/**
	 *  Îles suivantes, batailles navales et île finale après l'île de départ ; décoché = rien après l'île de départ
	 *  (demande de Paul du 2026-11-09 : déroulé repris à 0, flow en 3 parties)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Next Islands")
	bool bNextIslandsEnabled = true;

	/** Îles qui apparaissent quand l'île de départ commence à couler : en triangle (cercle) autour d'elle, places tirées au hasard */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Next Islands")
	TArray<TObjectPtr<UYSIslandData>> NextIslands;

	/** Écart entre la côte de l'île de départ et celle de chaque nouvelle île (rayons moyens) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Next Islands", meta=(ClampMin="0", Units="Centimeters"))
	float NextIslandsGap = 25000.0f;

	/**
	 *  Distance au centre de l'île de départ x ce multiplicateur (îles suivantes, et donc les batailles navales placées entre elles, et l'écart
	 *  laissé autour de l'île finale) ; 0.7 = 30 % plus près (demande de Paul, 2026-11-08)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Next Islands", meta=(ClampMin="0.1"))
	float NextIslandsDistanceMultiplier = 0.7f;

	/** Décalage au hasard de la direction de chaque île, autour de sa place régulière */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Next Islands", meta=(ClampMin="0", ClampMax="60", Units="Degrees"))
	float NextIslandsAngleJitter = 20.0f;

	/**
	 *  Îles suivantes prégénérées au lancement, sous l'eau (Next Islands Submerged Depth sous leur place) et inactives ; au moment où elles
	 *  apparaissaient (objectifs de l'île de départ livrés), elles remontent à la surface en Next Islands Rise Duration
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Next Islands")
	bool bPregenerateNextIslands = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Next Islands", meta=(ClampMin="0", Units="Centimeters", EditCondition="bPregenerateNextIslands"))
	float NextIslandsSubmergedDepth = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Next Islands", meta=(ClampMin="0.1", Units="s", EditCondition="bPregenerateNextIslands"))
	float NextIslandsRiseDuration = 15.0f;

	/**
	 *  Batailles navales : quand les îles suivantes remontent, une bataille par ligne de Naval Battle Rows, chacune au milieu (entre les côtes)
	 *  d'une paire d'îles différente tirée au hasard parmi les îles suivantes. Les bateaux de NME n'apparaissent plus que là.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Naval Battles", meta=(RequiredAssetDataTags="RowStructure=/Script/YohohoSurvivor.YSNavalBattleRow"))
	TObjectPtr<UDataTable> NavalBattleTable;

	/** Une bataille par nom de ligne (défaut : 2 batailles « Base ») */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Naval Battles")
	TArray<FName> NavalBattleRows;

	/**
	 *  Île finale : prégénérée sous l'eau au lancement (comme les îles suivantes), loin des autres îles, dans l'espace entre deux îles suivantes
	 *  laissé sans bataille navale ; elle remonte quand toutes les îles suivantes sont faites (objectifs principaux livrés, ou en train de couler)
	 *  et toutes les batailles navales gagnées. Vide = pas d'île finale
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Final Island")
	TObjectPtr<UYSIslandData> FinalIsland;

	/** Écart minimal entre la côte de l'île finale et celle de chaque autre île (rayons moyens) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Final Island", meta=(ClampMin="0", Units="Centimeters"))
	float FinalIslandGap = 25000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Final Island", meta=(ClampMin="0.1", Units="s"))
	float FinalIslandRiseDuration = 20.0f;

	/** « Go naviguer » : vagues en mer à chaque fois qu'une île commence à couler (vide par défaut : les bateaux de NME sont dans les batailles navales) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Navigation")
	TArray<FYSNavigationWave> NavigationWaves;

	/**
	 *  Zones d'intérêt (ZI) de la partie 2 : les îles suivantes (Next Islands) et les batailles navales (Naval Battle Rows) sont générées dès le
	 *  lancement, à la surface, sur les sommets d'un polygone régulier (pentagone pour 3 + 2) centré sur l'île de départ, places tirées au hasard.
	 *  Les objets ci-dessous y sont répartis : un CoreBoat par ZI, Compass Count boussoles (une au plus par ZI), les membres d'équipage sur les ZI
	 *  sans boussole, la Super Treasure Map sur une ZI au hasard. Sur une île ce sont des objectifs principaux (la Super Treasure Map : secondaire) ;
	 *  dans une bataille navale ils sont posés sur le pont du Galion à côté du capitaine. Pas d'île finale tant que c'est coché (partie 3 à venir)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Zones of Interest")
	bool bZonesOfInterest = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Zones of Interest", meta=(EditCondition="bZonesOfInterest"))
	TSubclassOf<AYSInteractiveObject> BoatCoreItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Zones of Interest", meta=(EditCondition="bZonesOfInterest"))
	TSubclassOf<AYSInteractiveObject> CompassItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Zones of Interest", meta=(ClampMin="0", EditCondition="bZonesOfInterest"))
	int32 CompassCount = 3;

	/** Membres d'équipage (Medic, Charpentier) : un par ZI sans boussole */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Zones of Interest", meta=(EditCondition="bZonesOfInterest"))
	TArray<TSubclassOf<AYSInteractiveObject>> CrewItemClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Zones of Interest", meta=(EditCondition="bZonesOfInterest"))
	TSubclassOf<AYSInteractiveObject> SuperTreasureMapClass;

	/**
	 *  Zones d'intérêt : un CoreBoat posé dans la zone du core du bateau des joueurs disparaît et part dans le stock (affiché sous les plumes de
	 *  phoenix) ; le bateau passe à l'étape Index dès que le stock atteint Core Boat Cost Per Stage[Index], qui est consommé
	 *  (défaut : sloop -> brigantin 2, brigantin -> galion 3 ; 0 = étape qui ne se gagne pas avec des CoreBoats : chaloupe -> sloop sur l'île de départ)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Zones of Interest", meta=(EditCondition="bZonesOfInterest"))
	TArray<int32> CoreBoatCostPerStage = { 0, 0, 2, 3 };

	/** Sur une île : distance à la plage (min, max) où apparaissent ces objets */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Zones of Interest", meta=(Units="Centimeters", EditCondition="bZonesOfInterest"))
	FVector2D ZoneItemDistanceFromBeach = FVector2D(3000.0, 9000.0);

	/**
	 *  Élites / boss de chaque nuit de la partie, dans l'ordre, quelle que soit l'île (nuit 1 = île de départ) ; ils remplacent les vagues
	 *  « au centre » des Night Events des îles du flow. Au-delà de la liste : la dernière nuit. Vide = Night Events des îles. Les capitaines
	 *  des Galions n'en font pas partie
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Night Sequence")
	TArray<FYSFlowNightSetup> NightSequence;

	/** Partie 2 : durée avant l'overtime (grand timer sous l'objectif des îles) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Part 2", meta=(ClampMin="1", Units="s"))
	float Part2Duration = 1200.0f;

	/** Overtime de la partie 2 : un Galion Nemesis tout de suite puis toutes les ... ; il fonce aborder le bateau des joueurs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Part 2", meta=(ClampMin="1", Units="s"))
	float Part2OvertimeInterval = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Part 2")
	TSubclassOf<AYSFoeGalion> Part2OvertimeGalionClass;

	/** Apparition du Galion de l'overtime : sur l'eau, à au moins / au plus cette distance de tous les joueurs (hors caméra) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Part 2", meta=(ClampMin="0", Units="Centimeters"))
	float Part2OvertimeGalionMinDistance = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Part 2", meta=(ClampMin="0", Units="Centimeters"))
	float Part2OvertimeGalionMaxDistance = 15000.0f;

	/** Partie 3 (à venir) : « BRAVO » (fin de jeu temporaire) puis retour au menu, ces secondes de jeu après son début (0 = jamais) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow|Part 3", meta=(ClampMin="0", Units="s"))
	float BravoDelayAfterPart3 = 60.0f;

	AYSFlowMode();

	virtual void InitGameState() override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/** Une île commence à couler (serveur) : « go naviguer », et les îles suivantes si c'est l'île de départ */
	void OnIslandSinkStarted(AYSIsland* SinkingIsland);

	/** Tous les objectifs principaux d'une île sont livrés (serveur) : GameBoat à l'étape suivante, et les îles suivantes si c'est l'île de départ */
	void OnIslandMainObjectivesDone(AYSIsland* DoneIsland);

	/** Îles suivantes autour de From (serveur, une seule fois) ; bSubmerged = prégénérées sous l'eau */
	void SpawnNextIslands(const AYSIsland* From, bool bSubmerged = false);

	/** Les îles suivantes arrivent dans le flow (serveur, une fois) : remontée (ou apparition), puis batailles navales */
	void BringNextIslands(const AYSIsland* From);

	/** Batailles navales entre les îles suivantes (serveur, une fois) */
	void SpawnNavalBattles();

	/** L'île finale arrive (serveur, une fois) : remontée (ou apparition) */
	void BringFinalIsland(const TCHAR* Reason);

	/** Toutes les îles suivantes faites et toutes les batailles navales gagnées */
	bool AreNextIslandsAndBattlesDone() const;

	/** 0 = île de départ, 1..N = îles suivantes (ordre de Next Islands), N + 1 = île finale */
	AYSIsland* GetIslandByIndex(int32 Index) const;

	AYSIsland* GetFinalIslandActor() const { return FinalIslandActor; }

	/** Debug : réveille toutes les batailles navales */
	void DebugActivateNavalBattles();

	const TArray<TObjectPtr<class AYSNavalBattle>>& GetNavalBattles() const { return NavalBattles; }

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

	/**
	 *  GameBoat : étape suivante (serveur). Depuis la chaloupe échouée : sur l'eau à côté d'elle ; depuis un bateau : à sa place (éloigné de l'île
	 *  la plus proche s'il n'y a pas assez de fond), ceux qui sont à bord et ce qui est posé sur le pont passent sur le nouveau bateau
	 */
	void UpgradeGameBoat();

	/** Étape suivante à l'image suivante (objectif livré dans la zone du core du bateau actuel) */
	void RequestGameBoatUpgrade(const TCHAR* Reason);

	virtual void OnEliteKilled(AYSFoeCharacter* Elite) override;

	/** Une île a fini de couler (serveur) : l'île de départ = début de la partie 2 */
	void OnIslandSunk(AYSIsland* SunkIsland);

	/** La partie 2 commence (serveur, une fois) : grand timer, objectif de l'île de départ effacé */
	void StartPart2(const TCHAR* Reason);

	/** Partie 3 (pas encore appelée) : « BRAVO » puis retour au menu après Bravo Delay After Part 3 */
	void StartPart3(const TCHAR* Reason);

	bool IsPart2Started() const { return bPart2Started; }

	/**
	 *  Nuit qui commence sur une île du flow (serveur) : élites / boss de la nuit suivante de la séquence (OutClasses) ; faux sans séquence
	 *  (Night Events de l'île)
	 */
	bool TakeNightSetup(const AYSIsland* NightIsland, FRandomStream& Rng, TArray<UClass*>& OutClasses);

	/**
	 *  Triche (flèche bas, partie 1) : sloop avec un poste de combat de chaque, joueurs sur son pont, l'île de départ coule, partie 2 tout de suite
	 */
	void DebugSkipToPart2();

	/** Debug : Galion de l'overtime de la partie 2 tout de suite */
	void DebugSpawnPart2Galion();

	/** Les CoreBoats livrés au bateau des joueurs partent dans le stock (zones d'intérêt) */
	bool StocksCoreBoats() const { return bZonesOfInterest && BoatCoreItemClass; }

	/** Objet à stocker : un CoreBoat (serveur) */
	bool IsStockedCoreBoat(const AYSInteractiveObject* Object) const;

	/** Objet d'une zone d'intérêt posé sur un Galion (boussole, équipage...) : le core du bateau des joueurs le prend */
	bool IsZoneItem(const AYSInteractiveObject* Object) const;

	/** CoreBoat posé dans la zone du core du bateau des joueurs (serveur) : détruit, +1 au stock ; l'étape suivante se fait à l'image suivante */
	void StoreCoreBoat(AYSInteractiveObject* Object);

	/** Debug : CoreBoats ajoutés au stock */
	void DebugAddCoreBoats(int32 Count);

	/** Boussole posée dans la zone du core du bateau des joueurs (serveur) : une de moins à trouver dans l'objectif en mer */
	void NotifyCompassDelivered(AYSInteractiveObject* Compass);

	bool IsCompass(const AYSInteractiveObject* Object) const;

	/** L'objectif en mer (boussoles, Galion à couler) remplace celui de l'île de départ dans le HUD (serveur, une fois) */
	void StartSeaObjectives(const TCHAR* Reason);

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
	bool bNextIslandsBrought = false;
	bool bNavalBattlesSpawned = false;
	bool bFinalIslandBrought = false;

	UPROPERTY(Transient)
	TObjectPtr<AYSIsland> FinalIslandActor;

	/** Les deux îles suivantes entre lesquelles est l'île finale : pas de bataille navale entre elles */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AYSIsland>> FinalIslandPair;

	/** Île finale (serveur, une fois) ; bSubmerged = prégénérée sous l'eau */
	void SpawnFinalIsland(bool bSubmerged);

	UPROPERTY(Transient)
	TArray<TObjectPtr<class AYSNavalBattle>> NavalBattles;

	/** Vagues en mer à venir : temps de partie et vague */
	TArray<TPair<float, FYSNavigationWave>> PendingNavigationWaves;

	/** Rang du prochain joueur qui apparaît */
	int32 NextPlayerSpot = 0;

	/** Étape actuelle du GameBoat */
	int32 GameBoatStage = 0;

	/** Étapes demandées pas encore faites (une par image) */
	int32 PendingGameBoatUpgrades = 0;

	/** Passe ceux qui sont à bord de OldBoat (joueurs, minions, NME, objets posés, PNJ livrés) sur NewBoat, aux mêmes places proportionnellement */
	void TransferAboard(AYSPirateShip* OldBoat, AYSPirateShip* NewBoat);

	/** Postes de combat posés sur les réceptacles de OldBoat : reposés sur ceux de NewBoat (même rang d'abord), sinon rendus en objet sur son pont */
	void TransferCombatPosts(AYSPirateShip* OldBoat, AYSPirateShip* NewBoat);

	/** Chaloupe de départ encore en route, ou nullptr */
	AYSGameBoatLongboat* GetArrivingBoat() const;

	/** Zones d'intérêt (serveur, au lancement) : îles et batailles navales en polygone autour de l'île de départ, objets répartis */
	void SpawnZonesOfInterest();

	/** Galion de l'overtime de la partie 2 (serveur) : faux s'il n'y a pas de place */
	bool SpawnPart2Galion();

	bool bPart2Started = false;
	bool bPart3Started = false;

	/** Nuits commencées sur les îles du flow (séquence de nuits) */
	int32 FlowNightsStarted = 0;

	/** Partie 2 : temps écoulé (depuis son début) du prochain Galion de l'overtime */
	float NextPart2GalionElapsed = -1.0f;

	/** CoreBoats en stock (serveur ; copié dans le GameState pour le HUD) */
	int32 CoreBoatStock = 0;

	/** Boussoles posées dans les zones d'intérêt, et ramenées au bateau */
	int32 CompassTotal = 0;
	int32 CompassesFound = 0;
	bool bSeaObjectivesStarted = false;

	/** Objets des zones d'intérêt posés sur les Galions */
	TArray<TWeakObjectPtr<AYSInteractiveObject>> ZoneItems;

	/** CoreBoats nécessaires pour l'étape après GameBoatStage (0 = aucune) */
	int32 GetNextStageCoreBoatCost() const;

	/** Stock et coût de l'étape suivante dans le GameState ; étape suivante si le stock suffit (serveur, Tick) */
	void UpdateCoreBoatStock();
};
