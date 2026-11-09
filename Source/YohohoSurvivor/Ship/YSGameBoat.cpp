#include "Ship/YSGameBoat.h"
#include "Camera/CameraComponent.h"
#include "Components/ChildActorComponent.h"
#include "Core/YSFlowMode.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"

AYSGameBoatLongboat::AYSGameBoatLongboat(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("PassengerStation")))
{
	ShipName = NSLOCTEXT("YS", "GameBoatLongboatName", "Chaloupe");
	bDrivable = true;

	// proue jusqu'au bord de l'eau : coque réelle de BP_Longboat, tirant d'eau presque nul
	MinWaterDepth = 5.0f;
	HullProbePoints = {
		FVector2D(82.0f, 0.0f), FVector2D(0.0f, -90.0f), FVector2D(0.0f, 90.0f),
		FVector2D(-218.0f, -90.0f), FVector2D(-218.0f, 90.0f), FVector2D(-218.0f, 0.0f)
	};

	// deux places entre les bancs, deux de chaque côté de la barre franche (pont à +30)
	const auto CreateSeat = [this](const TCHAR* Name, const FVector& Location)
	{
		UChildActorComponent* Seat = CreateDefaultSubobject<UChildActorComponent>(Name);
		Seat->SetupAttachment(GetRootComponent());
		Seat->SetChildActorClass(AYSGameBoatSeat::StaticClass());
		Seat->SetRelativeLocation(Location);
		return Seat;
	};
	Seat1 = CreateSeat(TEXT("Seat1"), FVector(-30.0f, -42.0f, 32.0f));
	Seat2 = CreateSeat(TEXT("Seat2"), FVector(-30.0f, 42.0f, 32.0f));
	Seat3 = CreateSeat(TEXT("Seat3"), FVector(-155.0f, -42.0f, 32.0f));
	Seat4 = CreateSeat(TEXT("Seat4"), FVector(-155.0f, 42.0f, 32.0f));

	// vue de dessus, proue en haut de l'écran
	IntroCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("IntroCamera"));
	IntroCamera->SetupAttachment(GetRootComponent());
	IntroCamera->SetRelativeLocationAndRotation(FVector(-900.0f, 0.0f, 800.0f), FRotator(-38.0f, 0.0f, 0.0f));
}

void AYSGameBoatLongboat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSGameBoatLongboat, bLanded);
}

UChildActorComponent* AYSGameBoatLongboat::GetSeatComponent(int32 Seat) const
{
	switch (Seat)
	{
	case 0: return Seat1;
	case 1: return Seat2;
	case 2: return Seat3;
	case 3: return Seat4;
	default: return nullptr;
	}
}

FTransform AYSGameBoatLongboat::GetSeatFeetTransform(int32 Seat) const
{
	const UChildActorComponent* Component = GetSeatComponent(Seat);
	return Component ? Component->GetComponentTransform() : GetActorTransform();
}

AYSGameBoatSeat* AYSGameBoatLongboat::GetSeat(int32 Seat) const
{
	const UChildActorComponent* Component = GetSeatComponent(Seat);
	return Component ? Cast<AYSGameBoatSeat>(Component->GetChildActor()) : nullptr;
}

void AYSGameBoatLongboat::StartSailing()
{
	if (HasAuthority() && !bSailing && !bLanded)
	{
		bSailing = true;
		UE_LOG(LogTemp, Log, TEXT("[YSGameBoat] %s part de %s (%d joueur(s) à bord)"), *GetName(), *GetActorLocation().ToCompactString(), GetOccupants().Num());
	}
}

bool AYSGameBoatLongboat::ComputeAutoPilot(float& OutThrottle, float& OutSteer)
{
	if (!bSailing)
	{
		return false;
	}
	OutThrottle = 1.0f;
	OutSteer = 0.0f;
	return true;
}

void AYSGameBoatLongboat::OnHullBlocked()
{
	if (!HasAuthority() || !bSailing)
	{
		return;
	}
	bSailing = false;
	bLanded = true;
	bEjected = true;
	ForceNetUpdate();

	FVector Landing;
	if (!FindLandLanding(Landing))
	{
		Landing = GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
	}
	const FVector Facing = GetActorForwardVector().GetSafeNormal2D();
	UE_LOG(LogTemp, Log, TEXT("[YSGameBoat] %s arrive à la plage en %s : débarquement en %s"), *GetName(), *GetActorLocation().ToCompactString(), *Landing.ToCompactString());
	EjectOccupants(Landing, Facing, TEXT("plage"));
	if (AYSFlowMode* FlowMode = GetWorld()->GetAuthGameMode<AYSFlowMode>())
	{
		FlowMode->OnGameBoatLanded(this, Landing);
	}
}

AYSGameBoatSeat::AYSGameBoatSeat()
{
	DisplayName = NSLOCTEXT("YS", "GameBoatSeatName", "Chaloupe");
	bCanLeave = false;
	bHandsOnStation = false;
	bLockAim = false;
	// caméra du joueur à pied pendant la traversée
	CameraArmLength = 1680.0f;
	bCameraOnShip = false;
	CameraOffsetMode = EYSStationCameraOffset::None;
}

AYSSloop::AYSSloop(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("CannonFront")))
{
	// demande de Paul (2026-11-09) : 4 réceptacles à poste de combat = les 2 emplacements à l'avant (Cannon Left / Cannon Right, tournés de 15°
	// vers l'extérieur) + 1 de chaque côté (Cannon Left 2 / Cannon Right 2) ; placés par Tools/EditorScripts/setup_combat_posts.py
	ShipName = NSLOCTEXT("YS", "SloopName", "Sloop");
	bObjectiveShip = false;
	bDrivable = true;

	// proue jusqu'au bord de l'eau : coque réelle de BP_Longboat, tirant d'eau presque nul
	MinWaterDepth = 5.0f;
	HullProbePoints = {
		FVector2D(82.0f, 0.0f), FVector2D(0.0f, -90.0f), FVector2D(0.0f, 90.0f),
		FVector2D(-218.0f, -90.0f), FVector2D(-218.0f, 90.0f), FVector2D(-218.0f, 0.0f)
	};
}

AYSBrigantin::AYSBrigantin(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// demande de Paul (2026-11-09) : 5 réceptacles à poste de combat = 1 à l'avant au centre (Cannon Front) + 2 de chaque côté
	// (Cannon Left / Left 2, Cannon Right / Right 2) ; placés par Tools/EditorScripts/setup_combat_posts.py
	ShipName = NSLOCTEXT("YS", "BrigantinName", "Brigantin");
	bObjectiveShip = false;
	bDrivable = true;
}
