#pragma once

#include "CoreMinimal.h"
#include "NPC/YSBrookerNPC.h"
#include "YSMedicNPC.generated.h"

/**
 *  « Medic » : PNJ comme le Brooker (zone au sol, bulle d'indication), mais il ne rachète rien : un cadavre de joueur reposé
 *  dans sa zone (AcceptRange) ressuscite ce joueur (AYSGameMode::RevivePlayer : même personnage, même set d'arme, mêmes
 *  upgrades et shards), là où le cadavre a été posé (à côté du Medic s'il est dans l'eau). La bulle s'affiche quand le joueur
 *  local porte un cadavre.
 *  Classe de base du Blueprint BP_NPC_Medic (copie de BP_NPC_Brooker en vert).
 */
UCLASS(meta=(PrioritizeCategories="Brooker Medic"))
class AYSMedicNPC : public AYSBrookerNPC
{
	GENERATED_BODY()

protected:

	/** Part des PV max du joueur ressuscité */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Medic", meta=(ClampMin="0.01", ClampMax="1"))
	float ReviveHealthRatio = 1.0f;

	/** Cadavre posé dans l'eau (ou sans sol) : le joueur réapparaît à cette distance du Medic, du côté du cadavre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Medic", meta=(ClampMin="0", Units="Centimeters"))
	float ReviveDistance = 200.0f;

public:

	AYSMedicNPC();

	virtual bool AcceptsObject(const AYSInteractiveObject* Object) const override;

	virtual bool IsBrooker() const override { return false; }

protected:

	virtual void ReceiveObject(AYSInteractiveObject* Object) override;

	virtual const TCHAR* GetLogTag() const override { return TEXT("[YSMedic]"); }
};
