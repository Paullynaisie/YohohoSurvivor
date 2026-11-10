#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/YSCustomization.h"
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

	/** Hauteurs (cm au-dessus du Z de l'acteur) de tous les sommets d'un Landscape, ligne par ligne (Y puis X) ; OutSize = sommets par côté */
	UFUNCTION(BlueprintCallable, Category="YS|Editor")
	static TArray<float> GetLandscapeHeights(class ALandscape* Landscape, int32& OutSizeX, int32& OutSizeY);

	/** Remplace les hauteurs de tous les sommets d'un Landscape existant (même ordre que GetLandscapeHeights), collisions comprises */
	UFUNCTION(BlueprintCallable, Category="YS|Editor")
	static bool SetLandscapeHeights(class ALandscape* Landscape, const TArray<float>& HeightsCm);

	/** Ajoute une ligne de couleur de personnalisation (DT_CustomizationColors) ; remplace la ligne du même nom */
	UFUNCTION(BlueprintCallable, Category="YS|Editor")
	static bool AddCustomizationColorRow(class UDataTable* Table, FName RowName, const FYSCustomizationColor& Color);

	/** Ajoute une ligne de visage de personnalisation (DT_CustomizationFaces) ; remplace la ligne du même nom */
	UFUNCTION(BlueprintCallable, Category="YS|Editor")
	static bool AddCustomizationFaceRow(class UDataTable* Table, FName RowName, class UTexture2D* Face);

	/** Remplace (ou ajoute) une ligne de DT_Chara_Upgrades avec cette liste d'upgrades */
	UFUNCTION(BlueprintCallable, Category="YS|Editor")
	static bool SetCharaUpgradesRow(class UDataTable* Table, FName RowName, const TArray<class UYSUpStats*>& Upgrades);

	/** Upgrades d'une ligne de DT_Chara_Upgrades (vide si la ligne n'existe pas) */
	UFUNCTION(BlueprintCallable, Category="YS|Editor")
	static TArray<class UYSUpStats*> GetCharaUpgradesRow(class UDataTable* Table, FName RowName);

	/**
	 *  Copie toutes les propriétés éditables (EditAnywhere / EditDefaultsOnly) de Source vers Target, pour celles que Target
	 *  possède aussi (même classe ou classe parente commune) ; retourne le nombre de propriétés copiées.
	 *  Ex. réglages de l'Auto swing de BP_Epee vers celui d'une nouvelle arme.
	 */
	UFUNCTION(BlueprintCallable, Category="YS|Editor")
	static int32 CopyEditableProperties(UObject* Source, UObject* Target);

	/**
	 *  Copie vers Target les propriétés éditables des classes du jeu dont la valeur diffère entre Source et Reference
	 *  (ex. ce qui fait d'un Skull un Skull Elite, appliqué à un Skull Gun), sauf celles d'ExcludedNames (noms C++).
	 *  Retourne la liste « Nom = valeur » des propriétés copiées (une par ligne).
	 */
	UFUNCTION(BlueprintCallable, Category="YS|Editor")
	static FString CopyChangedProperties(UObject* Source, UObject* Reference, UObject* Target, const TArray<FString>& ExcludedNames);

	/** Noms des composants hérités des Blueprints parents de Blueprint (SCS des parents, pas les composants C++) */
	UFUNCTION(BlueprintCallable, Category="YS|Editor")
	static TArray<FName> GetInheritedComponentNames(UBlueprint* Blueprint);

	/**
	 *  Gabarit modifiable d'un composant hérité d'un Blueprint parent POUR CE Blueprint seulement (surcharge dans son
	 *  Inheritable Component Handler, créée si besoin avec les valeurs actuelles du parent) : le parent n'est pas touché.
	 *  Nul si aucun parent n'a ce composant.
	 */
	UFUNCTION(BlueprintCallable, Category="YS|Editor")
	static UActorComponent* GetInheritedComponentOverride(UBlueprint* Blueprint, FName ComponentName);
#endif
};
