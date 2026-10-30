#include "Ship/YSPirateShip.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Core/YSGameState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Map/YSOcean.h"
#include "Net/UnrealNetwork.h"
#include "Ship/YSCrowsNest.h"
#include "Ship/YSLongboatLauncher.h"
#include "Ship/YSShipBoarding.h"
#include "Ship/YSShipCannon.h"
#include "Ship/YSShipCore.h"
#include "Ship/YSShipHelm.h"
#include "Ship/YSShipPath.h"

namespace YSPirateShip
{
	/** Recherche du fond sous la coque : depuis un peu au-dessus de la mer jusqu'à loin dessous */
	constexpr float DepthTraceUp = 100.0f;
	constexpr float DepthTraceDown = 3000.0f;

	/** Pas de simulation max (une grosse image est découpée) */
	constexpr float MaxStep = 1.0f / 30.0f;

	/** Autres bateaux traversés par la recherche du fond (chaloupe sous la coque) */
	constexpr int32 MaxIgnoredShips = 4;

	/** Poste créé par un composant optionnel (absent sur une chaloupe) */
	UChildActorComponent* CreateStation(AYSPirateShip* Ship, const FObjectInitializer& ObjectInitializer, USceneComponent* Parent, const TCHAR* Name, UClass* StationClass, const FVector& Location, float Yaw)
	{
		UChildActorComponent* Component = ObjectInitializer.CreateOptionalDefaultSubobject<UChildActorComponent>(Ship, Name);
		if (Component)
		{
			Component->SetupAttachment(Parent);
			Component->SetChildActorClass(StationClass);
			Component->SetRelativeLocationAndRotation(Location, FRotator(0.0f, Yaw, 0.0f));
		}
		return Component;
	}
}

AYSPirateShip::AYSPirateShip(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// présence (cachée tant que l'objectif de doublons n'est pas atteint dans GameMap) et pilotage : à chaque image, sur toutes les machines
	PrimaryActorTick.bCanEverTick = true;

	// position répliquée pour le bateau objectif (un bateau pilotable réplique Motion à la place, voir BeginPlay)
	bReplicates = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(30.0f);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	// barre : centre de la roue du BP_PirateShip (gaillard arrière)
	HelmStation = CreateDefaultSubobject<UChildActorComponent>(TEXT("HelmStation"));
	HelmStation->SetupAttachment(Root);
	HelmStation->SetChildActorClass(AYSShipHelm::StaticClass());
	HelmStation->SetRelativeLocation(FVector(-1020.0f, 0.0f, 582.0f));

	// postes du grand bateau (pont à +210) ; placés sur les formes du BP par setup_ship_stations.py
	CoreStation = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("CoreStation"), AYSShipCore::StaticClass(), FVector(-250.0f, 0.0f, 210.0f), 0.0f);
	CrowsNestStation = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("CrowsNestStation"), AYSCrowsNest::StaticClass(), FVector(100.0f, 0.0f, 210.0f), 0.0f);
	LongboatStation = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("LongboatStation"), AYSLongboatLauncher::StaticClass(), FVector(-660.0f, -180.0f, 210.0f), 180.0f);
	CannonFront = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("CannonFront"), AYSShipCannon::StaticClass(), FVector(1450.0f, 0.0f, 210.0f), 0.0f);
	CannonLeft = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("CannonLeft"), AYSShipCannon::StaticClass(), FVector(450.0f, -380.0f, 210.0f), -90.0f);
	CannonRight = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("CannonRight"), AYSShipCannon::StaticClass(), FVector(450.0f, 380.0f, 210.0f), 90.0f);
	// accès : flanc bâbord (-Y), à l'ouverture du bastingage et de la planche d'embarquement, au niveau de la mer
	BoardingAccess = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("BoardingAccess"), AYSShipBoarding::StaticClass(), FVector(0.0f, -500.0f, 0.0f), -90.0f);

	ShipName = NSLOCTEXT("YS", "PirateShipName", "Bateau pirate");

	// coque de BP_PirateShip : partie droite de -1200 à 1200, proue arrondie jusqu'à 1700, largeur 1000
	HullProbePoints = {
		FVector2D(1700.0f, 0.0f), FVector2D(1550.0f, -350.0f), FVector2D(1550.0f, 350.0f),
		FVector2D(1200.0f, -500.0f), FVector2D(1200.0f, 500.0f), FVector2D(0.0f, -500.0f), FVector2D(0.0f, 500.0f),
		FVector2D(-1200.0f, -500.0f), FVector2D(-1200.0f, 500.0f), FVector2D(-1200.0f, 0.0f)
	};
}

