#include "Enemies/YSFoeJollyShip.h"
#include "Components/CapsuleComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/WidgetComponent.h"
#include "Enemies/YSEnemySpawnRule.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Interaction/YSInteractiveObject.h"
#include "Map/YSGroundZoneComponent.h"
#include "Map/YSOcean.h"
#include "Player/YSPlayerCharacter.h"

namespace YSJollyShip
{
	/** Recherche du fond à l'apparition : depuis au-dessus des collines (une trace partie de plus bas ne voit pas le Landscape au-dessus d'elle) jusqu'à loin sous la mer */
	constexpr float DepthTraceUp = 2000.0f;
	constexpr float DepthTraceDown = 3000.0f;

	/** Apparition : profondeur en plus du tirant d'eau, distance minimale à la coque d'un autre bateau */
	constexpr float SpawnExtraDepth = 100.0f;
	constexpr float SpawnShipMargin = 500.0f;

	/** Pendant le tour, le fond devant n'est vérifié que de temps en temps (un bateau = plusieurs traces) */
	constexpr double ShoreCheckInterval = 0.25;

	/** Tour : force de la correction vers le cercle (écart relatif au rayon -> part de la direction vers la cible) */
	constexpr float CircleRadialGain = 4.0f;
}

AYSFoeJollyHull::AYSFoeJollyHull()
{
	FoeName = NSLOCTEXT("YS", "JollyHullName", "Jolly Ship");

	// PV du bateau ; ni XP (celle de l'équipage), ni attaque, ni apparition, ni ragdoll
	BaseHealth = 500.0f;
	HealthPerLevel = 100.0f;
	BaseExperience = 0.0f;
	ExperiencePerLevel = 0.0f;
	BaseContactDamage = 0.0f;
	ContactDamagePerLevel = 0.0f;
	bSpawnRise = false;
	bDeathRagdoll = false;

	// capsule (sans collision à bord) = rayon de la marque au sol ; hurtbox = cylindre aplati de la taille de la coque, de part et d'autre de
	// la ligne de flottaison (les balles volent à hauteur d'homme au-dessus de la mer)
	GetCapsuleComponent()->InitCapsuleSize(110.0f, 96.0f);
	GetHurtbox()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
	GetHurtbox()->SetRelativeScale3D(FVector(4.6f, 1.9f, 2.0f));
}

void AYSFoeJollyHull::BeginPlay()
{
	Super::BeginPlay();

	// invisible (le bateau se voit) mais toujours touchable ; la barre de vie et la marque au sol restent
	TInlineComponentArray<USceneComponent*> Components(this);
	for (USceneComponent* Component : Components)
	{
		if (!Component->IsA<UWidgetComponent>() && !Component->IsA<UYSGroundZoneComponent>())
		{
			Component->SetHiddenInGame(true);
		}
	}
}

void AYSFoeJollyHull::Die()
{
	AYSFoeJollyShip* Ship = ShipRide.Ship;
	Super::Die();
	if (Ship)
	{
		Ship->OnHullDestroyed();
	}
}

AYSFoeJollyShip::AYSFoeJollyShip(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ShipName = NSLOCTEXT("YS", "JollyShipName", "Jolly Ship");
	HullClass = AYSFoeJollyHull::StaticClass();

	// pas de poste pour les joueurs : l'équipage est gardé sur les places ci-dessous
	GetHelmStationComponent()->SetChildActorClass(nullptr);
	GetPassengerStationComponent()->SetChildActorClass(nullptr);

	// beaucoup de bateaux : les clients extrapolent entre deux états
	SetNetUpdateFrequency(10.0f);

	// mêmes emplacements que la barre et la place passager de la chaloupe (pont à +30)
	PilotSeat = CreateDefaultSubobject<USceneComponent>(TEXT("PilotSeat"));
	PilotSeat->SetupAttachment(GetRootComponent());
	PilotSeat->SetRelativeLocation(FVector(-150.0f, 0.0f, 30.0f));
	LeftSeat = CreateDefaultSubobject<USceneComponent>(TEXT("LeftSeat"));
	LeftSeat->SetupAttachment(GetRootComponent());
	LeftSeat->SetRelativeLocation(FVector(70.0f, -45.0f, 30.0f));
	RightSeat = CreateDefaultSubobject<USceneComponent>(TEXT("RightSeat"));
	RightSeat->SetupAttachment(GetRootComponent());
	RightSeat->SetRelativeLocation(FVector(70.0f, 45.0f, 30.0f));
}

