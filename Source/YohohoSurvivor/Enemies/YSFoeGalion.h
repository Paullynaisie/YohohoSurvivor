#pragma once

#include "CoreMinimal.h"
#include "Ship/YSPirateShip.h"
#include "YSFoeGalion.generated.h"

class AYSFoeCharacter;
class AYSShipStation;
class UMaterialInterface;

/** Groupe de matelots d'un NME Galion : Count NME tirés dans Classes (selon leur Spawn Weight) */
USTRUCT(BlueprintType)
struct FYSGalionCrewGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crew")
	TArray<TSubclassOf<AYSFoeCharacter>> Classes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crew", meta=(ClampMin="0"))
	int32 Count = 1;
};

/**
 *  NME Galion : le même bateau que les joueurs (copie rouge, Blueprint BP_NME_Galion) avec un équipage complet, même pont, mêmes postes,
 *  mêmes PV (modules + core).
 *  Capitaine (élite, Galion|Crew) à la barre : il s'approche à Engage Distance du bateau des joueurs le plus proche puis tourne autour
 *  (Galion|Routine) ; barre quittée = le bateau n'avance plus. Matelots : marchent jusqu'aux canons libres, s'y installent et tirent sur les
 *  modules du bateau des joueurs (et les joueurs à portée qui ne sont pas sur un bateau) ; les autres restent où ils sont. Ils ne quittent
 *  jamais le pont et ignorent les joueurs qui n'y sont pas.
 *  Joueur à bord (accès, chaloupe) : les membres d'équipage qui ne sont pas à un poste l'attaquent ; s'il n'y en a plus, tous ceux des postes
 *  (capitaine compris) quittent leur poste pour l'attaquer, jusqu'à ce qu'il n'y ait plus de joueur à bord.
 *  Détruit : tout le monde à l'eau, l'XP de l'équipage reste due.
 */
UCLASS(meta=(PrioritizeCategories="Galion Ship"))
class AYSFoeGalion : public AYSPirateShip
{
	GENERATED_BODY()

protected:

	/** PV max du Galion = Base Ship Health + Ship Health Per Day x jours écoulés (jour 1 : 0), recalculés à son réveil (bataille navale) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Scaling", meta=(ClampMin="0"))
	float BaseShipHealth = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Scaling", meta=(ClampMin="0"))
	float ShipHealthPerDay = 5000.0f;

	/** Dégâts de ses postes de combat x (1 + ceci x jours écoulés) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Scaling", meta=(ClampMin="0"))
	float PostDamagePerDay = 0.1f;

	/** Multiplicateur de base des dégâts de ses postes de combat (0.5 = 2x moins que les mêmes postes chez les joueurs) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Scaling", meta=(ClampMin="0"))
	float PostDamageMultiplier = 0.5f;

public:

	/** Jours écoulés depuis le jour 1 */
	int32 GetElapsedDays() const;

	/** Multiplicateur des dégâts de ses postes de combat (selon les jours écoulés, x Post Damage Multiplier) */
	float GetPostDamageMultiplier() const { return PostDamageMultiplier * (1.0f + PostDamagePerDay * GetElapsedDays()); }

	/** PV max recalculés selon les jours écoulés (serveur) */
	void ApplyDayScaling(const TCHAR* Reason);

protected:

	/** Capitaine (élite) : à la barre, seul à piloter */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew")
	TSubclassOf<AYSFoeCharacter> CaptainClass;

	/** Non vide : le capitaine est tiré au hasard dans cette liste (ex. élite Skull ou élite Gun) ; Captain Class est alors ignoré */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew")
	TArray<TSubclassOf<AYSFoeCharacter>> CaptainClasses;

	/**
	 *  Capitaines en plus (Galion Nemesis : un Skull et un Gun) : sur le pont, boss comme le capitaine (barre « Capitaine »), ils servent aux canons ;
	 *  le capitaine de la barre mort, l'un d'eux prend la barre. Le Galion n'est vaincu que quand tous les capitaines sont morts
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew")
	TArray<TSubclassOf<AYSFoeCharacter>> ExtraCaptainClasses;

	/** Capitaines (barre et en plus) : multiplicateurs des dégâts subis posés à l'apparition, à la place de ceux de leur Blueprint
	 *  (un capitaine en plus peut être un Skull de base) ; dégâts d'un personnage / d'un poste de combat du bateau des joueurs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew", meta=(ClampMin="0"))
	float CaptainPlayerDmgMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew", meta=(ClampMin="0"))
	float CaptainBoatDmgMultiplier = 0.5f;

	/** Le capitaine tué : le Galion coule (tout le monde à l'eau, comme un core détruit) ; pendant un abordage les planches sont retirées */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew")
	bool bSinkWhenCaptainKilled = true;

