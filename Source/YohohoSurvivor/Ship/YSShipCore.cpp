#include "Ship/YSShipCore.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/YSCombat.h"
#include "Core/YSFlowMode.h"
#include "Core/YSGameplayTags.h"
#include "Engine/StaticMesh.h"
#include "Interaction/YSPlayerCorpse.h"
#include "Island/YSIsland.h"
#include "Materials/MaterialInterface.h"
#include "Player/YSPlayerCharacter.h"
#include "Ship/YSPirateShip.h"
#include "UObject/ConstructorHelpers.h"

AYSShipCore::AYSShipCore()
{
	BrookerName = NSLOCTEXT("YS", "ShipCoreName", "Core du bateau");

	// zone Brooker + Medic au milieu du pont (doré et vert : les deux rôles)
	AcceptRange = 450.0f;
	ZoneColor = FLinearColor(0.55f, 0.85f, 0.2f);
	IndicatorColor = FLinearColor(0.55f, 0.85f, 0.2f);
	IndicatorHeight = 220.0f;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ZoneMaterialFinder(TEXT("/Game/Art/Materials/M_BrookerZone.M_BrookerZone"));
	if (ZoneMaterialFinder.Succeeded())
	{
		ZoneMaterial = ZoneMaterialFinder.Object;
	}

	// visuel provisoire : socle en cylindre, orbe lumineuse
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMaterialFinder(TEXT("/Game/Art/Materials/PH_materials/m_PH_monochrome_Brown.m_PH_monochrome_Brown"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OrbMaterialFinder(TEXT("/Game/Art/Materials/PH_materials/m_PH_emissive_turboGreen.m_PH_emissive_turboGreen"));

	CoreBase = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreBase"));
	CoreBase->SetupAttachment(GetRootComponent());
	CoreBase->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	CoreBase->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.8f));
	CoreBase->SetCollisionProfileName(TEXT("BlockAll"));
	CoreBase->SetMobility(EComponentMobility::Movable);

	CoreOrb = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreOrb"));
	CoreOrb->SetupAttachment(GetRootComponent());
	CoreOrb->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	CoreOrb->SetRelativeScale3D(FVector(0.7f));
	CoreOrb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoreOrb->SetMobility(EComponentMobility::Movable);

	if (CylinderFinder.Succeeded())
	{
		CoreBase->SetStaticMesh(CylinderFinder.Object);
	}
	if (SphereFinder.Succeeded())
	{
		CoreOrb->SetStaticMesh(SphereFinder.Object);
	}
	if (BaseMaterialFinder.Succeeded())
	{
		CoreBase->SetMaterial(0, BaseMaterialFinder.Object);
	}
	if (OrbMaterialFinder.Succeeded())
	{
		CoreOrb->SetMaterial(0, OrbMaterialFinder.Object);
	}

	// origine du core = pont (+210 au-dessus de la mer)
	CoreHurtbox = CreateDefaultSubobject<UBoxComponent>(TEXT("CoreHurtbox"));
	CoreHurtbox->SetupAttachment(GetRootComponent());
	CoreHurtbox->InitBoxExtent(FVector(120.0f, 120.0f, 300.0f));
	CoreHurtbox->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	CoreHurtbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoreHurtbox->SetGenerateOverlapEvents(false);
	CoreHurtbox->SetCanEverAffectNavigation(false);
}

void AYSShipCore::BeginPlay()
{
	Super::BeginPlay();

	const AYSPirateShip* Ship = AYSPirateShip::GetShipOf(this);
	if (Ship && !Ship->IsFoeShip())
	{
		YSCombat::ConfigureHurtbox(CoreHurtbox);
		// la hurtbox ne suit que les requêtes de combat : aucun overlap entretenu
		CoreHurtbox->SetGenerateOverlapEvents(false);

		// point de réparation du core (bateau en négatif), local comme le core
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = this;
		RepairPoint = GetWorld()->SpawnActor<AYSShipCoreRepair>(AYSShipCoreRepair::StaticClass(), GetActorTransform(), Params);
		if (RepairPoint)
		{
			RepairPoint->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		}
	}
}

void AYSShipCore::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	const AYSPirateShip* Ship = AYSPirateShip::GetShipOf(this);
	if (Ship && !Ship->IsFoeShip())
	{
		TagContainer.AddTag(YSTags::Team_Player);
	}
}

