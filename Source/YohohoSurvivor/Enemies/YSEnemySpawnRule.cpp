#include "Enemies/YSEnemySpawnRule.h"
#include "Components/CapsuleComponent.h"
#include "Core/YSGameMode.h"
#include "Core/YSGameState.h"
#include "Core/YSPlayerState.h"
#include "Enemies/YSFoeCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Player/YSPlayerCharacter.h"

void UYSEnemySpawnRule::Start(AYSGameMode* InGameMode)
{
	GameMode = InGameMode;
	StartRule();
}

void UYSEnemySpawnRule::Tick(float DeltaSeconds)
{
	if (bEnabled && GameMode)
	{
		TickRule(DeltaSeconds);
	}
}

UWorld* UYSEnemySpawnRule::GetWorld() const
{
	return GameMode ? GameMode->GetWorld() : nullptr;
}

TArray<APawn*> UYSEnemySpawnRule::GetActivePlayers() const
{
	TArray<APawn*> Players;

	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		return Players;
	}

	for (const APlayerState* Player : GameState->PlayerArray)
	{
		APawn* Pawn = Player ? Player->GetPawn() : nullptr;
		if (Cast<AYSPlayerCharacter>(Pawn))
		{
			Players.Add(Pawn);
		}
	}

	return Players;
}

int32 UYSEnemySpawnRule::GetPlayersInMatchCount() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		return 0;
	}

	int32 Count = 0;
	for (const APlayerState* Player : GameState->PlayerArray)
	{
		const AYSPlayerState* YSPlayer = Cast<AYSPlayerState>(Player);
		Count += (Player && Cast<AYSPlayerCharacter>(Player->GetPawn())) || (YSPlayer && YSPlayer->IsDead()) ? 1 : 0;
	}
	return Count;
}

float UYSEnemySpawnRule::GetMatchElapsedTime() const
{
	const AYSGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AYSGameState>() : nullptr;
	return GameState ? GameState->GetMatchElapsedTime() : 0.0f;
}

int32 UYSEnemySpawnRule::CountAliveEnemies() const
{
	int32 Count = 0;
	for (TActorIterator<AYSFoeCharacter> It(GetWorld()); It; ++It)
	{
		Count += IsValid(*It) ? 1 : 0;
	}
	return Count;
}

int32 UYSEnemySpawnRule::SpawnEnemies(int32 Count)
{
	UWorld* World = GetWorld();
	const TArray<APawn*> Players = GetActivePlayers();
	TArray<TSubclassOf<AYSFoeCharacter>> Classes = EnemyClasses;
	Classes.RemoveAll([](const TSubclassOf<AYSFoeCharacter>& Class) { return !Class; });
	if (!World || Count <= 0 || Players.Num() == 0 || Classes.Num() == 0)
	{
		return 0;
	}

	if (MaxAliveEnemies > 0)
	{
		Count = FMath::Min(Count, MaxAliveEnemies - CountAliveEnemies());
	}

	int32 Spawned = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const TSubclassOf<AYSFoeCharacter> EnemyClass = Classes[FMath::RandHelper(Classes.Num())];
		const AYSFoeCharacter* Default = EnemyClass->GetDefaultObject<AYSFoeCharacter>();
		const float HalfHeight = Default->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

		for (int32 Attempt = 0; Attempt < MaxPlacementAttempts; ++Attempt)
		{
			FVector Location;
			if (!FindSpawnLocation(Players, HalfHeight, Location))
			{
				continue;
			}

			// tourné vers le joueur le plus proche dès son apparition
			const APawn* Nearest = Players[0];
			for (const APawn* Player : Players)
			{
				if (FVector::DistSquared2D(Player->GetActorLocation(), Location) < FVector::DistSquared2D(Nearest->GetActorLocation(), Location))
				{
					Nearest = Player;
				}
			}
			const FRotator Rotation(0.0f, (Nearest->GetActorLocation() - Location).Rotation().Yaw, 0.0f);

			if (World->SpawnActor<AYSFoeCharacter>(EnemyClass, Location, Rotation, Params))
			{
				UE_LOG(LogTemp, Verbose, TEXT("[YSEnemySpawnRule] %s à %.0f cm du joueur le plus proche"), *EnemyClass->GetName(), FVector::Dist2D(Nearest->GetActorLocation(), Location));
				++Spawned;
				break;
			}
		}
	}

	return Spawned;
}

bool UYSEnemySpawnRule::FindSpawnLocation(const TArray<APawn*>& Players, float CapsuleHalfHeight, FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	if (!World || Players.Num() == 0)
	{
		return false;
	}

	// point au hasard dans la bande [SafeRadius, SafeRadius + SpawnBandWidth] autour d'un joueur tiré au hasard
	const APawn* Reference = Players[FMath::RandHelper(Players.Num())];
	const FVector Center = Reference->GetActorLocation();
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Distance = FMath::FRandRange(SafeRadius, SafeRadius + SpawnBandWidth);
	const FVector Candidate = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance;

	// hors écran de TOUS les joueurs
	for (const APawn* Player : Players)
	{
		if (FVector::DistSquared2D(Player->GetActorLocation(), Candidate) < FMath::Square(SafeRadius))
		{
			return false;
		}
	}

	// sol : trace verticale sur la géométrie statique (hors de la map = pas de sol = rejeté)
	FHitResult Hit;
	const FVector TraceStart(Candidate.X, Candidate.Y, Center.Z + 1000.0f);
	const FVector TraceEnd(Candidate.X, Candidate.Y, Center.Z - 5000.0f);
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	if (!World->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, Objects) || Hit.ImpactNormal.Z < 0.7f)
	{
		return false;
	}

	// même niveau que le joueur (pas sur le dessus d'un obstacle)
	const UCapsuleComponent* ReferenceCapsule = Reference->FindComponentByClass<UCapsuleComponent>();
	const float ReferenceFeetZ = Center.Z - (ReferenceCapsule ? ReferenceCapsule->GetScaledCapsuleHalfHeight() : 0.0f);
	if (FMath::Abs(Hit.ImpactPoint.Z - ReferenceFeetZ) > MaxHeightDifference)
	{
		return false;
	}

	OutLocation = Hit.ImpactPoint + FVector(0.0f, 0.0f, CapsuleHalfHeight + 2.0f);
	return true;
}

void UYSTimedWaveSpawnRule::StartRule()
{
	NextWaveTime = FirstWaveDelay;
}

int32 UYSTimedWaveSpawnRule::GetWaveSize() const
{
	// minute en cours : 1 de 0 à 60 s, 2 de 60 à 120 s, ...
	const int32 Minute = FMath::FloorToInt(GetMatchElapsedTime() / MinuteDuration) + 1;
	return GetPlayersInMatchCount() * EnemiesPerPlayer * Minute;
}

void UYSTimedWaveSpawnRule::TickRule(float DeltaSeconds)
{
	const float Elapsed = GetMatchElapsedTime();
	if (Elapsed < NextWaveTime)
	{
		return;
	}

	// une seule vague même après une longue pause (pas de rattrapage)
	NextWaveTime += WaveInterval;
	if (NextWaveTime <= Elapsed)
	{
		NextWaveTime = Elapsed + WaveInterval;
	}

	const int32 WaveSize = GetWaveSize();
	const int32 Spawned = SpawnEnemies(WaveSize);
	UE_LOG(LogTemp, Log, TEXT("[YSTimedWaveSpawnRule] %.1f s : vague de %d NME (%d apparus)"), Elapsed, WaveSize, Spawned);
}
