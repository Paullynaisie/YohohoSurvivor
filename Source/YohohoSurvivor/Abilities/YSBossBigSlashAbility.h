#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSDashingSlashAbility.h"
#include "YSBossBigSlashAbility.generated.h"

class UMaterialInterface;

/**
 *  « Boss Big Slash » : attaque en plus d'un élite tant que son bouclier de boss est actif (AYSFoeCharacter, Foe|Boss).
 *  Même Dashing Slash que celui du Swordsman (animation, poses, hitbox de l'arme ; réglages copiés de BP_BattleSword),
 *  porté par l'arme du NME (BP_NME_EliteGreatSword). Plus prioritaire que l'attaque de base de l'arme (Ability > Auto atk).
 *  Lancée par le NME (serveur) quand sa cible est à moins de Trigger Range et que l'action est rechargée :
 *  1. préparation (Step Back Duration, 2 s) : immobile, direction figée vers la cible, zone rouge au sol sur toute la
 *     longueur du dash (Warning*) ;
 *  2. dash (Dash Duration) de Dash Distance vers la cible, arrivée jamais dans l'eau (reculée vers le NME par pas de
 *     Water Retreat Step) ; la hitbox de l'arme frappe pendant tout le dash (coupe).
 *  Le mouvement est calculé par le NME (FYSFoeBossDash répliqué, même trajet sur toutes les machines).
 *  Recharge = Cooldown, comptée du début d'une attaque au début de la suivante.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Boss Big Slash"))
class UYSBossBigSlashAbility : public UYSDashingSlashAbility
{
	GENERATED_BODY()

public:

	/** Lancée quand la cible du NME est à moins de cette distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss Big Slash", meta=(ClampMin="0", Units="Centimeters"))
	float TriggerRange = 1000.0f;

	/** Distance parcourue par le dash (raccourcie si l'arrivée est dans l'eau) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss Big Slash", meta=(ClampMin="0", Units="Centimeters"))
	float DashDistance = 3000.0f;

	/** Arrivée dans l'eau : reculée vers le NME par pas de cette distance jusqu'à un sol hors de l'eau */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss Big Slash", meta=(ClampMin="10", Units="Centimeters"))
	float WaterRetreatStep = 50.0f;

	/** Dégâts de chaque coup = dégâts de contact du NME (niveau, élite) x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss Big Slash", meta=(ClampMin="0"))
	float ContactDamageMultiplier = 2.0f;

	/** Zone au sol pendant la préparation : largeur (centrée sur le trajet) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss Big Slash|Warning", meta=(ClampMin="0", Units="Centimeters"))
	float WarningWidth = 400.0f;

	/** Longueur ajoutée au bout du trajet (portée de la lame à l'arrivée) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss Big Slash|Warning", meta=(ClampMin="0", Units="Centimeters"))
	float WarningExtraLength = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss Big Slash|Warning")
	FLinearColor WarningColor = FLinearColor(1.0f, 0.05f, 0.02f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss Big Slash|Warning", meta=(ClampMin="0", ClampMax="1"))
	float WarningOpacity = 0.4f;

	/** Matériau de la zone (M_BrookerZone : paramètres Color, Glow, FillOpacity, EdgeOpacity, Radius, EdgeWidth) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss Big Slash|Warning")
	TObjectPtr<UMaterialInterface> WarningMaterial;

public:

	UYSBossBigSlashAbility();

	/** Recharge comptée du début d'une attaque au début de la suivante */
	virtual float GetCooldownDuration() const override { return FMath::Max(Cooldown, GetActionDuration()); }

	virtual float GetBaseCooldown() const override { return Cooldown; }

	/** Part de Dash Distance parcourue à Alpha (0 à 1) du dash : vitesse qui décroît de Dash Speed Multiplier à Dash End Speed Multiplier */
	float GetDashDistanceFraction(float Alpha) const;

	/** Arrivée du dash (serveur) : Dash Distance dans Direction, reculée tant qu'elle est dans l'eau ; pieds au sol + HalfHeight */
	FVector FindDashEnd(const FVector& Start, const FVector& Direction, float HalfHeight, const AActor* IgnoredActor) const;

#if WITH_EDITOR
	virtual void GetUnusedProperties(TArray<FName>& OutNames) const override;
#endif
};
