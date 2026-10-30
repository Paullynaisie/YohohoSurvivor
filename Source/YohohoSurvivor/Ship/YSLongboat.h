#pragma once

#include "CoreMinimal.h"
#include "Ship/YSPirateShip.h"
#include "Ship/YSShipHelm.h"
#include "YSLongboat.generated.h"

class AYSLongboatSeat;

/**
 *  Chaloupe : tout petit bateau pilotable lancé par la chalouperie d'un bateau pirate, 2 joueurs max (pilote à la barre de la
 *  chaloupe + un passager : E à côté, aussi depuis l'eau). Même pilotage que le bateau pirate (réglages Ship|Helm, plus nerveux).
 *  Personne ne peut en descendre avec E : foncer sur une île (eau trop peu profonde) ou sur un bateau éjecte les joueurs dessus et
 *  détruit la chaloupe ; sans pilote, elle disparaît après Empty Lifetime (le passager tombe à l'eau).
 *  Visuel (primitives) dans le Blueprint /Game/Placeable/Ship/BP_Longboat (Tools/EditorScripts/setup_ship_stations.py).
 */
UCLASS(meta=(PrioritizeCategories="Longboat Ship"))
class AYSLongboat : public AYSPirateShip
{
	GENERATED_BODY()

private:

	/** Place du passager (AYSLongboatSeat) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> PassengerStation;

protected:

	/** Contact avec un autre bateau : un point de la coque à moins de cette distance de son emprise */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Longboat", meta=(ClampMin="0", Units="Centimeters"))
	float ShipContactMargin = 20.0f;

	/** Pas de contact avec les bateaux pendant ce délai après l'apparition (lancée juste derrière son bateau) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Longboat", meta=(ClampMin="0", Units="s"))
	float ShipContactGraceTime = 0.5f;

	/** Éjection sur un bateau : posé sur son pont, à cette distance au moins du bord de sa coque */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Longboat", meta=(ClampMin="0", Units="Centimeters"))
	float ShipLandingInset = 250.0f;

	/** Éjection sur une île : sol hors de l'eau cherché devant la proue, par pas de cette longueur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Longboat", meta=(ClampMin="10", Units="Centimeters"))
	float LandSearchStep = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Longboat", meta=(ClampMin="1"))
	int32 LandSearchSteps = 25;

	/** Écart entre les deux joueurs éjectés */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Longboat", meta=(ClampMin="0", Units="Centimeters"))
	float EjectSpacing = 120.0f;

	/** Sans pilote (mort, déconnecté), la chaloupe disparaît après ce délai */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Longboat", meta=(ClampMin="0", Units="s"))
	float EmptyLifetime = 2.0f;

	/** Touche le fond : éjection sur l'île devant */
	virtual void OnHullBlocked() override;

	/** Sans pilote, elle disparaît après Empty Lifetime (faux pour le NME Jolly Ship) */
	virtual bool DisappearsWithoutPilot() const { return true; }

	/** Joueurs reposés autour de Location (tournés vers Facing), chaloupe détruite (serveur) */
	virtual void EjectAndDestroy(const FVector& Location, const FVector& Facing, const TCHAR* Reason);

	UChildActorComponent* GetPassengerStationComponent() const { return PassengerStation; }

	bool bEjected = false;

public:

	AYSLongboat(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/** Bateau qui l'a lancée */
	void SetMotherShip(AYSPirateShip* Ship) { MotherShip = Ship; }

	AYSLongboatSeat* GetPassengerSeat() const;

	/** Joueurs à bord (pilote, passager) */
	TArray<AYSPlayerCharacter*> GetOccupants() const;

private:

	TWeakObjectPtr<AYSPirateShip> MotherShip;

	double SpawnTime = 0.0;
	double EmptySinceTime = -1.0;

	/** Contact avec un autre bateau (serveur), ou nullptr */
	AYSPirateShip* FindContactShip() const;

	/** Sol d'un bateau sous Location (pont, gaillard) ; faux si rien */
	bool FindShipLanding(const AYSPirateShip* Ship, FVector& OutLocation) const;

	/** Sol hors de l'eau devant la proue ; faux si rien */
	bool FindLandLanding(FVector& OutLocation) const;
};

/** Barre de la chaloupe : prise seulement en lançant la chaloupe (chalouperie), on ne la quitte pas avec E */
UCLASS(meta=(PrioritizeCategories="Station Interaction"))
class AYSLongboatHelm : public AYSShipHelm
{
	GENERATED_BODY()

public:

	AYSLongboatHelm();

	virtual bool CanInteract(const AYSPlayerCharacter* Character) const override { return false; }

protected:

	virtual const TCHAR* GetLogTag() const override { return TEXT("[YSLongboat]"); }
};

/** Place du passager de la chaloupe : E à côté (aussi en nageant) quand elle a un pilote ; on ne la quitte pas avec E */
UCLASS(meta=(PrioritizeCategories="Station Interaction"))
class AYSLongboatSeat : public AYSShipStation
{
	GENERATED_BODY()

public:

	AYSLongboatSeat();

	virtual bool CanInteract(const AYSPlayerCharacter* Character) const override;

protected:

	virtual const TCHAR* GetLogTag() const override { return TEXT("[YSLongboat]"); }
};
