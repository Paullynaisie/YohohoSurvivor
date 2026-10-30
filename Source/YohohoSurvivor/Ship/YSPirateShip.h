#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YSPirateShip.generated.h"

class UChildActorComponent;
class UWidgetComponent;
class AYSShipHelm;
class AYSShipCore;
class AYSShipStation;
class AYSShipPath;
class AYSShipBoarding;

/** État du déplacement d'un bateau pilotable, répliqué par le serveur (les clients l'extrapolent) */
USTRUCT()
struct FYSShipMotion
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	float Yaw = 0.0f;

	/** Vitesse le long de la proue (cm/s, négative en marche arrière) */
	UPROPERTY()
	float Speed = 0.0f;

	/** Vitesse de rotation (degrés/s) */
	UPROPERTY()
	float YawRate = 0.0f;

	/** Commandes du pilote (-1..1) : Z / S, D / Q */
	UPROPERTY()
	float Throttle = 0.0f;

	UPROPERTY()
	float Steer = 0.0f;

	/** Heure serveur de cet état */
	UPROPERTY()
	double ServerTime = 0.0;
};

/**
 *  Bateau pirate : énorme bateau qui transporte les joueurs ; ils marchent sur le pont pendant qu'il avance
 *  (le pont est une base de mouvement : le CharacterMovementComponent emporte les persos et les NME posés dessus ; les objets
 *  reposés sur le pont y sont accrochés).
 *  Deux rôles indépendants : bateau OBJECTIF de fin de partie (caché jusqu'à l'objectif de doublons, monter à bord = victoire)
 *  et bateau PILOTABLE (barre : E pour la prendre, ZQSD = avancer / reculer avec inertie, tourner).
 *  Pilotage : simulé par le serveur (FYSShipMotion répliqué), extrapolé par les clients ; le bateau s'arrête en eau trop peu
 *  profonde (Hull Probe Points). Origine de l'acteur = ligne de flottaison (à poser au niveau de la mer), +X = proue.
 *  Postes (acteurs enfants, réglages dans leur Blueprint) : barre, core (stats du bateau, zone Brooker + Medic), nid d'pie,
 *  chalouperie (chaloupe), 3 modules de combat (canonneries avant / bâbord / tribord).
 *  Visuel (primitives) dans le Blueprint enfant /Game/Placeable/Ship/BP_PirateShip (Tools/EditorScripts/setup_pirate_ship.py,
 *  postes placés par Tools/EditorScripts/setup_ship_stations.py).
 */
UCLASS(meta=(PrioritizeCategories="Ship"))
class AYSPirateShip : public AActor
{
	GENERATED_BODY()

private:

	/** Racine mobile : toutes les formes du bateau y sont attachées */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Root;

	/** Barre (acteur AYSShipHelm répliqué, créé par le serveur) : placer ce composant au centre de la roue */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> HelmStation;

	/** Core (AYSShipCore, local sur chaque machine) : stats du bateau, zone Brooker + Medic ; au milieu du pont */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> CoreStation;

	/** Nid d'pie (AYSCrowsNest) : placer ce composant au pied du grand mât */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> CrowsNestStation;

	/** Chalouperie (AYSLongboatLauncher) : placer ce composant devant la porte de la cabine */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> LongboatStation;

	/** Modules de combat (AYSShipCannon) : avant (vers la proue), bâbord (gauche), tribord (droite) ; +X du composant = direction de tir */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> CannonFront;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> CannonLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> CannonRight;

	/** Seconds canons de bâbord et de tribord */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> CannonLeft2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> CannonRight2;

	/** Barre de vie du bateau (PV du core), au-dessus du pont, affichée dès qu'il est abîmé (hauteur = position du composant) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UWidgetComponent> ShipHealthBar;

	/** Accès (AYSShipBoarding) : E pour monter sur le pont, même en nageant ; à l'ouverture du bastingage, +X vers le large */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> BoardingAccess;

protected:

	/** Nom affiché */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship")
	FText ShipName;

	/**
	 *  Bateau objectif de fin de partie : caché tant que l'objectif de doublons n'est pas atteint (Ship Arrives At Doubloon Goal du
	 *  GameMode), monter à bord compte pour la victoire (Ship Boarding Victory). Faux = toujours présent, sans rôle d'objectif.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship")
	bool bObjectiveShip = true;

	/** La barre peut être prise (E) : le joueur pilote le bateau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm")
	bool bDrivable = false;

	/** Vitesse max en marche avant (cm/s ; les joueurs marchent à 600) ; x Speed Multiplier du core */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Speed", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float MaxForwardSpeed = 900.0f;

