#pragma once

#include "CoreMinimal.h"
#include "NPC/YSMedicNPC.h"
#include "YSShipCore.generated.h"

class UStaticMeshComponent;

/**
 *  Core d'un bateau pirate (acteur enfant CoreStation, au milieu du pont, local sur chaque machine) : définit les stats du bateau
 *  (PV, multiplicateurs de vitesse, d'accélération et de rotation, catégorie Core|Ship Stats) et porte une zone qui est à la fois
 *  Brooker (coffre reposé dedans = doublons) et Medic (cadavre reposé dedans = joueur ressuscité). Zone, bulle et résurrection :
 *  réglages Brooker / Medic hérités. Indisponible tant que le bateau est absent.
 */
UCLASS(meta=(PrioritizeCategories="Core Brooker Medic"))
class AYSShipCore : public AYSMedicNPC
{
	GENERATED_BODY()

private:

	/** Visuel du core (primitives, à retoucher dans le Blueprint) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> CoreBase;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> CoreOrb;

protected:

	/** PV du bateau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Core|Ship Stats", meta=(ClampMin="0"))
	float ShipMaxHealth = 5000.0f;

	/** Vitesses max (avant et arrière) du bateau x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Core|Ship Stats", meta=(ClampMin="0"))
	float SpeedMultiplier = 1.0f;

	/** Accélérations (Z / S) du bateau x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Core|Ship Stats", meta=(ClampMin="0"))
	float AccelerationMultiplier = 1.0f;

	/** Vitesse de rotation max du bateau x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Core|Ship Stats", meta=(ClampMin="0"))
	float TurnRateMultiplier = 1.0f;

	virtual void ReceiveObject(AYSInteractiveObject* Object) override;

	virtual const TCHAR* GetLogTag() const override { return TEXT("[YSShipCore]"); }

public:

	AYSShipCore();

	/** Coffre (valeur en doublons) ou cadavre */
	virtual bool AcceptsObject(const AYSInteractiveObject* Object) const override;

	virtual bool IsAvailable() const override;

	virtual bool IsBrooker() const override { return true; }

	float GetShipMaxHealth() const { return ShipMaxHealth; }
	float GetSpeedMultiplier() const { return SpeedMultiplier; }
	float GetAccelerationMultiplier() const { return AccelerationMultiplier; }
	float GetTurnRateMultiplier() const { return TurnRateMultiplier; }
};
