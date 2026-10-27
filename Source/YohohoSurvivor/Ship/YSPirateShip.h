#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YSPirateShip.generated.h"

class UChildActorComponent;
class AYSShipHelm;

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
 *  Visuel (primitives) dans le Blueprint enfant /Game/Placeable/Ship/BP_PirateShip (Tools/EditorScripts/setup_pirate_ship.py).
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

	/** Vitesse max en marche avant (cm/s ; les joueurs marchent à 600) */
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

	/** Caméra du pilote : longueur du bras (le joueur à pied : 1680) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Camera", meta=(ClampMin="0", Units="Centimeters"))
	float HelmCameraArmLength = 3600.0f;

	/** Caméra du pilote centrée sur le bateau (sinon sur le pilote) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Camera")
	bool bHelmCameraOnShip = true;

	/** Vitesse du dézoom / zoom et du recentrage en prenant / quittant la barre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Camera", meta=(ClampMin="0"))
	float HelmCameraInterpSpeed = 2.0f;

	/** Clients : vitesse de rattrapage d'un écart avec l'état reçu du serveur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ship|Helm|Network", meta=(ClampMin="0"))
	float ClientCorrectionSpeed = 6.0f;

	/** Déplacement du bateau pilotable (serveur -> clients) */
	UPROPERTY(ReplicatedUsing=OnRep_Motion)
	FYSShipMotion Motion;

	UFUNCTION()
	void OnRep_Motion();

public:

	AYSPirateShip();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	FText GetShipName() const { return ShipName; }

	bool IsObjectiveShip() const { return bObjectiveShip; }

	bool IsDrivable() const { return bDrivable; }

	AYSShipHelm* GetHelm() const;

	/** Commandes du pilote (serveur) : Throttle Z / S, Steer D / Q, de -1 à 1 */
	void SetPilotInput(float Throttle, float Steer);

	float GetHelmCameraArmLength() const { return HelmCameraArmLength; }
	bool IsHelmCameraOnShip() const { return bHelmCameraOnShip; }
	float GetHelmCameraInterpSpeed() const { return HelmCameraInterpSpeed; }

	/** Vitesse actuelle le long de la proue (cm/s) */
	float GetSpeed() const { return Motion.Speed; }

	/** Bateau objectif : présence (AYSGameState::IsShipPresent), absent = caché, sans collision. Pilotable : déplacement */
	virtual void Tick(float DeltaSeconds) override;

	/** Bateau dont fait partie le sol sous Location (pont, gaillard...), ou nullptr */
	static AYSPirateShip* FindShipBelow(const UWorld* World, const FVector& Location, const AActor* IgnoredActor = nullptr);

private:

	/** Présence appliquée (-1 = pas encore) */
	int8 AppliedPresence = -1;

	/** Avance l'état d'un pas (vitesse, rotation) : même calcul sur le serveur et pour l'extrapolation des clients */
	void StepMotion(FYSShipMotion& State, float DeltaSeconds) const;

	/** Plus petite profondeur d'eau sous les points de la coque à cette position */
	float GetHullMinDepth(const FVector& Location, float Yaw) const;

	/** Le déplacement From -> To est permis : eau assez profonde, ou pas moins profonde qu'avant */
	bool CanMoveHull(const FYSShipMotion& From, const FYSShipMotion& To) const;

	/** Serveur : déplacement du pas, arrêt contre le fond */
	void TickServerMotion(float DeltaSeconds);

	/** Clients : état reçu extrapolé jusqu'à maintenant, écart rattrapé en douceur */
	void TickClientMotion(float DeltaSeconds);

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

	double LastMotionLogTime = -1.0e6;
};
