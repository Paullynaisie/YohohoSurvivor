#include "Ship/YSLongboat.h"
#include "Components/ChildActorComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Interaction/YSInteractiveObject.h"
#include "Map/YSOcean.h"
#include "Player/YSPlayerCharacter.h"

namespace YSLongboat
{
	/** Recherche du sol d'éjection : depuis haut au-dessus de la mer (collines, pont du gaillard) */
	constexpr float LandingTraceUp = 2000.0f;
	constexpr float LandingTraceDown = 2000.0f;
}

AYSLongboat::AYSLongboat(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer
		.DoNotCreateDefaultSubobject(TEXT("CoreStation"))
		.DoNotCreateDefaultSubobject(TEXT("CrowsNestStation"))
		.DoNotCreateDefaultSubobject(TEXT("LongboatStation"))
		.DoNotCreateDefaultSubobject(TEXT("CannonFront"))
		.DoNotCreateDefaultSubobject(TEXT("CannonLeft"))
		.DoNotCreateDefaultSubobject(TEXT("CannonRight"))
		.DoNotCreateDefaultSubobject(TEXT("BoardingAccess")))
{
	ShipName = NSLOCTEXT("YS", "LongboatName", "Chaloupe");
	bObjectiveShip = false;
	bDrivable = true;

	// petite et nerveuse
	MaxForwardSpeed = 1100.0f;
	ForwardAcceleration = 700.0f;
	MaxReverseSpeed = 400.0f;
	ReverseAcceleration = 500.0f;
	Deceleration = 300.0f;
	MaxTurnRate = 90.0f;
	FullTurnSpeed = 250.0f;
	StandstillTurnRatio = 0.5f;
	TurnResponsiveness = 5.0f;
	MinWaterDepth = 40.0f;

	// coque de BP_Longboat : de -220 à 230 en X, 180 de large ; pont à +30
	HullProbePoints = {
		FVector2D(230.0f, 0.0f), FVector2D(150.0f, -90.0f), FVector2D(150.0f, 90.0f),
		FVector2D(-220.0f, -90.0f), FVector2D(-220.0f, 90.0f), FVector2D(-220.0f, 0.0f)
	};

	// pilote à l'arrière, passager à l'avant
	UChildActorComponent* Helm = GetHelmStationComponent();
	Helm->SetChildActorClass(AYSLongboatHelm::StaticClass());
	Helm->SetRelativeLocation(FVector(-150.0f, 0.0f, 30.0f));

	PassengerStation = CreateDefaultSubobject<UChildActorComponent>(TEXT("PassengerStation"));
	PassengerStation->SetupAttachment(GetRootComponent());
	PassengerStation->SetChildActorClass(AYSLongboatSeat::StaticClass());
	PassengerStation->SetRelativeLocation(FVector(90.0f, 0.0f, 30.0f));
}

void AYSLongboat::BeginPlay()
{
	Super::BeginPlay();

	SpawnTime = GetWorld()->GetTimeSeconds();
}

AYSLongboatSeat* AYSLongboat::GetPassengerSeat() const
{
	return PassengerStation ? Cast<AYSLongboatSeat>(PassengerStation->GetChildActor()) : nullptr;
}

TArray<AYSPlayerCharacter*> AYSLongboat::GetOccupants() const
{
	TArray<AYSPlayerCharacter*> Occupants;
	for (const AYSShipStation* Station : GetStations())
	{
		if (AYSPlayerCharacter* Character = Station->GetOccupant(); IsValid(Character))
		{
			Occupants.AddUnique(Character);
		}
	}
	return Occupants;
}

void AYSLongboat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || bEjected || IsActorBeingDestroyed())
	{
		return;
	}

	// foncé sur un bateau : joueurs posés sur son pont
	if (AYSPirateShip* Ship = FindContactShip())
	{
		FVector Landing;
		if (!FindShipLanding(Ship, Landing))
		{
			Landing = Ship->GetClosestHullPoint(GetActorLocation(), ShipLandingInset) + FVector(0.0f, 0.0f, 250.0f);
		}
		const FVector Facing = (Ship->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		UE_LOG(LogTemp, Log, TEXT("[YSLongboat] %s fonce sur %s : éjection sur le pont"), *GetName(), *Ship->GetName());
		EjectAndDestroy(Landing, Facing, TEXT("bateau"));
		return;
	}

	// plus de pilote : la chaloupe disparaît (le passager tombe à l'eau)
	const AYSShipHelm* Helm = GetHelm();
	if (DisappearsWithoutPilot() && (!Helm || !Helm->GetOccupant()))
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (EmptySinceTime < 0.0)
		{
			EmptySinceTime = Now;
		}
		else if (Now - EmptySinceTime >= EmptyLifetime)
		{
			UE_LOG(LogTemp, Log, TEXT("[YSLongboat] %s sans pilote : disparaît"), *GetName());
			bEjected = true;
			for (AYSPlayerCharacter* Character : GetOccupants())
			{
				Character->LeaveStation(TEXT("chaloupe sans pilote"), true);
			}
			Destroy();
		}
	}
	else
	{
		EmptySinceTime = -1.0;
	}
}

