#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "YSEditorScriptLibrary.generated.h"

class UAnimSequence;
class UBlendSpace;

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
#endif
};
