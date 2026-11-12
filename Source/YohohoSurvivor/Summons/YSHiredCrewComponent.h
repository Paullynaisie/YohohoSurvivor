#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "YSHiredCrewComponent.generated.h"

class AAIController;
class AYSFoeCharacter;
class AYSInteractiveObject;
class AYSPirateShip;
class AYSPlayerCharacter;
class AYSShipCannon;

/**
 *  Hired crew (case de l'hôte sur l'écran de sélection) : composant qui fait d'un personnage un HiredMouss piloté par IA (serveur, Tick ;
 *  pas de PlayerState). Ajouté dans /Game/Placeable/Character/Player/PlayerCharacter_HiredMouss, Blueprint ENFANT de PlayerCharacter_mouss
 *  (même set, mêmes upgrades, même passif, suit les retouches de Mouss). Les HiredMouss prennent les places vides jusqu'à 4 personnages ;
 *  créés et réanimés par AYSGameMode (TickHiredCrew).
 *  Il suit le chef = le personnage de l'hôte (sinon un joueur vivant). Priorités :
 *   1. objet porté (coffre, membre d'équipage, boussole, poste de combat) : le ramène (zone du PNJ qui l'accepte, réceptacle libre du
 *      bateau pour un poste de combat ; monte à bord s'il le faut) ;
 *   2. objet de ce type à moins de Pickup Trigger Distance du chef : va le prendre (un HiredMouss par objet) ;
 *   3. chef à bord du bateau des joueurs : monte à bord, s'installe à un poste de combat libre (tir automatique) ; aucun libre : se met
 *      au centre du bateau et attaque les NME qui arrivent ;
 *   4. NME à portée : s'approche, attaque auto toute seule, Ability dès qu'elle est prête et le NME à sa portée ;
 *   5. sinon suit le chef.
 *  Grimpe comme les NME (il butte contre un mur en avançant -> vol vers le haut). Stamina comme un joueur (port, nage, noyade ; barre
 *  sous ses PV) mais jamais de MoveAction. Mort : ragdoll puis disparition ; réanimé au bateau sans plume de phoenix.
 */
