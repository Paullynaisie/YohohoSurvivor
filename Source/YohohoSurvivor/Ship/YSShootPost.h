#pragma once

#include "CoreMinimal.h"
#include "Ship/YSShipStation.h"
#include "YSShootPost.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;

/**
 *  Place du ShootPost (plateforme de tir autour du mât avant ; composants ShootPost1..4 du bateau, une place par composant, donc
 *  plusieurs joueurs en même temps). E au pied du mât -> le joueur monte sur la plateforme (Occupant Point), ne bouge plus ; caméra
 *  dézoomée comme à la barre (Helm Camera Arm Length du bateau), vers le curseur. Son attaque auto, si c'est un tir (Coup d Petoir,
 *  Auto shot...), reste permise avec des balles qui vivent Ammo Life Time Multiplier fois plus longtemps, et un trait droit montre
 *  où elle part (local) ; Ability, MoveAction et attaques de mêlée sont rangées. E le redescend au pied du mât (Exit Point).
 */
UCLASS(meta=(PrioritizeCategories="Shoot Post Station Interaction"))
class AYSShootPost : public AYSShipStation
{
	GENERATED_BODY()

public:

	AYSShootPost();

	virtual void Tick(float DeltaSeconds) override;

	/** Même dézoom que la barre du bateau */
	virtual float GetCameraArmLength() const override;

	virtual bool KeepsWeaponsVisible() const override { return true; }

	/** L'attaque auto de l'occupant, si c'est un tir, est permise */
	bool AllowsAutoShot() const { return bAllowAutoShot; }

	float GetAmmoLifeTimeMultiplier() const { return AmmoLifeTimeMultiplier; }

protected:

	/** L'occupant garde son attaque auto si c'est un tir */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shoot Post")
	bool bAllowAutoShot = true;

	/** Durée de vie des balles de l'attaque auto de l'occupant x ceci (portée x3) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shoot Post", meta=(ClampMin="0.1"))
	float AmmoLifeTimeMultiplier = 3.0f;

	/** Prévisu (joueur local au poste) : trait droit du joueur vers le curseur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shoot Post|Preview", meta=(ClampMin="0", Units="Centimeters"))
	float PreviewLength = 3500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shoot Post|Preview", meta=(ClampMin="1", Units="Centimeters"))
	float PreviewWidth = 40.0f;

	/** Hauteur du trait au-dessus des pieds du joueur (hauteur des mains, d où partent les balles) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shoot Post|Preview", meta=(Units="Centimeters"))
	float PreviewHeight = 100.0f;

	/** Couleur au départ (alpha = opacité) ; le trait s estompe jusqu à End Alpha au bout */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shoot Post|Preview")
	FLinearColor PreviewColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.75f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shoot Post|Preview", meta=(ClampMin="0", ClampMax="1"))
	float PreviewEndAlpha = 0.1f;

	/** Vide = M_ShipPreview (translucide non éclairé, couleur des sommets) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shoot Post|Preview")
	TObjectPtr<UMaterialInterface> PreviewMaterial;

private:

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> PreviewMesh;

	bool bPreviewVisible = false;

	void UpdatePreview();
};
