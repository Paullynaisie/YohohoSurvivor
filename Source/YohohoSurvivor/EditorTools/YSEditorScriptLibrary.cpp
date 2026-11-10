#include "EditorTools/YSEditorScriptLibrary.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMeshSocket.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/Engine.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInterface.h"
#include "Upgrades/YSShardUpgrade.h"
#endif

#if WITH_EDITOR

void UYSEditorScriptLibrary::SetBlendSpaceAxis(UBlendSpace* BlendSpace, int32 AxisIndex, const FString& DisplayName, float Min, float Max, int32 GridNum, bool bWrapInput)
{
	const FStructProperty* Property = FindFProperty<FStructProperty>(UBlendSpace::StaticClass(), TEXT("BlendParameters"));
	if (!BlendSpace || !Property || AxisIndex < 0 || AxisIndex >= Property->ArrayDim)
	{
		return;
	}

	BlendSpace->Modify();

	// BlendParameters est protégé : accès par réflexion
	FBlendParameter* Parameter = Property->ContainerPtrToValuePtr<FBlendParameter>(BlendSpace, AxisIndex);
	Parameter->DisplayName = DisplayName;
	Parameter->Min = Min;
	Parameter->Max = Max;
	Parameter->GridNum = FMath::Max(1, GridNum);
	Parameter->bWrapInput = bWrapInput;

	BlendSpace->PostEditChange();
	BlendSpace->MarkPackageDirty();
}

int32 UYSEditorScriptLibrary::SetBlendSpaceSamples(UBlendSpace* BlendSpace, const TArray<UAnimSequence*>& Animations, const TArray<FVector>& Positions)
{
	const FArrayProperty* Property = FindFProperty<FArrayProperty>(UBlendSpace::StaticClass(), TEXT("SampleData"));
	if (!BlendSpace || !Property || Animations.Num() != Positions.Num())
	{
		return 0;
	}

	BlendSpace->Modify();
	Property->ContainerPtrToValuePtr<TArray<FBlendSample>>(BlendSpace)->Reset();

	int32 Added = 0;
	for (int32 Index = 0; Index < Animations.Num(); ++Index)
	{
		if (Animations[Index] && BlendSpace->AddSample(Animations[Index], Positions[Index]) != INDEX_NONE)
		{
			++Added;
		}
	}

	BlendSpace->ValidateSampleData();
	BlendSpace->ResampleData();
	BlendSpace->PostEditChange();
	BlendSpace->MarkPackageDirty();
	return Added;
}

bool UYSEditorScriptLibrary::SetSkeletonSocket(USkeleton* Skeleton, FName SocketName, FName BoneName, FVector RelativeLocation, FRotator RelativeRotation)
{
	if (!Skeleton || Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName) == INDEX_NONE)
	{
		return false;
	}

	Skeleton->Modify();

	USkeletalMeshSocket* Socket = Skeleton->FindSocket(SocketName);
	if (!Socket)
	{
		Socket = NewObject<USkeletalMeshSocket>(Skeleton);
		Socket->SocketName = SocketName;
		Skeleton->Sockets.Add(Socket);
	}

	Socket->BoneName = BoneName;
	Socket->RelativeLocation = RelativeLocation;
	Socket->RelativeRotation = RelativeRotation;

	Skeleton->MarkPackageDirty();
	return true;
}

ALandscape* UYSEditorScriptLibrary::CreateLandscapeFromHeights(UObject* WorldContextObject, FVector MinCorner, FVector Scale, int32 ComponentCount, int32 QuadsPerComponent,
	const TArray<float>& HeightsCm, UMaterialInterface* Material, const FString& Label)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	const int32 Size = ComponentCount * QuadsPerComponent + 1;
	if (!World || ComponentCount <= 0 || QuadsPerComponent <= 0 || HeightsCm.Num() != Size * Size || Scale.Z <= 0.0)
	{
		UE_LOG(LogTemp, Error, TEXT("[YSEditorScriptLibrary] CreateLandscapeFromHeights : données invalides (%d hauteurs pour %d x %d)"), HeightsCm.Num(), Size, Size);
		return nullptr;
	}

	// hauteurs en unités du landscape : 32768 = Z de l'acteur, 1 unité = Scale.Z / 128 cm
	TArray<uint16> HeightData;
	HeightData.SetNumUninitialized(HeightsCm.Num());
	for (int32 Index = 0; Index < HeightsCm.Num(); ++Index)
	{
		HeightData[Index] = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(32768.0 + HeightsCm[Index] * 128.0 / Scale.Z), 0, 65535));
	}

	// même enchaînement que le panneau New Landscape de l'éditeur
	TMap<FGuid, TArray<uint16>> HeightDataPerLayers;
	HeightDataPerLayers.Add(FGuid(), MoveTemp(HeightData));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayers;
	MaterialLayerDataPerLayers.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());

	ALandscape* Landscape = World->SpawnActor<ALandscape>(MinCorner, FRotator::ZeroRotator);
	Landscape->LandscapeMaterial = Material;
	Landscape->SetActorRelativeScale3D(Scale);
	Landscape->Import(FGuid::NewGuid(), 0, 0, Size - 1, Size - 1, 1, QuadsPerComponent, HeightDataPerLayers, TEXT(""), MaterialLayerDataPerLayers,
		ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());

	if (ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo())
	{
		LandscapeInfo->UpdateLayerInfoMap(Landscape);
	}
	if (!Label.IsEmpty())
	{
		Landscape->SetActorLabel(Label);
	}
	return Landscape;
}

