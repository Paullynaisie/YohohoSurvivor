#include "Ship/YSCombatPost.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "Ship/YSPirateShip.h"
#include "Ship/YSShipStation.h"
#include "UObject/ConstructorHelpers.h"

AYSCombatPostItem::AYSCombatPostItem()
{
	DisplayName = NSLOCTEXT("YS", "CombatPostItemName", "Poste de combat");
}

AYSCombatPostReceptacle::AYSCombatPostReceptacle()
{
	DisplayName = NSLOCTEXT("YS", "CombatPostReceptacleName", "Réceptacle à poste de combat");
	bCarriable = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MarkFinder(TEXT("/Game/Art/Materials/PH_materials/m_PH_monochrome_Yellow.m_PH_monochrome_Yellow"));

	// disque plat sur le pont (origine = pont), sans collision : on marche dessus
	Plate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plate"));
	Plate->SetupAttachment(GetRootComponent());
	Plate->SetRelativeLocation(FVector(0.0f, 0.0f, 2.0f));
	Plate->SetRelativeScale3D(FVector(1.3f, 1.3f, 0.04f));
	Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Plate->SetMobility(EComponentMobility::Movable);
	if (CylinderFinder.Succeeded())
	{
		Plate->SetStaticMesh(CylinderFinder.Object);
	}
	if (MarkFinder.Succeeded())
	{
		Plate->SetMaterial(0, MarkFinder.Object);
	}
}

void AYSCombatPostReceptacle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSCombatPostReceptacle, InstalledPost);
}

void AYSCombatPostReceptacle::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && StartingPostClass)
	{
		InstallPost(StartingPostClass, TEXT("poste de départ"));
	}
}

void AYSCombatPostReceptacle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// le poste posé part avec le réceptacle (bateau détruit ou remplacé)
	if (HasAuthority() && IsValid(InstalledPost))
	{
		InstalledPost->Destroy();
	}
	InstalledPost = nullptr;

	Super::EndPlay(EndPlayReason);
}

void AYSCombatPostReceptacle::OnRep_InstalledPost()
{
	Plate->SetVisibility(!InstalledPost);
}

AYSPirateShip* AYSCombatPostReceptacle::GetShip() const
{
	return AYSPirateShip::GetShipOf(this);
}

bool AYSCombatPostReceptacle::CanInteract(const AYSPlayerCharacter* Character) const
{
	const AYSPirateShip* Ship = GetShip();
	const AYSCombatPostItem* Item = Character ? Cast<AYSCombatPostItem>(Character->GetCarriedObject()) : nullptr;
	return !InstalledPost && Item && Item->GetPostClass() && Ship && !Ship->IsFoeShip() && !Ship->IsDisabled() && Character->IsAlive() && !Character->IsSwimming();
}

void AYSCombatPostReceptacle::Interact(AYSPlayerCharacter* Character)
{
	if (!HasAuthority() || !CanInteract(Character))
	{
		return;
	}
	const AYSCombatPostItem* Item = Cast<AYSCombatPostItem>(Character->GetCarriedObject());
	if (InstallPost(Item->GetPostClass(), *FString::Printf(TEXT("posé par %s"), *Character->GetName())))
	{
		Character->ConsumeCarriedObject(TEXT("poste de combat posé"));
	}
}

bool AYSCombatPostReceptacle::InstallPost(TSubclassOf<AYSShipStation> PostClass, const TCHAR* Reason)
{
	if (!HasAuthority() || !PostClass || IsValid(InstalledPost))
	{
		return false;
	}
	const FTransform Transform = GetActorTransform();
	AYSShipStation* Post = GetWorld()->SpawnActorDeferred<AYSShipStation>(PostClass, Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Post)
	{
		return false;
	}
	Post->SetHostReceptacle(this);
	Post->FinishSpawning(Transform);
	// accroché au réceptacle : il suit le bateau (attachement répliqué par le moteur)
	Post->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	InstalledPost = Post;
	OnRep_InstalledPost();
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSCombatPost] %s : %s posé sur %s (%s)"), *GetNameSafe(GetShip()), *Post->GetName(), *GetName(), Reason);
	return true;
}

TSubclassOf<AYSShipStation> AYSCombatPostReceptacle::RemovePost(const TCHAR* Reason)
{
	if (!HasAuthority() || !IsValid(InstalledPost))
	{
		return nullptr;
	}
	TSubclassOf<AYSShipStation> PostClass = InstalledPost->GetClass();
	UE_LOG(LogTemp, Log, TEXT("[YSCombatPost] %s : %s retiré de %s (%s)"), *GetNameSafe(GetShip()), *InstalledPost->GetName(), *GetName(), Reason);
	InstalledPost->Destroy();
	InstalledPost = nullptr;
	OnRep_InstalledPost();
	ForceNetUpdate();
	return PostClass;
}

bool AYSCombatPostReceptacle::CanDismount(const AYSPlayerCharacter* Character) const
{
	const AYSShipStation* Post = InstalledPost;
	return Post && Post->GetCombatPostItemClass() && Post->IsFree() && !Post->IsBroken() && !Post->IsRepairing() && Character && Character->IsAlive()
		&& !Character->IsCarrying() && !Character->IsAtStation() && !Character->IsSwimming();
}

void AYSCombatPostReceptacle::Dismount(AYSPlayerCharacter* Character)
{
	if (!HasAuthority() || !CanDismount(Character))
	{
		return;
	}
	const TSubclassOf<AYSInteractiveObject> ItemClass = InstalledPost->GetCombatPostItemClass();
	const FVector Location = InstalledPost->GetActorLocation();
	RemovePost(*FString::Printf(TEXT("démonté par %s"), *Character->GetName()));
	AYSInteractiveObject* Item = AYSInteractiveObject::SpawnOnGround(GetWorld(), ItemClass, Location, GetActorRotation().Yaw);
	if (Item)
	{
		Character->StartCarrying(Item);
	}
}
