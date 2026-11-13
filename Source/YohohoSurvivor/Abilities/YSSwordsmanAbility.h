#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSMarkForDeleteAbility.h"
#include "YSSwordsmanAbility.generated.h"

class UYSGroundZoneComponent;
class UMaterialInterface;

/** Ability de l'épée du Swordsman, selon ses upgrades d'arme */
UENUM()
enum class EYSSwordsmanMode : uint8
{
	/** Dashing Slash (ou Mark for Delete, avec la Golden de The Gun was better) */
	Dash,
	BladeThrow,
	BladeMastery,
};

/**
 *  Ability de l'épée du Swordsman (BP_BattleSword, BP_GunSword ; 2026-11-13) : Dashing Slash / Mark for Delete (classe parente),
 *  remplacée par une upgrade d'arme (propriétés d'arme, lues par le serveur et répliquées par Mode ; si plusieurs : Blade Mastery,
 *  puis Blade Throw, choix de Claude) :
 *  - Blade Throw : pas en arrière de Step Back Duration x Blade Throw Step Back Multiplier (2) à l'opposé du curseur, avec une
 *    prévisu au sol (cercle de Critical Zone Radius) au point visé (curseur au clic, à Blade Throw Max Range au plus) ; à la fin,
 *    2 Lesser Spinning Blades en courbes symétriques (apex Blade Throw Apex à droite et à gauche) qui se croisent au point visé
 *    en Blade Throw Duration, puis reviennent au départ par la courbe de l'autre (Swordsrang : 2x plus lentes, puis vers le joueur).
 *    Critiques dans la zone. Recharge Blade Throw Cooldown (12 s).
 *  - Blade Mastery : 1 level de Self Spinning Sword (Bladeguedon : au level max, elle disparaît et Bladeguedon Count Lesser
 *    Spinning Blades partent en parallèle devant le joueur). Recharge Blade Mastery Cooldown (5 s).
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Swordsman Ability"))
class UYSSwordsmanAbility : public UYSMarkForDeleteAbility
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blade Throw", meta=(ClampMin="0", Units="Seconds"))
	float BladeThrowCooldown = 12.0f;

	/** Pas en arrière : durée de celui du Dashing Slash x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blade Throw", meta=(ClampMin="0"))
	float BladeThrowStepBackMultiplier = 2.0f;

	/** Point de croisement : le curseur au clic, à cette distance au plus */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blade Throw", meta=(ClampMin="100", Units="Centimeters"))
	float BladeThrowMaxRange = 2000.0f;

	/** Distance minimale du point de croisement (curseur sur le joueur) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blade Throw", meta=(ClampMin="0", Units="Centimeters"))
	float BladeThrowMinRange = 300.0f;

	/** Écart max des courbes à droite et à gauche */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blade Throw", meta=(ClampMin="0", Units="Centimeters"))
	float BladeThrowApex = 500.0f;

	/** Aller (jusqu'au croisement) et retour, chacun */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blade Throw", meta=(ClampMin="0.05", Units="Seconds"))
	float BladeThrowDuration = 1.0f;

	/** Swordsrang : durées x ce multiplicateur (2 = 2x moins vite) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blade Throw", meta=(ClampMin="0.1"))
	float SwordsrangDurationMultiplier = 2.0f;

	/** Zone critique autour du point de croisement (et rayon de la prévisu) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blade Throw", meta=(ClampMin="0", Units="Centimeters"))
	float CriticalZoneRadius = 300.0f;

	/** Prévisu : matériau (paramètres de M_BrookerZone) et couleur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blade Throw|Preview")
	TObjectPtr<UMaterialInterface> PreviewMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blade Throw|Preview")
	FLinearColor PreviewColor = FLinearColor(1.0f, 0.2f, 0.1f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blade Throw|Preview", meta=(ClampMin="0", ClampMax="1"))
	float PreviewFillOpacity = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blade Throw|Preview", meta=(ClampMin="0", ClampMax="1"))
	float PreviewEdgeOpacity = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blade Mastery", meta=(ClampMin="0", Units="Seconds"))
	float BladeMasteryCooldown = 5.0f;

	/** Bladeguedon : lames lancées, écart entre elles, portée et durée du trajet */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blade Mastery", meta=(ClampMin="1"))
	int32 BladeguedonCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blade Mastery", meta=(ClampMin="0", Units="Centimeters"))
	float BladeguedonSpacing = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blade Mastery", meta=(ClampMin="100", Units="Centimeters"))
	float BladeguedonRange = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blade Mastery", meta=(ClampMin="0.05", Units="Seconds"))
	float BladeguedonDuration = 1.0f;

	UYSSwordsmanAbility();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	EYSSwordsmanMode GetMode() const { return Mode; }

	virtual float GetCooldownDuration() const override;
	virtual float GetBaseCooldown() const override;
	virtual float GetActionDuration() const override;
	virtual float GetSwingElapsed() const override;
	virtual bool IsGrantingInvulnerability() const override;
	virtual float GetMoveSpeedMultiplier() const override;
	virtual bool GetForcedMoveDirection(FVector& OutDirection) const override;
	virtual bool OverridesMoveInput() const override;
	virtual bool GetLockedAimDirection(FVector& OutDirection) const override;
	virtual bool GetBodyRoll(float& OutAngle, float& OutHeightOffset) const override;

protected:

	virtual void OnActivationPredicted(const FVector& TargetLocation) override;
	virtual void ExecuteAbility(const FVector& TargetLocation) override;
	virtual void OnActionCancelled() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	UPROPERTY(Replicated)
	EYSSwordsmanMode Mode = EYSSwordsmanMode::Dash;

	float GetThrowStepBackDuration() const { return StepBackDuration * BladeThrowStepBackMultiplier; }

	/** Blade Throw : pas en arrière en cours (alpha 0..1) */
	bool IsInThrowStepBack(float& OutAlpha) const;

	/** Point de croisement d'après le point visé (à plat, Min / Max Range) */
	FVector ComputeThrowTarget(const FVector& TargetLocation) const;

	/** Serveur : les 2 lames */
	void Throw();

	/** Serveur : Blade Mastery */
	void DoBladeMastery();

	/** Prévisu locale (joueur qui lance) */
	void ShowPreview(const FVector& Location);
	void HidePreview();

	UPROPERTY(Transient)
	TObjectPtr<UYSGroundZoneComponent> PreviewZone;

	FVector ThrowTarget = FVector::ZeroVector;
	bool bThrowPending = false;
};
