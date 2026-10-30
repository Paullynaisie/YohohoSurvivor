#pragma once

#include "CoreMinimal.h"
#include "Ship/YSPirateShip.h"
#include "YSFoeGalion.generated.h"

class AYSFoeCharacter;
class AYSShipStation;

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

	/** Capitaine (élite) : à la barre, seul à piloter */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Galion|Crew")
	TSubclassOf<AYSFoeCharacter> CaptainClass;

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

	/** Pilote automatique : la routine, tant que le capitaine est à la barre */
	virtual bool ComputeAutoPilot(float& OutThrottle, float& OutSteer) override;

	/** Échoué : il recule et repart dans l'autre sens */
	virtual void OnHullBlocked() override;

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

private:

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
