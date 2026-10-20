#include "Enemies/YSEnemySpawnRule.h"
#include "Components/CapsuleComponent.h"
#include "Core/YSGameMode.h"
#include "Core/YSGameState.h"
#include "Core/YSPlayerState.h"
#include "Enemies/YSFoeCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Map/YSOcean.h"
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

TSubclassOf<AYSFoeCharacter> UYSEnemySpawnRule::PickEnemyClass(const TArray<TSubclassOf<AYSFoeCharacter>>& Classes)
{
	// tirage pondéré par le Spawn Weight de chaque NME (catégorie Foe|Spawn)
	float TotalWeight = 0.0f;
	for (const TSubclassOf<AYSFoeCharacter>& Class : Classes)
	{
		TotalWeight += Class ? FMath::Max(Class->GetDefaultObject<AYSFoeCharacter>()->GetSpawnWeight(), 0.0f) : 0.0f;
	}
	if (TotalWeight <= 0.0f)
	{
		return Classes.Num() > 0 ? Classes[FMath::RandHelper(Classes.Num())] : nullptr;
	}

	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (const TSubclassOf<AYSFoeCharacter>& Class : Classes)
	{
		const float Weight = Class ? FMath::Max(Class->GetDefaultObject<AYSFoeCharacter>()->GetSpawnWeight(), 0.0f) : 0.0f;
		if (Weight > 0.0f && Roll <= Weight)
		{
			return Class;
		}
		Roll -= Weight;
	}
	return Classes.Last();
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
		Spawned += SpawnEnemy(PickEnemyClass(Classes)) ? 1 : 0;
	}
	return Spawned;
}

AYSFoeCharacter* UYSEnemySpawnRule::SpawnEnemy(TSubclassOf<AYSFoeCharacter> EnemyClass)
{
	UWorld* World = GetWorld();
	const TArray<APawn*> Players = GetActivePlayers();
	if (!World || !EnemyClass || Players.Num() == 0)
	{
		return nullptr;
	}

	// la taille du NME (élite x3) est appliquée à son BeginPlay : on la prévoit pour le poser sur le sol
	const AYSFoeCharacter* Default = EnemyClass->GetDefaultObject<AYSFoeCharacter>();
	const float HalfHeight = Default->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * Default->GetSizeMultiplier();

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

		if (AYSFoeCharacter* Enemy = World->SpawnActor<AYSFoeCharacter>(EnemyClass, Location, Rotation, Params))
		{
			UE_LOG(LogTemp, Verbose, TEXT("[YSEnemySpawnRule] %s à %.0f cm du joueur le plus proche"), *EnemyClass->GetName(), FVector::Dist2D(Nearest->GetActorLocation(), Location));
			return Enemy;
		}
	}
	return nullptr;
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

	// pas dans l'océan (fond marin ou bord de plage sous l'eau)
	if (AYSOcean::IsUnderwater(World, Hit.ImpactPoint))
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
	NextWaveTime = GetMatchElapsedTime() + FirstWaveDelay;
	bElitePending = false;
}

void UYSTimedWaveSpawnRule::OnPhaseChanged(bool bNight)
{
	// nouvelle phase : vagues avec ses propres réglages, à partir de maintenant ; élite au début de la nuit
	NextWaveTime = GetMatchElapsedTime() + FirstWaveDelay;
	bElitePending = bNight;
	NextEliteAttemptTime = 0.0f;
	bEliteFailureLogged = false;
}

bool UYSTimedWaveSpawnRule::IsNight() const
{
	const AYSGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AYSGameState>() : nullptr;
	return GameState && GameState->IsNight();
}

int32 UYSTimedWaveSpawnRule::GetDayNumber() const
{
	const AYSGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AYSGameState>() : nullptr;
	return GameState ? GameState->GetDayNumber() : 1;
}

int32 UYSTimedWaveSpawnRule::GetWaveSize() const
{
	return GetPlayersInMatchCount() * (IsNight() ? NightEnemiesPerPlayer : EnemiesPerPlayer) * GetDayNumber();
}

bool UYSTimedWaveSpawnRule::SpawnElite()
{
	TArray<TSubclassOf<AYSFoeCharacter>> Classes = EliteClasses;
	Classes.RemoveAll([](const TSubclassOf<AYSFoeCharacter>& Class) { return !Class; });
	if (Classes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSTimedWaveSpawnRule] Nuit sans élite : EliteClasses est vide (la nuit ne pourra pas se terminer)"));
		return true;
	}

	// personne autour de qui le placer (tous morts) : on attend sans rien signaler
	if (GetActivePlayers().Num() == 0)
	{
		return false;
	}

	const AYSFoeCharacter* Elite = SpawnEnemy(Classes[FMath::RandHelper(Classes.Num())]);
	if (Elite || !bEliteFailureLogged)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSTimedWaveSpawnRule] Nuit %d : élite %s"), GetDayNumber(), Elite ? *Elite->GetName() : TEXT("pas d'emplacement, nouvel essai toutes les 0,5 s"));
		bEliteFailureLogged = !Elite;
	}
	return Elite != nullptr;
}

void UYSTimedWaveSpawnRule::TickRule(float DeltaSeconds)
{
	if (bElitePending && GetMatchElapsedTime() >= NextEliteAttemptTime)
	{
		NextEliteAttemptTime = GetMatchElapsedTime() + 0.5f;
		bElitePending = !SpawnElite();
	}

	const float Elapsed = GetMatchElapsedTime();
	if (Elapsed < NextWaveTime)
	{
		return;
	}

	// une seule vague même après une longue pause (pas de rattrapage)
	const float Interval = IsNight() ? NightWaveInterval : WaveInterval;
	NextWaveTime += Interval;
	if (NextWaveTime <= Elapsed)
	{
		NextWaveTime = Elapsed + Interval;
	}

	const int32 WaveSize = GetWaveSize();
	const int32 Spawned = SpawnEnemies(WaveSize);
	UE_LOG(LogTemp, Log, TEXT("[YSTimedWaveSpawnRule] %.1f s, jour %d%s : vague de %d NME (%d apparus)"), Elapsed, GetDayNumber(), IsNight() ? TEXT(" (nuit)") : TEXT(""), WaveSize, Spawned);
}
