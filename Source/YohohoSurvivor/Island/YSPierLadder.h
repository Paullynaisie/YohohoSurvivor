#pragma once

#include "CoreMinimal.h"
#include "Ship/YSShipBoarding.h"
#include "YSPierLadder.generated.h"

/**
 *  Échelle de corde contre le bord d'un ponton (quai de la plage : aux deux bouts, côté large ; FYSIslandGenerationRules::bBeachQuayLadders) :
 *  un joueur (ou un HiredMouss) qui nage à côté appuie sur E et est reposé sur le ponton. Même échelle et même zone d'accès que celles des
 *  bateaux (AYSShipBoarding : Access Reach / Half Width x Access Range Multiplier), sans bateau. Origine = bord du ponton au niveau de la mer,
 *  +X vers le large. Créée localement par l'île sur chaque machine (comme le ponton) : pas répliquée, le serveur utilise la sienne.
 */
UCLASS(meta=(PrioritizeCategories="Boarding Interaction"))
class AYSPierLadder : public AYSShipBoarding
{
	GENERATED_BODY()

public:

	AYSPierLadder();

	/** Hauteur du dessus du ponton au-dessus de l'origine (niveau de la mer) : haut de l'échelle et point d'arrivée (avant BeginPlay) */
	void SetupPier(float DeckHeight, float LandingInset);

	/** Personnage vivant, à aucun poste, qui nage (Allow Walkers : aussi à pied, sous le dessus du ponton) */
	virtual bool CanInteract(const AYSPlayerCharacter* Character) const override;
};
