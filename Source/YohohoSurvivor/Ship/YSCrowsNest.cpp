#include "Ship/YSCrowsNest.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"

AYSCrowsNest::AYSCrowsNest()
{
	DisplayName = NSLOCTEXT("YS", "CrowsNestName", "Nid d'pie");

	// origine = pied du grand mât (pont) ; nid de BP_PirateShip à +1530 au-dessus du pont, joueur à côté du mât (pas dedans)
	GetOccupantPoint()->SetRelativeLocation(FVector(-100.0f, 0.0f, 1565.0f));
	GetExitPoint()->SetRelativeLocation(FVector(-120.0f, 0.0f, 0.0f));

	NestFloor = CreateDefaultSubobject<UBoxComponent>(TEXT("NestFloor"));
	NestFloor->SetupAttachment(GetRootComponent());
	NestFloor->SetRelativeLocation(FVector(-60.0f, 0.0f, 1560.0f));
	NestFloor->SetBoxExtent(FVector(140.0f, 140.0f, 5.0f));
	NestFloor->SetCollisionProfileName(TEXT("BlockAll"));
	NestFloor->SetMobility(EComponentMobility::Movable);

	// pas de mains sur le poste, regard libre ; caméra encore plus dézoomée, vers le curseur
	bTeleportOnLeave = true;
	bHandsOnStation = false;
	bLockAim = false;
	CameraArmLength = 6000.0f;
	bCameraOnShip = false;
	CameraOffsetMode = EYSStationCameraOffset::Cursor;
	CameraOffsetDistance = 2500.0f;
}

void AYSCrowsNest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSCrowsNest, Ping);
}

void AYSCrowsNest::OnOccupantChanged(AYSPlayerCharacter* Previous)
{
	if (HasAuthority() && Ping.bActive)
	{
		Ping = FYSShipPing();
		UE_LOG(LogTemp, Log, TEXT("[YSCrowsNest] %s : ping arrêté (joueur parti)"), *GetName());
		ForceNetUpdate();
	}
}

void AYSCrowsNest::ServerHandleAction(AYSPlayerCharacter* Character, EYSStationAction Action, const FVector& AimLocation)
{
	if (!HasAuthority() || Action != EYSStationAction::Primary || !Character || Character != GetOccupant())
	{
		return;
	}

	const AGameStateBase* GameState = GetWorld()->GetGameState();
	Ping.bActive = true;
	Ping.Location = AimLocation;
	Ping.Pinger = Character->GetPlayerState();
	Ping.ServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSCrowsNest] %s : ping de %s en %s"), *GetName(), *Character->GetName(), *AimLocation.ToCompactString());
}

void AYSCrowsNest::OnRep_Ping()
{
	UE_LOG(LogTemp, Verbose, TEXT("[YSCrowsNest] %s : ping %s en %s"), *GetName(), Ping.bActive ? TEXT("reçu") : TEXT("arrêté"), *Ping.Location.ToCompactString());
}

bool AYSCrowsNest::GetActivePing(FVector& OutLocation, APlayerState*& OutPinger) const
{
	if (!Ping.bActive || !Ping.Pinger)
	{
		return false;
	}
	if (PingDuration > 0.0f)
	{
		const AGameStateBase* GameState = GetWorld()->GetGameState();
		if (GameState && GameState->GetServerWorldTimeSeconds() - Ping.ServerTime > PingDuration)
		{
			return false;
		}
	}
	OutLocation = Ping.Location + FVector(0.0f, 0.0f, PingHeight);
	OutPinger = Ping.Pinger;
	return true;
}

bool AYSCrowsNest::IsPingBlinkVisible() const
{
	if (PingBlinkFrequency <= 0.0f)
	{
		return true;
	}
	const double Phase = FMath::Frac(GetWorld()->GetRealTimeSeconds() * PingBlinkFrequency);
	return Phase < PingVisibleRatio;
}
