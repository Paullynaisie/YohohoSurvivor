#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSMoveActionAbility.h"
#include "YSCamouflageAbility.generated.h"

/**
 *  « Camouflage » (MoveAction maintenue, demande de Paul du 2026-11-13) : invisible aux NME (ils changent de cible et l'ignorent :
 *  AYSFoeCharacter::FindNearestPlayer / IsTargetStillValid) pendant Min Duration (1 s, Stamina Cost 60), puis tant qu'Espace est tenu
 *  (Stamina Per Second 10). Déplacement normal. Attaquer ou lancer une Ability y met fin. Personnage translucide (AYSPlayerCharacter,
 *  catégorie 3C|Camouflage : Camouflage Opacity 0.2). Camouflage ++ : coût d'activation - Bonus Gold Value (10) chacun (60 -> 10).
 *  Golden (ordre des variantes de DA_Up_MoveAction_Camouflage) :
 *  0 Like a Shadow : intangible et invulnérable pendant le camouflage, mais consommation x Like A Shadow Cost Multiplier (2) ;
 *  1 Sucker Punch : y mettre fin en attaquant bumpe sur place les NME touchés par cette attaque ;
 *  2 Thief : camouflage possible en portant un objet, et le port ne consomme plus de stamina pendant le camouflage ;
 *  3 Shadow strike : y mettre fin en attaquant fait cette attaque comme à Shadow Strike Shard Level (20) / 30 en Shoot system
 *     (arme à distance) ou Melee Range (arme de mêlée), puis l'arme reprend ses réglages.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Camouflage"))
class UYSCamouflageAbility : public UYSSustainedMoveActionAbility
{
	GENERATED_BODY()

public:

	/** Like a Shadow : activation et maintien x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camouflage|Golden", meta=(ClampMin="0"))
	float LikeAShadowCostMultiplier = 2.0f;

	/** Sucker Punch : les coups de l'attaque bumpent pendant sa durée + ce délai (balles en vol) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camouflage|Golden", meta=(ClampMin="0", Units="Seconds"))
	float SuckerPunchExtraWindow = 0.5f;

	/** Sucker Punch : bump sur place (hauteur, durée, tours) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camouflage|Golden", meta=(ClampMin="0", Units="Centimeters"))
	float SuckerPunchApexHeight = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camouflage|Golden", meta=(ClampMin="0.05", Units="Seconds"))
	float SuckerPunchBumpDuration = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camouflage|Golden")
	float SuckerPunchSpinTurns = 1.0f;

	/** Sucker Punch : les élites aussi (ils tombent sur place) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camouflage|Golden")
	bool bSuckerPunchElites = true;

	/** Shadow strike : niveau de shard de l'attaque (sur 30) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camouflage|Golden", meta=(ClampMin="0", ClampMax="30"))
	int32 ShadowStrikeShardLevel = 20;

	/** Shadow strike : le niveau de shard dure au moins ce temps (puis toute l'attaque, + 0.1 s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camouflage|Golden", meta=(ClampMin="0", Units="Seconds"))
	float ShadowStrikeMinDuration = 0.3f;

	/** Index des variantes Golden (ordre de l'asset DA_Up_MoveAction_Camouflage) */
	static constexpr int32 GoldenLikeAShadow = 0;
	static constexpr int32 GoldenSuckerPunch = 1;
	static constexpr int32 GoldenThief = 2;
	static constexpr int32 GoldenShadowStrike = 3;

	UYSCamouflageAbility();

	virtual float GetActivationStaminaCost() const override;

	virtual float GetMoveActionBonusDisplayValue(float BonusTotal) const override { return FMath::Max(0.0f, StaminaCost - BonusTotal); }

	virtual bool IsHidingHolder() const override { return IsSustained(); }

	/** Like a Shadow */
	virtual bool IsGrantingInvulnerability() const override { return IsSustained() && GetMoveActionGolden() == GoldenLikeAShadow; }

	/** Thief */
	virtual bool AllowsCarrying() const override { return GetMoveActionGolden() == GoldenThief; }

	virtual bool SuppressesCarryStaminaCost() const override { return AllowsCarrying() && IsSustained(); }

	virtual bool OnHolderDealtDamage(AYSFoeCharacter* Foe) override;

protected:

	virtual float GetSustainCostMultiplier() const override { return GetMoveActionGolden() == GoldenLikeAShadow ? LikeAShadowCostMultiplier : 1.0f; }

	virtual void OnSustainStarted() override;

	virtual void OnSustainStopped() override;

	virtual void OnCancelledByAttack(UYSAbilityComponent* Action, float Consumed) override;

	virtual const TCHAR* GetLogVerb() const override { return TEXT("se camoufle"); }

private:

	/** Sucker Punch : fin de la fenêtre où les coups bumpent (temps serveur local) */
	double SuckerPunchEndTime = -1.0e6;

	/** Like a Shadow : capsule qui traverse les personnages (cette machine) */
	bool bPassThroughApplied = false;
};