void AYSLongboat::OnHullBlocked()
{
	if (!HasAuthority() || bEjected)
	{
		return;
	}

	// foncé sur une île : joueurs posés sur la terre ferme devant
	FVector Landing;
	if (FindLandLanding(Landing))
	{
		UE_LOG(LogTemp, Log, TEXT("[YSLongboat] %s touche le fond : éjection sur l'île en %s"), *GetName(), *Landing.ToCompactString());
		EjectAndDestroy(Landing, GetActorForwardVector().GetSafeNormal2D(), TEXT("île"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[YSLongboat] %s touche le fond : pas de terre ferme devant, éjection sur place"), *GetName());
		EjectAndDestroy(GetActorLocation() + FVector(0.0f, 0.0f, 60.0f), GetActorForwardVector().GetSafeNormal2D(), TEXT("fond"));
	}
}

AYSPirateShip* AYSLongboat::FindContactShip() const
{
	if (GetWorld()->GetTimeSeconds() - SpawnTime < ShipContactGraceTime)
	{
		return nullptr;
	}

	TArray<FVector> Points;
	GetWorldHullPoints(Points);
	for (TActorIterator<AYSPirateShip> It(GetWorld()); It; ++It)
	{
		AYSPirateShip* Ship = *It;
		if (Ship == this || Ship->IsFoeShip() || !Ship->IsPresent() || Ship->IsActorBeingDestroyed())
		{
			continue;
		}
		for (const FVector& Point : Points)
		{
			if (Ship->IsInsideHull(Point, ShipContactMargin))
			{
				return Ship;
			}
		}
	}
	return nullptr;
}

bool AYSLongboat::FindShipLanding(const AYSPirateShip* Ship, FVector& OutLocation) const
{
	const FVector Point = Ship->GetClosestHullPoint(GetActorLocation(), ShipLandingInset);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSLongboatShipLanding), false, this);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Point + FVector(0.0f, 0.0f, YSLongboat::LandingTraceUp), Point - FVector(0.0f, 0.0f, YSLongboat::LandingTraceDown),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params) && AYSPirateShip::GetShipOf(Hit.GetActor()) == Ship)
	{
		OutLocation = Hit.ImpactPoint;
		return true;
	}
	return false;
}

bool AYSLongboat::FindLandLanding(FVector& OutLocation) const
{
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	FBox2D Bounds(ForceInit);
	for (const FVector2D& Probe : HullProbePoints)
	{
		Bounds += Probe;
	}
	const FVector Bow = GetActorLocation() + Forward * (Bounds.bIsValid ? Bounds.Max.X : 0.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSLongboatLandLanding), false, this);
	for (int32 Step = 0; Step <= LandSearchSteps; ++Step)
	{
		const FVector Point = Bow + Forward * (LandSearchStep * Step);
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, Point + FVector(0.0f, 0.0f, YSLongboat::LandingTraceUp), Point - FVector(0.0f, 0.0f, YSLongboat::LandingTraceDown),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			continue;
		}
		if (AYSInteractiveObject::IsSafeDropLocation(GetWorld(), Hit.ImpactPoint) && !AYSPirateShip::GetShipOf(Hit.GetActor()))
		{
			OutLocation = Hit.ImpactPoint;
			return true;
		}
	}
	return false;
}

void AYSLongboat::EjectAndDestroy(const FVector& Location, const FVector& Facing, const TCHAR* Reason)
{
	bEjected = true;
	const TArray<AYSPlayerCharacter*> Occupants = GetOccupants();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Facing.IsNearlyZero() ? GetActorForwardVector() : Facing).GetSafeNormal2D();
	for (int32 Index = 0; Index < Occupants.Num(); ++Index)
	{
		AYSPlayerCharacter* Character = Occupants[Index];
		Character->LeaveStation(TEXT("éjecté de la chaloupe"), true);
		const float Offset = Occupants.Num() > 1 ? (Index - 0.5f * (Occupants.Num() - 1)) * EjectSpacing : 0.0f;
		Character->TeleportToFeetLocation(Location + Side * Offset, Facing);
		UE_LOG(LogTemp, Log, TEXT("[YSLongboat] %s éjecté (%s) en %s"), *Character->GetName(), Reason, *(Location + Side * Offset).ToCompactString());
	}
	Destroy();
}

AYSLongboatHelm::AYSLongboatHelm()
{
	DisplayName = NSLOCTEXT("YS", "LongboatHelmName", "Chaloupe");
	bCanLeave = false;

	// origine = pieds du pilote (arrière de la chaloupe), mains devant lui sur la barre franche
	GetOccupantPoint()->SetRelativeLocation(FVector::ZeroVector);
	GetHandPoint(true)->SetRelativeLocation(FVector(45.0f, -22.0f, 85.0f));
	GetHandPoint(false)->SetRelativeLocation(FVector(45.0f, 22.0f, 85.0f));
}

AYSLongboatSeat::AYSLongboatSeat()
{
	DisplayName = NSLOCTEXT("YS", "LongboatSeatName", "Chaloupe (passager)");
	bCanLeave = false;
	bAllowSwimmingEntry = true;
	bHandsOnStation = false;
	bLockAim = false;
	CameraArmLength = 3600.0f;
	bCameraOnShip = true;
	CameraOffsetMode = EYSStationCameraOffset::None;
}

bool AYSLongboatSeat::CanInteract(const AYSPlayerCharacter* Character) const
{
	// seulement une chaloupe qui a un pilote
	const AYSPirateShip* Ship = GetShip();
	const AYSShipHelm* Helm = Ship ? Ship->GetHelm() : nullptr;
	return Helm && Helm->GetOccupant() && Super::CanInteract(Character);
}
