#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "YSFoeBomb.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

/** Lancer d'une bombe : tout ce qu'il faut pour rejouer la même trajectoire et la même explosion sur chaque machine */
USTRUCT()
struct FYSBombThrow
{
	GENERATED_BODY()

	/** Point de départ et point d'arrivée (au sol, sous la cible au moment du lancer) */
	UPROPERTY()
	FVector Start = FVector::ZeroVector;

	UPROPERTY()
	FVector Target = FVector::ZeroVector;

	/** Hauteur du sommet de la cloche au-dessus du plus haut des deux points */
	UPROPERTY()
	float ApexHeight = 300.0f;

	/** Durée du vol (la gravité de la cloche en découle) */
	UPROPERTY()
	float FlightDuration = 1.5f;

	/** Posée au sol, la bombe attend ce temps avant d'exploser, en passant du noir au rouge lumineux */
	UPROPERTY()
	float FuseDuration = 1.0f;

	/** Couleur en vol, couleur à la fin de la mèche, et intensité de sa lueur (émissif) */
	UPROPERTY()
	FLinearColor BombColor = FLinearColor(0.02f, 0.02f, 0.02f);

	UPROPERTY()
	FLinearColor FuseColor = FLinearColor(1.0f, 0.05f, 0.02f);

	UPROPERTY()
	float FuseGlow = 20.0f;

	UPROPERTY()
	float Damage = 20.0f;

	UPROPERTY()
	float ExplosionRadius = 500.0f;

	/** La sphère d'explosion grossit de 0 au rayon en ce temps */
	UPROPERTY()
	float ExplosionGrowDuration = 0.14f;

	/** Puis s'efface en ce temps */
	UPROPERTY()
	float ExplosionFadeDuration = 0.2f;

	UPROPERTY()
	FLinearColor ExplosionColor = FLinearColor(1.0f, 0.0f, 0.0f, 0.35f);

	UPROPERTY()
	FLinearColor LandingZoneColor = FLinearColor(1.0f, 0.1f, 0.05f);

	UPROPERTY()
	float LandingZoneFillOpacity = 0.15f;

	UPROPERTY()
	float LandingZoneEdgeOpacity = 0.7f;

	/** Matériaux : zone d impact (M_BrookerZone) et sphère d explosion (translucide, paramètres Color / Opacity) */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> LandingZoneMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ExplosionMaterial;

	/** Matériau de la bombe (M_FoeBomb : paramètres Color et Emissive) */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> BombMaterial;
};

/**
 *  Bombe lancée en cloche par un NME à distance (Skull Bomb). Trajectoire calculée (pas de physique) depuis FYSBombThrow,
 *  identique sur toutes les machines ; prévisualisation rouge du point d'impact au sol (disque de la zone du Brooker)
 *  pendant le vol et la mèche. Arrivée au sol, elle attend FuseDuration en passant du noir au rouge lumineux, puis (serveur) explose : Damage aux joueurs (HitTags) dont la hurtbox touche la sphère
 *  ExplosionRadius, jamais aux NME ; sphère rouge translucide qui grossit puis s'efface (toutes les machines).
 *  Les réglages viennent du NME qui lance (catégorie Foe|Ranged).
 */
UCLASS()
class AYSFoeBomb : public AActor
{
	GENERATED_BODY()

private:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> BombMesh;

	/** Disque rouge au sol au point d'impact (coordonnées monde) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> LandingZone;

	/** Sphère de l'explosion (coordonnées monde) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> ExplosionSphere;

public:

	/** Cibles blessées par l'explosion : acteurs portant un de ces tags (joueurs) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb")
	FGameplayTagContainer HitTags;

	AYSFoeBomb();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/** Fixe le lancer (serveur, avant FinishSpawning) */
	void SetThrow(const FYSBombThrow& InThrow) { Throw = InThrow; }

	/** Durée du vol pour un lancer */
	static float GetFlightDuration(const FYSBombThrow& Throw);

	/** Position sur la trajectoire au temps Time */
	static FVector GetFlightLocation(const FYSBombThrow& Throw, float Time);

protected:

	UPROPERTY(Replicated)
	FYSBombThrow Throw;

	/** Explosion (serveur : dégâts) puis effet sur toutes les machines */
	void Explode();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExploded();

private:

	/** Début du vol vu par cette machine (temps local) */
	double FlightStartTime = 0.0;

	/** Début de l'explosion vue par cette machine (temps local ; < 0 = pas encore) */
	double ExplosionStartTime = -1.0;

	bool bExploded = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ExplosionMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BombMaterialInstance;

	/** Couleur de la bombe : du noir au rouge lumineux pendant la mèche (Alpha 0 -> 1) */
	void SetFuseAlpha(float Alpha);

	/** Apparence selon Throw (zone d'impact, couleurs) */
	void ApplyThrowVisuals();

	void UpdateExplosion();
};