void AYSFoeJollyShip::BeginPlay()
{
	Super::BeginPlay();

	// rien ne s'y pose ni ne s'y arrête : les balles passent et touchent la hurtbox de la coque ou l'équipage
	SetActorEnableCollision(false);

	if (!HasAuthority())
	{
		return;
	}
	CircleSide = FMath::RandBool() ? 1.0f : -1.0f;
	PhaseStartTime = GetWorld()->GetTimeSeconds();

	const FTransform HullTransform(GetActorRotation(), GetSeatFeetLocation(SeatHull) + FVector(0.0f, 0.0f, 100.0f));
	HullFoe = GetWorld()->SpawnActorDeferred<AYSFoeJollyHull>(HullClass, HullTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (HullFoe)
	{
		HullFoe->StartShipRide(this, SeatHull, false, 1.0f, 1.0f);
		HullFoe->FinishSpawning(HullTransform);
	}
	SpawnCrewMember(PilotFoeClasses, SeatPilot, false);
	SpawnCrewMember(LeftSeatFoeClasses, SeatLeft, true);
	SpawnCrewMember(RightSeatFoeClasses, SeatRight, true);
	UE_LOG(LogTemp, Log, TEXT("[YSJollyShip] %s apparaît en (%.0f, %.0f) : %.0f PV, %d NME à bord"), *GetName(), GetActorLocation().X, GetActorLocation().Y, HullFoe ? HullFoe->GetMaxHealth() : 0.0f, Crew.Num());
}

void AYSFoeJollyShip::SpawnCrewMember(const TArray<TSubclassOf<AYSFoeCharacter>>& Classes, int32 Seat, bool bCanAttack)
{
	TArray<TSubclassOf<AYSFoeCharacter>> ValidClasses = Classes;
	ValidClasses.RemoveAll([](const TSubclassOf<AYSFoeCharacter>& Class) { return !Class; });
	if (ValidClasses.Num() == 0)
	{
		return;
	}

	const FTransform SpawnTransform(GetActorRotation(), GetSeatFeetLocation(Seat) + FVector(0.0f, 0.0f, 100.0f));
	AYSFoeCharacter* Foe = GetWorld()->SpawnActorDeferred<AYSFoeCharacter>(UYSEnemySpawnRule::PickEnemyClass(ValidClasses), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Foe)
	{
		Foe->StartShipRide(this, Seat, bCanAttack, CrewAttackRangeMultiplier, CrewAmmoLifeTimeMultiplier);
		Foe->FinishSpawning(SpawnTransform);
		Crew.Add(Foe);
	}
}

void AYSFoeJollyShip::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && EndPlayReason == EEndPlayReason::Destroyed)
	{
		ReleaseCrew(false);
	}
	Super::EndPlay(EndPlayReason);
}

void AYSFoeJollyShip::ReleaseCrew(bool bExperienceInWater)
{
	for (AYSFoeCharacter* Foe : Crew)
	{
		if (IsValid(Foe))
		{
			Foe->EndShipRide(Foe->GetActorLocation() - FVector(0.0f, 0.0f, Foe->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), bExperienceInWater);
		}
	}
	Crew.Reset();
	if (IsValid(HullFoe) && !HullFoe->IsActorBeingDestroyed())
	{
		HullFoe->Destroy();
	}
	HullFoe = nullptr;
}

