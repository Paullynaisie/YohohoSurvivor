#pragma once

#include "CoreMinimal.h"
#include "Interaction/YSInteractiveObject.h"
#include "YSShipHelm.generated.h"

class AYSPirateShip;

/**
 *  Barre d'un bateau pirate pilotable (Ship|Helm du bateau, acteur enfant créé par son composant HelmStation) : touche E à côté
 *  -> le joueur prend la barre (placé sur PilotPoint, tourné vers la proue, mains sur LeftHandPoint / RightHandPoint, armes
 *  rangées, plus de clic), ZQSD pilote le bateau ; E à nouveau la quitte. Un seul pilote à la fois. Origine = centre de la roue.
 *  Serveur autoritaire ; pilote répliqué.
 */
UCLASS(meta=(PrioritizeCategories="Interaction"))
class AYSShipHelm : public AYSInteractiveObject
{
	GENERATED_BODY()

private:

	/** Où se tient le pilote (ses pieds), tourné vers +X de la barre */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> PilotPoint;

	/** Joueur à la barre (nullptr = personne) */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Interaction", meta=(AllowPrivateAccess="true"))
	TObjectPtr<AYSPlayerCharacter> Pilot;

public:

	AYSShipHelm();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void Tick(float DeltaSeconds) override;

	/** Bateau pilotable, personne à la barre, joueur vivant aux mains libres et hors de l'eau */
	virtual bool CanInteract(const AYSPlayerCharacter* Character) const override;

	/** Le joueur prend la barre (serveur) */
	virtual void Interact(AYSPlayerCharacter* Character) override;

	/** Pilote (serveur, appelé par le personnage qui prend / quitte la barre) */
	void SetPilot(AYSPlayerCharacter* NewPilot);

	AYSPlayerCharacter* GetPilot() const { return Pilot; }

	/** Bateau qui porte la barre */
	AYSPirateShip* GetShip() const;

	/** Position du pilote (ses pieds) et direction de son regard */
	FVector GetPilotFeetLocation() const;
	FVector GetPilotForward() const;
};