UCLASS(ClassGroup=(YohohoSurvivor), meta=(BlueprintSpawnableComponent, PrioritizeCategories="Hired Crew"))
class UYSHiredCrewComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Distance au chef au-delà de laquelle il revient vers lui (sans cible) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew", meta=(ClampMin="0", Units="Centimeters"))
	float FollowDistance = 500.0f;

	/** En revenant vers le chef, il s'arrête à cette distance de lui */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew", meta=(ClampMin="0", Units="Centimeters"))
	float FollowStopDistance = 250.0f;

	/** Un NME à moins de cette distance devient sa cible (le plus proche) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew", meta=(ClampMin="0", Units="Centimeters"))
	float AggroRange = 1000.0f;

	/** Un NME plus loin que cette distance du chef n'est pas poursuivi */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew", meta=(ClampMin="0", Units="Centimeters"))
	float MaxChaseDistanceFromLeader = 1500.0f;

	/** Arme de mêlée : il s'approche de sa cible jusqu'à cette distance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew", meta=(ClampMin="0", Units="Centimeters"))
	float MeleeStopDistance = 150.0f;

	/** Attaque auto à distance seulement (Gun) : il s'arrête à cette distance de sa cible */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew", meta=(ClampMin="0", Units="Centimeters"))
	float RangedStopDistance = 600.0f;

	/** Ability (clic droit) lancée vers sa cible dès qu'elle est prête et la cible à cette distance : action de mêlée / de Gun */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew", meta=(ClampMin="0", Units="Centimeters"))
	float AbilityRangeMelee = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew", meta=(ClampMin="0", Units="Centimeters"))
	float AbilityRangeGun = 1000.0f;

	/** Plus loin que cette distance du chef (à plat) sans rien porter : téléporté à côté de lui (resté sur une île, bloqué ; 0 = jamais) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew", meta=(ClampMin="0", Units="Centimeters"))
	float TeleportDistanceFromLeader = 8000.0f;

	/** Nom au-dessus de lui ({0} = son numéro) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew")
	FText NameFormat = NSLOCTEXT("YS", "HiredMoussName", "Hired Mouss {0}");

	/** Objet à ramener (coffre, membre d'équipage, boussole, poste de combat) à moins de cette distance du chef : il va le prendre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew|Carry", meta=(ClampMin="0", Units="Centimeters"))
	float PickupTriggerDistance = 800.0f;

	/** Objet porté : il le repose à moins de cette part du rayon de la zone qui l'accepte (0.5 = à mi-chemin du centre) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew|Carry", meta=(ClampMin="0.05", ClampMax="1"))
	float DeliverRangeRatio = 0.5f;

	/** Pour rejoindre l'accès du bateau depuis l'autre flanc, il passe à cette distance de la proue ou de la poupe */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew|Ship", meta=(ClampMin="0", Units="Centimeters"))
	float BoardingHullMargin = 250.0f;

	/** Il s'installe au poste de combat (ou pose un poste sur un réceptacle) une fois à cette distance de sa place (à plat) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew|Ship", meta=(ClampMin="0", Units="Centimeters"))
	float PostReachDistance = 100.0f;

	/** Sans poste libre : il reste à moins de cette distance du centre du bateau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew|Ship", meta=(ClampMin="0", Units="Centimeters"))
	float DeckCenterStopDistance = 200.0f;

	/** Plus lent que cette vitesse pendant Stuck Delay en marchant vers un but (mât, marche) = bloqué : contourne pendant Sidestep Duration */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew|Ship", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float StuckSpeed = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew|Ship", meta=(ClampMin="0", Units="s"))
	float StuckDelay = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew|Ship", meta=(ClampMin="0", Units="s"))
	float SidestepDuration = 0.8f;

	/** Escalade comme les NME (mêmes réglages que Foe|Movement|Climb) : en butant contre une surface en avançant, il grimpe */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew|Climb")
	bool bClimbWhenBlocked = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew|Climb", meta=(ClampMin="0", EditCondition="bClimbWhenBlocked"))
	float ClimbSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew|Climb", meta=(ClampMin="0", ClampMax="1", EditCondition="bClimbWhenBlocked"))
	float ClimbMinLostSpeedRatio = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew|Climb", meta=(ClampMin="0", Units="s", EditCondition="bClimbWhenBlocked"))
	float ClimbBlockMemory = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew|Climb", meta=(ClampMin="0", ClampMax="1", EditCondition="bClimbWhenBlocked"))
	float ClimbFullSpeedKeptRatio = 0.5f;

	/** En nageant contre la coque d'un bateau : grimpe à pleine vitesse verticale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hired Crew|Climb", meta=(EditCondition="bClimbWhenBlocked"))
	bool bSwimmerFullClimbOnShips = true;

	UYSHiredCrewComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Après l'apparition (serveur) : numéro, couleur, visage */
	void InitHired(int32 InNumber, int32 InColorIndex, int32 InFaceIndex);

	int32 GetHiredNumber() const { return HiredNumber; }
	int32 GetColorIndex() const { return ColorIndex; }
	int32 GetFaceIndex() const { return FaceIndex; }

	/** « Hired Mouss N » */
	FText GetDisplayName() const { return FText::Format(NameFormat, FText::AsNumber(HiredNumber)); }

	/** Point visé (Ability, attaque auto) : sa cible, sinon droit devant */
	FVector GetAimLocation() const;

	/** Mort du personnage (serveur, avant son ragdoll) : le GameMode garde ses upgrades */
	void OnOwnerDies();

	/** Escalade : surface contre laquelle le déplacement a buté (appelé par le personnage) */
	void OnMoveBlocked(const FHitResult& Impact);

	/** Objet à ramener pour l'équipage : coffre / objectif accepté par un PNJ (Brooker, core, zone de la plage), poste de combat ; pas un cadavre */
	static bool IsHaulable(const AYSInteractiveObject* Object);

	/** Composant Hired crew d'un acteur (nullptr si ce n'est pas un HiredMouss) */
	static UYSHiredCrewComponent* Get(const AActor* Actor);