namespace YSEditorLandscape
{
	/** Edit layer de base du Landscape (GUID vide sans edit layers) */
	FGuid GetBaseEditLayerGuid(const ALandscape* Landscape)
	{
		const ULandscapeEditLayerBase* Layer = Landscape->HasLayersContent() ? Landscape->GetEditLayerConst(0) : nullptr;
		return Layer ? Layer->GetGuid() : FGuid();
	}
}

TArray<float> UYSEditorScriptLibrary::GetLandscapeHeights(ALandscape* Landscape, int32& OutSizeX, int32& OutSizeY)
{
	TArray<float> Heights;
	OutSizeX = OutSizeY = 0;
	ULandscapeInfo* Info = Landscape ? Landscape->GetLandscapeInfo() : nullptr;
	int32 MinX, MinY, MaxX, MaxY;
	if (!Info || !Info->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
	{
		return Heights;
	}
	OutSizeX = MaxX - MinX + 1;
	OutSizeY = MaxY - MinY + 1;
	TArray<uint16> Data;
	Data.SetNumZeroed(OutSizeX * OutSizeY);
	FLandscapeEditDataInterface Edit(Info, YSEditorLandscape::GetBaseEditLayerGuid(Landscape));
	Edit.GetHeightData(MinX, MinY, MaxX, MaxY, Data.GetData(), 0);

	const double ScaleZ = Landscape->GetActorScale3D().Z;
	Heights.SetNumUninitialized(Data.Num());
	for (int32 Index = 0; Index < Data.Num(); ++Index)
	{
		Heights[Index] = static_cast<float>((Data[Index] - 32768.0) * ScaleZ / 128.0);
	}
	return Heights;
}

bool UYSEditorScriptLibrary::SetLandscapeHeights(ALandscape* Landscape, const TArray<float>& HeightsCm)
{
	ULandscapeInfo* Info = Landscape ? Landscape->GetLandscapeInfo() : nullptr;
	int32 MinX, MinY, MaxX, MaxY;
	if (!Info || !Info->GetLandscapeExtent(MinX, MinY, MaxX, MaxY) || HeightsCm.Num() != (MaxX - MinX + 1) * (MaxY - MinY + 1))
	{
		UE_LOG(LogTemp, Error, TEXT("[YSEditorScriptLibrary] SetLandscapeHeights : données invalides (%d hauteurs)"), HeightsCm.Num());
		return false;
	}

	const double ScaleZ = Landscape->GetActorScale3D().Z;
	TArray<uint16> Data;
	Data.SetNumUninitialized(HeightsCm.Num());
	for (int32 Index = 0; Index < HeightsCm.Num(); ++Index)
	{
		Data[Index] = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(32768.0 + HeightsCm[Index] * 128.0 / ScaleZ), 0, 65535));
	}

	Landscape->Modify();
	{
		FLandscapeEditDataInterface Edit(Info, YSEditorLandscape::GetBaseEditLayerGuid(Landscape));
		Edit.SetHeightData(MinX, MinY, MaxX, MaxY, Data.GetData(), 0, true);
		Edit.Flush();
	}
	if (Landscape->HasLayersContent())
	{
		Landscape->ForceUpdateLayersContent();
	}
	Landscape->MarkPackageDirty();
	return true;
}

TArray<FLinearColor> UYSEditorScriptLibrary::GetMaterialColorConstants(UMaterial* Material)
{
	TArray<FLinearColor> Colors;
	if (!Material)
	{
		return Colors;
	}
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (const UMaterialExpressionConstant3Vector* C3 = Cast<UMaterialExpressionConstant3Vector>(Expression))
		{
			Colors.Add(C3->Constant);
		}
		else if (const UMaterialExpressionConstant4Vector* C4 = Cast<UMaterialExpressionConstant4Vector>(Expression))
		{
			Colors.Add(C4->Constant);
		}
		else if (const UMaterialExpressionVectorParameter* Parameter = Cast<UMaterialExpressionVectorParameter>(Expression))
		{
			Colors.Add(Parameter->DefaultValue);
		}
	}
	return Colors;
}