	/** Matelots : groupes (défaut : 6 Skull, 3 Skull Gun, 6 au hasard entre Skull, Skull Bomb et Skull Gun) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew")
	TArray<FYSGalionCrewGroup> CrewGroups;

	/** Apparition des matelots : rectangle du pont principal (X / Y locaux) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew")
	FVector2D DeckSpawnMin = FVector2D(-1100.0f, -700.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew")
	FVector2D DeckSpawnMax = FVector2D(2200.0f, 700.0f);

	/** Hauteur du pont principal (locale) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew", meta=(Units="Centimeters"))
	float DeckHeight = 210.0f;

	/** Écart minimal entre deux matelots à l'apparition */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew", meta=(ClampMin="0", Units="Centimeters"))
	float CrewSpawnSpacing = 200.0f;

	/** Le matelot s'installe au poste une fois à cette distance de sa place (à plat) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew", meta=(ClampMin="0", Units="Centimeters"))
	float PostReachDistance = 120.0f;

	/** Répartition de l'équipage (postes, joueurs à bord) : toutes les ... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew", meta=(ClampMin="0", Units="s"))
	float CrewUpdateInterval = 0.25f;

	/** Marche sur le pont : plus lent que cette vitesse pendant Crew Stuck Delay = bloqué, il contourne sur le côté pendant Crew Sidestep Duration */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float CrewStuckSpeed = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew", meta=(ClampMin="0", Units="s"))
	float CrewStuckDelay = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew", meta=(ClampMin="0", Units="s"))
	float CrewSidestepDuration = 0.8f;

	/** Un matelot ne fait pas un pas si ce point devant lui n'est plus sur le pont (ouverture du bastingage) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew", meta=(ClampMin="0", Units="Centimeters"))
	float DeckEdgeLookAhead = 100.0f;

	/** Bord du pont : rectangle de la coque réduit de ça */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew", meta=(ClampMin="0", Units="Centimeters"))
	float DeckEdgeInset = 40.0f;

	/** Gaillard arrière : X local sous lequel on est dessus */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew|Stairs")
	float CastleFrontX = -1400.0f;

	/** Haut de l'escalier du gaillard (X / Y locaux), sur le gaillard, dans l'ouverture de son bastingage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew|Stairs")
	FVector2D StairsTop = FVector2D(-1480.0f, 460.0f);

	/** Bas de l'escalier (X / Y locaux), sur le pont */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew|Stairs")
	FVector2D StairsBottom = FVector2D(-720.0f, 460.0f);

	/** Largeur de l'escalier (Y local, autour de Stairs Bottom) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew|Stairs", meta=(ClampMin="0", Units="Centimeters"))
	float StairsHalfWidth = 280.0f;

	/** Distance à garder entre son flanc et la coque de sa cible (bateau des joueurs le plus proche) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Routine", meta=(ClampMin="0", Units="Centimeters"))
	float EngageDistance = 2000.0f;

	/** En dessous de Engage Distance + cette marge il commence à tourner autour de sa cible, au-dessus il fonce droit sur elle (rayon de virage du bateau) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Routine", meta=(ClampMin="1", Units="Centimeters"))
	float CircleBand = 2500.0f;

	/** Écart d'angle avec la direction voulue à partir duquel la barre est à fond */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Routine", meta=(ClampMin="1", Units="Degrees"))
	float SteerFullAngle = 30.0f;

	/** Fond trop haut à cette distance devant lui dans la direction voulue (plage) -> il prend la direction libre la plus proche et longe la côte au lieu de s'échouer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Routine", meta=(ClampMin="0", Units="Centimeters"))
	float ShoreLookAhead = 3000.0f;

	/** Pendant le tour, côte qui barre le passage : il repart dans l'autre sens, pas plus d'une fois par ce délai */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Routine", meta=(ClampMin="0", Units="s"))
	float FlipCooldown = 8.0f;

