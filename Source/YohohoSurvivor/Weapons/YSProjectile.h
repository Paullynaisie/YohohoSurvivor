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
UCLASS(abstract)
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

private:

	/** Évite de toucher deux cibles dans la même frame */
	bool bHasHit = false;

public:

	AYSProjectile();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void PostInitializeComponents() override;

	virtual void BeginPlay() override;

	void SetDamage(float NewDamage) { Damage = NewDamage; }

protected:

	UFUNCTION()
	void OnCollisionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnCollisionHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Vrai si l'acteur porte un des HitTags */
	bool CanHit(const AActor* Target) const;
};