void AYSFoeJollyShip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || bEjected || IsActorBeingDestroyed())
	{
		return;
	}

	// plus personne à bord : la chaloupe vide n'a plus de raison d'être
	const bool bCrewAlive = Crew.ContainsByPredicate([](const AYSFoeCharacter* Foe) { return IsValid(Foe) && !Foe->IsDying(); });
	if (!bCrewAlive)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSJollyShip] %s : plus d'équipage, disparaît"), *GetName());
		bEjected = true;
		Destroy();
	}
}

FVector AYSFoeJollyShip::GetSeatFeetLocation(int32 Seat) const
{
	const USceneComponent* Component = Seat == SeatPilot ? PilotSeat.Get() : Seat == SeatLeft ? LeftSeat.Get() : Seat == SeatRight ? RightSeat.Get() : nullptr;
	return Component ? Component->GetComponentLocation() : GetActorLocation();
}

void AYSFoeJollyShip::OnHullDestroyed()
{
	if (!HasAuthority() || bEjected)
	{
		return;
	}
	bEjected = true;
	UE_LOG(LogTemp, Log, TEXT("[YSJollyShip] %s : coque détruite, %d NME à l'eau"), *GetName(), Crew.Num());
	HullFoe = nullptr;
	ReleaseCrew(true);
	Destroy();
}

void AYSFoeJollyShip::EjectAndDestroy(const FVector& Location, const FVector& Facing, const TCHAR* Reason)
{
	bEjected = true;

	// posé au-dessus de l'eau (pas de terre ferme devant) : ils y tombent, leur XP reste due
	const bool bInWater = AYSOcean::IsUnderwater(GetWorld(), AYSInteractiveObject::FindGround(GetWorld(), Location, this));
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Facing.IsNearlyZero() ? GetActorForwardVector() : Facing).GetSafeNormal2D();
	int32 Index = 0;
	for (AYSFoeCharacter* Foe : Crew)
	{
		if (IsValid(Foe) && !Foe->IsDying())
		{
			const FVector Feet = Location + Side * ((Index - 0.5f * (Crew.Num() - 1)) * EjectSpacing);
			Foe->EndShipRide(Feet, bInWater);
			Foe->SetActorRotation(FRotator(0.0f, Facing.Rotation().Yaw, 0.0f));
		}
		++Index;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSJollyShip] %s percute (%s) : équipage posé en %s"), *GetName(), Reason, *Location.ToCompactString());
	ReleaseCrew(false);
	Destroy();
}

APawn* AYSFoeJollyShip::FindNearestPlayerPawn() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	APawn* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();
	for (const APlayerState* Player : GameState ? GameState->PlayerArray : TArray<TObjectPtr<APlayerState>>())
	{
		AYSPlayerCharacter* Character = Player ? Cast<AYSPlayerCharacter>(Player->GetPawn()) : nullptr;
		if (!Character || !Character->IsAlive())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(Character->GetActorLocation(), GetActorLocation());
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Character;
		}
	}
	return Nearest;
}

FVector AYSFoeJollyShip::GetTargetPoint(const APawn* TargetPawn, bool& bOutOnShip) const
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(TargetPawn);
	const AYSPirateShip* Ship = Character ? Character->GetShipUnderFeet() : nullptr;
	bOutOnShip = Ship != nullptr;
	return Ship ? Ship->GetClosestHullPoint(GetActorLocation()) : TargetPawn->GetActorLocation();
}

void AYSFoeJollyShip::SetPhase(EYSJollyShipPhase NewPhase)
{
	Phase = NewPhase;
	PhaseStartTime = GetWorld()->GetTimeSeconds();
	UE_LOG(LogTemp, Verbose, TEXT("[YSJollyShip] %s : %s (cible %s, à (%.0f, %.0f))"), *GetName(),
		Phase == EYSJollyShipPhase::Approach ? TEXT("approche") : Phase == EYSJollyShipPhase::Circle ? TEXT("tour") : TEXT("fonce"),
		ShipTarget ? *ShipTarget->GetName() : TEXT("aucune"), GetActorLocation().X, GetActorLocation().Y);
}