	/** Plus près de sa cible que le rayon du tour x ce ratio : il s'éloigne droit devant lui */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Routine", meta=(ClampMin="0", ClampMax="1"))
	float TooCloseRatio = 0.6f;

	/** Échoué : marche arrière pendant cette durée */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Routine", meta=(ClampMin="0", Units="s"))
	float ReverseDuration = 4.0f;

	/**
	 *  Bataille navale : Ram Delay après son activation, s'il peut encore bouger (capitaine à la barre), il fonce sur le bateau des joueurs ;
	 *  s'il le percute en fonçant : les deux bateaux s'immobilisent, des planches d'abordage relient les ponts, l'équipage (capitaine compris)
	 *  quitte ses postes pour attaquer les joueurs et un second équipage apparaît sur le pont. Capitaine tué ou Galion coulé : planches retirées,
	 *  le bateau des joueurs repart. Ram Delay est donné par la bataille (Data Table) ; 0 = jamais.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Ram", meta=(ClampMin="0", Units="s"))
	float RamDelay = 60.0f;

	/** En fonçant : le bateau visé est considéré comme percuté si sa coque est à moins de ça de la sienne */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Ram", meta=(ClampMin="0", Units="Centimeters"))
	float RamContactDistance = 150.0f;

	/** Planches d'abordage : largeur (3 PJ), épaisseur, distance dans chaque pont depuis le bord, hauteur au-dessus des ponts (bastingage) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Ram", meta=(ClampMin="50", Units="Centimeters"))
	float BoardingPlankWidth = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Ram", meta=(ClampMin="5", Units="Centimeters"))
	float BoardingPlankThickness = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Ram", meta=(ClampMin="0", Units="Centimeters"))
	float BoardingPlankInset = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Ram", meta=(Units="Centimeters"))
	float BoardingPlankHeight = 32.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Ram")
	TObjectPtr<UMaterialInterface> BoardingPlankMaterial;

	/** Planches en plus de chaque côté de celle du milieu (plus de place pour se battre ; demande de Paul, 2026-11-11 : 1 à gauche, 1 à droite) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Ram", meta=(ClampMin="0"))
	int32 BoardingSidePlanks = 1;

	/** Distance entre les axes de deux planches voisines (Boarding Plank Width 260 : 600 = 3.4 m d'eau entre elles ; choix de Claude) ;
	 *  une planche dont un bout tombe hors d'une des deux coques n'est pas posée */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Ram", meta=(ClampMin="50", Units="Centimeters"))
	float BoardingSidePlankSpacing = 600.0f;

	/** Second équipage qui apparaît à l'abordage (hors capitaine) ; vide = les mêmes groupes que l'équipage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Ram")
	TArray<FYSGalionCrewGroup> BoardingCrewGroups;

	/** Pilote automatique : la routine, tant que le capitaine est à la barre */
	virtual bool ComputeAutoPilot(float& OutThrottle, float& OutSteer) override;

	/** Échoué : il recule et repart dans l'autre sens */
	virtual void OnHullBlocked() override;

	/** Autre bateau : comme échoué ; en fonçant sur le bateau des joueurs : abordage */
	virtual void OnShipCollision(AYSPirateShip* Other) override;

	/** Éperonné par le bateau des joueurs (« À l'abordage ») sur un point abordable : phase d'abordage (planche, équipage lâché) */
	virtual bool TryStartBoardingBy(AYSPirateShip* Rammer, const FVector& ImpactLocation) override;

	/** Core à 0 PV : plus personne aux postes, plus de poste à occuper */
	virtual void OnShipDisabled() override;

	/** Détruit : équipage libéré, il tombe à l'eau */
	virtual void OnShipDestroyedByDamage() override;

public:

	AYSFoeGalion(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaSeconds) override;

	virtual bool IsFoeShip() const override { return true; }

	/** Poste Seat : 0 = barre, 1.. = canons (même ordre sur toutes les machines) */
	virtual FVector GetSeatFeetLocation(int32 Seat) const override;

	virtual float GetSeatYaw(int32 Seat) const override;

	/** Le matelot s'installe au poste (serveur) */
	void SeatCrewMember(AYSFoeCharacter* Foe, AYSShipStation* Post);

