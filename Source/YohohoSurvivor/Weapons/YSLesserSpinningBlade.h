#pragma once

#include "CoreMinimal.h"
#include "Weapons/YSSpinningSword.h"
#include "YSLesserSpinningBlade.generated.h"

/** Trajet d'une Lesser Spinning Blade */
UENUM(BlueprintType)
enum class EYSBladePathMode : uint8
{
	/** Blade Throw : courbe jusqu'au point visé, puis retour au départ par la courbe de l'autre lame (ou vers le joueur : Swordsrang) */
	Curve,
	/** Bladeguedon : tout droit jusqu'à Target, puis disparaît */
	Straight,
};

USTRUCT(BlueprintType)
struct FYSBladePath
{
	GENERATED_BODY()

	UPROPERTY()
	EYSBladePathMode Mode = EYSBladePathMode::Curve;

	/** Départ et arrivée (au sol) */
	UPROPERTY()
	FVector_NetQuantize Start = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantize Target = FVector::ZeroVector;

	/** Courbe : côté (+1 droite, -1 gauche) et écart max de la courbe sur le côté (apex, cm) */
	UPROPERTY()
	float Side = 1.0f;

	UPROPERTY()
	float Apex = 500.0f;

	/** Aller et retour (s) */
	UPROPERTY()
	float OutDuration = 1.0f;

	UPROPERTY()
	float ReturnDuration = 1.0f;

	/** Swordsrang : après l'aller, vers le joueur (jusqu'à l'atteindre) au lieu du retour par l'autre courbe */
	UPROPERTY()
	bool bHomingReturn = false;

	/** Coups critiques à moins de ce rayon de Target (0 = jamais) */
	UPROPERTY()
	float CriticalRadius = 0.0f;
};

/**
 *  « Lesser Spinning Blade » (2026-11-13) : Spinning Sword level 1 seulement, qui tourne 4.5x plus vite (Rotation Speed 1620),
 *  la poignée collée au centre (Sword Location -35, 0, 0), qui suit un trajet (FYSBladePath) puis disparaît (pas de perte de level).
 *  Trajet calculé sur chaque machine depuis l'heure serveur de création ; dégâts côté serveur (ceux de la Spinning Sword,
 *  critiques dans la zone de Target). Classe utilisée : /Game/Placeable/Weapons/Player/BP_LesserSpinningBlade, sinon celle-ci.
 */
UCLASS(Blueprintable)
class AYSLesserSpinningBlade : public AYSSpinningSword
{
	GENERATED_BODY()

public:

	AYSLesserSpinningBlade();

	/** Serveur : lance une Lesser Spinning Blade de Character sur ce trajet */
	static AYSLesserSpinningBlade* Launch(AYSPlayerCharacter* Character, const FYSBladePath& Path);

	/** Swordsrang : distance au joueur à laquelle la lame disparaît */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SpinningSword|Lesser", meta=(ClampMin="0", Units="Centimeters"))
	float HomingArriveDistance = 100.0f;

	/** Swordsrang : écart d'angle max (bruit de Perlin propre à chaque lame) de la direction vers le joueur, atténué à l'approche */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SpinningSword|Lesser", meta=(ClampMin="0", ClampMax="89", Units="Degrees"))
	float HomingNoiseAngle = 45.0f;

	/** Swordsrang : fréquence du bruit (changements de direction par seconde, environ) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SpinningSword|Lesser", meta=(ClampMin="0"))
	float HomingNoiseFrequency = 1.5f;

	/** Swordsrang : le bruit s'efface en dessous de cette distance au joueur (pour l'atteindre) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SpinningSword|Lesser", meta=(ClampMin="1", Units="Centimeters"))
	float HomingNoiseFadeDistance = 400.0f;

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual bool IsCriticalAt(const FVector& TargetLocation) const override;

private:

	UPROPERTY(Replicated)
	FYSBladePath Path;

	/** Position au sol (2D) sur le trajet à Time secondes ; faux une fois le trajet fini (hors retour vers le joueur) */
	bool EvaluatePath(float Time, FVector& OutLocation) const;

	/** Swordsrang : position et vitesse du retour vers le joueur */
	bool bHoming = false;
	FVector HomingLocation = FVector::ZeroVector;
	float HomingSpeed = 0.0f;
};
