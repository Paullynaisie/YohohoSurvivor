#pragma once

#include "CoreMinimal.h"
#include "Interaction/YSInteractiveObject.h"
#include "YSShipStation.generated.h"

class AYSPirateShip;
class UTexture2D;

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
class AYSShipStation : public AYSInteractiveObject
{
	GENERATED_BODY()

private:

	/** Où se tient le joueur (ses pieds), tourné vers +X */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> OccupantPoint;

	/** Où le joueur est reposé en quittant le poste (Teleport On Leave) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> ExitPoint;

	/** Joueur au poste (nullptr = personne) */
	UPROPERTY(ReplicatedUsing=OnRep_Occupant, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Station", meta=(AllowPrivateAccess="true"))
	TObjectPtr<AYSPlayerCharacter> Occupant;

	UFUNCTION()
	void OnRep_Occupant();

protected:

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

	/** PV du poste (0 = pas de PV) ; pas encore de dégâts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station", meta=(ClampMin="0"))
	float MaxHealth = 0.0f;

	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Station")
	float Health = 0.0f;

	/** Caméra du joueur au poste : longueur du bras (à pied : 1680) */
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

	/** Préfixe des logs */
	virtual const TCHAR* GetLogTag() const { return TEXT("[YSStation]"); }

public:

	AYSShipStation();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaSeconds) override;

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
	virtual bool GetActionIcon(UTexture2D*& OutIcon, FLinearColor& OutTint, float& OutRemaining, float& OutDuration) const { return false; }

	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
};