	/** Un module vient de tomber à 0 PV : celui qui y était s'en va (serveur) */
	void OnModuleBroken(AYSShipStation* Station);

	/** Point vers lequel marcher sur le pont pour aller de From à To : To, ou l'escalier du gaillard s'ils ne sont pas au même niveau */
	FVector GetDeckRoute(const FVector& From, const FVector& To) const;

	/** Le point (monde, à plat) est sur le pont, loin du bord */
	bool IsOnDeck(const FVector& WorldLocation) const;

	float GetCrewStuckSpeed() const { return CrewStuckSpeed; }
	float GetCrewStuckDelay() const { return CrewStuckDelay; }
	float GetCrewSidestepDuration() const { return CrewSidestepDuration; }
	float GetDeckEdgeLookAhead() const { return DeckEdgeLookAhead; }
	float GetPostReachDistance() const { return PostReachDistance; }

	/** Un Galion sur l'eau, assez loin de tous les joueurs ; nullptr si rien */
	static AYSFoeGalion* SpawnAtSea(UWorld* World, TSubclassOf<AYSFoeGalion> ShipClass, float MinDistance, float MaxDistance);

	/** Bataille navale activée (serveur) : il se réveille ; InRamDelay < 0 = garde Ram Delay */
	void ActivateBattle(float InRamDelay);

	/** Vaincu : coulé (ou en train d'être détruit) ou tous les capitaines morts */
	bool IsDefeated() const;

	/** Il fonce tout de suite sur le bateau des joueurs pour l'aborder (serveur ; réveillé s'il dormait) */
	void TriggerRam(const TCHAR* Reason);

	/**
	 *  Objets posés sur le pont à côté du capitaine (serveur, objectifs d'une bataille navale) : si l'un d'eux est pris pendant que le capitaine
	 *  est à la barre, le Galion fonce sur le bateau des joueurs pour l'aborder
	 */
	TArray<class AYSInteractiveObject*> SpawnGuardedItems(const TArray<TSubclassOf<class AYSInteractiveObject>>& Classes);

	/** Debug : premier objet gardé encore sur le pont, ou nullptr */
	AYSInteractiveObject* GetGuardedItem() const;

	/**
	 *  Abordage mis en scène (serveur, combat contre Davy Jones) : planches entre son pont et celui de Other, lui-même immobilisé, sans équipage
	 *  lâché ni second équipage ; il ne s'arrête qu'avec End Scripted Boarding (ou s'il coule). Other n'est pas immobilisé par lui (l'arène s'en charge)
	 */
	void StartScriptedBoarding(AYSPirateShip* Other);
	void EndScriptedBoarding(const TCHAR* Reason);
	bool IsScriptedBoarding() const { return bScriptedBoarding; }

	/** NME libres sur son pont (serveur) : ils quittent tout de suite l'équipage et attaquent les joueurs ; renvoie ceux qui sont apparus */
	TArray<AYSFoeCharacter*> SpawnFreeCrew(const TArray<FYSGalionCrewGroup>& Groups);

	/** Plateforme de Davy Jones (composant Platform Component Name, à la place du grand mât) : point au milieu de son dessus ; faux sans elle */
	bool GetPlatformTop(FVector& OutLocation) const;

	/** Composant du Blueprint sur lequel se tient Davy Jones (nom de variable ; Galion de Davy Jones) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Davy Jones")
	FName PlatformComponentName = TEXT("DavyPlatform");

	/** Distance entre le capitaine (barre) et chaque objet gardé */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Guarded Items", meta=(ClampMin="50", Units="Centimeters"))
	float GuardedItemDistance = 220.0f;

	/** Écart d'angle entre deux objets gardés (autour du capitaine, côté proue) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Guarded Items", meta=(ClampMin="5", Units="Degrees"))
	float GuardedItemAngleStep = 40.0f;

private:

	/** Capitaines en plus encore suivis (aussi dans Crew tant qu'ils ne sont pas à la barre) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AYSFoeCharacter>> ExtraCaptains;

	/** Objectifs posés à côté du capitaine */
	TArray<TWeakObjectPtr<class AYSInteractiveObject>> GuardedItems;

	/** Un capitaine en plus prend la place du capitaine mort (serveur) */
	void PromoteCaptain();

