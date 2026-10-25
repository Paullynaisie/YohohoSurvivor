#include "EditorTools/YSEditorScriptLibrary.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMeshSocket.h"

#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
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

#endif