void AYSPirateShip::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSPirateShip, Motion);
	DOREPLIFETIME(AYSPirateShip, Health);
	DOREPLIFETIME(AYSPirateShip, bPlaytestMode);
}

void AYSPirateShip::BeginPlay()
{
	Super::BeginPlay();

	// mode playtest : il se déplace comme un bateau pilotable
	bDrivable |= bPlaytestMode;

	// pilotable : les clients extrapolent Motion (mouvement fluide sous les pieds des persos) au lieu de recevoir la position brute
	if (bDrivable)
	{
		SetReplicatingMovement(false);
		if (HasAuthority())
		{
			Motion.Location = GetActorLocation();
			Motion.Yaw = GetActorRotation().Yaw;
		}
	}

	// PV du bateau : donnés par le core
	if (HasAuthority())
	{
		Health = GetMaxHealth();
	}

	FString StationNames;
	for (const AYSShipStation* Station : GetStations())
	{
		StationNames += FString::Printf(TEXT(" %s"), *Station->GetClass()->GetName());
	}
	UE_LOG(LogTemp, Log, TEXT("[YSShip] %s : PV %.0f, core %s, postes :%s"), *GetName(), GetMaxHealth(), GetCore() ? *GetCore()->GetClass()->GetName() : TEXT("aucun"), *StationNames);
}

AYSShipHelm* AYSPirateShip::GetHelm() const
{
	return HelmStation ? Cast<AYSShipHelm>(HelmStation->GetChildActor()) : nullptr;
}

AYSShipCore* AYSPirateShip::GetCore() const
{
	return CoreStation ? Cast<AYSShipCore>(CoreStation->GetChildActor()) : nullptr;
}

AYSShipBoarding* AYSPirateShip::GetBoardingAccess() const
{
	return BoardingAccess ? Cast<AYSShipBoarding>(BoardingAccess->GetChildActor()) : nullptr;
}

FVector AYSPirateShip::GetRouteAroundHull(const FVector& From, const FVector& To, float Margin) const
{
	const FBox2D Bounds = GetHullBounds();
	if (!Bounds.bIsValid)
	{
		return To;
	}
	const FTransform& Transform = GetActorTransform();
	const FVector LocalFrom = Transform.InverseTransformPositionNoScale(From);
	const FVector LocalTo = Transform.InverseTransformPositionNoScale(To);

	// déjà du côté de To (au-delà du flanc) : tout droit
	const float Side = LocalTo.Y >= Bounds.GetCenter().Y ? 1.0f : -1.0f;
	const float Flank = Side > 0.0f ? Bounds.Max.Y : Bounds.Min.Y;
	if ((LocalFrom.Y - Flank) * Side >= 0.0f)
	{
		return To;
	}

	// le tour le plus court : par la proue ou par la poupe ; d'abord le long du flanc, puis en travers une fois la coque dépassée
	const float Bow = Bounds.Max.X + Margin;
	const float Stern = Bounds.Min.X - Margin;
	const float EndX = FMath::Abs(LocalFrom.X - Bow) + FMath::Abs(LocalTo.X - Bow) < FMath::Abs(LocalFrom.X - Stern) + FMath::Abs(LocalTo.X - Stern) ? Bow : Stern;
	const bool bPastHull = LocalFrom.X >= Bow - 1.0f || LocalFrom.X <= Stern + 1.0f;
	return Transform.TransformPositionNoScale(FVector(EndX, bPastHull ? Flank + Side * Margin : LocalFrom.Y, LocalFrom.Z));
}