int32 UYSEditorScriptLibrary::ReplaceMaterialColorConstants(UMaterial* Material, FLinearColor From, FLinearColor To, float Tolerance)
{
	if (!Material)
	{
		return 0;
	}
	const auto Matches = [&](const FLinearColor& Color)
	{
		return FMath::Abs(Color.R - From.R) <= Tolerance && FMath::Abs(Color.G - From.G) <= Tolerance && FMath::Abs(Color.B - From.B) <= Tolerance;
	};
	const auto Replaced = [&](const FLinearColor& Color) { return FLinearColor(To.R, To.G, To.B, Color.A); };
	int32 Count = 0;
	Material->Modify();
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (UMaterialExpressionConstant3Vector* C3 = Cast<UMaterialExpressionConstant3Vector>(Expression))
		{
			if (Matches(C3->Constant)) { C3->Modify(); C3->Constant = Replaced(C3->Constant); ++Count; }
		}
		else if (UMaterialExpressionConstant4Vector* C4 = Cast<UMaterialExpressionConstant4Vector>(Expression))
		{
			if (Matches(C4->Constant)) { C4->Modify(); C4->Constant = Replaced(C4->Constant); ++Count; }
		}
		else if (UMaterialExpressionVectorParameter* Parameter = Cast<UMaterialExpressionVectorParameter>(Expression))
		{
			if (Matches(Parameter->DefaultValue)) { Parameter->Modify(); Parameter->DefaultValue = Replaced(Parameter->DefaultValue); ++Count; }
		}
	}
	if (Count > 0)
	{
		Material->PreEditChange(nullptr);
		Material->PostEditChange();
		Material->MarkPackageDirty();
	}
	return Count;
}

bool UYSEditorScriptLibrary::SetCustomizationColorEmissive(UDataTable* Table, FName RowName, UMaterialInterface* EmissiveMaterial, FLinearColor EmissiveColor)
{
	if (!Table || Table->GetRowStruct() != FYSCustomizationColor::StaticStruct())
	{
		return false;
	}
	FYSCustomizationColor* Row = Table->FindRow<FYSCustomizationColor>(RowName, TEXT("SetCustomizationColorEmissive"), false);
	if (!Row)
	{
		return false;
	}
	Table->Modify();
	Row->EmissiveMaterial = EmissiveMaterial;
	Row->EmissiveColor = EmissiveColor;
	Table->MarkPackageDirty();
	return true;
}

bool UYSEditorScriptLibrary::AddCustomizationColorRow(UDataTable* Table, FName RowName, const FYSCustomizationColor& Color)
{
	if (!Table || Table->GetRowStruct() != FYSCustomizationColor::StaticStruct() || RowName.IsNone())
	{
		return false;
	}
	Table->Modify();
	Table->AddRow(RowName, Color);
	Table->MarkPackageDirty();
	return true;
}

bool UYSEditorScriptLibrary::AddCustomizationFaceRow(UDataTable* Table, FName RowName, UTexture2D* Face)
{
	if (!Table || Table->GetRowStruct() != FYSCustomizationFace::StaticStruct() || RowName.IsNone() || !Face)
	{
		return false;
	}
	FYSCustomizationFace Row;
	Row.Face = Face;
	Table->Modify();
	Table->AddRow(RowName, Row);
	Table->MarkPackageDirty();
	return true;
}

bool UYSEditorScriptLibrary::SetCharaUpgradesRow(UDataTable* Table, FName RowName, const TArray<UYSUpStats*>& Upgrades)
{
	if (!Table || Table->GetRowStruct() != FYSCharaUpgradesRow::StaticStruct() || RowName.IsNone())
	{
		return false;
	}
	FYSCharaUpgradesRow Row;
	for (UYSUpStats* Stats : Upgrades)
	{
		Row.Upgrades.Add(Stats);
	}
	Table->Modify();
	Table->AddRow(RowName, Row);
	Table->MarkPackageDirty();
	return true;
}

