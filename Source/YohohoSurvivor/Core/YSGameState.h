#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/YSCustomization.h"
#include "Core/YSShipRewardTypes.h"
#include "YSGameState.generated.h"

class AYSDamageNumber;
class AYSPlayerCharacter;
class AYSFoeCharacter;
class AYSPirateShip;
class UTexture2D;
class UMaterialInterface;

/** Objectif affiché du flow en 3 parties (partie 1 : île de départ) */
UENUM(BlueprintType)
enum class EYSFlowObjective : uint8
{
	/** Rien d'affiché (traversée, pas d'île à objectifs) */
	None,
	/** Trouve et ramène un CoreBoat */
	FindCore,
	/** Survivez et défendez le navire (bateau non pilotable) */
	Defend,
	/** Quittez l'île : compteur de joueurs à bord, puis l'île coule */
	Evacuate,
	/** Partie 2 atteinte : « Victoire » (temporaire) */
	Victory,
	/** En mer (après l'île de départ) : boussoles à trouver, ou Galion à couler pendant une bataille navale */
	Sea
};

/** État du flow affiché par le HUD (UYSFlowHUDWidget), écrit par l'île à objectifs (serveur) */
USTRUCT(BlueprintType)
struct FYSFlowStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Flow")
	EYSFlowObjective Objective = EYSFlowObjective::None;

	UPROPERTY(BlueprintReadOnly, Category="Flow")
	FText ObjectiveText;

	/** Heure serveur du début de la phase (jour, nuit, évacuation) et sa durée (0 = pas de barre) */
	UPROPERTY(BlueprintReadOnly, Category="Flow")
	double PhaseStartServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="Flow")
	float PhaseDuration = 0.0f;

	/** Phase de nuit (couleur de la barre) */
	UPROPERTY(BlueprintReadOnly, Category="Flow")
	bool bNight = false;

	/** Nuit dont le temps est écoulé : « OVERTIME » à la place de la barre */
	UPROPERTY(BlueprintReadOnly, Category="Flow")
	bool bOvertime = false;

	/** Évacuation : joueurs vivants à bord du bateau des joueurs / joueurs vivants */
	UPROPERTY(BlueprintReadOnly, Category="Flow")
	int32 BoardedPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category="Flow")
	int32 BoardingTotal = 0;

	/** Le bateau des joueurs ne peut pas être piloté (barre refusée) */
	UPROPERTY(BlueprintReadOnly, Category="Flow")
	bool bHelmLocked = false;

	/** Chronomètre arrêté à la victoire (temps de partie, -1 = il tourne) */
	UPROPERTY(BlueprintReadOnly, Category="Flow")
	float FinalMatchTime = -1.0f;

	/** Île qui affiche cet objectif (en mer, chaque joueur voit l'objectif de la mer tant qu'il n'est pas sur elle) */
	UPROPERTY(BlueprintReadOnly, Category="Flow")
	TObjectPtr<AActor> Island;
};

/** Barre de vie du boss Kraken (HUD, à la place de la première barre d'élite), écrite par l'arène (serveur) */
USTRUCT(BlueprintType)
struct FYSKrakenBossBar
{
	GENERATED_BODY()

	UPROPERTY()
	bool bActive = false;

	UPROPERTY()
	FText Name;

	UPROPERTY()
	float Health = 0.0f;

	UPROPERTY()
	float MaxHealth = 0.0f;

	/** Étape du combat (1 à 3) */
	UPROPERTY()
	int32 Stage = 0;
};

/**
 *  État de partie partagé. Sert de canal pour les événements visuels globaux
 *  (chiffres de dégâts) : il existe toujours, même quand la victime vient d'être détruite.
 *  Porte aussi l'état de la sélection de personnage (liste proposée, partie démarrée).
 */
UCLASS()
class AYSGameState : public AGameStateBase
{
	GENERATED_BODY()

protected:

	/** Acteur utilisé pour afficher les chiffres de dégâts */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<AYSDamageNumber> DamageNumberClass;