TArray<AYSShipStation*> AYSPirateShip::GetStations() const
{
	TArray<AYSShipStation*> Stations;
	TInlineComponentArray<UChildActorComponent*> Components(this);
	for (const UChildActorComponent* Component : Components)
	{
		if (AYSShipStation* Station = Cast<AYSShipStation>(Component->GetChildActor()))
		{
			Stations.Add(Station);
		}
	}
	return Stations;
}

float AYSPirateShip::GetMaxHealth() const
{
	const AYSShipCore* Core = GetCore();
	return Core ? Core->GetShipMaxHealth() : 0.0f;
}

void AYSPirateShip::SyncMotionToActor()
{
	if (!HasAuthority())
	{
		return;
	}
	Motion.Location = GetActorLocation();
	Motion.Yaw = GetActorRotation().Yaw;
	Motion.Speed = 0.0f;
	Motion.YawRate = 0.0f;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSShip] %s : pilotage recalé sur sa position (%s, yaw %.0f)"), *GetName(), *Motion.Location.ToCompactString(), Motion.Yaw);
}

bool AYSPirateShip::IsOnPlaytestAutoPilot() const
{
	const AYSShipHelm* Helm = GetHelm();
	return bPlaytestMode && !(Helm && Helm->GetOccupant());
}

void AYSPirateShip::SetPlaytestMode(bool bEnabled)
{
	if (HasAuthority() && bDrivable)
	{
		bPlaytestMode = bEnabled;
		ForceNetUpdate();
		UE_LOG(LogTemp, Log, TEXT("[YSShip] %s : mode playtest %s"), *GetName(), bEnabled ? TEXT("activé") : TEXT("désactivé"));
	}
}

bool AYSPirateShip::ComputeAutoPilot(float& OutThrottle, float& OutSteer)
{
	if (!IsOnPlaytestAutoPilot())
	{
		return false;
	}

	const AYSShipPath* Path = PlaytestPath;
	if (!Path)
	{
		TActorIterator<AYSShipPath> It(GetWorld());
		Path = It ? *It : nullptr;
	}
	FVector Ahead;
	if (!Path || !Path->GetPointAhead(GetActorLocation(), PlaytestLookAhead, Ahead))
	{
		if (!bPlaytestPathMissingLogged)
		{
			bPlaytestPathMissingLogged = true;
			UE_LOG(LogTemp, Warning, TEXT("[YSShip] %s : mode playtest sans trajet (poser un AYSShipPath dans la map)"), *GetName());
		}
		return false;
	}

	OutThrottle = 1.0f;
	OutSteer = ComputeSteerTowards(Ahead - GetActorLocation(), PlaytestSteerFullAngle);
	return true;
}

float AYSPirateShip::ComputeSteerTowards(const FVector& Direction, float FullSteerAngle) const
{
	const float Delta = FRotator::NormalizeAxis(Direction.Rotation().Yaw - Motion.Yaw);
	return FMath::RoundToFloat(FMath::Clamp(Delta / FMath::Max(FullSteerAngle, 1.0f), -1.0f, 1.0f) * 10.0f) / 10.0f;
}

void AYSPirateShip::SetPilotInput(float Throttle, float Steer)
{
	if (!HasAuthority())
	{
		return;
	}
	Motion.Throttle = FMath::Clamp(Throttle, -1.0f, 1.0f);
	Motion.Steer = FMath::Clamp(Steer, -1.0f, 1.0f);
	if (Motion.Throttle != LastSentThrottle || Motion.Steer != LastSentSteer)
	{
		LastSentThrottle = Motion.Throttle;
		LastSentSteer = Motion.Steer;
		UE_LOG(LogTemp, Verbose, TEXT("[YSShip] %s : barre Z/S %.0f, Q/D %.0f (vitesse %.0f)"), *GetName(), Motion.Throttle, Motion.Steer, Motion.Speed);
		ForceNetUpdate();
	}
}

