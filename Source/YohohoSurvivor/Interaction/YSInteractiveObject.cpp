#include "Interaction/YSInteractiveObject.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Map/YSOcean.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "UI/YSInteractPromptWidget.h"

namespace YSInteractiveObject
{
	/** Recherche du sol : depuis un peu au-dessus du point jusqu'à loin en dessous */
	constexpr float GroundTraceUp = 200.0f;
	constexpr float GroundTraceDown = 5000.0f;
}

AYSInteractiveObject::AYSInteractiveObject()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	LeftHandPoint = CreateDefaultSubobject<USceneComponent>(TEXT("LeftHandPoint"));
	LeftHandPoint->SetupAttachment(Root);
	LeftHandPoint->SetRelativeLocation(FVector(0.0f, -40.0f, 26.0f));

	RightHandPoint = CreateDefaultSubobject<USceneComponent>(TEXT("RightHandPoint"));
	RightHandPoint->SetupAttachment(Root);
	RightHandPoint->SetRelativeLocation(FVector(0.0f, 40.0f, 26.0f));

	InteractPrompt = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractPrompt"));
	InteractPrompt->SetupAttachment(Root);
	InteractPrompt->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	InteractPrompt->SetWidgetSpace(EWidgetSpace::Screen);
	InteractPrompt->SetDrawAtDesiredSize(true);
	InteractPrompt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractPrompt->SetVisibility(false);

	PromptWidgetClass = UYSInteractPromptWidget::StaticClass();
}

void AYSInteractiveObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSInteractiveObject, Carrier);
}

void AYSInteractiveObject::BeginPlay()
{
	Super::BeginPlay();

	OriginalScale = GetActorScale3D();
	LastSafeLocation = GetActorLocation();

	if (PromptWidgetClass)
	{
		InteractPrompt->SetWidgetClass(PromptWidgetClass);
	}
	ApplyCarryState();
}

bool AYSInteractiveObject::CanInteract(const AYSPlayerCharacter* Character) const
{
	return bCarriable && !Carrier && Character && Character->IsAlive() && !Character->IsCarrying();
}

void AYSInteractiveObject::Interact(AYSPlayerCharacter* Character)
{
	if (HasAuthority() && CanInteract(Character))
	{
		Character->StartCarrying(this);
	}
}

void AYSInteractiveObject::SetCarrier(AYSPlayerCharacter* NewCarrier, const FVector& DropLocation, float DropYaw)
{
	if (!HasAuthority())
	{
		return;
	}

	Carrier = NewCarrier;
	if (Carrier)
	{
		// porté devant le corps : suit l'inclinaison et le sautillement du porteur
		AttachToComponent(Carrier->GetBodyPivot(), FAttachmentTransformRules::KeepRelativeTransform);
		SetActorRelativeLocation(CarryLocation);
		SetActorRelativeRotation(CarryRotation);
		// échelle du monde inchangée : sinon elle est multipliée par celle du porteur (personnage x1.25), et le reste une fois posé
		SetActorScale3D(OriginalScale);
	}
	else
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		SetActorLocationAndRotation(DropLocation, FRotator(0.0f, DropYaw, 0.0f));
		SetActorScale3D(OriginalScale);
	}
	ApplyCarryState();
	ForceNetUpdate();
}

void AYSInteractiveObject::OnRep_Carrier()
{
	ApplyCarryState();
}

void AYSInteractiveObject::ApplyCarryState()
{
	// porté : ne bloque ni son porteur ni les autres
	SetActorEnableCollision(Carrier == nullptr);
	if (Carrier)
	{
		SetPromptVisible(false);
	}
}

void AYSInteractiveObject::SetPromptVisible(bool bVisible)
{
	if (InteractPrompt->IsVisible() != bVisible)
	{
		InteractPrompt->SetVisibility(bVisible);
	}
}

FVector AYSInteractiveObject::FindGround(const UWorld* World, const FVector& Location, const AActor* IgnoredActor)
{
	if (!World)
	{
		return Location;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSFindGround), false, IgnoredActor);
	const FVector Start = Location + FVector(0.0f, 0.0f, YSInteractiveObject::GroundTraceUp);
	const FVector End = Location - FVector(0.0f, 0.0f, YSInteractiveObject::GroundTraceDown);
	return World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params) ? Hit.ImpactPoint : Location;
}

bool AYSInteractiveObject::IsSafeDropLocation(const UWorld* World, const FVector& Ground)
{
	const AWorldSettings* WorldSettings = World ? World->GetWorldSettings() : nullptr;
	return World && !AYSOcean::IsUnderwater(World, Ground) && (!WorldSettings || !WorldSettings->bEnableWorldBoundsChecks || Ground.Z > WorldSettings->KillZ);
}

AYSInteractiveObject* AYSInteractiveObject::SpawnOnGround(UWorld* World, TSubclassOf<AYSInteractiveObject> ObjectClass, const FVector& Location, float Yaw)
{
	if (!World || !ObjectClass)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AYSInteractiveObject>(ObjectClass, FindGround(World, Location, nullptr), FRotator(0.0f, Yaw, 0.0f), Params);
}