	/** Décalage horizontal aléatoire max des chiffres, pour éviter qu'ils se superposent */
	UPROPERTY(EditAnywhere, Category="UI", meta=(ClampMin="0"))
	float DamageNumberJitter = 25.0f;

	/** Personnages proposés dans le menu de sélection (copiés depuis AYSGameMode) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Character Select")
	TArray<TSubclassOf<AYSPlayerCharacter>> SelectableCharacters;

	/** Couleurs et visages proposés à l'écran de sélection (copiés depuis AYSGameMode) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Customization")
	TArray<FYSCustomizationColor> CustomizationColors;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Customization")
	TArray<TObjectPtr<UTexture2D>> CustomizationFaces;

	/** Vrai une fois que tous les joueurs ont choisi leur personnage */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Character Select")
	bool bMatchStarted = false;

	/** FlowMod : sélection des personnages avec le GameBoat en fond */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Character Select")
	bool bBoatIntro = false;

	/** FlowMod : le bateau des joueurs, à son étape actuelle */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Game Boat")
	TObjectPtr<AYSPirateShip> GameBoat;

	/** Objectif et timer du flow (HUD) */
	UPROPERTY(Replicated)
	FYSFlowStatus FlowStatus;

	/** Flow : partie en cours (1 = île de départ, 2 = îles et batailles navales, 3 = à venir) */
	UPROPERTY(Replicated)
	int32 FlowPart = 1;

	/** Partie 2 : temps de partie (GetMatchElapsedTime) de son début (-1 = pas commencée) et durée avant l'overtime */
	UPROPERTY(Replicated)
	float Part2StartMatchTime = -1.0f;

	UPROPERTY(Replicated)
	float Part2Duration = 0.0f;

	/** Flow (zones d'intérêt) : CoreBoats en stock (inventaire du HUD), et combien il en faut pour l'étape suivante du bateau (0 = aucune) */
	UPROPERTY(Replicated)
	bool bCoreBoatStock = false;

	UPROPERTY(Replicated)
	int32 CoreBoatStock = 0;

	UPROPERTY(Replicated)
	int32 NextStageCoreBoatCost = 0;

	/** Objectif en mer affiché (fin de l'île de départ) : boussoles ramenées au bateau / posées dans les zones d'intérêt */
	UPROPERTY(Replicated)
	bool bSeaObjectives = false;

	UPROPERTY(Replicated)
	int32 CompassTotal = 0;

	UPROPERTY(Replicated)
	int32 CompassesFound = 0;

	/** Partie 3 : barre de vie du boss Kraken */
	UPROPERTY(Replicated)
	FYSKrakenBossBar KrakenBossBar;

	/** Temps serveur (GetServerWorldTimeSeconds) du démarrage de la partie */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Match")
	double MatchStartServerTime = 0.0;

