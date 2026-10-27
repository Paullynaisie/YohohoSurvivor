#include "Ship/YSPirateShip.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Core/YSGameState.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Map/YSOcean.h"
#include "Net/UnrealNetwork.h"
#include "Ship/YSShipHelm.h"

namespace YSPirateShip
{
	/** Recherche du fond sous la coque : depuis un peu au-dessus de la mer jusqu'à loin dessous */
	constexpr float DepthTraceUp = 100.0f;
	constexpr float DepthTraceDown = 3000.0f;

	/** Pas de simulation max (une grosse image est découpée) */
	constexpr float MaxStep = 1.0f / 30.0f;
}

AYSPirateShip::AYSPirateShip()
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
}

void AYSPirateShip::BeginPlay()
{
	Super::BeginPlay();

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
}

AYSShipHelm* AYSPirateShip::GetHelm() const
{
	return HelmStation ? Cast<AYSShipHelm>(HelmStation->GetChildActor()) : nullptr;
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
			SetActorHiddenInGame(Presence == 0);
			SetActorEnableCollision(Presence == 1);
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

void AYSPirateShip::StepMotion(FYSShipMotion& State, float DeltaSeconds) const
{
	// vitesse : Z accélère vers l'avant, S freine puis recule, sans commande le bateau glisse et ralentit (inertie)
	if (State.Throttle > 0.0f)
	{
		const float Rate = State.Speed < 0.0f ? FMath::Max(ForwardAcceleration, Deceleration) : ForwardAcceleration;
		State.Speed = FMath::Min(State.Speed + Rate * State.Throttle * DeltaSeconds, MaxForwardSpeed);
	}
	else if (State.Throttle < 0.0f)
	{
		const float Rate = State.Speed > 0.0f ? FMath::Max(ReverseAcceleration, Deceleration) : ReverseAcceleration;
		State.Speed = FMath::Max(State.Speed + Rate * State.Throttle * DeltaSeconds, -MaxReverseSpeed);
	}
	else
	{
		State.Speed = FMath::FInterpConstantTo(State.Speed, 0.0f, DeltaSeconds, Deceleration);
	}

	// rotation : la barre donne une vitesse de rotation visée (plus faible à basse vitesse, inversée en marche arrière),
	// suivie avec inertie
	const float SpeedRatio = FMath::Clamp(FMath::Abs(State.Speed) / FullTurnSpeed, 0.0f, 1.0f);
	const float TurnRatio = FMath::Lerp(StandstillTurnRatio, 1.0f, SpeedRatio);
	const float TargetYawRate = State.Steer * MaxTurnRate * TurnRatio * (State.Speed < -1.0f ? -1.0f : 1.0f);
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
	if (const AActor* Helm = GetHelm())
	{
		Params.AddIgnoredActor(Helm);
	}
	const float Sea = Ocean->GetSeaLevel();
	const FRotator Rotation(0.0f, Yaw, 0.0f);
	for (const FVector2D& Probe : HullProbePoints)
	{
		const FVector Point = Location + Rotation.RotateVector(FVector(Probe.X, Probe.Y, 0.0f));
		FHitResult Hit;
		if (World->LineTraceSingleByObjectType(Hit, FVector(Point.X, Point.Y, Sea + YSPirateShip::DepthTraceUp),
			FVector(Point.X, Point.Y, Sea - YSPirateShip::DepthTraceDown), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			MinDepth = FMath::Min(MinDepth, Sea - Hit.ImpactPoint.Z);
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
	// le pilote parti, la barre ne commande plus
	const AYSShipHelm* Helm = GetHelm();
	if (!Helm || !Helm->GetPilot())
	{
		SetPilotInput(0.0f, 0.0f);
	}

	const bool bWasMoving = !FMath::IsNearlyZero(Motion.Speed) || !FMath::IsNearlyZero(Motion.YawRate);
	float Remaining = DeltaSeconds;
	while (Remaining > KINDA_SMALL_NUMBER)
	{
		const float Step = FMath::Min(Remaining, YSPirateShip::MaxStep);
		Remaining -= Step;

		FYSShipMotion Next = Motion;
		StepMotion(Next, Step);
		// fond trop haut (plage, ponton) : le bateau s'arrête net ; il peut toujours repartir vers l'eau profonde
		const bool bBlocked = !CanMoveHull(Motion, Next);
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
	return Cast<AYSPirateShip>(Hit.GetActor());
}
