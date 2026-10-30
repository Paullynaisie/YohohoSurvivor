#pragma once

#include "CoreMinimal.h"
#include "Enemies/YSFoeCharacter.h"
#include "Ship/YSLongboat.h"
#include "YSFoeJollyShip.generated.h"

class AYSFoeJollyShip;

/**
 *  Coque d'un NME Jolly Ship : NME invisible posé sur le bateau, c'est lui qui est ciblé et touché (balles, coups, explosions, marque,
 *  barre de vie...). PV = Base Health 500 + Health Per Level 100 x niveau (x le multiplicateur du nombre de joueurs), pas d'XP (celle
 *  de l'équipage suffit). Sa mort détruit le bateau. Hurtbox = BodyMesh (caché), à retoucher dans le Blueprint FoeCharacter_JollyHull.
 */
UCLASS(meta=(PrioritizeCategories="Foe"))
class AYSFoeJollyHull : public AYSFoeCharacter
{
	GENERATED_BODY()

public:

	AYSFoeJollyHull();

	virtual void BeginPlay() override;

protected:

	/** Bateau détruit, équipage à l'eau */
	virtual void Die() override;
};

/** Phase de la routine d'un NME Jolly Ship */
UENUM()
enum class EYSJollyShipPhase : uint8
{
	/** Fonce vers sa cible jusqu'à Engage Distance */
	Approach,
	/** Tourne autour d'elle pendant Circle Duration */
	Circle,
	/** Fonce droit sur elle, jusqu'au choc */
	Rush
};

/**
 *  NME Jolly Ship : amalgame de NME sur une chaloupe (mêmes réglages de déplacement que celle des joueurs, Blueprint BP_NME_JollyShip).
 *  Équipage (Jolly Ship|Crew, listes par poste) : pilote + 2 places ; ceux des places tirent quand leur cible est à portée, avec portée
 *  et durée de vie des AMMO multipliées. Coque = AYSFoeJollyHull (PV du bateau).
 *  Routine (serveur, Jolly Ship|Routine) : avance vers le joueur le plus proche jusqu'à Engage Distance, tourne autour Circle Duration
 *  secondes, puis fonce sur lui. Cible à bord d'un bateau : distances mesurées jusqu'à la coque de ce bateau.
 *  Choc contre le bateau des joueurs ou la plage : chaloupe détruite, équipage posé dessus. Coque détruite : équipage à l'eau, leur XP
 *  est donnée quand ils y meurent.
 */
UCLASS(meta=(PrioritizeCategories="Jolly Ship Longboat Ship"))
class AYSFoeJollyShip : public AYSLongboat
{
	GENERATED_BODY()

private:

	/** Places de l'équipage (pieds) ; à déplacer dans le Blueprint */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> PilotSeat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> LeftSeat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> RightSeat;

protected:

	/** Coque (PV du bateau) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jolly Ship")
	TSubclassOf<AYSFoeJollyHull> HullClass;

	/** NME possibles par poste (un tiré au hasard selon leur Spawn Weight ; vide = poste vide) ; le pilote n'attaque pas */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jolly Ship|Crew")
	TArray<TSubclassOf<AYSFoeCharacter>> PilotFoeClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jolly Ship|Crew")
	TArray<TSubclassOf<AYSFoeCharacter>> LeftSeatFoeClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jolly Ship|Crew")
	TArray<TSubclassOf<AYSFoeCharacter>> RightSeatFoeClasses;

	/** Portée d'attaque des NME à bord x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jolly Ship|Crew", meta=(ClampMin="0"))
	float CrewAttackRangeMultiplier = 3.0f;

	/** Durée de vie des AMMO des NME à bord x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jolly Ship|Crew", meta=(ClampMin="0"))
	float CrewAmmoLifeTimeMultiplier = 3.0f;

	/** Avance jusqu'à cette distance de sa cible, puis tourne autour d'elle à cette distance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jolly Ship|Routine", meta=(ClampMin="0", Units="Centimeters"))
	float EngageDistance = 1500.0f;

	/** Le tour se fait à Engage Distance moins ça : l'équipage reste à portée (Attack Range 500 x 3 = 1500, comme Engage Distance) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jolly Ship|Routine", meta=(ClampMin="0", Units="Centimeters"))
	float CircleRadiusInset = 100.0f;

	/** Durée du tour autour de la cible, avant de foncer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jolly Ship|Routine", meta=(ClampMin="0", Units="s"))
	float CircleDuration = 10.0f;

	/** Écart d'angle avec la direction voulue à partir duquel la barre est à fond */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jolly Ship|Routine", meta=(ClampMin="1", Units="Degrees"))
	float SteerFullAngle = 30.0f;

