#pragma once

#include "CoreMinimal.h"
#include "GameplayTagAssetInterface.h"
#include "Interaction/YSInteractiveObject.h"
#include "YSShipStation.generated.h"

class AYSPirateShip;
class UBoxComponent;
class UTexture2D;
class UWidgetComponent;
class AYSFoeCharacter;
class AYSCombatPostReceptacle;

/** Décalage de la caméra d'un joueur à un poste */
UENUM(BlueprintType)
enum class EYSStationCameraOffset : uint8
{
	/** Centrée (sur le joueur, ou sur le bateau avec Camera On Ship) */
	None		UMETA(DisplayName="Centrée"),
	/** Comme à pied : vers le curseur, proportionnel à sa distance au centre de l'écran, jusqu'à Camera Offset Distance */
	Cursor		UMETA(DisplayName="Vers le curseur"),
	/** Toujours à Camera Offset Distance, dans la direction visée par le poste (canon : butée à son angle max) */
	Aim			UMETA(DisplayName="Vers la visée (au maximum)")
};

/** Touches maintenues au poste (AYSPlayerCharacter::GetStationHeldInputs) */
namespace YSStationInput
{
	constexpr uint8 Left = 1;
	constexpr uint8 Right = 2;
	constexpr uint8 Space = 4;
}

/** Travail du joueur au poste (touche maintenue) : rien d'autre n'est possible pendant ce temps */
UENUM()
enum class EYSStationWork : uint8
{
	None,
	/** Espace maintenu : PV rendus petit à petit (ils restent si on relâche) */
	Repair,
	/** Clic droit maintenu (poste de combat posé) : démonté au bout de Dismount Hold Duration du réceptacle, à refaire si on relâche */
	Dismount
};

/** Icône d'une action du poste en bas de l'écran (une par touche : clic gauche, clic droit, Espace) */
struct FYSStationActionIcon
{
	bool bShow = false;
	UTexture2D* Icon = nullptr;
	FLinearColor Tint = FLinearColor::White;
	FText Name;
	float Remaining = 0.0f;
	float Duration = 0.0f;
	bool bBlocked = false;
};

/** Action envoyée par le joueur à son poste */
UENUM()
enum class EYSStationAction : uint8
{
	/** Clic droit (Ability à pied) */
	Fire,
	/** Clic gauche, à l'appui (attaque auto à pied) */
	Primary
};

/**
 *  Poste d'un bateau (acteur enfant du bateau, interaction touche E) : le joueur s'y installe, il ne bouge plus (gardé sur
 *  OccupantPoint, tourné vers son +X), armes rangées (ni attaque auto ni Ability), E le quitte (si Can Leave). Un seul joueur à la
 *  fois ; poste indisponible tant que le bateau est absent. Caméra propre au poste (catégorie Station|Camera), PV propres (à venir).
 *  Base de la barre, du nid d'pie, des canonneries, de la chalouperie et des places de la chaloupe.
 *  Serveur autoritaire ; occupant répliqué.
 */
UCLASS(meta=(PrioritizeCategories="Station Interaction"))
class AYSShipStation : public AYSInteractiveObject, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