void AYSPirateShip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// bateau objectif : présent seulement une fois l'objectif de doublons atteint (GameMap)
	if (bObjectiveShip)
	{
		const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
		const int8 Presence = !YSGameState || YSGameState->IsShipPresent() ? 1 : 0;
		if (Presence != AppliedPresence)
		{
			AppliedPresence = Presence;
			ApplyPresence(Presence == 1);
			// caché, il ne simulait pas son pilotage : il apparaît là où il a été posé (port), pas à sa position du BeginPlay
			if (Presence == 1 && bDrivable)
			{
				SyncMotionToActor();
			}
			UE_LOG(LogTemp, Log, TEXT("[YSShip] %s %s"), *GetName(), Presence ? TEXT("présent") : TEXT("absent (caché, sans collision)"));
		}
	}

	if (bDrivable && !IsHidden())
	{
		if (HasAuthority())
		{
			TickServerMotion(DeltaSeconds);
		}
		else
		{
			TickClientMotion(DeltaSeconds);
		}
	}
}

void AYSPirateShip::ApplyPresence(bool bPresent)
{
	SetActorHiddenInGame(!bPresent);
	SetActorEnableCollision(bPresent);
	// postes (acteurs enfants) et ce qui est accroché au bateau : cachés avec lui
	TArray<AActor*> Attached;
	GetAttachedActors(Attached, true, true);
	for (AActor* Actor : Attached)
	{
		if (Actor->IsA<AYSShipStation>() || Actor->IsA<AYSShipCore>() || Actor->IsA<AYSShipBoarding>())
		{
			Actor->SetActorHiddenInGame(!bPresent);
			Actor->SetActorEnableCollision(bPresent);
		}
	}
}

void AYSPirateShip::StepMotion(FYSShipMotion& State, float DeltaSeconds) const
{
	// stats du core (multiplicateurs, mêmes valeurs sur toutes les machines : acteur local créé depuis le Blueprint)
	const AYSShipCore* Core = GetCore();
	const float SpeedMultiplier = Core ? Core->GetSpeedMultiplier() : 1.0f;
	const float AccelerationMultiplier = Core ? Core->GetAccelerationMultiplier() : 1.0f;
	const float TurnMultiplier = Core ? Core->GetTurnRateMultiplier() : 1.0f;
	const float MaxForward = MaxForwardSpeed * SpeedMultiplier * (IsOnPlaytestAutoPilot() ? PlaytestSpeedMultiplier : 1.0f);
	const float MaxReverse = MaxReverseSpeed * SpeedMultiplier;
	const float ForwardRate = ForwardAcceleration * AccelerationMultiplier;
	const float ReverseRate = ReverseAcceleration * AccelerationMultiplier;

	// vitesse : Z accélère vers l'avant, S freine puis recule, sans commande le bateau glisse et ralentit (inertie)
	if (State.Throttle > 0.0f)
	{
		const float Rate = State.Speed < 0.0f ? FMath::Max(ForwardRate, Deceleration) : ForwardRate;
		State.Speed = FMath::Min(State.Speed + Rate * State.Throttle * DeltaSeconds, MaxForward);
	}
	else if (State.Throttle < 0.0f)
	{
		const float Rate = State.Speed > 0.0f ? FMath::Max(ReverseRate, Deceleration) : ReverseRate;
		State.Speed = FMath::Max(State.Speed + Rate * State.Throttle * DeltaSeconds, -MaxReverse);
	}
	else
	{
		State.Speed = FMath::FInterpConstantTo(State.Speed, 0.0f, DeltaSeconds, Deceleration);
	}

	// rotation : la barre donne une vitesse de rotation visée (plus faible à basse vitesse, inversée en marche arrière),
	// suivie avec inertie
	const float SpeedRatio = FMath::Clamp(FMath::Abs(State.Speed) / FullTurnSpeed, 0.0f, 1.0f);
	const float TurnRatio = FMath::Lerp(StandstillTurnRatio, 1.0f, SpeedRatio);
	const float TargetYawRate = State.Steer * MaxTurnRate * TurnMultiplier * TurnRatio * (State.Speed < -1.0f ? -1.0f : 1.0f);
	State.YawRate = TurnResponsiveness > 0.0f ? FMath::FInterpTo(State.YawRate, TargetYawRate, DeltaSeconds, TurnResponsiveness) : TargetYawRate;

	State.Yaw = FRotator::NormalizeAxis(State.Yaw + State.YawRate * DeltaSeconds);
	State.Location += FRotator(0.0f, State.Yaw, 0.0f).Vector() * State.Speed * DeltaSeconds;
}

