#include "Weapons/YSWeapon.h"
#include "Abilities/YSCoupDPetoirAbility.h"
#include "Abilities/YSAbilityComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSDanglingChainComponent.h"

void AYSWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSWeapon, ChainSizeBonus);
	DOREPLIFETIME(AYSWeapon, ChainLengthBonus);
}

void AYSWeapon::SetChainSizeBonus(float SizeBonus, float LengthBonus)
{
	if (!HasAuthority() || (FMath::IsNearlyEqual(ChainSizeBonus, SizeBonus) && FMath::IsNearlyEqual(ChainLengthBonus, LengthBonus)))
	{
		return;
	}
	ChainSizeBonus = SizeBonus;
	ChainLengthBonus = LengthBonus;
	ForceNetUpdate();
	OnRep_ChainSizeBonus();
}

void AYSWeapon::OnRep_ChainSizeBonus()
{
	TInlineComponentArray<UYSDanglingChainComponent*> Chains(this);
	for (UYSDanglingChainComponent* Chain : Chains)
	{
		Chain->SetEndSizeMultiplier(1.0f + ChainSizeBonus, 1.0f + ChainLengthBonus);
	}
}

AYSWeapon::AYSWeapon()
{
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(Root);

	OffHandSocket = CreateDefaultSubobject<USceneComponent>(TEXT("OffHandSocket"));
	OffHandSocket->SetupAttachment(Root);

	MainHandMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Art/Materials/PH_materials/m_PH_emissive_turboBlue.m_PH_emissive_turboBlue")));
	OffHandMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Art/Materials/PH_materials/m_PH_emissive_turboGreen.m_PH_emissive_turboGreen")));
	ClassUpgradeMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Art/Materials/PH_materials/m_PH_emissive_turboYellow.m_PH_emissive_turboYellow")));
}

void AYSWeapon::OptimizeForCrowd(bool bCastShadow)
{
	if (bCrowdOptimized || !GetWorld() || !GetWorld()->IsGameWorld())
	{
		return;
	}
	bCrowdOptimized = true;

	// mesh fusionné (repère de Root, échelle 1) à la place de toutes les formes
	if (MergedMesh)
	{
		TInlineComponentArray<UStaticMeshComponent*> Shapes(this);
		UStaticMeshComponent* Merged = NewObject<UStaticMeshComponent>(this, TEXT("MergedMeshComponent"));
		Merged->SetStaticMesh(MergedMesh);
		Merged->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Merged->SetupAttachment(Root);
		Merged->RegisterComponent();
		AddInstanceComponent(Merged);
		for (UStaticMeshComponent* Shape : Shapes)
		{
			// ce qui est accroché à une forme (Muzzle, Hitbox...) reste à sa place, remonté sur le parent de la forme
			USceneComponent* Parent = Shape->GetAttachParent() ? Shape->GetAttachParent() : Root.Get();
			const TArray<TObjectPtr<USceneComponent>> ShapeChildren = Shape->GetAttachChildren();
			for (USceneComponent* Child : ShapeChildren)
			{
				Child->AttachToComponent(Parent, FAttachmentTransformRules::KeepWorldTransform);
			}
			if (Shape == Mesh)
			{
				// composant natif (référencé par le code) : vidé, invisible et détaché (plus mis à jour quand l'arme bouge)
				Mesh->SetStaticMesh(nullptr);
				Mesh->SetVisibility(false);
				Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			}
			else
			{
				Shape->DestroyComponent();
			}
		}
	}

	TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		Primitive->SetGenerateOverlapEvents(false);
		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Primitive->SetCastShadow(bCastShadow);
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSWeapon] %s optimisée (%s, %d primitive(s))"), *GetName(), MergedMesh ? TEXT("mesh fusionné") : TEXT("formes du Blueprint"), Primitives.Num());
}