TArray<UYSUpStats*> UYSEditorScriptLibrary::GetCharaUpgradesRow(UDataTable* Table, FName RowName)
{
	TArray<UYSUpStats*> Result;
	const FYSCharaUpgradesRow* Row = Table && Table->GetRowStruct() == FYSCharaUpgradesRow::StaticStruct()
		? Table->FindRow<FYSCharaUpgradesRow>(RowName, TEXT("GetCharaUpgradesRow"), false) : nullptr;
	if (Row)
	{
		for (const TObjectPtr<UYSUpStats>& Stats : Row->Upgrades)
		{
			Result.Add(Stats.Get());
		}
	}
	return Result;
}

int32 UYSEditorScriptLibrary::CopyEditableProperties(UObject* Source, UObject* Target)
{
	if (!Source || !Target)
	{
		return 0;
	}

	Target->Modify();
	int32 Copied = 0;
	for (TFieldIterator<FProperty> It(Source->GetClass()); It; ++It)
	{
		const FProperty* Property = *It;
		const UClass* Owner = Property->GetOwnerClass();
		// réglages éditables des classes du jeu seulement (pas ceux du moteur : transform, collision, tick...)
		if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_EditConst)
			|| !Owner || !Target->IsA(Owner) || !Owner->GetOutermost()->GetName().StartsWith(TEXT("/Script/YohohoSurvivor")))
		{
			continue;
		}
		Property->CopyCompleteValue_InContainer(Target, Source);
		++Copied;
	}
	Target->MarkPackageDirty();
	return Copied;
}

FString UYSEditorScriptLibrary::CopyChangedProperties(UObject* Source, UObject* Reference, UObject* Target, const TArray<FString>& ExcludedNames)
{
	if (!Source || !Reference || !Target)
	{
		return FString();
	}

	Target->Modify();
	FString Report;
	for (TFieldIterator<FProperty> It(Source->GetClass()); It; ++It)
	{
		const FProperty* Property = *It;
		const UClass* Owner = Property->GetOwnerClass();
		// réglages éditables des classes du jeu communs aux trois objets
		if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_EditConst)
			|| !Owner || !Target->IsA(Owner) || !Reference->IsA(Owner) || !Owner->GetOutermost()->GetName().StartsWith(TEXT("/Script/YohohoSurvivor"))
			|| ExcludedNames.Contains(Property->GetName()) || Property->Identical_InContainer(Source, Reference))
		{
			continue;
		}
		Property->CopyCompleteValue_InContainer(Target, Source);
		FString Value;
		Property->ExportText_InContainer(0, Value, Source, Source, nullptr, PPF_None);
		Report += FString::Printf(TEXT("%s = %s\n"), *Property->GetName(), *Value);
	}
	Target->MarkPackageDirty();
	return Report;
}

namespace YSEditorScript
{
	/** Nœud SCS d'un Blueprint parent qui porte ce nom (nul sinon) */
	USCS_Node* FindParentNode(UBlueprint* Blueprint, FName ComponentName)
	{
		for (UClass* Class = Blueprint && Blueprint->ParentClass ? Blueprint->ParentClass.Get() : nullptr; Class; Class = Class->GetSuperClass())
		{
			const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Class);
			if (!Generated || !Generated->SimpleConstructionScript)
			{
				continue;
			}
			if (USCS_Node* Node = Generated->SimpleConstructionScript->FindSCSNode(ComponentName))
			{
				return Node;
			}
		}
		return nullptr;
	}
}

TArray<FName> UYSEditorScriptLibrary::GetInheritedComponentNames(UBlueprint* Blueprint)
{
	TArray<FName> Names;
	for (UClass* Class = Blueprint && Blueprint->ParentClass ? Blueprint->ParentClass.Get() : nullptr; Class; Class = Class->GetSuperClass())
	{
		if (const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Class); Generated && Generated->SimpleConstructionScript)
		{
			for (const USCS_Node* Node : Generated->SimpleConstructionScript->GetAllNodes())
			{
				if (Node && Node->ComponentTemplate)
				{
					Names.AddUnique(Node->GetVariableName());
				}
			}
		}
	}
	return Names;
}

UActorComponent* UYSEditorScriptLibrary::GetInheritedComponentOverride(UBlueprint* Blueprint, FName ComponentName)
{
	USCS_Node* Node = YSEditorScript::FindParentNode(Blueprint, ComponentName);
	if (!Node)
	{
		return nullptr;
	}
	UInheritableComponentHandler* Handler = Blueprint->GetInheritableComponentHandler(true);
	if (!Handler)
	{
		return nullptr;
	}
	const FComponentKey Key(Node);
	UActorComponent* Override = Handler->GetOverridenComponentTemplate(Key);
	if (!Override)
	{
		Override = Handler->CreateOverridenComponentTemplate(Key);
	}
	if (Override)
	{
		Override->Modify();
		Blueprint->Modify();
	}
	return Override;
}

#endif
