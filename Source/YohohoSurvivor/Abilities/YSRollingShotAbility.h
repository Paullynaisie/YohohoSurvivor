#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSCoupDPetoirAbility.h"
#include "YSRollingShotAbility.generated.h"

/**
 *  « Rolling shot » : Ability des pistolets (Gun, shoot system PewPew ; remplace le Coup d'Petoir, dont elle hérite :
 *  mêmes réglages de tir, et les mêmes upgrades s'y appliquent, Ricochet compris).
 *  À l'activation, interrompt les actions moins prioritaires des DEUX armes (BlocksAllWeapons), puis en 3 temps :
 *  1. Roulade (Roll Duration) : le joueur garde le contrôle, sa vitesse passe tout de suite à x Roll Speed Multiplier puis
 *     décroît jusqu'à la vitesse de la visée ; invulnérable, traverse les NME ; serveur et client qui active (prédiction).
 *     Animation : le corps fait des tours sur lui-même vers la direction de la roulade.
 *  2. Visée : Aim Duration immobile (vitesse x Aim Move Speed Multiplier), le personnage regarde toujours le curseur.
 *  3. Tir : le Coup d'Petoir (rafale PewPew) dans la direction du regard ; en akimbo, l'autre pistolet tire après Akimbo Fire Delay.
 *  La recharge (Cooldown, réduite par CD reduction) commence une fois les balles tirées.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Rolling shot"))
class UYSRollingShotAbility : public UYSCoupDPetoirAbility
{
	GENERATED_BODY()

public:

	/** Roulade : durée */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rolling Shot|Roll", meta=(ClampMin="0.01", Units="Seconds"))
	float RollDuration = 0.2f;

	/** Roulade : vitesse de déplacement x ce multiplicateur au départ, qui décroît jusqu'à celui de la visée en fin de roulade */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rolling Shot|Roll", meta=(ClampMin="0"))
	float RollSpeedMultiplier = 2.0f;

	/** Roulade sans touche de direction pressée : le personnage avance quand même, à l'opposé du curseur (direction prise au lancement) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rolling Shot|Roll")
	bool bForceMoveWithoutInput = true;

	/** Roulade : courbe de la décroissance (1 = linéaire ; plus grand = le bonus retombe plus vite au début) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rolling Shot|Roll", meta=(ClampMin="1"))
	float RollSpeedDecayExponent = 1.0f;

	/** Pendant la roulade, le personnage traverse les NME et les autres joueurs (sinon un NME devant l'arrête net) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rolling Shot|Roll")
	bool bRollThroughFoes = true;

	/** Invulnérable pendant la roulade, plus ce délai */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rolling Shot|Roll", meta=(ClampMin="0", Units="Seconds"))
	float ExtraInvulnerability = 0.0f;

	/** Visée : temps immobile avant le tir */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rolling Shot|Aim", meta=(ClampMin="0", Units="Seconds"))
	float AimDuration = 0.25f;

	/** Visée : vitesse de déplacement x ce multiplicateur (0 = immobile) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rolling Shot|Aim", meta=(ClampMin="0", ClampMax="1"))
	float AimMoveSpeedMultiplier = 0.0f;

	/** Animation de roulade : nombre de tours du corps (1 = 360°) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rolling Shot|Roll Anim")
	float RollAnimTurns = 1.0f;

	/** Animation de roulade : durée (0 = celle de la roulade) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rolling Shot|Roll Anim", meta=(ClampMin="0", Units="Seconds"))
	float RollAnimDuration = 0.0f;

	/** Animation de roulade : courbe EaseInOut de la rotation (1 = vitesse constante) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rolling Shot|Roll Anim", meta=(ClampMin="1"))
	float RollAnimEaseExponent = 1.5f;

	/** Animation de roulade : le corps descend de cette hauteur au milieu de la roulade (en boule) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rolling Shot|Roll Anim", meta=(Units="Centimeters"))
	float RollAnimTuckHeight = 40.0f;

public:

	UYSRollingShotAbility();

	/** Roulade + visée + rafale, puis la recharge (CD reduction : seulement la recharge) */
	virtual float GetCooldownDuration() const override { return RollDuration + AimDuration + GetBurstTime() + Cooldown * GetCooldownMultiplier(); }

	/** Priorités : en cours pendant la roulade, la visée et le recul des balles */
	virtual float GetActionDuration() const override { return RollDuration + AimDuration + Super::GetActionDuration(); }

	virtual bool BlocksAllWeapons() const override { return true; }

	virtual bool IsGrantingInvulnerability() const override;

	virtual float GetMoveSpeedMultiplier() const override;

	virtual bool GetBodyRoll(float& OutAngle, float& OutHeightOffset) const override;

	virtual bool GetForcedMoveDirection(FVector& OutDirection) const override;

	/** Secondes depuis le début de la dernière roulade vue par cette machine */
	float GetTimeSinceRoll() const;

protected:

	/** Client : roulade prédite */
	virtual void OnActivationPredicted(const FVector& TargetLocation) override;

	/** Serveur : roulade, puis le tir après la visée */
	virtual void ExecuteAbility(const FVector& TargetLocation) override;

	virtual void OnActionCancelled() override;

	/** Le tir vient après la roulade et la visée : la première balle n'est pas prédite */
	virtual bool PredictsFirstBullet() const override { return false; }

	/** Les balles partent dans la direction du regard (le curseur pendant la visée) */
	virtual FVector GetBurstTargetLocation(int32 BulletIndex) const override;

	/** Animation de roulade sur toutes les machines (le client qui l'a prédite ne la rejoue pas) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRollStarted();

	/** Lance la roulade (bonus de vitesse, traversée des NME) et note son début */
	void StartRoll();

private:

	/** Fin de la visée : rafale de cette arme, puis de l'autre en akimbo (serveur) */
	void Shoot();

	/** En akimbo : le Rolling shot de l'autre main si celle-ci est la principale */
	UYSRollingShotAbility* GetAkimboPartner() const;

	/** Début de la dernière roulade vue par cette machine (temps local) */
	double LastRollTime = -1.0e6;

	FTimerHandle ShootTimer;
	FTimerHandle PartnerTimer;
	FTimerHandle RollCollisionTimer;

	/** Direction de la roulade sans input (machine qui la joue : client local ou hôte) */
	FVector ForcedRollDirection = FVector::ZeroVector;
};
