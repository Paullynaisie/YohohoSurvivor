#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "YSProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 *  Projectile en ligne droite. Touche uniquement les acteurs portant un des HitTags
 *  (via IGameplayTagAssetInterface), traverse les autres pawns, est détruit contre le décor.
 *  Les dégâts sont fixés par la capacité qui le tire (SetDamage) et appliqués sur le serveur.
 *  Classe de base du Blueprint PetoirAMMO.
 */
UCLASS(abstract, meta=(PrioritizeCategories="AMMO"))
class AYSProjectile : public AActor
{
	GENERATED_BODY()

private:

	/** Collision (racine) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USphereComponent> Collision;

	/** Apparence */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

protected:

	/** Vitesse de déplacement */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float Speed = 1200.0f;

	/** Durée de vie avant destruction automatique */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO", meta=(ClampMin="0", Units="Seconds"))
	float LifeTime = 5.0f;

	/** Le projectile touche les acteurs qui portent au moins un de ces tags ; il traverse les autres */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Collision")
	FGameplayTagContainer HitTags;

	/** Détruit le projectile quand il heurte le décor (murs, obstacles) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Collision")
	bool bDestroyOnWorldHit = true;

	/** Couleur appliquée au mesh */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Appearance")
	FLinearColor Color = FLinearColor::Black;

	/** Paramètre vectoriel du matériau du mesh qui reçoit Color */
	UPROPERTY(EditAnywhere, Category="AMMO|Appearance")
	FName ColorParameterName = TEXT("Color");

	/** Dégâts infligés, fixés par la capacité au moment du tir (serveur) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="AMMO")
	float Damage = 0.0f;

	/** Ricochets restants : après un dégât, la balle repart vers le NME le plus proche pas encore touché (fixé au tir, upgrade Ricochet) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="AMMO|Ricochet")
	int32 RemainingRicochets = 0;

	/** Multiplicateur des dégâts appliqué à chaque ricochet (Ricochet bonus) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="AMMO|Ricochet")
	float RicochetDamageMultiplier = 1.0f;

	/** Distance max (depuis l'impact) pour trouver la cible d'un ricochet ; sans cible, la balle disparaît */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AMMO|Ricochet", meta=(ClampMin="0", Units="Centimeters"))
	float RicochetRange = 1500.0f;

private:

	/** Plus de dégâts possibles (balle en cours de destruction) */
	bool bHasHit = false;

	/** Cibles déjà touchées (un ricochet ne les retouche pas) */
	TArray<TWeakObjectPtr<AActor>> HitActors;

public:

	AYSProjectile();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void PostInitializeComponents() override;

	virtual void BeginPlay() override;

	void SetDamage(float NewDamage) { Damage = NewDamage; }

	/** Ricochets (upgrade Ricochet) : nombre et multiplicateur de dégâts à chaque ricochet */
	void SetRicochets(int32 Count, float DamageMultiplier) { RemainingRicochets = Count; RicochetDamageMultiplier = DamageMultiplier; }

	/** Tags des acteurs que ce projectile peut toucher */
	const FGameplayTagContainer& GetHitTags() const { return HitTags; }

	/** Vrai si l'acteur porte un des HitTags */
	bool CanHit(const AActor* Target) const;

protected:

	/** Repart vers le NME le plus proche pas encore touché ; faux si aucun à portée */
	bool Ricochet();

protected:

	UFUNCTION()
	void OnCollisionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnCollisionHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};
