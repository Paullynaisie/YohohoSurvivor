#include "Interaction/YSInteractiveObject.h"
#include "Core/YSGameMode.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Island/YSIsland.h"
#include "Map/YSOcean.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "Ship/YSPirateShip.h"
#include "UI/YSInteractPromptWidget.h"

namespace YSInteractiveObject
{
	/** Recherche du sol : depuis un peu au-dessus du point jusqu'à loin en dessous */
	constexpr float GroundTraceUp = 200.0f;
	constexpr float GroundTraceDown = 5000.0f;
	/** Objet sous l'eau : vérifié toutes les ..., à partir de cette profondeur sous la surface */
	constexpr float UnderwaterCheckInterval = 0.25f;
	constexpr float UnderwaterTolerance = 50.0f;
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
	DOREPLIFETIME(AYSInteractiveObject, bDelivered);
	DOREPLIFETIME(AYSInteractiveObject, bIndicatorRevealed);
}

void AYSInteractiveObject::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// coffres, postes de combat x1.5, cartes au trésor, boussoles x2 (demande de Paul du 2026-11-10) ; en jeu seulement
	const UWorld* World = GetWorld();
	// (un client reçoit l'échelle du serveur à l'apparition de l'acteur répliqué : seulement le serveur, et les objets posés dans la map)
	if (World && World->IsGameWorld() && !FMath::IsNearlyEqual(ItemScale, 1.0f) && (HasAuthority() || IsNetStartupActor()))
	{
		SetActorScale3D(GetActorScale3D() * ItemScale);
	}
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

	// objectif : remonte à la surface s'il finit sous l'eau (île ou bateau qui coule)
	if (HasAuthority() && RisesToSurface())
	{
		GetWorldTimerManager().SetTimer(UnderwaterTimer, this, &AYSInteractiveObject::CheckUnderwater, YSInteractiveObject::UnderwaterCheckInterval, true,
			FMath::FRandRange(0.0f, YSInteractiveObject::UnderwaterCheckInterval));
	}
	// bulle révélée à l'approche d'un joueur (serveur)
	if (HasAuthority() && bShowIndicator && bRevealIndicatorNearby)
	{
		GetWorldTimerManager().SetTimer(IndicatorTimer, this, &AYSInteractiveObject::UpdateIndicatorReveal, YSInteractiveObject::UnderwaterCheckInterval, true,
			FMath::FRandRange(0.0f, YSInteractiveObject::UnderwaterCheckInterval));
	}
}

void AYSInteractiveObject::UpdateIndicatorReveal()
{
	if (IsActorBeingDestroyed())
	{
		return;
	}
	const AYSOcean* Ocean = AYSOcean::Find(GetWorld());
	const bool bUnderwater = !Carrier && Ocean && GetActorLocation().Z < Ocean->GetSeaLevel() - YSInteractiveObject::UnderwaterTolerance;
	float NearestSq = TNumericLimits<float>::Max();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AYSPlayerCharacter* Character = It->Get() ? Cast<AYSPlayerCharacter>(It->Get()->GetPawn()) : nullptr;
		if (Character && Character->IsAlive())
		{
			NearestSq = FMath::Min(NearestSq, static_cast<float>(FVector::DistSquared(Character->GetActorLocation(), GetActorLocation())));
		}
	}
	const bool bRevealed = bUnderwater ? false
		: bIndicatorRevealed ? NearestSq <= FMath::Square(IndicatorHideDistance)
		: NearestSq <= FMath::Square(IndicatorRevealDistance);
	if (bRevealed != bIndicatorRevealed)
	{
		bIndicatorRevealed = bRevealed;
		ForceNetUpdate();
		UE_LOG(LogTemp, Verbose, TEXT("[YSCarry] %s : bulle %s"), *GetName(), bRevealed ? TEXT("révélée") : bUnderwater ? TEXT("cachée (sous l'eau)") : TEXT("cachée (joueurs trop loin)"));
	}
}

void AYSInteractiveObject::CheckUnderwater()
{
	const AYSOcean* Ocean = AYSOcean::Find(GetWorld());
	if (!Ocean || Carrier || bDelivered || IsActorBeingDestroyed())
	{
		return;
	}
	const FVector Location = GetActorLocation();
	const float Sea = Ocean->GetSeaLevel();
	if (Location.Z >= Sea - YSInteractiveObject::UnderwaterTolerance)
	{
		return;
	}
	const AActor* Parent = GetAttachParentActor();
	// accroché à une île qui coule : il coule avec elle (demande de Paul du 2026-11-10)
	for (const AActor* Ancestor = Parent; Ancestor; Ancestor = Ancestor->GetAttachParentActor())
	{
		if (Ancestor->IsA<AYSIsland>())
		{
			return;
		}
	}
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorLocationAndRotation(FVector(Location.X, Location.Y, Sea), FRotator(0.0f, GetActorRotation().Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSCarry] %s sous l'eau (%.0f cm sous la surface%s) : remonte et flotte en %s"), *GetName(), Sea - Location.Z,
		Parent ? *FString::Printf(TEXT(", était accroché à %s"), *Parent->GetName()) : TEXT(""), *GetActorLocation().ToCompactString());
}

bool AYSInteractiveObject::CanInteract(const AYSPlayerCharacter* Character) const
{
	return !bDelivered && bCarriable && !Carrier && Character && Character->IsAlive() && !Character->IsCarrying();
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
		// le passif du porteur peut imposer sa place (Quartermaster : sur le flanc)
		FVector Location = CarryLocation;
		FRotator Rotation = CarryRotation;
		Carrier->GetCarryPlacement(Location, Rotation);
		SetActorRelativeLocation(Location);
		SetActorRelativeRotation(Rotation);
		// échelle du monde inchangée : sinon elle est multipliée par celle du porteur (personnage x1.25), et le reste une fois posé
		SetActorScale3D(OriginalScale);
	}
	else
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		SetActorLocationAndRotation(DropLocation, FRotator(0.0f, DropYaw, 0.0f));
		SetActorScale3D(OriginalScale);
		AttachToShipBelow();
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
	OnCarrierChanged();
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
	AYSInteractiveObject* Object = World->SpawnActor<AYSInteractiveObject>(ObjectClass, FindGround(World, Location, nullptr), FRotator(0.0f, Yaw, 0.0f), Params);
	if (Object)
	{
		Object->AttachToShipBelow();
	}
	return Object;
}

void AYSInteractiveObject::OnDelivered()
{
	AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	if (GameMode && DoubloonValue > 0)
	{
		GameMode->AddDoubloons(DoubloonValue);
		if (PhoenixFeatherValue > 0)
		{
			GameMode->AddPhoenixFeathers(PhoenixFeatherValue);
		}
		Destroy();
		return;
	}
	bDelivered = true;
	SetPromptVisible(false);
	ForceNetUpdate();
}

void AYSInteractiveObject::AttachToShipBelow()
{
	// posé sur le pont d'un bateau : il voyage avec lui (attachement répliqué)
	if (AYSPirateShip* Ship = HasAuthority() ? AYSPirateShip::FindShipBelow(GetWorld(), GetActorLocation(), this) : nullptr)
	{
		AttachToActor(Ship, FAttachmentTransformRules::KeepWorldTransform);
		UE_LOG(LogTemp, Log, TEXT("[YSCarry] %s posé sur %s : voyage avec lui"), *GetName(), *Ship->GetName());
	}
}
