#include "Core/YSGameState.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "UI/YSDamageNumber.h"

AYSGameState::AYSGameState()
{
	DamageNumberClass = AYSDamageNumber::StaticClass();
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
