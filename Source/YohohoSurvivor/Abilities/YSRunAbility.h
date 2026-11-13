#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSMoveActionAbility.h"
#include "YSRunAbility.generated.h"

/**
 *  « Run » (MoveAction maintenue, demande de Paul du 2026-11-13) : Stamina Cost (20) = Min Duration (1 s) de course, puis
 *  Stamina Per Second (10) tant qu'Espace est tenu ; vitesse x (Run Speed Multiplier + bonus des ++). Attaquer ou lancer une
 *  Ability arrête la course. Run ++ (UYSMoveActionUpgrade) : + Bonus Gold Value (0.1) de vitesse chacun (x1.5 -> x2).
 *  Golden (ordre des variantes de DA_Up_MoveAction_Run) :
 *  0 Free running : intangible pendant la course (traverse les NME et les joueurs) ;
 *  1 Fenwick : course possible en portant un objet, et le port ne consomme plus de stamina pendant la course ;
 *  2 Run and Hit : arrêter la course en attaquant ajoute la stamina dépensée aux dégâts de base de l'attaque ;
 *  3 Impulsion : la première seconde est remplacée par un dash (Impulse Duration, intangible et invulnérable), coût Impulse Stamina Cost.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Run"))
class UYSRunAbility : public UYSSustainedMoveActionAbility
{
	GENERATED_BODY()

public:

	/** Vitesse de déplacement x ce multiplicateur pendant la course (1.5 = +50 % ; + bonus des Run ++) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Run", meta=(ClampMin="0"))
	float RunSpeedMultiplier = 1.5f;

	/** Impulsion (Golden) : coût de l'activation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Run|Golden", meta=(ClampMin="0"))
	float ImpulseStaminaCost = 40.0f;

	/** Impulsion : durée du dash (remplace la première seconde de course) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Run|Golden", meta=(ClampMin="0.05", Units="Seconds"))
	float ImpulseDuration = 0.25f;

	/** Impulsion : vitesse x ce multiplicateur pendant le dash */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Run|Golden", meta=(ClampMin="0"))
	float ImpulseSpeedMultiplier = 3.0f;

	/** Index des variantes Golden (ordre de l'asset DA_Up_MoveAction_Run) */
	static constexpr int32 GoldenFreeRunning = 0;
	static constexpr int32 GoldenFenwick = 1;
	static constexpr int32 GoldenRunAndHit = 2;
	static constexpr int32 GoldenImpulse = 3;

	UYSRunAbility();

	/** Pendant la course : x Run Speed Multiplier (+ ++) ; Impulsion : x Impulse Speed Multiplier pendant le dash */
	virtual float GetMoveSpeedMultiplier() const override;

	virtual float GetActivationStaminaCost() const override;

	virtual float GetMoveActionBonusDisplayValue(float BonusTotal) const override { return RunSpeedMultiplier + BonusTotal; }

	/** Impulsion : invulnérable pendant le dash */
	virtual bool IsGrantingInvulnerability() const override;

	/** Fenwick */
	virtual bool AllowsCarrying() const override { return GetMoveActionGolden() == GoldenFenwick; }

	virtual bool SuppressesCarryStaminaCost() const override { return AllowsCarrying() && IsSustained(); }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category="Run")
	bool IsRunning() const { return IsSustained(); }

protected:

	virtual float GetMinDuration() const override;

	virtual void OnSustainStarted() override;

	virtual void OnSustainStopped() override;

	virtual void OnCancelledByAttack(UYSAbilityComponent* Action, float Consumed) override;

	virtual const TCHAR* GetLogVerb() const override { return TEXT("court"); }

private:

	/** Free running / Impulsion : capsule qui traverse les personnages (cette machine) */
	bool bPassThroughApplied = false;

	/** Le personnage traverse les autres en ce moment (Free running : toute la course ; Impulsion : le dash) */
	bool WantsPassThrough() const;

	bool IsImpulseDashing() const;
};
