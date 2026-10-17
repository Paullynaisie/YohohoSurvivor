#include "Core/YSGameState.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "UI/YSDamageNumber.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"

AYSGameState::AYSGameState()
{
	DamageNumberClass = AYSDamageNumber::StaticClass();
}

void AYSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSGameState, SelectableCharacters);
	DOREPLIFETIME(AYSGameState, CustomizationColors);
	DOREPLIFETIME(AYSGameState, CustomizationFaces);
	DOREPLIFETIME(AYSGameState, bMatchStarted);
	DOREPLIFETIME(AYSGameState, MatchStartServerTime);
	DOREPLIFETIME(AYSGameState, TeamLevel);
	DOREPLIFETIME(AYSGameState, TeamExperience);
	DOREPLIFETIME(AYSGameState, ExperienceToNextLevel);
	DOREPLIFETIME(AYSGameState, bUpgradeSelectionActive);
	DOREPLIFETIME(AYSGameState, UpgradeSelectionLevel);
	DOREPLIFETIME(AYSGameState, DayNumber);
	DOREPLIFETIME(AYSGameState, bNight);
	DOREPLIFETIME(AYSGameState, DayStartServerTime);
	DOREPLIFETIME(AYSGameState, CurrentElite);
}

void AYSGameState::SetMatchStarted()
{
	bMatchStarted = true;
	MatchStartServerTime = GetServerWorldTimeSeconds();
	StartDay(1);
}

float AYSGameState::GetMatchElapsedTime() const
{
	return bMatchStarted ? FMath::Max(0.0, GetServerWorldTimeSeconds() - MatchStartServerTime) : 0.0f;
}

void AYSGameState::StartDay(int32 Day)
{
	DayNumber = Day;
	bNight = false;
	CurrentElite = nullptr;
	DayStartServerTime = GetServerWorldTimeSeconds();
}

float AYSGameState::GetDayElapsedTime() const
{
	return bMatchStarted ? FMath::Max(0.0, GetServerWorldTimeSeconds() - DayStartServerTime) : 0.0f;
}

void AYSGameState::ReportDamage(const AActor* Victim, float Amount, const FVector& ImpactLocation, const AController* InstigatorController)
{
	if (!Victim || !Victim->HasAuthority())
	{
		return;
	}

	AYSGameState* GameState = Victim->GetWorld()->GetGameState<AYSGameState>();
	if (!GameState)
	{
		return;
	}

	const APawn* VictimPawn = Cast<APawn>(Victim);
	GameState->MulticastDamageNumber(
		Amount,
		ImpactLocation,
		InstigatorController ? InstigatorController->PlayerState.Get() : nullptr,
		VictimPawn ? VictimPawn->GetPlayerState() : nullptr);
}

void AYSGameState::MulticastDamageNumber_Implementation(float Amount, FVector_NetQuantize Location, APlayerState* InstigatorPlayer, APlayerState* VictimPlayer)
{
	UWorld* World = GetWorld();
	if (!DamageNumberClass || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// la couleur dépend du point de vue du joueur local de cette machine
	const APlayerController* LocalController = World->GetFirstPlayerController();
	const APlayerState* Me = LocalController ? LocalController->PlayerState.Get() : nullptr;

	EYSDamageNumberKind Kind = EYSDamageNumberKind::DealtByOther;
	if (Me && InstigatorPlayer == Me)
	{
		Kind = EYSDamageNumberKind::DealtByMe;
	}
	else if (Me && VictimPlayer == Me)
	{
		Kind = EYSDamageNumberKind::TakenByMe;
	}
	else if (VictimPlayer && !InstigatorPlayer)
	{
		Kind = EYSDamageNumberKind::TakenByOther;
	}

	const FVector Jitter(FMath::FRandRange(-DamageNumberJitter, DamageNumberJitter), FMath::FRandRange(-DamageNumberJitter, DamageNumberJitter), 0.0f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AYSDamageNumber* Number = World->SpawnActor<AYSDamageNumber>(DamageNumberClass, Location + Jitter, FRotator::ZeroRotator, Params))
	{
		Number->Init(Amount, Kind);
	}
}
