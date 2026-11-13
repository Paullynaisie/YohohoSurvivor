#pragma once

#include "CoreMinimal.h"
#include "Weapons/YSProjectile.h"
#include "YSBeamShot.generated.h"

/**
 *  BeamShot (upgrade d'arme du Gunslinger, 2026-11-13) : « BeamAMMO » tirée à la place des balles de l'Auto atk et de l'Ability
 *  des pistolets. Cylindre immobile qui part du canon, dans la direction du tir, et reste Life Time (0.1 s) : il touche chaque NME
 *  qui s'y trouve (ou y entre pendant ce temps) une fois, sans limite de NME (perte de dégâts multi-cibles du tireur, du plus
 *  proche au plus loin). Longueur et largeur x Size Multiplier (shoot system Explosif de l'arme : rayon des explosions).
 *  Mesh : cylindre de base du moteur (1 m de diamètre, 1 m de haut), couché le long du tir ; émissif de la couleur du joueur
 *  (Use Player Color). Le reste (dégâts, critiques, CircleBullet à l'impact...) : comme une balle (AYSProjectile).
 */
UCLASS(meta=(PrioritizeCategories="AMMO"))
class AYSBeamShot : public AYSProjectile
{
	GENERATED_BODY()

public:

	AYSBeamShot();

	/** Longueur du rayon depuis le canon (avant Size Multiplier) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Beam", meta=(ClampMin="1", Units="Centimeters"))
	float BeamLength = 500.0f;

	/** Épaisseur (diamètre) du rayon, avant Size Multiplier : 35 cm = échelle 0.35 du cylindre de base de 1 m */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Beam", meta=(ClampMin="1", Units="Centimeters"))
	float BeamWidth = 35.0f;

	/** Avant FinishSpawning : longueur et épaisseur x Multiplier (shoot system Explosif de l'arme) */
	void SetSizeMultiplier(float Multiplier) { SizeMultiplier = FMath::Max(0.01f, Multiplier); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual FVector GetImpactLocation(const AActor* Target) const override;

	virtual FVector GetDisappearLocation() const override;

private:

	/** Répliqué à l'apparition (taille du mesh chez les clients) */
	UPROPERTY(Replicated)
	float SizeMultiplier = 1.0f;

	float GetScaledLength() const { return BeamLength * SizeMultiplier; }

	float GetScaledRadius() const { return 0.5f * BeamWidth * SizeMultiplier; }

	/** Serveur : NME dans le cylindre, du plus proche au plus loin */
	void SweepBeam();
};