	/** Accélération en tenant Z (cm/s²) : temps pour atteindre la vitesse max = Max Forward Speed / Forward Acceleration */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Speed", meta=(ClampMin="0"))
	float ForwardAcceleration = 180.0f;

	/** Vitesse max en marche arrière (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Speed", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float MaxReverseSpeed = 300.0f;

	/** Accélération en tenant S (cm/s²) ; freine aussi en marche avant */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Speed", meta=(ClampMin="0"))
	float ReverseAcceleration = 150.0f;

	/** Ralentissement sans commande (cm/s²) : l'inertie (plus petit = le bateau glisse plus longtemps) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Speed", meta=(ClampMin="0"))
	float Deceleration = 90.0f;

	/** Vitesse de rotation max (degrés/s) en tenant Q / D, atteinte à partir de Full Turn Speed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Steering", meta=(ClampMin="0", Units="DegreesPerSecond"))
	float MaxTurnRate = 25.0f;

	/** Vitesse (cm/s) à partir de laquelle le bateau tourne à Max Turn Rate ; en dessous, la rotation diminue avec la vitesse */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Steering", meta=(ClampMin="1", Units="CentimetersPerSecond"))
	float FullTurnSpeed = 400.0f;

	/** Part de Max Turn Rate disponible à l'arrêt (0 = un bateau immobile ne tourne pas) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Steering", meta=(ClampMin="0", ClampMax="1"))
	float StandstillTurnRatio = 0.25f;

	/** Inertie de la rotation : vitesse à laquelle la rotation suit la barre (plus petit = plus lent à virer et à se redresser) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Steering", meta=(ClampMin="0"))
	float TurnResponsiveness = 2.0f;

	/** Points de la coque (X / Y locaux, cm) où la profondeur de l'eau est vérifiée : le bateau s'arrête s'il y a moins de Min Water Depth */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Hull")
	TArray<FVector2D> HullProbePoints;

	/** Tirant d'eau : profondeur minimale sous chaque point de la coque (fond = Landscape et décor statique) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Hull", meta=(ClampMin="0", Units="Centimeters"))
	float MinWaterDepth = 200.0f;

	/** Clients : vitesse de rattrapage d'un écart avec l'état reçu du serveur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Network", meta=(ClampMin="0"))
	float ClientCorrectionSpeed = 6.0f;

	/**
	 *  Mode playtest : à cocher sur le bateau posé dans la scène. Le bateau suit tout seul Playtest Path (mêmes commandes qu'un pilote :
	 *  accélération, virages, inertie ; pas d'arrêt contre le fond). Un joueur qui prend la barre reprend la main le temps d'y rester.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Ship|Helm|Playtest")
	bool bPlaytestMode = false;

	/** Trajet à suivre (AYSShipPath posé dans la map) ; vide = le premier trouvé dans la map */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Playtest", meta=(EditCondition="bPlaytestMode"))
	TObjectPtr<AYSShipPath> PlaytestPath;

	/** Le bateau vise le point de la spline à cette distance devant lui (plus grand = virages plus doux, coupe plus les courbes) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Playtest", meta=(ClampMin="1", Units="Centimeters", EditCondition="bPlaytestMode"))
	float PlaytestLookAhead = 2000.0f;

	/** Vitesse max x ce multiplicateur pendant le pilote automatique */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Playtest", meta=(ClampMin="0", EditCondition="bPlaytestMode"))
	float PlaytestSpeedMultiplier = 1.0f;

	/** Écart d'angle avec le point visé à partir duquel la barre est à fond */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Playtest", meta=(ClampMin="1", Units="Degrees", EditCondition="bPlaytestMode"))
	float PlaytestSteerFullAngle = 20.0f;

	/** PV du bateau (max = Ship Max Health du core) : le core prend les coups reçus par un module à 0 PV ; peut descendre sous 0 une fois le bateau hors d'usage */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Ship")
	float Health = 0.0f;

	/** Barre de vie du bateau x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship", meta=(ClampMin="0.1"))
	float ShipHealthBarScale = 3.0f;

	/** Core à 0 PV : tous les modules désactivés, le bateau ne peut plus bouger (serveur -> clients) */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Ship")
	bool bDisabled = false;