float AYSShipCore::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	AYSPirateShip* Ship = AYSPirateShip::GetShipOf(this);
	if (!HasAuthority() || !Ship || Ship->IsFoeShip() || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}
	Ship->TakeCoreDamage(DamageAmount, EventInstigator, DamageCauser ? DamageCauser->GetActorLocation() : GetActorLocation());
	return DamageAmount;
}

bool AYSShipCore::AcceptsObject(const AYSInteractiveObject* Object) const
{
	// Medic (cadavre), Brooker (objet qui vaut des doublons), ou zone de dépôt des objectifs des îles (bateau des joueurs)
	return Super::AcceptsObject(Object) || AYSBrookerNPC::AcceptsObject(Object) || IsIslandObjective(Object);
}

bool AYSShipCore::IsIslandObjective(const AYSInteractiveObject* Object) const
{
	const AYSPirateShip* Ship = AYSPirateShip::GetShipOf(this);
	if (!Object || !Ship || Ship->IsFoeShip())
	{
		return false;
	}
	// zones d'intérêt (serveur) : CoreBoat à ranger dans le stock, objet posé sur un Galion
	const AYSFlowMode* FlowMode = GetWorld()->GetAuthGameMode<AYSFlowMode>();
	if (FlowMode && (FlowMode->IsStockedCoreBoat(Object) || FlowMode->IsZoneItem(Object)))
	{
		return true;
	}
	return AYSIsland::FindExpecting(GetWorld(), Object, GetActorLocation()) != nullptr;
}

bool AYSShipCore::IsAvailable() const
{
	const AYSPirateShip* Ship = AYSPirateShip::GetShipOf(this);
	return !Ship || (Ship->IsPresent() && !Ship->IsFoeShip());
}

void AYSShipCore::ReceiveObject(AYSInteractiveObject* Object)
{
	if (Object && Object->IsA<AYSPlayerCorpse>())
	{
		UE_LOG(LogTemp, Log, TEXT("[YSShipCore] %s : cadavre %s -> résurrection"), *GetName(), *Object->GetName());
		Super::ReceiveObject(Object);
		return;
	}
	// zones d'intérêt : un CoreBoat part dans le stock (inventaire sous les plumes de phoenix), le bateau change d'étape quand il y en a assez
	AYSFlowMode* FlowMode = GetWorld()->GetAuthGameMode<AYSFlowMode>();
	if (FlowMode && FlowMode->IsStockedCoreBoat(Object))
	{
		UE_LOG(LogTemp, Log, TEXT("[YSShipCore] %s : %s posé dans la zone du bateau -> stock"), *GetName(), *Object->GetName());
		FlowMode->StoreCoreBoat(Object);
		return;
	}
	// objectif d'une île qui ne vaut pas de doublons (Charpentier, Medic, boussole...) : livré, il reste au bateau
	// (l'île a déjà été prévenue par TryBuy)
	if (Object && Object->GetDoubloonValue() <= 0)
	{
		// boussole : une de moins à trouver dans l'objectif en mer
		if (FlowMode && FlowMode->IsCompass(Object) && !Object->IsDelivered())
		{
			FlowMode->NotifyCompassDelivered(Object);
		}
		UE_LOG(LogTemp, Log, TEXT("[YSShipCore] %s : %s posé dans la zone du bateau (objectif)"), *GetName(), *Object->GetName());
		Object->OnDelivered();
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSShipCore] %s : %s -> doublons"), *GetName(), Object ? *Object->GetName() : TEXT("?"));
	AYSBrookerNPC::ReceiveObject(Object);
}

void AYSShipCore::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(RepairPoint))
	{
		RepairPoint->Destroy();
	}
	Super::EndPlay(EndPlayReason);
}

AYSShipCoreRepair::AYSShipCoreRepair()
{
	// local, créé sur chaque machine par le core (comme lui) : le serveur gère la réparation, les clients n'affichent que l'invite
	bReplicates = false;
	SetReplicatingMovement(false);
	bCarriable = false;
}

bool AYSShipCoreRepair::CanInteract(const AYSPlayerCharacter* Character) const
{
	const AYSPirateShip* Ship = AYSPirateShip::GetShipOf(this);
	return Ship && Ship->CanRepairCore() && Character && Character->IsAlive() && !Character->IsCarrying() && !Character->IsAtStation() && !Character->IsSwimming()
		&& !Character->IsRepairingShip();
}

void AYSShipCoreRepair::Interact(AYSPlayerCharacter* Character)
{
	if (CanInteract(Character))
	{
		Character->StartShipRepair(AYSPirateShip::GetShipOf(this));
	}
}