float AYSPirateShip::GetHullMinDepth(const FVector& Location, float Yaw) const
{
	const UWorld* World = GetWorld();
	const AYSOcean* Ocean = AYSOcean::Find(World);
	float MinDepth = YSPirateShip::DepthTraceDown;
	if (!Ocean)
	{
		return MinDepth;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSShipDepth), false, this);
	TArray<AActor*> OwnActors;
	GetAttachedActors(OwnActors, true, true);
	Params.AddIgnoredActors(OwnActors);
	const float Sea = Ocean->GetSeaLevel();
	const FRotator Rotation(0.0f, Yaw, 0.0f);
	for (const FVector2D& Probe : HullProbePoints)
	{
		const FVector Point = Location + Rotation.RotateVector(FVector(Probe.X, Probe.Y, 0.0f));
		// les autres bateaux (chaloupe le long de la coque, leurs postes) ne sont pas le fond : traversés
		FCollisionQueryParams ProbeParams = Params;
		for (int32 Attempt = 0; Attempt <= YSPirateShip::MaxIgnoredShips; ++Attempt)
		{
			FHitResult Hit;
			if (!World->LineTraceSingleByObjectType(Hit, FVector(Point.X, Point.Y, Sea + YSPirateShip::DepthTraceUp),
				FVector(Point.X, Point.Y, Sea - YSPirateShip::DepthTraceDown), FCollisionObjectQueryParams(ECC_WorldStatic), ProbeParams))
			{
				break;
			}
			if (AYSPirateShip* OtherShip = GetShipOf(Hit.GetActor()))
			{
				TArray<AActor*> OtherActors;
				OtherShip->GetAttachedActors(OtherActors, true, true);
				ProbeParams.AddIgnoredActor(OtherShip);
				ProbeParams.AddIgnoredActors(OtherActors);
				continue;
			}
			MinDepth = FMath::Min(MinDepth, Sea - Hit.ImpactPoint.Z);
			break;
		}
	}
	return MinDepth;
}

bool AYSPirateShip::CanMoveHull(const FYSShipMotion& From, const FYSShipMotion& To) const
{
	// eau assez profonde partout, ou au moins pas moins profonde qu'avant (un bateau posé trop près du bord peut s'en éloigner)
	const float ToDepth = GetHullMinDepth(To.Location, To.Yaw);
	return ToDepth >= MinWaterDepth || ToDepth >= GetHullMinDepth(From.Location, From.Yaw) - KINDA_SMALL_NUMBER;
}