void AYSWeapon::SetColoration(EYSWeaponColoration NewColoration)
{
	if (NewColoration == AppliedColoration || NewColoration == EYSWeaponColoration::None)
	{
		return;
	}
	AppliedColoration = NewColoration;

	const TSoftObjectPtr<UMaterialInterface>& MaterialPtr = NewColoration == EYSWeaponColoration::MainHand ? MainHandMaterial
		: NewColoration == EYSWeaponColoration::OffHand ? OffHandMaterial : ClassUpgradeMaterial;
	UMaterialInterface* Material = MaterialPtr.LoadSynchronous();
	if (!Material)
	{
		return;
	}

	// composant Coloration du Blueprint (nom de variable, espaces ignorés), puis tous ses descendants
	const FString WantedName = ColorationComponentName.ToString().Replace(TEXT(" "), TEXT(""));
	TInlineComponentArray<USceneComponent*> Components(this);
	for (USceneComponent* Component : Components)
	{
		if (!Component->GetName().Replace(TEXT(" "), TEXT("")).Equals(WantedName, ESearchCase::IgnoreCase))
		{
			continue;
		}
		TArray<USceneComponent*> Targets;
		Component->GetChildrenComponents(true, Targets);
		Targets.Add(Component);
		for (USceneComponent* Target : Targets)
		{
			if (UMeshComponent* MeshComponent = Cast<UMeshComponent>(Target))
			{
				for (int32 Index = 0; Index < MeshComponent->GetNumMaterials(); ++Index)
				{
					MeshComponent->SetMaterial(Index, Material);
				}
			}
		}
		return;
	}
}

UYSAbilityComponent* AYSWeapon::GetAction() const
{
	return FindComponentByClass<UYSAbilityComponent>();
}

UYSAbilityComponent* AYSWeapon::GetActiveAction() const
{
	TInlineComponentArray<UYSAbilityComponent*> Actions(this);
	for (UYSAbilityComponent* Action : Actions)
	{
		if (!Action->IsAutomatic())
		{
			return Action;
		}
	}
	return nullptr;
}

UYSAbilityComponent* AYSWeapon::GetAutoAction() const
{
	TInlineComponentArray<UYSAbilityComponent*> Actions(this);
	for (UYSAbilityComponent* Action : Actions)
	{
		if (Action->IsAutomatic())
		{
			return Action;
		}
	}
	return nullptr;
}

APawn* AYSWeapon::GetHolder() const
{
	return Cast<APawn>(GetOwner());
}

bool AYSWeapon::GetRecoil(float& OutPitch, float& OutKickBack) const
{
	// action de tir la plus récemment activée (l'Ability et l'Auto shot ont chacune leurs réglages)
	const UYSCoupDPetoirAbility* Latest = nullptr;
	TInlineComponentArray<UYSCoupDPetoirAbility*> Shots(this);
	for (const UYSCoupDPetoirAbility* Shot : Shots)
	{
		if (!Latest || Shot->GetTimeSinceActivation() < Latest->GetTimeSinceActivation())
		{
			Latest = Shot;
		}
	}

	const float Alpha = Latest ? Latest->GetRecoilAlpha() : 0.0f;
	OutPitch = Latest ? Latest->RecoilPitch * Alpha : 0.0f;
	OutKickBack = Latest ? Latest->RecoilKickBack * Alpha : 0.0f;
	return Alpha > 0.0f;
}

FVector AYSWeapon::GetMuzzleLocation() const
{
	return Muzzle->GetComponentLocation();
}

FText AYSWeapon::GetWeaponName() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	FString Name = GetClass()->GetName();
	Name.RemoveFromStart(TEXT("BP_"));
	Name.RemoveFromEnd(TEXT("_C"));
	return FText::FromString(Name);
}

FRotator AYSWeapon::GetReloadSpin() const
{
	// rechargement le plus récent parmi les actions de tir de l'arme
	const UYSCoupDPetoirAbility* Latest = nullptr;
	TInlineComponentArray<UYSCoupDPetoirAbility*> Shots(this);
	for (const UYSCoupDPetoirAbility* Shot : Shots)
	{
		if (!Latest || Shot->GetTimeSinceReload() < Latest->GetTimeSinceReload())
		{
			Latest = Shot;
		}
	}
	return Latest ? Latest->GetReloadSpin() : FRotator::ZeroRotator;
}