protected:

	UPROPERTY(Replicated)
	int32 HiredNumber = 1;

	UFUNCTION()
	void OnRep_Customization();

	UPROPERTY(ReplicatedUsing=OnRep_Customization)
	int32 ColorIndex = 0;

	UPROPERTY(ReplicatedUsing=OnRep_Customization)
	int32 FaceIndex = 0;

private:

	AYSPlayerCharacter* GetCharacter() const;

	/** Objet porté : vers la zone qui l'accepte (ou un réceptacle libre pour un poste de combat) ; vrai = rien d'autre à faire */
	bool UpdateDelivery(AAIController* AIController, float DeltaSeconds);

	/** Objet à prendre près du chef : marche jusqu'à lui et le prend ; vrai = occupé */
	bool UpdatePickup(AAIController* AIController, float DeltaSeconds, const AYSPlayerCharacter* Leader);

	/** Chef à bord du bateau des joueurs : monter à bord, poste de combat, centre du pont ; vrai = occupé */
	bool UpdateShipCrew(AAIController* AIController, float DeltaSeconds, const AYSPlayerCharacter* Leader, AYSFoeCharacter* Target);

	/** Pas encore sur ce bateau : marche / nage jusqu'à son accès et monte ; vrai = en route (pas encore à bord) */
	bool BoardShip(AYSPirateShip* Ship, AAIController* AIController, float DeltaSeconds);

	/** Au poste de combat : vise le plus proche à portée, tire dès que le poste est rechargé */
	void UpdateCannonFire(AYSShipCannon* Cannon, AAIController* AIController);

	/** Poste de combat libre du bateau, pas déjà visé par un autre HiredMouss (garde le sien s'il est encore libre), ou nullptr */
	AYSShipCannon* FindFreeCombatPost(const AYSPirateShip* Ship) const;

	/** Cible : NME le plus proche dans Aggro Range, pas plus loin que Max Chase Distance From Leader du chef (sans chef : partout) */
	AYSFoeCharacter* FindTarget(const AYSPlayerCharacter* Leader) const;

	/** Combat : s'approche (sauf bHoldPosition), tourné vers la cible ; Ability dès qu'elle est prête et à portée */
	void FightTarget(AYSFoeCharacter* Target, AAIController* AIController, float DeltaSeconds, bool bHoldPosition);

	/** Attaque auto à distance seulement : il garde ses distances */
	bool HasOnlyRangedAutoAttack() const;

	/** Une action qui occupe tout le personnage (roulade, dash) est en cours : son déplacement est celui de l'action */
	bool IsBlockingActionInProgress() const;

	/** Marche vers Goal (à plat) jusqu'à Stop Distance, regarde où il va, contourne ce qui le bloque ; vrai = arrivé */
	bool WalkToward(const FVector& Goal, float StopDistance, AAIController* AIController, float DeltaSeconds);

	/** Escalade (serveur, début du Tick) : d'après le déplacement de l'image précédente */
	void UpdateClimb();

	void StopClimbing(const TCHAR* Reason);

	/** Téléporté à côté du chef (resté trop loin) */
	void TeleportNear(const AYSPlayerCharacter* Leader);

	/** Objet qu'il va chercher (un seul HiredMouss par objet) */
	TWeakObjectPtr<AYSInteractiveObject> ClaimedObject;

	/** Poste de combat vers lequel il marche */
	TWeakObjectPtr<AYSShipCannon> AssignedPost;

	/** Cible actuelle (serveur) */
	TWeakObjectPtr<AYSFoeCharacter> CurrentTarget;

	/** Revient vers le chef (au-delà de Follow Distance) jusqu'à Follow Stop Distance */
	bool bReturningToLeader = false;

	/** Bloqué en marchant : temps sous Stuck Speed, temps restant de contournement, côté */
	float StuckTime = 0.0f;
	float SidestepTime = 0.0f;
	float SidestepSide = 1.0f;

	/** Escalade */
	bool bClimbing = false;
	bool bMoveInputThisFrame = false;
	double LastClimbBlockTime = -1.0;
	bool bLastClimbBlockOnShipSwimming = false;
	bool bFullSpeedClimb = false;

	/** Recherche d'objet à prendre (pas à chaque image) */
	float PickupScanTimer = 0.0f;
};
