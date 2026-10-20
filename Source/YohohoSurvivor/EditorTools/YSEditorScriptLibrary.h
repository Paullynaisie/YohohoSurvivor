#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "YSEditorScriptLibrary.generated.h"

class UAnimSequence;
class UBlendSpace;
class USkeleton;

/**
 *  Fonctions éditeur appelées par les scripts Python de Tools/EditorScripts
 *  pour ce que l'API Python d'Unreal n'expose pas directement.
 */
UCLASS()
class UYSEditorScriptLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

#if WITH_EDITOR
	/** Configure un axe d'un BlendSpace (0 = X, 1 = Y) */
	UFUNCTION(BlueprintCallable, Category="YS|Editor")
	static void SetBlendSpaceAxis(UBlendSpace* BlendSpace, int32 AxisIndex, const FString& DisplayName, float Min, float Max, int32 GridNum, bool bWrapInput);

	/** Remplace tous les samples d'un BlendSpace ; retourne le nombre de samples ajoutés */
	UFUNCTION(BlueprintCallable, Category="YS|Editor")
	static int32 SetBlendSpaceSamples(UBlendSpace* BlendSpace, const TArray<UAnimSequence*>& Animations, const TArray<FVector>& Positions);

	/** Ajoute (ou met à jour) un socket sur un squelette ; retourne false si l'os n'existe pas */
	UFUNCTION(BlueprintCallable, Category="YS|Editor")
	static bool SetSkeletonSocket(USkeleton* Skeleton, FName SocketName, FName BoneName, FVector RelativeLocation, FRotator RelativeRotation);

	/**
	 *  Crée un Landscape dans le monde de l'éditeur, comme le panneau « New Landscape » (calques d'édition, collisions) :
	 *  ComponentCount x ComponentCount composants de QuadsPerComponent quads (1 section par composant),
	 *  coin (X, Y min) à MinCorner, échelle Scale (Z = 100 : une unité de hauteur = 1/128 cm).
	 *  HeightsCm = hauteur en cm de chaque sommet, ligne par ligne (Y puis X), (ComponentCount x QuadsPerComponent + 1)² valeurs.
	 *  Retourne le Landscape créé (nullptr si les données ne correspondent pas).
	 */
	UFUNCTION(BlueprintCallable, Category="YS|Editor", meta=(WorldContext="WorldContextObject"))
	static class ALandscape* CreateLandscapeFromHeights(UObject* WorldContextObject, FVector MinCorner, FVector Scale, int32 ComponentCount, int32 QuadsPerComponent,
		const TArray<float>& HeightsCm, class UMaterialInterface* Material, const FString& Label);
#endif
};