void AYSPirateShip::TickServerMotion(float DeltaSeconds)
{
	// pilote automatique (mode playtest, NME Jolly Ship) ; sinon, le pilote parti, la barre ne commande plus
	const AYSShipHelm* Helm = GetHelm();
	float AutoThrottle = 0.0f;
	float AutoSteer = 0.0f;
	if (ComputeAutoPilot(AutoThrottle, AutoSteer))
	{
		SetPilotInput(AutoThrottle, AutoSteer);
	}
	else if (!Helm || !Helm->GetOccupant())
	{
		SetPilotInput(0.0f, 0.0f);
	}
	// le trajet du mode playtest est réglé à la main : pas d'arrêt contre le fond (ponton, hauts-fonds)
	const bool bCheckHull = !IsOnPlaytestAutoPilot();

	const bool bWasMoving = !FMath::IsNearlyZero(Motion.Speed) || !FMath::IsNearlyZero(Motion.YawRate);
	float Remaining = DeltaSeconds;
	while (Remaining > KINDA_SMALL_NUMBER)
	{
		const float Step = FMath::Min(Remaining, YSPirateShip::MaxStep);
		Remaining -= Step;

		FYSShipMotion Next = Motion;
		StepMotion(Next, Step);
		// fond trop haut (plage, ponton) : le bateau s'arrête net ; il peut toujours repartir vers l'eau profonde
		const bool bBlocked = bCheckHull && !CanMoveHull(Motion, Next);
		if (bBlocked)
		{
			// la rotation seule passe peut-être encore
			FYSShipMotion Turned = Motion;
			Turned.Yaw = Next.Yaw;
			Turned.YawRate = Next.YawRate;
			Turned.Speed = 0.0f;
			const bool bTurnOk = CanMoveHull(Motion, Turned);
			Motion.Speed = 0.0f;
			Motion.YawRate = bTurnOk ? Next.YawRate : 0.0f;
			Motion.Yaw = bTurnOk ? Next.Yaw : Motion.Yaw;
			if (!bGrounded)
			{
				bGrounded = true;
				UE_LOG(LogTemp, Log, TEXT("[YSShip] %s s'arrête : eau trop peu profonde devant (%s)"), *GetName(), *Motion.Location.ToCompactString());
				ForceNetUpdate();
				OnHullBlocked();
				if (!IsValid(this) || IsActorBeingDestroyed())
				{
					return;
				}
			}
		}
		else
		{
			Motion = Next;
			bGrounded = false;
		}
	}

	const AGameStateBase* GameState = GetWorld()->GetGameState();
	Motion.ServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	Motion.Location.Z = GetActorLocation().Z;
	if (bWasMoving || !GetActorLocation().Equals(Motion.Location, 0.1f))
	{
		SetActorLocationAndRotation(Motion.Location, FRotator(0.0f, Motion.Yaw, 0.0f));
		// une ligne par seconde en mouvement
		if (Motion.ServerTime - LastMotionLogTime >= 1.0)
		{
			LastMotionLogTime = Motion.ServerTime;
			UE_LOG(LogTemp, Verbose, TEXT("[YSShip] %s en (%.0f, %.0f) yaw %.1f, vitesse %.0f, rotation %.1f°/s"), *GetName(), Motion.Location.X, Motion.Location.Y, Motion.Yaw, Motion.Speed, Motion.YawRate);
		}
	}
}

void AYSPirateShip::OnRep_Motion()
{
	if (!bDrivable || HasAuthority())
	{
		return;
	}

	// nouvel état : l'écart avec ce qui était affiché est rattrapé en douceur (pas de saut sous les pieds des persos)
	const FVector Displayed = GetActorLocation();
	const float DisplayedYaw = GetActorRotation().Yaw;
	ClientPredicted = Motion;
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const float Elapsed = GameState ? FMath::Clamp(static_cast<float>(GameState->GetServerWorldTimeSeconds() - Motion.ServerTime), 0.0f, 0.5f) : 0.0f;
	for (float Remaining = Elapsed; Remaining > KINDA_SMALL_NUMBER; Remaining -= YSPirateShip::MaxStep)
	{
		StepMotion(ClientPredicted, FMath::Min(Remaining, YSPirateShip::MaxStep));
	}
	if (bHasClientMotion)
	{
		ClientLocationError = Displayed - ClientPredicted.Location;
		ClientLocationError.Z = 0.0f;
		ClientYawError = FRotator::NormalizeAxis(DisplayedYaw - ClientPredicted.Yaw);
		// trop loin (téléport, premier état) : pas de rattrapage
		if (ClientLocationError.SizeSquared() > FMath::Square(500.0f))
		{
			ClientLocationError = FVector::ZeroVector;
			ClientYawError = 0.0f;
		}
	}
	bHasClientMotion = true;
}

