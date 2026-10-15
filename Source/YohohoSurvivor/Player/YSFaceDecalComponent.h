#pragma once

#include "CoreMinimal.h"
#include "Components/DecalComponent.h"
#include "YSFaceDecalComponent.generated.h"

class UMaterialInstanceDynamic;
class UTexture2D;

/**
 *  Decal de visage : expose FaceTexture directement sur le composant.
 *  La texture est appliquée au paramètre FaceTextureParameterName du DecalMaterial via un
 *  matériau dynamique transient (visible dans l'éditeur et en jeu) ; le DecalMaterial réglé
 *  dans le Blueprint n'est jamais écrasé à la sauvegarde.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent))
class UYSFaceDecalComponent : public UDecalComponent
{
	GENERATED_BODY()

public:

	/** Icône du visage (vide = texture par défaut du DecalMaterial) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decal")
	TObjectPtr<UTexture2D> FaceTexture;

	/** Paramètre texture du DecalMaterial qui reçoit FaceTexture */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Decal")
	FName FaceTextureParameterName = TEXT("FaceTexture");

	/** Change le visage en cours de partie (expressions, etc.) */
	UFUNCTION(BlueprintCallable, Category="Decal")
	void SetFaceTexture(UTexture2D* NewTexture);

protected:

	virtual void OnRegister() override;

	virtual void PreSave(FObjectPreSaveContext SaveContext) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:

	/** Crée / met à jour le matériau dynamique avec FaceTexture */
	void ApplyFaceTexture();

	/** Matériau dynamique actuellement affiché (jamais sauvegardé) */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FaceMaterial;
};