bool AYSFoeJollyShip::ComputeAutoPilot(float& OutThrottle, float& OutSteer)
{
	// cible : la plus proche pendant l'approche, gardée ensuite ; morte : routine reprise
	const AYSPlayerCharacter* TargetCharacter = Cast<AYSPlayerCharacter>(ShipTarget);
	if (!TargetCharacter || !TargetCharacter->IsAlive())
	{
		ShipTarget = FindNearestPlayerPawn();
		if (Phase != EYSJollyShipPhase::Approach)
		{
			SetPhase(EYSJollyShipPhase::Approach);
		}
	}
	else if (Phase == EYSJollyShipPhase::Approach)
	{
		ShipTarget = FindNearestPlayerPawn();
	}
	if (!ShipTarget)
	{
		OutThrottle = 0.0f;
		OutSteer = 0.0f;
		return true;
	}

	bool bTargetOnShip = false;
	const FVector Point = GetTargetPoint(ShipTarget, bTargetOnShip);
	const FVector Location = GetActorLocation();
	const FVector ToPoint = FVector(Point.X - Location.X, Point.Y - Location.Y, 0.0f);
	const float Distance = ToPoint.Size();
	const FVector Inward = ToPoint.GetSafeNormal();
	const double Now = GetWorld()->GetTimeSeconds();

	if (Phase == EYSJollyShipPhase::Approach && Distance <= EngageDistance)
	{
		SetPhase(EYSJollyShipPhase::Circle);
	}
	else if (Phase == EYSJollyShipPhase::Circle && Now - PhaseStartTime >= CircleDuration)
	{
		SetPhase(EYSJollyShipPhase::Rush);
	}
	else if (Phase == EYSJollyShipPhase::Rush && !bTargetOnShip && Distance <= RushReachDistance)
	{
		// elle nage : rien à percuter, on repart pour un tour
		SetPhase(EYSJollyShipPhase::Approach);
	}

	if (Now - LastRoutineLogTime >= 1.0)
	{
		LastRoutineLogTime = Now;
		UE_LOG(LogTemp, Verbose, TEXT("[YSJollyShip] %s en (%.0f, %.0f) : phase %d, à %.0f cm de sa cible, vitesse %.0f"), *GetName(), Location.X, Location.Y, static_cast<int32>(Phase), Distance, GetSpeed());
	}

	FVector Desired = Inward;
	if (Phase == EYSJollyShipPhase::Circle)
	{
		// la plage devant : demi-tour plutôt que de s'échouer pendant le tour
		if (CircleShoreLookAhead > 0.0f && Now >= NextShoreCheckTime)
		{
			NextShoreCheckTime = Now + YSJollyShip::ShoreCheckInterval;
			if (Now - LastFlipTime >= CircleFlipCooldown && GetHullMinDepth(Location + GetActorForwardVector().GetSafeNormal2D() * CircleShoreLookAhead, GetActorRotation().Yaw) < MinWaterDepth)
			{
				LastFlipTime = Now;
				CircleSide = -CircleSide;
				UE_LOG(LogTemp, Verbose, TEXT("[YSJollyShip] %s : fond trop haut devant, change de sens"), *GetName());
			}
		}
		// tangente au cercle + correction vers le cercle (forte : sinon il dérive en régime établi, ~170 cm trop loin)
		const FVector Tangent = FVector::CrossProduct(FVector::UpVector, Inward) * CircleSide;
		const float Radius = FMath::Max(EngageDistance - CircleRadiusInset, 1.0f);
		const float RadialError = FMath::Clamp((Distance - Radius) / Radius * YSJollyShip::CircleRadialGain, -1.0f, 1.0f);
		Desired = Tangent + Inward * RadialError;
	}

	OutThrottle = 1.0f;
	OutSteer = ComputeSteerTowards(Desired, SteerFullAngle);
	return true;
}

