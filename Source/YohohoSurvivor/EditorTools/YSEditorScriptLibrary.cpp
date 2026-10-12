#include "EditorTools/YSEditorScriptLibrary.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMeshSocket.h"

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

#endif