private:

	/** Où se tient le joueur (ses pieds), tourné vers +X */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> OccupantPoint;

	/** Où le joueur est reposé en quittant le poste (Teleport On Leave) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> ExitPoint;

	/**
	 *  Hurtbox du module (Takes Damage) : subit les attaques du camp adverse. Haute : du niveau de la mer (les AMMO de canon volent sous le
	 *  pont) jusqu'au-dessus du canon. À ajuster dans le Blueprint du poste.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> ModuleHurtbox;

	/** PV du module (Takes Damage) : à l'écran, affichée dès qu'il est abîmé ; progression pendant une réparation */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UWidgetComponent> ModuleHealthBar;

	/** NME au poste (équipage d'un Galion ; nullptr = aucun) */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Station", meta=(AllowPrivateAccess="true"))
	TObjectPtr<AYSFoeCharacter> FoeOccupant;

	/** Travail en cours de l'occupant (réparation, démontage) */
	UPROPERTY(Replicated, Transient)
	EYSStationWork Work = EYSStationWork::None;

	/** Joueur au poste (nullptr = personne) */
	UPROPERTY(ReplicatedUsing=OnRep_Occupant, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Station", meta=(AllowPrivateAccess="true"))
	TObjectPtr<AYSPlayerCharacter> Occupant;

	UFUNCTION()
	void OnRep_Occupant();

	/** Poste de combat posé sur ce réceptacle (nullptr = poste fixe du bateau) */
	UPROPERTY(Replicated, Transient)
	TObjectPtr<AYSCombatPostReceptacle> HostReceptacle;

	/** Démontage en cours (clic droit maintenu) : heure serveur de la fin (0 = aucun) */
	UPROPERTY(Replicated, Transient)
	double DismountEndServerTime = 0.0;

protected:

	/** Poste de combat : objet ramassable rendu quand on le démonte de son réceptacle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station|Combat Post")
	TSubclassOf<AYSInteractiveObject> CombatPostItemClass;

	/** E quitte le poste (faux : le joueur y reste, ex. chaloupe) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station")
	bool bCanLeave = true;

	/** En quittant le poste, le joueur est reposé sur Exit Point (nid d'pie : au pied du mât) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station")
	bool bTeleportOnLeave = false;

	/** Un joueur qui nage peut s'installer (place passager de la chaloupe) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station")
	bool bAllowSwimmingEntry = false;

	/** Mains du joueur sur Left / Right Hand Point du poste (barre, canon) ; sinon mains au repos */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station")
	bool bHandsOnStation = true;

	/** Regard figé vers le +X d'Occupant Point ; sinon le joueur regarde le curseur (canon : il vise) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station")
	bool bLockAim = true;

	/** Un joueur prend la place d'un minion (E) : le minion est reposé à cette distance sur le côté */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station", meta=(ClampMin="0", Units="Centimeters"))
	float ReplacedOccupantOffset = 150.0f;

	/** Le module a une hurtbox et subit les attaques du camp adverse (modules de combat, chalouperie) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station|Health")
	bool bTakesDamage = false;

	/** PV du poste */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station|Health", meta=(ClampMin="0", EditCondition="bTakesDamage"))
	float MaxHealth = 0.0f;

	/** Réparation (Espace maintenu au poste) : temps pour rendre tous les PV (de 0 au max) ; relâché, les PV rendus restent */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station|Health", meta=(ClampMin="0", Units="s", EditCondition="bTakesDamage"))
	float RepairDuration = 10.0f;

	/** Icône de l'action « Réparer » (Espace, en bas de l'écran) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station|Health", meta=(EditCondition="bTakesDamage"))
	TObjectPtr<UTexture2D> RepairIcon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station|Action Icons")
	FText RepairActionName = NSLOCTEXT("YS", "StationRepair", "Réparer");

	/** Poste de combat posé : icône et nom de l'action « Démonter » (clic droit maintenu) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station|Action Icons")
	TObjectPtr<UTexture2D> DismountIcon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station|Action Icons")
	FText DismountActionName = NSLOCTEXT("YS", "StationDismount", "Démonter");

	UPROPERTY(ReplicatedUsing=OnRep_Health, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Station|Health")
	float Health = 0.0f;

	UFUNCTION()
	void OnRep_Health();

	/** Caméra du joueur au poste : longueur du bras (à pied : 1840) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station|Camera", meta=(ClampMin="0", Units="Centimeters"))
	float CameraArmLength = 3600.0f;

	/** Caméra centrée sur le bateau (sinon sur le joueur) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station|Camera")
	bool bCameraOnShip = false;

	/** Décalage de la caméra : centrée, vers le curseur, ou toujours au maximum vers la visée du poste */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station|Camera")
	EYSStationCameraOffset CameraOffsetMode = EYSStationCameraOffset::None;

	/** Décalage max (Vers le curseur) ou fixe (Vers la visée) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station|Camera", meta=(ClampMin="0", Units="Centimeters", EditCondition="CameraOffsetMode != EYSStationCameraOffset::None"))
	float CameraOffsetDistance = 1000.0f;

	/** Vitesse du dézoom / zoom et du recentrage en prenant / quittant le poste */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station|Camera", meta=(ClampMin="0"))
	float CameraInterpSpeed = 2.0f;

	/** Occupant changé (toutes machines ; serveur : juste après le changement) */
	virtual void OnOccupantChanged(AYSPlayerCharacter* Previous) {}

	/** Poste utilisable : bateau présent (le bateau objectif est caché avant l'objectif de doublons) */
	bool IsShipAvailable() const;

	/** Module tombé à 0 PV, ou réparé (toutes machines, visuel) */
	virtual void OnBrokenChanged(bool bBroken) {}

	/** Préfixe des logs */
	virtual const TCHAR* GetLogTag() const { return TEXT("[YSStation]"); }

	/** Serveur : réparation ou démontage arrêté (relâché, plus possible) ; canon : son Ability se met en recharge */
	virtual void OnWorkStopped(EYSStationWork StoppedWork) {}

public:

	AYSShipStation();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaSeconds) override;

	/** Camp du bateau : Team.Foe sur un bateau de NME, Team.Player sinon (les attaques filtrent sur ces tags) */
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;

	/**
	 *  Coup reçu par le module (serveur) : PV du module ; à 0 PV (ou bateau hors d'usage) c'est le core qui prend le coup, et le joueur qui
	 *  réparait le module aussi
	 */
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Explosion : dégâts aux modules portant un de ces tags dont la hurtbox touche la sphère (serveur) */
	static void DamageModulesInRadius(UWorld* World, const FVector& Center, float Radius, const FGameplayTagContainer& HitTags, float Damage, AController* EventInstigator);

	/** Module à 0 PV : inutilisable, E le répare */
	bool IsBroken() const { return bTakesDamage && Health <= 0.0f && bHealthInitialized; }

	/** Le module a des PV (hurtbox) */
	bool TakesDamage() const { return bTakesDamage; }

	/** Un joueur le répare (toutes machines) */
	bool IsRepairing() const { return Work == EYSStationWork::Repair; }

	/** L'occupant répare ou démonte : pas d'autre action au poste */
	bool IsWorking() const { return Work != EYSStationWork::None; }

	/** Secondes pour finir de réparer (aux PV max) */
	float GetRepairRemaining() const;

	AYSFoeCharacter* GetFoeOccupant() const { return FoeOccupant; }

	/** NME au poste (serveur) */
	void SetFoeOccupant(AYSFoeCharacter* NewOccupant);

	/** Poste libre : ni joueur ni NME */
	bool IsFree() const { return !Occupant && !FoeOccupant; }

	/** Utilisable : pas à 0 PV, bateau pas hors d'usage */
	bool IsUsable() const;

	/** Bateau présent, poste libre (ou tenu par un minion, si c'est un joueur qui veut sa place), joueur vivant aux mains libres (hors de l'eau sauf Allow Swimming Entry) et à aucun poste */
	virtual bool CanInteract(const AYSPlayerCharacter* Character) const override;

	/** Le poste est tenu par un minion et Character est un joueur : il peut lui voler la place (un minion ne vole jamais, personne ne vole un joueur) */
	bool CanTakeOverFrom(const AYSPlayerCharacter* Character) const;

	/** Le joueur s'installe au poste (serveur) */
	virtual void Interact(AYSPlayerCharacter* Character) override;

	/** Occupant (serveur, appelé par le personnage qui s'installe / part) */
	void SetOccupant(AYSPlayerCharacter* NewOccupant);

	AYSPlayerCharacter* GetOccupant() const { return Occupant; }

	/** Bateau qui porte le poste */
	AYSPirateShip* GetShip() const;

	/** Position du joueur (ses pieds) et direction de son regard */
	FVector GetOccupantFeetLocation() const;
	FVector GetOccupantForward() const;
	FVector GetExitFeetLocation() const;

	USceneComponent* GetOccupantPoint() const { return OccupantPoint; }
	USceneComponent* GetExitPoint() const { return ExitPoint; }

	bool CanLeave() const { return bCanLeave; }
	bool TeleportsOnLeave() const { return bTeleportOnLeave; }
	bool AllowsSwimming() const { return bAllowSwimmingEntry; }
	bool PutsHandsOnStation() const { return bHandsOnStation; }
	bool LocksAim() const { return bLockAim; }

	/** Les armes restent visibles à ce poste (toujours sans capacité) */
	virtual bool KeepsWeaponsVisible() const { return false; }

	/** La caméra garde le nord en haut de l'écran au lieu de tourner avec le bateau */
	virtual bool KeepsCameraNorthUp() const { return false; }

	/** ZQSD pilote un bateau (barre) au lieu de rien faire */
	virtual bool UsesPilotInput() const { return false; }

	/** Commandes ZQSD reçues (serveur ; X = Q / D, Y = Z / S) */
	virtual void ApplyPilotInput(AYSPlayerCharacter* Character, const FVector2D& Input) {}

	/** Le poste prend cette action (machine qui contrôle : le clic est envoyé au serveur au lieu d'aller aux armes) */
	virtual bool HandlesAction(EYSStationAction Action) const { return false; }

	/** Action du joueur au poste (serveur) vers AimLocation */
	virtual void ServerHandleAction(AYSPlayerCharacter* Character, EYSStationAction Action, const FVector& AimLocation) {}

	/** Caméra (machine qui contrôle) */
	float GetCameraArmLength() const { return CameraArmLength; }
	bool IsCameraOnShip() const { return bCameraOnShip; }
	EYSStationCameraOffset GetCameraOffsetMode() const { return CameraOffsetMode; }
	float GetCameraOffsetDistance() const { return CameraOffsetDistance; }
	float GetCameraInterpSpeed() const { return CameraInterpSpeed; }

	/** Direction visée par le poste (Camera Offset Mode = Vers la visée) ; faux = aucune */
	virtual bool GetCameraAimDirection(FVector& OutDirection) const { return false; }

	/** Icône d'action du poste (en bas de l'écran, à la place de celles des armes) : faux = pas d'icône */
	virtual bool GetActionIcon(UTexture2D*& OutIcon, FLinearColor& OutTint, float& OutRemaining, float& OutDuration) const;

	/**
	 *  Icônes des actions du poste à la place de celles des armes, une par touche (clic gauche / clic droit / Espace) ; défaut : Get Action
	 *  Icon au clic droit, réparer à Espace si le module a des PV, démonter au clic droit d'un poste de combat posé
	 */
	virtual void GetStationActionIcons(FYSStationActionIcon& OutLeft, FYSStationActionIcon& OutRight, FYSStationActionIcon& OutSpace) const;

	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }

	/** Poste de combat posé sur un réceptacle (serveur, avant FinishSpawning) */
	void SetHostReceptacle(AYSCombatPostReceptacle* Receptacle) { HostReceptacle = Receptacle; }
	AYSCombatPostReceptacle* GetHostReceptacle() const { return HostReceptacle; }

	TSubclassOf<AYSInteractiveObject> GetCombatPostItemClass() const { return CombatPostItemClass; }

	bool IsBeingDismounted() const { return Work == EYSStationWork::Dismount && DismountEndServerTime > 0.0; }

private:

	/** PV du module affichés, réparation comprise (local) */
	void UpdateModuleHealthBar();

	/** Une réparation peut continuer : bateau pas hors d'usage */
	bool IsUsableForRepair() const;

	/** Serveur, chaque image : réparation (Espace) et démontage (clic droit) de l'occupant */
	void UpdateWork(float DeltaSeconds);
	void StopWork(const TCHAR* Reason);

	/** Visuel appliqué : à 0 PV ou non (local) */
	bool bAppliedBroken = false;

	/** Clients : PV reçus (avant : Health 0, ce n'est pas un module cassé) */
	bool bHealthInitialized = false;

	float DisplayedHealth = -1.0f;
	float DisplayedRepairProgress = -1.0f;
	bool bDisplayedRepairing = false;
};