	/** Niveau de l'équipe (partagé par tous les joueurs) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Level")
	int32 TeamLevel = 0;

	/** XP accumulée dans le niveau en cours (partagée par tous les joueurs) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Level")
	float TeamExperience = 0.0f;

	/** XP nécessaire pour passer au niveau suivant (calculée par le GameMode) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Level")
	float ExperienceToNextLevel = 50.0f;

	/** Sélection d'upgrade en cours (partie en pause) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Level")
	bool bUpgradeSelectionActive = false;

	/** Niveau atteint par la sélection d'upgrade en cours (plusieurs niveaux gagnés d'un coup : sélections enchaînées) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Level")
	int32 UpgradeSelectionLevel = 0;

	/** Type de la sélection d'upgrade en cours : level up ou récompense de la jauge de doublons (niveau du bateau = Upgrade Selection Level) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Level")
	EYSUpgradeSelectionKind UpgradeSelectionKind = EYSUpgradeSelectionKind::LevelUp;

	/** Jour en cours (1 au départ) : sert aussi de niveau aux NME et de multiplicateur aux vagues */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="DayNight")
	int32 DayNumber = 1;

	/** Nuit en cours (dure tant que l'élite est en vie) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="DayNight")
	bool bNight = false;

	/** Temps serveur du début du jour en cours */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="DayNight")
	double DayStartServerTime = 0.0;

	/** Élite de la nuit en cours (nullptr hors nuit ou une fois mort) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="DayNight")
	TObjectPtr<AYSFoeCharacter> CurrentElite;

	/** Objectif commun : doublons gagnés par l'équipe = XP du bateau, dans son niveau actuel, et seuil du niveau suivant */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Objective")
	int32 Doubloons = 0;

	/** Stock commun de plumes de phoenix : une plume = une réanimation */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Objective")
	int32 PhoenixFeathers = 0;

	/** Niveau du bateau (0 = pas encore arrivé ; 1 = arrivé au premier objectif de doublons) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Objective")
	int32 ShipLevel = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Objective")
	int32 DoubloonGoal = 100;

	/** Le bateau est là (GameMap : seulement une fois l'objectif de doublons atteint) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Ship")
	bool bShipPresent = true;

	/** Joueurs vivants à bord du bateau / joueurs vivants (« X/Y sur le bateau ») */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Ship")
	int32 ShipAboardPlayers = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Ship")
	int32 ShipTotalPlayers = 0;

	/** Fin de jeu temporaire : tous les joueurs sont à bord (« BRAVO » puis retour au menu après VictoryReturnDelay) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Ship")
	bool bVictory = false;

	/** Game over : le bateau des joueurs a coulé (« GAME OVER » puis retour au menu après VictoryReturnDelay) */
	UPROPERTY(Replicated)
	bool bGameOver = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Ship")
	float VictoryReturnDelay = 5.0f;

	/**
	 *  Chrono global (HUD en haut à droite, écran des scores) : il continue pendant les pauses du choix d'upgrade (demande de Paul du
	 *  2026-11-12), contrairement au temps de partie du jeu (phases, partie 2, vagues). Durée des pauses finies, pause en cours,
	 *  temps final (fin de partie ; -1 = il tourne)
	 */
	UPROPERTY(Replicated)
	float ClockPausedTime = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_ClockPaused)
	bool bClockPaused = false;

	UPROPERTY(Replicated)
	float FinalClockTime = -1.0f;

	/** Début de la pause en cours, en temps réel de cette machine */
	double ClockPauseStartRealTime = 0.0;

	UFUNCTION()
	void OnRep_ClockPaused();

	/** Fin de partie (serveur, une fois) : chrono arrêté, upgrades des joueurs copiées pour l'écran des scores */
	void FinishMatch();

	/** L'île est en overtime */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Overtime")
	bool bOvertime = false;

	/** L'île coule : temps serveur du début (0 = non), durée, profondeur, courbe ; chaque machine déplace localement l'île et ce qui y est accroché */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Overtime")
	double IslandSinkStartServerTime = 0.0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Overtime")
	float IslandSinkDuration = 30.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Overtime")
	float IslandSinkDepth = 1200.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Overtime")
	float IslandSinkEaseExponent = 1.5f;

	/** Acteurs qui coulent (Landscape et acteurs accrochés, récursivement) et leur position de départ (local) */
	TArray<TPair<TWeakObjectPtr<AActor>, FVector>> SinkingActors;
	bool bSinkingActorsGathered = false;

	/** Descente de l'île appliquée sur cette machine */
	void UpdateIslandSink();

	/** Port placé au hasard (AYSGameMode::PlaceHarbor) : nouvelle position du ponton (tag HarborPivot) et rotation autour de lui */
	UPROPERTY(Replicated)
	bool bHarborPlaced = false;

	UPROPERTY(Replicated)
	FVector HarborPivot = FVector::ZeroVector;

	UPROPERTY(Replicated)
	float HarborYawDelta = 0.0f;

	/** Port déjà déplacé sur cette machine */
	bool bHarborApplied = false;

	/** Déplace les acteurs du port (tag Harbor) sur cette machine, une fois la position reçue */
	void UpdateHarborPlacement();