	/** Pendant le tour : fond trop haut à cette distance devant (plage) -> il repart dans l'autre sens au lieu de s'échouer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jolly Ship|Routine", meta=(ClampMin="0", Units="Centimeters"))
	float CircleShoreLookAhead = 800.0f;

	/** Pas plus d'un changement de sens par ce délai */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jolly Ship|Routine", meta=(ClampMin="0", Units="s"))
	float CircleFlipCooldown = 2.0f;

	/** Cible sans bateau (elle nage) atteinte à cette distance : rien à percuter, la routine reprend */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jolly Ship|Routine", meta=(ClampMin="0", Units="Centimeters"))
	float RushReachDistance = 300.0f;

	/** Pilote automatique : la routine */
	virtual bool ComputeAutoPilot(float& OutThrottle, float& OutSteer) override;

	/** Pas de barre : la chaloupe reste sans pilote */
	virtual bool DisappearsWithoutPilot() const override { return false; }

	/** Choc (bateau, plage) : équipage posé autour de Location, chaloupe détruite */
	virtual void EjectAndDestroy(const FVector& Location, const FVector& Facing, const TCHAR* Reason) override;

public:

	AYSFoeJollyShip(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaSeconds) override;

	virtual bool IsFoeShip() const override { return true; }

	virtual bool IsBoardable() const override { return false; }

	/** Pieds du NME au poste Seat (SeatPilot, SeatLeft, SeatRight, SeatHull = origine du bateau) */
	virtual FVector GetSeatFeetLocation(int32 Seat) const override;

	/** Coque détruite (serveur) : équipage à l'eau (il donne son XP en y mourant), chaloupe détruite */
	void OnHullDestroyed();

	/**
	 *  Un Jolly Ship sur l'eau : direction au hasard autour d'un joueur tiré au hasard, premier point à partir de MinDistance (loin de tous
	 *  les joueurs, jusqu'à MaxDistance) où l'eau est assez profonde et qui n'est pas sous un bateau, de préférence sans terre entre lui et
	 *  le joueur (Attempts directions essayées) ; tourné vers le joueur. nullptr si rien.
	 */
	static AYSFoeJollyShip* SpawnAtSea(UWorld* World, TSubclassOf<AYSFoeJollyShip> ShipClass, float MinDistance, float MaxDistance, float Step, int32 Attempts);

	static constexpr int32 SeatHull = -1;
	static constexpr int32 SeatPilot = 0;
	static constexpr int32 SeatLeft = 1;
	static constexpr int32 SeatRight = 2;

private:

	UPROPERTY(Transient)
	TObjectPtr<AYSFoeJollyHull> HullFoe;

	/** Équipage (serveur) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AYSFoeCharacter>> Crew;

	/** Cible (serveur) : joueur le plus proche pendant l'approche, gardé ensuite tant qu'il est vivant */
	UPROPERTY(Transient)
	TObjectPtr<APawn> ShipTarget;

	EYSJollyShipPhase Phase = EYSJollyShipPhase::Approach;
	double PhaseStartTime = 0.0;
	double LastFlipTime = -1.0e6;
	double NextShoreCheckTime = 0.0;
	double LastRoutineLogTime = -1.0e6;

	/** Sens du tour : 1 ou -1 */
	float CircleSide = 1.0f;

	/** Un NME à bord (serveur) ; nullptr si le poste est vide */
	void SpawnCrewMember(const TArray<TSubclassOf<AYSFoeCharacter>>& Classes, int32 Seat, bool bCanAttack);

	/** Joueur vivant le plus proche, à la nage compris */
	APawn* FindNearestPlayerPawn() const;

	/** Point visé : la cible, ou le point le plus proche de la coque du bateau où elle se trouve */
	FVector GetTargetPoint(const APawn* TargetPawn, bool& bOutOnShip) const;

	void SetPhase(EYSJollyShipPhase NewPhase);

	/** Coque et équipage encore à bord libérés (fin du bateau, quelle qu'en soit la raison) */
	void ReleaseCrew(bool bExperienceInWater);
};
