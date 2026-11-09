#pragma once

#include "CoreMinimal.h"
#include "Ship/YSShipStation.h"
#include "YSShipHelm.generated.h"

/**
 *  Barre d'un bateau pirate pilotable (Ship|Helm du bateau, acteur enfant créé par son composant HelmStation) : touche E à côté
 *  -> le joueur prend la barre (placé sur OccupantPoint, tourné vers la proue, mains sur LeftHandPoint / RightHandPoint, armes
 *  rangées, plus de clic), ZQSD pilote le bateau ; E à nouveau la quitte. Un seul pilote à la fois. Origine = centre de la roue.
 *  Caméra du pilote : catégorie Station|Camera (centrée sur le bateau, dézoomée).
 */
UCLASS(meta=(PrioritizeCategories="Station Interaction"))
class AYSShipHelm : public AYSShipStation
{
	GENERATED_BODY()

public:

	AYSShipHelm();

	/** Bateau pilotable et barre libre (poste) */
	virtual bool CanInteract(const AYSPlayerCharacter* Character) const override;

	virtual bool UsesPilotInput() const override { return true; }

	/** ZQSD -> commandes du bateau (Z / S = avancer / reculer, D / Q = tourner) */
	virtual void ApplyPilotInput(AYSPlayerCharacter* Character, const FVector2D& Input) override;

	AYSPlayerCharacter* GetPilot() const { return GetOccupant(); }

	/** Serveur : « À l'abordage » tant que le pilote maintient le clic gauche (barre pas verrouillée) */
	virtual void Tick(float DeltaSeconds) override;

	/** Icône « À l'abordage » en bas de l'écran (vide = pas d'icône) */
	virtual bool GetActionIcon(UTexture2D*& OutIcon, FLinearColor& OutTint, float& OutRemaining, float& OutDuration) const override;

protected:

	/** Icône de l'action « À l'abordage » (clic gauche maintenu) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Helm")
	TObjectPtr<UTexture2D> RamIcon;


	/** Personne à la barre : le bateau garde son élan et ralentit seul */
	virtual void OnOccupantChanged(AYSPlayerCharacter* Previous) override;

	virtual const TCHAR* GetLogTag() const override { return TEXT("[YSHelm]"); }
};
