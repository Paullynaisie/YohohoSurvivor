#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "YSPlayerBomb.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;
class UYSGroundZoneComponent;

/**
 *  Bombe posée au sol par un joueur (Roll and Bomb du Shotgun). Visuel = composants du Blueprint (bombe en primitives, comme
 *  celle des NME) ; le mesh principal (Mesh) passe du noir au rouge lumineux pendant la mèche (Fuse Duration), un disque posé
 *  sur le relief montre le rayon de l'explosion. Fin de la mèche (serveur) : Damage (fixé par l'action, avec ses upgrades)
 *  aux acteurs portant un des HitTags (NME) dont la hurtbox touche la sphère Explosion Radius ; jamais aux joueurs.
 *  Sphère d'explosion qui grossit puis s'efface (toutes les machines). Origine de l'acteur = le sol.
 *  Classe de base de /Game/Placeable/AMMO/BP_CannoneerBomb.
 */
UCLASS(meta=(PrioritizeCategories="Bomb"))
class AYSPlayerBomb : public AActor
{
	GENERATED_BODY()

private:

	/** Racine, posée au sol */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Root;

	/** Corps de la bombe : colorié par la mèche (Bomb Material) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Disque posé sur le relief, du rayon de l'explosion (coordonnées monde) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UYSGroundZoneComponent> ExplosionZone;

	/** Sphère de l'explosion */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> ExplosionSphere;

protected:

	/** Mèche : temps entre la pose et l'explosion */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb", meta=(ClampMin="0", Units="Seconds"))
	float FuseDuration = 1.0f;

	/** Rayon de l'explosion */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb", meta=(ClampMin="1", Units="Centimeters"))
	float ExplosionRadius = 300.0f;

	/** Cibles blessées par l'explosion : acteurs portant un de ces tags (NME) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb")
	FGameplayTagContainer HitTags;

	/** Matériau du corps (M_FoeBomb : paramètres Color et Emissive) ; vide = matériau du Mesh, sans effet de mèche */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Fuse")
	TObjectPtr<UMaterialInterface> BombMaterial;

	/** Couleur à la pose, couleur en fin de mèche et intensité de sa lueur (émissif) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Fuse")
	FLinearColor BombColor = FLinearColor(0.02f, 0.02f, 0.02f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Fuse")
	FLinearColor FuseColor = FLinearColor(1.0f, 0.05f, 0.02f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Fuse", meta=(ClampMin="0"))
	float FuseGlow = 20.0f;

	/** Disque du rayon de l'explosion au sol */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Zone")
	bool bShowZone = true;

	/** Matériau du disque (M_BrookerZone : Color, Glow, FillOpacity, EdgeOpacity, Radius, EdgeWidth) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Zone", meta=(EditCondition="bShowZone"))
	TObjectPtr<UMaterialInterface> ZoneMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Zone", meta=(EditCondition="bShowZone"))
	FLinearColor ZoneColor = FLinearColor(1.0f, 0.55f, 0.05f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Zone", meta=(ClampMin="0", ClampMax="1", EditCondition="bShowZone"))
	float ZoneFillOpacity = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Zone", meta=(ClampMin="0", ClampMax="1", EditCondition="bShowZone"))
	float ZoneEdgeOpacity = 0.7f;

	/** Matériau de la sphère d'explosion (M_FoeExplosion : paramètres Color et Opacity) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Explosion")
	TObjectPtr<UMaterialInterface> ExplosionMaterial;

	/** Couleur de la sphère (A = opacité de départ) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Explosion")
	FLinearColor ExplosionColor = FLinearColor(1.0f, 0.45f, 0.0f, 0.35f);

	/** La sphère grossit de 0 au rayon en ce temps, puis s'efface en Explosion Fade Duration */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Explosion", meta=(ClampMin="0", Units="Seconds"))
	float ExplosionGrowDuration = 0.14f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Explosion", meta=(ClampMin="0", Units="Seconds"))
	float ExplosionFadeDuration = 0.2f;

	/** Dégâts de l'explosion, fixés par l'action qui pose la bombe (serveur, avant FinishSpawning) */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Bomb")
	float Damage = 0.0f;

public:

	AYSPlayerBomb();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	void SetDamage(float NewDamage) { Damage = NewDamage; }

protected:

	/** Explosion (serveur : dégâts) puis effet sur toutes les machines */
	void Explode();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExploded();

private:

	/** Pose vue par cette machine (temps local) */
	double PlacedTime = 0.0;

	/** Début de l'explosion vue par cette machine (temps local ; < 0 = pas encore) */
	double ExplosionStartTime = -1.0;

	bool bExploded = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BombMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ExplosionMaterialInstance;

	/** Couleur du corps : du noir au rouge lumineux pendant la mèche (Alpha 0 -> 1) */
	void SetFuseAlpha(float Alpha);

	void UpdateExplosion();
};
