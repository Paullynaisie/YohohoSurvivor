#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSAbilityComponent.h"
#include "YSBombThrowerAbility.generated.h"

class AYSPlayerBomb;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 *  « BombThrower » : Ability du Sniper (Gun). Lance une bombe (Bomb Class : même Blueprint que la Roll and Bomb, donc même rayon
 *  d'explosion) en cloche vers le curseur ; elle met Flight Duration à arriver et explose en touchant le sol (pas de mèche).
 *  Point d'impact (serveur) : le curseur, ramené à Max Throw Distance du joueur ; il doit être sur l'île (seul le Landscape compte,
 *  les props sont traversés) et hors de l'eau. Sinon, on recule vers le joueur jusqu'à un sol valide ; aucun : lancée sur place.
 *  Dégâts : Bomb Damage x Damage bonus de l'Ability.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="BombThrower"))
class UYSBombThrowerAbility : public UYSAbilityComponent
{
	GENERATED_BODY()

public:

	/** Blueprint de la bombe lancée (/Game/Placeable/AMMO/BP_CannoneerBomb) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower")
	TSubclassOf<AYSPlayerBomb> BombClass;

	/** Dégâts de l'explosion à chaque NME touché (x Damage bonus de l'Ability) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower", meta=(ClampMin="0"))
	float BombDamage = 50.0f;

	/** Upgrade « Une Mine ! » : Blueprint lancé à la place de la bombe (/Game/Placeable/AMMO/BP_CannoneerMine) ; mêmes dégâts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower")
	TSubclassOf<AYSPlayerBomb> MineClass;

	/** Durée du vol jusqu'au point d'impact */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw", meta=(ClampMin="0.05", Units="Seconds"))
	float FlightDuration = 1.0f;

	/** Hauteur du sommet de la cloche au-dessus du plus haut des deux points */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw", meta=(ClampMin="1", Units="Centimeters"))
	float ApexHeight = 300.0f;

	/** Bombes lancées en même temps : la première vers le curseur, les autres de part et d'autre en arc de cercle autour du joueur (Shotgun : 3) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw", meta=(ClampMin="1"))
	int32 BombCount = 1;

	/** Angle entre deux bombes voisines de l'arc (vu depuis le joueur, même distance que la première) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw", meta=(ClampMin="0", ClampMax="180", Units="Degrees"))
	float BombSpreadAngle = 30.0f;

	/**
	 *  Écart minimum entre deux bombes / mines lancées ensemble (arc de Bomb Count, Golden Mine) : trop proches, l'angle de l'arc
	 *  s'ouvre (au plus : réparties sur tout le cercle) puis elles s'éloignent du joueur ; 0 = pas de minimum
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw", meta=(ClampMin="0", Units="Centimeters"))
	float MinBombSpacing = 350.0f;

	/** Golden Mine : mines en plus sur la ligne de la première (1 = une plus près ; 2 = une plus près et une plus loin, même écart) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw", meta=(ClampMin="0"))
	int32 GoldenMineCount = 2;

	/** Distance max du point d'impact au joueur (le curseur plus loin est ramené à cette distance) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw", meta=(ClampMin="0", Units="Centimeters"))
	float MaxThrowDistance = 1000.0f;

	/** Point d'impact invalide (eau, hors de l'île) : on recule vers le joueur de ce pas jusqu'à un sol valide */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw", meta=(ClampMin="10", Units="Centimeters"))
	float RetreatStep = 50.0f;

	/** Animation de lancer (la même que le NME bomb throw) ; décoché = la bombe part tout de suite de la bouche de l'arme */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw Anim")
	bool bPlayThrowAnim = true;

	/** Le lancer est fait par l'AUTRE main (celle posée sur l'Off Hand Socket de l'arme à deux mains) ; sinon par la main qui tient l'arme */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw Anim", meta=(EditCondition="bPlayThrowAnim"))
	bool bThrowWithOffHand = true;

	/** Phase 1 : la main recule et monte (pose de préparation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw Anim", meta=(ClampMin="0.01", Units="Seconds", EditCondition="bPlayThrowAnim"))
	float ThrowWindupDuration = 0.3f;

	/** Phase 2 : la main part vers l'avant ; la bombe quitte la main à la fin de cette phase */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw Anim", meta=(ClampMin="0.01", Units="Seconds", EditCondition="bPlayThrowAnim"))
	float ThrowDuration = 0.12f;

	/** Phase 3 : retour à la pose normale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw Anim", meta=(ClampMin="0.01", Units="Seconds", EditCondition="bPlayThrowAnim"))
	float ThrowRecoverDuration = 0.35f;

	/** Pose de préparation (main droite, relative au corps ; X avant, Y droite, Z haut ; symétrique pour la main gauche) : derrière l'épaule */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw Anim", meta=(EditCondition="bPlayThrowAnim"))
	FVector WindupHandLocation = FVector(-40.0f, 45.0f, 50.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw Anim", meta=(EditCondition="bPlayThrowAnim"))
	FRotator WindupHandRotation = FRotator(-30.0f, 0.0f, 0.0f);

	/** Pose au lâcher : bras tendu devant, en hauteur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw Anim", meta=(EditCondition="bPlayThrowAnim"))
	FVector ReleaseHandLocation = FVector(70.0f, 30.0f, 40.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw Anim", meta=(EditCondition="bPlayThrowAnim"))
	FRotator ReleaseHandRotation = FRotator(20.0f, 0.0f, 0.0f);

	/** Courbes : préparation EaseOut, lancer EaseOut marqué (cet exposant), retour EaseInOut */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Throw Anim", meta=(ClampMin="1", EditCondition="bPlayThrowAnim"))
	float ThrowEaseExponent = 3.0f;

	/** Bombe affichée dans la main qui lance, pendant la préparation et le lancer (visuel local, rien de répliqué) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Held Bomb", meta=(EditCondition="bPlayThrowAnim"))
	bool bShowHeldBomb = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Held Bomb", meta=(EditCondition="bShowHeldBomb"))
	TObjectPtr<UStaticMesh> HeldBombMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Held Bomb", meta=(EditCondition="bShowHeldBomb"))
	TObjectPtr<UMaterialInterface> HeldBombMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Held Bomb", meta=(EditCondition="bShowHeldBomb"))
	FVector HeldBombScale = FVector(0.3f);

	/** Position de la bombe par rapport à la main */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BombThrower|Held Bomb", meta=(EditCondition="bShowHeldBomb"))
	FVector HeldBombOffset = FVector::ZeroVector;

	UYSBombThrowerAbility();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual float GetBaseDamage() const override { return BombDamage; }

	/** CD reduction x Mine Cooldown Multiplier de l'upgrade « Une Mine ! » */
	virtual float GetCooldownMultiplier() const override;

	/** Préparation + lancer : les actions moins prioritaires (Auto shot) attendent le lâcher */
	virtual float GetActionDuration() const override { return bPlayThrowAnim ? ThrowWindupDuration + ThrowDuration : 0.0f; }

	virtual bool EvaluateHandPose(float& OutIdleWeight, FVector& OutLocation, FQuat& OutRotation) const override;

	virtual bool AnimatesOffHand() const override;

	/** Point d'impact pour un curseur en TargetLocation (sol valide le plus proche du curseur vers le joueur, sinon sous le joueur) */
	FVector FindLandingPoint(const FVector& TargetLocation) const;

protected:

	/** Lance la bombe (serveur) : au lâcher de l'animation, vers le point visé au clic */
	virtual void ExecuteAbility(const FVector& TargetLocation) override;

	virtual void OnActionCancelled() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Fait partir la bombe vers Landing (serveur) */
	void ReleaseBomb(FVector Landing, TArray<FVector> ExtraLandings);

	/** Main du porteur qui lance (l'autre main si bThrowWithOffHand et arme à deux mains) */
	USceneComponent* GetThrowingHand() const;

	/** Bombe dans la main (créée à la première animation) */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HeldBomb;

	FTimerHandle ReleaseTimer;
};
