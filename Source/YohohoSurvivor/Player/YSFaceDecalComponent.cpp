#include "Player/YSFaceDecalComponent.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ObjectSaveContext.h"

void UYSFaceDecalComponent::SetFaceTexture(UTexture2D* NewTexture)
{
	FaceTexture = NewTexture;
	ApplyFaceTexture();
}

void UYSFaceDecalComponent::OnRegister()
{
	Super::OnRegister();

	ApplyFaceTexture();
}

void UYSFaceDecalComponent::PreSave(FObjectPreSaveContext SaveContext)
{
	// on sauvegarde le matériau de base, pas le matériau dynamique transient
	if (FaceMaterial && DecalMaterial == FaceMaterial)
	{
		DecalMaterial = FaceMaterial->Parent;
		FaceMaterial = nullptr;
	}

	Super::PreSave(SaveContext);
}

#if WITH_EDITOR
void UYSFaceDecalComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplyFaceTexture();
}
#endif

void UYSFaceDecalComponent::ApplyFaceTexture()
{
	// les modèles (CDO / archétypes de Blueprint) gardent le matériau de base
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}

	// matériau de base : celui réglé dans le Blueprint, ou le parent du matériau dynamique déjà appliqué
	UMaterialInterface* BaseMaterial = (FaceMaterial && DecalMaterial == FaceMaterial) ? FaceMaterial->Parent.Get() : DecalMaterial.Get();
	if (!BaseMaterial)
	{
		return;
	}

	if (!FaceTexture)
	{
		if (DecalMaterial != BaseMaterial)
		{
			SetDecalMaterial(BaseMaterial);
		}
		FaceMaterial = nullptr;
		return;
	}

	if (!FaceMaterial || FaceMaterial->Parent != BaseMaterial)
	{
		FaceMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		FaceMaterial->SetFlags(RF_Transient);
	}

	FaceMaterial->SetTextureParameterValue(FaceTextureParameterName, FaceTexture);
	if (DecalMaterial != FaceMaterial)
	{
		SetDecalMaterial(FaceMaterial);
	}
}