	/** Un objet gardé a été pris (porté, ou plus sur le pont) */
	bool IsGuardedItemTaken();

	static bool IsFoeAlive(const AYSFoeCharacter* Foe);

	enum class ERamState : uint8 { None, Ramming, Boarded, Done };
	ERamState RamState = ERamState::None;

	/** Un capitaine a été placé (sans capitaine dès l'apparition, le Galion ne coule pas tout seul) */
	bool bHadCaptain = false;

	/** Heure (monde) de l'activation de la bataille ; < 0 = pas de bataille */
	double BattleActivationTime = -1.0;

	UPROPERTY(Transient)
	TObjectPtr<AYSPirateShip> BoardedShip;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> BoardingPlanks;

	/** Abordage de Other (serveur) : bateaux immobilisés, planches, équipage lâché, second équipage */
	void StartBoarding(AYSPirateShip* Other);

	/** Fin de l'abordage (serveur) : planches retirées, bateau des joueurs libéré */
	void EndBoarding(const TCHAR* Reason);

	/** Planches entre son pont et celui de Other, au point de contact */
	void SpawnBoardingPlanks(AYSPirateShip* Other);

	/** Hauteur du pont de Ship sous Point (trace), sinon Fallback */
	float FindDeckHeight(const AYSPirateShip* Ship, const FVector& Point, float Fallback) const;

	/** Groupes de matelots sur le pont (serveur) ; bFree = NME libres (pas d'équipage : ils vont n'importe où) ; OutSpawned : ceux qui sont apparus */
	void SpawnCrewGroups(const TArray<FYSGalionCrewGroup>& Groups, bool bFree, TArray<AYSFoeCharacter*>* OutSpawned = nullptr);

	/** Abordage mis en scène (Davy Jones) */
	bool bScriptedBoarding = false;

	/** Capitaine, puis matelots (serveur) */
	UPROPERTY(Transient)
	TObjectPtr<AYSFoeCharacter> Captain;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AYSFoeCharacter>> Crew;

	/** Joueur à bord et plus personne hors des postes : tout l'équipage quitte les postes jusqu'à ce qu'il n'y ait plus de joueur à bord */
	bool bAllHands = false;

	float CrewUpdateTimer = 0.0f;
	double ReverseUntilTime = -1.0;
	double LastFlipTime = -1.0e6;

	/** Écart (degrés) appliqué à la direction voulue pour éviter la côte ; gardé d'une recherche à l'autre */
	float ShoreAvoidYaw = 0.0f;
	double NextShoreCheckTime = 0.0;
	double LastRoutineLogTime = -1.0e6;

	/** Sens du tour : 1 ou -1 ; 0 = choisi en arrivant sur le cercle */
	float CircleSide = 0.0f;

	/** Poste Seat : acteur si déjà là */
	AYSShipStation* GetSeatStation(int32 Seat) const;

	/** Index de poste, ou INDEX_NONE */
	int32 GetSeatOf(const AYSShipStation* Station) const;

	void SpawnCrew();

	/** Un NME à bord (serveur) ; Seat >= 0 : déjà à ce poste */
	AYSFoeCharacter* SpawnCrewMember(TSubclassOf<AYSFoeCharacter> FoeClass, const FVector& FeetLocation, int32 Seat);

	/** Poste libéré : le NME reprend pied à côté */
	void UnseatCrewMember(AYSFoeCharacter* Foe);

	/** Répartition : joueurs à bord, postes à occuper */
	void UpdateCrew();

	/** Canon libre utilisable le plus proche, pas déjà visé par un autre matelot */
	AYSShipStation* FindFreeCannon(const AYSFoeCharacter* Foe) const;

	/** Direction voulue, tournée au plus près pour que le fond reste assez profond devant lui */
	FVector AvoidShore(const FVector& Desired, const FVector& Inward, bool bOrbiting);

	/** Cible du tour : bateau des joueurs le plus proche, ou nullptr */
	AYSPirateShip* FindTargetShip() const;

	/** Joueur vivant le plus proche (aucun bateau des joueurs sur l'eau) */
	const APawn* FindNearestPlayerPawn() const;

	/** Équipage encore vivant libéré du bateau (fin du bateau) */
	void ReleaseCrew();
};
