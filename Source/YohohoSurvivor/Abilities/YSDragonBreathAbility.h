#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSCoupDPetoirAbility.h"
#include "YSDragonBreathAbility.generated.h"

/**
 *  « Dragon breath » : Ability du Shotgun du Harquebusier. Un tir en éventail (comme l'attaque auto : AMMO réparties à angles
 *  égaux, Spread Ammo Count 7) de DragonBreathAMMO, qui traversent tous les NME et les bumpent (réglages de l'AMMO).
 *  Recul de 0.25 s (Recoil Kick + Recover Duration), puis la recharge de l'attaque auto est rendue (Auto Atk Reset).
 *  Pas de Ricochet (exclu dans l'upgrade).
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Dragon breath"))
class UYSDragonBreathAbility : public UYSCoupDPetoirAbility
{
	GENERATED_BODY()

public:

	UYSDragonBreathAbility();
};