	/** Serveur : le core vient de passer à 0 PV ; postes libérés */
	virtual void OnShipDisabled();

	/** Serveur : PV sous -PV max, le bateau va être détruit ; ce qui est à bord tombe à l'eau */
	virtual void OnShipDestroyedByDamage() {}

	/** Canons dans un ordre fixe (le même sur toutes les machines) ; absents sur une chaloupe */
	TArray<UChildActorComponent*, TInlineAllocator<8>> GetCannonComponents() const;

	/** Déplacement du bateau pilotable (serveur -> clients) */
	UPROPERTY(ReplicatedUsing=OnRep_Motion)
	FYSShipMotion Motion;

	UFUNCTION()
	void OnRep_Motion();

	/** Serveur : la coque touche le fond (eau trop peu profonde), une fois par arrêt ; ex. chaloupe : éjection sur l'île */
	virtual void OnHullBlocked() {}

	/** Rectangle local (X / Y) des Hull Probe Points */
	FBox2D GetHullBounds() const;

	/** Serveur, à chaque image : pilote automatique ; vrai = Throttle / Steer remplacent les commandes du pilote */
	virtual bool ComputeAutoPilot(float& OutThrottle, float& OutSteer);

	/** Commande de barre (-1..1) pour tourner vers cette direction, à fond à partir de FullSteerAngle ; arrondie : les commandes ne changent (et ne sont envoyées) que par crans */
	float ComputeSteerTowards(const FVector& Direction, float FullSteerAngle) const;

	/** Plus petite profondeur d'eau sous les points de la coque à cette position */
	float GetHullMinDepth(const FVector& Location, float Yaw) const;

	UChildActorComponent* GetHelmStationComponent() const { return HelmStation; }

public:

	AYSPirateShip(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	FText GetShipName() const { return ShipName; }

	bool IsObjectiveShip() const { return bObjectiveShip; }

	bool IsDrivable() const { return bDrivable; }

	/** Bateau du camp des NME (NME Jolly Ship, NME Galion) : ses modules sont Team.Foe, les joueurs n'utilisent pas ses postes */
	virtual bool IsFoeShip() const { return false; }

	/** On peut monter dessus (pont solide) ; faux : NME Jolly Ship, sans collision, ni base de mouvement ni contact pour les chaloupes */
	virtual bool IsBoardable() const { return true; }

	/** Pieds du NME à ce poste (NME à bord, position calculée sur chaque machine) */
	virtual FVector GetSeatFeetLocation(int32 Seat) const { return GetActorLocation(); }

	/** Regard du NME à ce poste */
	virtual float GetSeatYaw(int32 Seat) const { return GetActorRotation().Yaw; }

	/** Mode playtest actif et personne à la barre : le bateau suit sa spline tout seul */
	bool IsOnPlaytestAutoPilot() const;

	/** Debug (serveur) : mode playtest changé en jeu */
	void SetPlaytestMode(bool bEnabled);

	/** Bateau présent (le bateau objectif est caché avant l'objectif de doublons) */
	bool IsPresent() const { return AppliedPresence != 0; }

	AYSShipHelm* GetHelm() const;

	/** Core : stats du bateau (nullptr = valeurs du bateau seules) */
	AYSShipCore* GetCore() const;

	/** Accès du bateau (nullptr sur une chaloupe) */
	AYSShipBoarding* GetBoardingAccess() const;

	/**
	 *  Point vers lequel marcher (à plat) pour aller de From à To sans buter contre la coque : To si From est déjà du même côté du bateau,
	 *  sinon le long du flanc jusqu'à Margin au-delà de la proue ou de la poupe, puis de l'autre côté
	 */
	FVector GetRouteAroundHull(const FVector& From, const FVector& To, float Margin) const;

	/** Postes du bateau (barre, nid d'pie, chalouperie, canonneries ; pas le core), acteurs enfants existants */
	TArray<AYSShipStation*> GetStations() const;

	/** Commandes du pilote (serveur) : Throttle Z / S, Steer D / Q, de -1 à 1 */
	void SetPilotInput(float Throttle, float Steer);

	/** Vitesse actuelle le long de la proue (cm/s) */
	float GetSpeed() const { return Motion.Speed; }

	/** Bateau déplacé d'un coup par le jeu (placement du port) : le pilotage repart de sa position, à l'arrêt (serveur) */
	void SyncMotionToActor();

	float GetHealth() const { return Health; }
	float GetMaxHealth() const;

	/** Core à 0 PV (ou moins) : modules désactivés, bateau immobile */
	bool IsDisabled() const { return bDisabled; }

	/**
	 *  Dégâts au core (serveur) : un module à 0 PV en a pris. À 0 PV le bateau est hors d'usage ; les PV peuvent alors passer en
	 *  négatif ; sous -PV max, il est détruit et tout ce qui est à bord tombe à l'eau.
	 */
	void TakeCoreDamage(float Damage, AController* EventInstigator, const FVector& ImpactLocation);

	/**
	 *  Point sur l'eau pour un bateau (serveur) : direction au hasard autour d'un joueur tiré au hasard, premier point à partir de MinDistance
	 *  (loin de tous les joueurs, jusqu'à MaxDistance) avec assez de fond sur ClearanceRadius autour, hors de l'emprise (+ ShipMargin) des
	 *  bateaux à pont, de préférence sans terre jusqu'au joueur. OutYaw = tourné vers le joueur. Faux si rien.
	 */
	static bool FindSeaSpawnPoint(UWorld* World, float RequiredDepth, float MinDistance, float MaxDistance, float Step, int32 Attempts, float ShipMargin, float ClearanceRadius, FVector& OutPoint, float& OutYaw);

	/** Bateau objectif : présence (AYSGameState::IsShipPresent), absent = caché, sans collision. Pilotable : déplacement */
	virtual void Tick(float DeltaSeconds) override;

	/** Bateau dont fait partie le sol sous Location (pont, gaillard, poste...), ou nullptr */
	static AYSPirateShip* FindShipBelow(const UWorld* World, const FVector& Location, const AActor* IgnoredActor = nullptr);

	/** Bateau auquel appartient cet acteur (lui-même, ou un de ses postes / acteurs accrochés), ou nullptr */
	static AYSPirateShip* GetShipOf(const AActor* Actor);

	/** Le point (monde) est dans l'emprise de la coque (rectangle des Hull Probe Points, agrandi de Margin), à plat */
	bool IsInsideHull(const FVector& WorldLocation, float Margin = 0.0f) const;

	/** Point (monde, à plat) de la coque le plus proche de WorldLocation, Inset vers l'intérieur */
	FVector GetClosestHullPoint(const FVector& WorldLocation, float Inset = 0.0f) const;

	/** Points de la coque en coordonnées monde (à la hauteur de l'acteur) */
	void GetWorldHullPoints(TArray<FVector>& OutPoints) const;

private:

	/** Présence appliquée (-1 = pas encore) */
	int8 AppliedPresence = -1;

	/** Avance l'état d'un pas (vitesse, rotation) : même calcul sur le serveur et pour l'extrapolation des clients */
	void StepMotion(FYSShipMotion& State, float DeltaSeconds) const;

	/** Le déplacement From -> To est permis : eau assez profonde, ou pas moins profonde qu'avant */
	bool CanMoveHull(const FYSShipMotion& From, const FYSShipMotion& To) const;

	/** Serveur : déplacement du pas, arrêt contre le fond */
	void TickServerMotion(float DeltaSeconds);

	/** Clients : état reçu extrapolé jusqu'à maintenant, écart rattrapé en douceur */
	void TickClientMotion(float DeltaSeconds);

	/** Bateau et postes cachés / montrés (présence du bateau objectif) */
	void ApplyPresence(bool bPresent);

	/** Barre de vie du bateau (local, chaque image si elle a changé) */
	void UpdateShipHealthBar();

	float DisplayedShipHealth = -1.0e9f;

	/** Dernier état reçu (clients), extrapolé à chaque image */
	FYSShipMotion ClientPredicted;
	bool bHasClientMotion = false;

	/** Écart entre la position affichée et l'état extrapolé (clients), réduit à chaque image */
	FVector ClientLocationError = FVector::ZeroVector;
	float ClientYawError = 0.0f;

	/** Dernières commandes envoyées aux clients (pour ne forcer l'envoi qu'aux changements) */
	float LastSentThrottle = 0.0f;
	float LastSentSteer = 0.0f;

	/** Bloqué contre le fond (log une fois) */
	bool bGrounded = false;

	/** Mode playtest sans trajet dans la map (log une fois) */
	bool bPlaytestPathMissingLogged = false;

	double LastMotionLogTime = -1.0e6;
};