void AYSPirateShip::TickClientMotion(float DeltaSeconds)
{
	if (!bHasClientMotion)
	{
		return;
	}

	// extrapolation avec les commandes reçues, puis l'écart avec l'affichage précédent disparaît peu à peu
	for (float Remaining = DeltaSeconds; Remaining > KINDA_SMALL_NUMBER; Remaining -= YSPirateShip::MaxStep)
	{
		StepMotion(ClientPredicted, FMath::Min(Remaining, YSPirateShip::MaxStep));
	}
	// le serveur l'a arrêté contre le fond : pas d'extrapolation au-delà
	if (FMath::IsNearlyZero(Motion.Speed) && FMath::IsNearlyZero(Motion.Throttle))
	{
		ClientPredicted.Location = FMath::VInterpTo(ClientPredicted.Location, Motion.Location, DeltaSeconds, ClientCorrectionSpeed);
	}
	const float Decay = ClientCorrectionSpeed > 0.0f ? FMath::Exp(-ClientCorrectionSpeed * DeltaSeconds) : 0.0f;
	ClientLocationError *= Decay;
	ClientYawError *= Decay;

	const FVector Location(ClientPredicted.Location.X + ClientLocationError.X, ClientPredicted.Location.Y + ClientLocationError.Y, GetActorLocation().Z);
	const FRotator Rotation(0.0f, ClientPredicted.Yaw + ClientYawError, 0.0f);
	if (!GetActorLocation().Equals(Location, 0.05f) || !FMath::IsNearlyEqual(GetActorRotation().Yaw, Rotation.Yaw, 0.01f))
	{
		SetActorLocationAndRotation(Location, Rotation);
	}
}

AYSPirateShip* AYSPirateShip::FindShipBelow(const UWorld* World, const FVector& Location, const AActor* IgnoredActor)
{
	if (!World)
	{
		return nullptr;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSShipBelow), false, IgnoredActor);
	if (!World->LineTraceSingleByObjectType(Hit, Location + FVector(0.0f, 0.0f, 50.0f), Location - FVector(0.0f, 0.0f, 150.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return nullptr;
	}
	return GetShipOf(Hit.GetActor());
}

AYSPirateShip* AYSPirateShip::GetShipOf(const AActor* Actor)
{
	// postes = acteurs enfants (parent actor), objets posés = accrochés au bateau
	for (int32 Depth = 0; Actor && Depth < 8; ++Depth)
	{
		if (const AYSPirateShip* Ship = Cast<AYSPirateShip>(Actor))
		{
			return const_cast<AYSPirateShip*>(Ship);
		}
		Actor = Actor->GetParentActor() ? Actor->GetParentActor() : Actor->GetAttachParentActor();
	}
	return nullptr;
}

FBox2D AYSPirateShip::GetHullBounds() const
{
	FBox2D Bounds(ForceInit);
	for (const FVector2D& Probe : HullProbePoints)
	{
		Bounds += Probe;
	}
	return Bounds;
}

bool AYSPirateShip::IsInsideHull(const FVector& WorldLocation, float Margin) const
{
	const FBox2D Bounds = GetHullBounds();
	if (!Bounds.bIsValid)
	{
		return false;
	}
	const FVector Local = GetActorTransform().InverseTransformPositionNoScale(WorldLocation);
	return Local.X >= Bounds.Min.X - Margin && Local.X <= Bounds.Max.X + Margin && Local.Y >= Bounds.Min.Y - Margin && Local.Y <= Bounds.Max.Y + Margin;
}

FVector AYSPirateShip::GetClosestHullPoint(const FVector& WorldLocation, float Inset) const
{
	const FBox2D Bounds = GetHullBounds();
	if (!Bounds.bIsValid)
	{
		return GetActorLocation();
	}
	const FVector Local = GetActorTransform().InverseTransformPositionNoScale(WorldLocation);
	const float InsetX = FMath::Min(Inset, Bounds.GetExtent().X);
	const float InsetY = FMath::Min(Inset, Bounds.GetExtent().Y);
	const FVector Clamped(FMath::Clamp(Local.X, Bounds.Min.X + InsetX, Bounds.Max.X - InsetX), FMath::Clamp(Local.Y, Bounds.Min.Y + InsetY, Bounds.Max.Y - InsetY), 0.0f);
	return GetActorTransform().TransformPositionNoScale(Clamped);
}

void AYSPirateShip::GetWorldHullPoints(TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	const FTransform& Transform = GetActorTransform();
	for (const FVector2D& Probe : HullProbePoints)
	{
		OutPoints.Add(Transform.TransformPositionNoScale(FVector(Probe.X, Probe.Y, 0.0f)));
	}
}