public:

	AYSGameState();

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const TArray<TSubclassOf<AYSPlayerCharacter>>& GetSelectableCharacters() const { return SelectableCharacters; }

	/** Personnage unique (Unique Character) déjà choisi par un autre joueur qu'Except (serveur et clients : menu grisé) */
	bool IsCharacterTaken(int32 CharacterIndex, const APlayerState* Except) const;

	bool IsMatchStarted() const { return bMatchStarted; }

	bool UsesBoatIntro() const { return bBoatIntro; }

	AYSPirateShip* GetGameBoat() const { return GameBoat; }

	/** Serveur uniquement */
	void SetBoatIntro(bool bEnabled) { bBoatIntro = bEnabled; ForceNetUpdate(); }

	/** Serveur uniquement */
	void SetGameBoat(AYSPirateShip* Boat) { GameBoat = Boat; ForceNetUpdate(); }

	const FYSFlowStatus& GetFlowStatus() const { return FlowStatus; }

	/** Serveur uniquement (île à objectifs du flow) */
	void SetFlowStatus(const FYSFlowStatus& Status) { FlowStatus = Status; ForceNetUpdate(); }

	/** Temps restant de la phase du flow (0 sans barre) */
	float GetFlowPhaseRemaining() const;

	int32 GetFlowPart() const { return FlowPart; }

	/** Serveur uniquement (AYSFlowMode) : partie 2 commencée maintenant (temps de partie), Duration avant l'overtime */
	void StartFlowPart2(float StartMatchTime, float Duration) { FlowPart = 2; Part2StartMatchTime = StartMatchTime; Part2Duration = Duration; ForceNetUpdate(); }

	/** Serveur uniquement (AYSFlowMode) : partie 3 (combat contre le Kraken, sans timer) */
	void StartFlowPart3() { FlowPart = 3; ForceNetUpdate(); }

	const FYSKrakenBossBar& GetKrakenBossBar() const { return KrakenBossBar; }

	/** Serveur uniquement (AYSKrakenArena) ; envoyé seulement s'il change */
	void SetKrakenBossBar(const FYSKrakenBossBar& Bar)
	{
		if (Bar.bActive != KrakenBossBar.bActive || Bar.Stage != KrakenBossBar.Stage || !FMath::IsNearlyEqual(Bar.Health, KrakenBossBar.Health)
			|| !FMath::IsNearlyEqual(Bar.MaxHealth, KrakenBossBar.MaxHealth) || !Bar.Name.EqualTo(KrakenBossBar.Name))
		{
			KrakenBossBar = Bar;
			ForceNetUpdate();
		}
	}

	/** Toutes les boussoles ramenées au bateau (objectif « Trouvez la créature ») */
	bool AreAllCompassesFound() const { return bSeaObjectives && CompassTotal > 0 && CompassesFound >= CompassTotal; }

	/** Partie 2 : temps écoulé depuis son début (-1 = pas commencée) */
	float GetPart2Elapsed() const { return Part2StartMatchTime >= 0.0f ? FMath::Max(GetMatchElapsedTime() - Part2StartMatchTime, 0.0f) : -1.0f; }

	float GetPart2Duration() const { return Part2Duration; }

	bool UsesCoreBoatStock() const { return bCoreBoatStock; }
	int32 GetCoreBoatStock() const { return CoreBoatStock; }
	int32 GetNextStageCoreBoatCost() const { return NextStageCoreBoatCost; }

	bool ShowsSeaObjectives() const { return bSeaObjectives; }
	int32 GetCompassTotal() const { return CompassTotal; }
	int32 GetCompassesFound() const { return CompassesFound; }

	/** Serveur uniquement (AYSFlowMode) */
	void SetSeaObjectives(bool bShow, int32 Total, int32 Found)
	{
		bSeaObjectives = bShow;
		CompassTotal = Total;
		CompassesFound = Found;
		ForceNetUpdate();
	}

	/** Serveur uniquement (AYSFlowMode) */
	void SetCoreBoatStock(bool bUsed, int32 Stock, int32 NextCost)
	{
		bCoreBoatStock = bUsed;
		CoreBoatStock = Stock;
		NextStageCoreBoatCost = NextCost;
		ForceNetUpdate();
	}

	/** Ce bateau est celui des joueurs et sa barre est verrouillée par l'objectif en cours */
	bool IsHelmLockedFor(const AYSPirateShip* Ship) const { return FlowStatus.bHelmLocked && Ship && Ship == GameBoat; }

	/** Serveur uniquement (AYSGameMode) */
	void SetSelectableCharacters(const TArray<TSubclassOf<AYSPlayerCharacter>>& Characters) { SelectableCharacters = Characters; }
	void SetCustomizationOptions(const TArray<FYSCustomizationColor>& Colors, const TArray<TObjectPtr<UTexture2D>>& Faces) { CustomizationColors = Colors; CustomizationFaces = Faces; }

	const TArray<FYSCustomizationColor>& GetCustomizationColors() const { return CustomizationColors; }
	const TArray<TObjectPtr<UTexture2D>>& GetCustomizationFaces() const { return CustomizationFaces; }

	/** Matériau de couleur / texture de visage d'un index (nullptr si invalide) */
	UMaterialInterface* GetCustomizationMaterial(int32 ColorIndex) const { return CustomizationColors.IsValidIndex(ColorIndex) ? CustomizationColors[ColorIndex].Material.Get() : nullptr; }
	UMaterialInterface* GetCustomizationEmissive(int32 ColorIndex) const { return CustomizationColors.IsValidIndex(ColorIndex) ? CustomizationColors[ColorIndex].EmissiveMaterial.Get() : nullptr; }
	UTexture2D* GetCustomizationFace(int32 FaceIndex) const { return CustomizationFaces.IsValidIndex(FaceIndex) ? CustomizationFaces[FaceIndex].Get() : nullptr; }
	void SetMatchStarted();

	int32 GetTeamLevel() const { return TeamLevel; }
	float GetTeamExperience() const { return TeamExperience; }
	float GetExperienceToNextLevel() const { return ExperienceToNextLevel; }
	bool IsUpgradeSelectionActive() const { return bUpgradeSelectionActive; }

	/** Serveur uniquement (AYSGameMode) */
	void SetTeamProgress(int32 Level, float Experience, float ExperienceNeeded) { TeamLevel = Level; TeamExperience = Experience; ExperienceToNextLevel = ExperienceNeeded; ForceNetUpdate(); }
	void SetUpgradeSelectionActive(bool bActive) { bUpgradeSelectionActive = bActive; ForceNetUpdate(); }
	void SetUpgradeSelectionLevel(int32 Level) { UpgradeSelectionLevel = Level; ForceNetUpdate(); }
	int32 GetUpgradeSelectionLevel() const { return UpgradeSelectionLevel; }
	void SetUpgradeSelectionKind(EYSUpgradeSelectionKind Kind) { UpgradeSelectionKind = Kind; ForceNetUpdate(); }
	EYSUpgradeSelectionKind GetUpgradeSelectionKind() const { return UpgradeSelectionKind; }

	int32 GetDayNumber() const { return DayNumber; }
	bool IsNight() const { return bNight; }

	/** Nuit à cet endroit (une île générée a sa propre phase) */
	bool IsNightAt(const FVector& Location) const;

	AYSFoeCharacter* GetCurrentElite() const { return CurrentElite; }

	/** Secondes écoulées depuis le début du jour en cours */
	float GetDayElapsedTime() const;

	/** Serveur uniquement (AYSGameMode / élite) */
	void StartDay(int32 Day);
	void StartNight() { bNight = true; }
	void SetCurrentElite(AYSFoeCharacter* Elite) { CurrentElite = Elite; }

	int32 GetDoubloons() const { return Doubloons; }
	int32 GetDoubloonGoal() const { return DoubloonGoal; }

	/** Serveur uniquement (AYSGameMode) */
	void SetDoubloons(int32 Amount, int32 Goal) { Doubloons = Amount; DoubloonGoal = Goal; ForceNetUpdate(); }
	void SetShipLevel(int32 Level) { ShipLevel = Level; ForceNetUpdate(); }

	int32 GetPhoenixFeathers() const { return PhoenixFeathers; }

	/** Serveur uniquement (AYSGameMode) */
	void SetPhoenixFeathers(int32 Amount) { PhoenixFeathers = FMath::Max(0, Amount); ForceNetUpdate(); }

	int32 GetShipLevel() const { return ShipLevel; }

	bool IsShipPresent() const { return bShipPresent; }
	int32 GetShipAboardPlayers() const { return ShipAboardPlayers; }
	int32 GetShipTotalPlayers() const { return ShipTotalPlayers; }
	bool IsVictory() const { return bVictory; }
	float GetVictoryReturnDelay() const { return VictoryReturnDelay; }
	bool IsOvertime() const { return bOvertime; }
	bool IsIslandSinking() const { return IslandSinkStartServerTime > 0.0; }

	/** Une île coule à cet endroit (map fixe : toute l'île ; îles générées : celle qui couvre ce point, pendant sa descente) */
	bool IsSinkingAt(const FVector& Location) const;

	/** Serveur uniquement (AYSGameMode) */
	void SetShipPresent(bool bPresent) { bShipPresent = bPresent; ForceNetUpdate(); }
	void SetShipBoarding(int32 Aboard, int32 Total) { ShipAboardPlayers = Aboard; ShipTotalPlayers = Total; ForceNetUpdate(); }
	void SetVictory(float ReturnDelay) { bVictory = true; VictoryReturnDelay = ReturnDelay; FinishMatch(); ForceNetUpdate(); }

	/** Game over : « GAME OVER » puis écran des scores après ReturnDelay */
	void SetGameOver(float ReturnDelay) { bGameOver = true; VictoryReturnDelay = ReturnDelay; FinishMatch(); ForceNetUpdate(); }
	bool IsGameOver() const { return bGameOver; }

	/** Victoire ou game over */
	bool IsMatchEnded() const { return bVictory || bGameOver; }

	/** Serveur uniquement (AYSGameMode : pause du choix d'upgrade) */
	void SetClockPaused(bool bPaused);

	/** Chrono global : temps de partie + pauses du choix d'upgrade, arrêté à la fin de la partie */
	float GetMatchClockTime() const;
	void SetOvertime() { bOvertime = true; ForceNetUpdate(); }
	void StartIslandSink(float Duration, float Depth, float EaseExponent);

	/** Port au hasard (serveur) : le ponton va en Pivot, tout le port tourne de YawDelta autour de lui (appliqué sur toutes les machines) */
	void SetHarborPlacement(const FVector& Pivot, float YawDelta);

	/** Tags des acteurs du port placés dans la map (ponton = pivot, Brooker sur la terre ferme, bateau en mer) */
	static const FName HarborTag;
	static const FName HarborPivotTag;
	static const FName HarborLandTag;
	static const FName HarborSeaTag;

	/** Acteur pivot du port (tag HarborPivot), nullptr s'il n'y en a pas */
	static AActor* FindHarborPivot(const UWorld* World);

	/** Transform d'un acteur du port une fois le ponton déplacé de OldPivot à NewPivot et le port tourné de YawDelta autour de lui */
	static FTransform GetHarborTransform(const FTransform& Original, const FVector& OldPivot, const FVector& NewPivot, float YawDelta);

	/** Secondes écoulées depuis le démarrage de la partie (0 avant ; valable sur serveur et clients) */
	UFUNCTION(BlueprintPure, Category="Match")
	float GetMatchElapsedTime() const;

	/**
	 *  À appeler sur le serveur quand des dégâts sont infligés : affiche le chiffre chez tous les joueurs.
	 *  InstigatorController / Victim servent à déterminer la couleur selon chaque point de vue.
	 */
	static void ReportDamage(const AActor* Victim, float Amount, const FVector& ImpactLocation, const AController* InstigatorController, bool bCritical = false, const AActor* DamageCauser = nullptr);

protected:

	/** Les joueurs sont identifiés par leur PlayerState (toujours valide, contrairement au pawn qui peut mourir) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDamageNumber(float Amount, FVector_NetQuantize Location, APlayerState* InstigatorPlayer, APlayerState* VictimPlayer, bool bCritical);
};