AYSFoeJollyShip* AYSFoeJollyShip::SpawnAtSea(UWorld* World, TSubclassOf<AYSFoeJollyShip> ShipClass, float MinDistance, float MaxDistance, float Step, int32 Attempts)
{
	const AYSOcean* Ocean = AYSOcean::Find(World);
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!Ocean || !GameState || !ShipClass)
	{
		return nullptr;
	}

	TArray<const APawn*> Players;
	for (const APlayerState* Player : GameState->PlayerArray)
	{
		const AYSPlayerCharacter* Character = Player ? Cast<AYSPlayerCharacter>(Player->GetPawn()) : nullptr;
		if (Character && Character->IsAlive())
		{
			Players.Add(Character);
		}
	}
	if (Players.Num() == 0)
	{
		return nullptr;
	}

	const float Sea = Ocean->GetSeaLevel();
	const float RequiredDepth = ShipClass->GetDefaultObject<AYSFoeJollyShip>()->MinWaterDepth + YSJollyShip::SpawnExtraDepth;
	bool bFound = false;
	FVector BestPoint = FVector::ZeroVector;
	FVector BestDirection = FVector::ForwardVector;
	for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
	{
		const APawn* Reference = Players[FMath::RandHelper(Players.Num())];
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		for (float Distance = MinDistance; Distance <= MaxDistance; Distance += FMath::Max(Step, 10.0f))
		{
			const FVector Point = Reference->GetActorLocation() + Direction * Distance;
			// assez d'eau (ni l'île, ni un pont)
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(YSJollyShipSpawn), false);
			if (World->LineTraceSingleByObjectType(Hit, FVector(Point.X, Point.Y, Sea + YSJollyShip::DepthTraceUp), FVector(Point.X, Point.Y, Sea - YSJollyShip::DepthTraceDown),
				FCollisionObjectQueryParams(ECC_WorldStatic), Params) && Sea - Hit.ImpactPoint.Z < RequiredDepth)
			{
				continue;
			}
			// loin de tous les joueurs (autre joueur plus près que le joueur de référence)
			if (Players.ContainsByPredicate([&Point, MinDistance](const APawn* Player) { return FVector::DistSquared2D(Player->GetActorLocation(), Point) < FMath::Square(MinDistance); }))
			{
				continue;
			}
			// pas dans un autre bateau
			bool bInsideShip = false;
			for (TActorIterator<AYSPirateShip> It(World); It && !bInsideShip; ++It)
			{
				bInsideShip = !It->IsFoeShip() && It->IsPresent() && It->IsInsideHull(Point, YSJollyShip::SpawnShipMargin);
			}
			if (bInsideShip)
			{
				continue;
			}

			// de préférence avec de l'eau jusqu'au joueur (sinon il s'échoue sur l'île sans jamais atteindre le bateau où il se trouve) ;
			// aucune direction comme ça (joueur à terre) : le premier emplacement trouvé
			bool bClearWater = true;
			for (float Along = Step; Along < Distance && bClearWater; Along += FMath::Max(Step, 10.0f))
			{
				const FVector Between = Reference->GetActorLocation() + Direction * Along;
				bClearWater = !World->LineTraceSingleByObjectType(Hit, FVector(Between.X, Between.Y, Sea + YSJollyShip::DepthTraceUp), FVector(Between.X, Between.Y, Sea - YSJollyShip::DepthTraceDown),
					FCollisionObjectQueryParams(ECC_WorldStatic), Params) || AYSPirateShip::GetShipOf(Hit.GetActor()) || Sea - Hit.ImpactPoint.Z >= RequiredDepth;
			}
			if (!bFound || bClearWater)
			{
				bFound = true;
				BestPoint = Point;
				BestDirection = Direction;
			}
			if (bClearWater)
			{
				Attempt = Attempts;
			}
			break;
		}
	}
	if (!bFound)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AYSFoeJollyShip>(ShipClass, FVector(BestPoint.X, BestPoint.Y, Sea), FRotator(0.0f, (-BestDirection).Rotation().Yaw, 0.0f), SpawnParams);
}
