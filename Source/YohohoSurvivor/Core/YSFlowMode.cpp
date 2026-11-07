#include "Core/YSFlowMode.h"
#include "Components/CapsuleComponent.h"
#include "Core/YSGameState.h"
#include "Core/YSPlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Enemies/YSFoeCharacter.h"
#include "Enemies/YSNavalBattle.h"
#include "Island/YSIsland.h"
#include "Island/YSIslandData.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Player/YSPlayerCharacter.h"
#include "Ship/YSGameBoat.h"

namespace YSFlowMode
{
	/** Apparition au-dessus de la place, le temps d'être installé au poste */
	constexpr float SeatSpawnHeight = 100.0f;
}

AYSFlowMode::AYSFlowMode()
{
	// demande de Paul (2026-11-07) : plus de bateaux de NME dans le flow, seulement dans les batailles navales
	bJollyShipWaves = false;
	bGalionWaves = false;
	NavalBattleRows = { FName(TEXT("Base")), FName(TEXT("Base")) };
}

void AYSFlowMode::InitGameState()
{
	Super::InitGameState();

	if (AYSGameState* YSGameState = GetGameState<AYSGameState>())
	{
		YSGameState->SetBoatIntro(GameBoatData != nullptr);
	}
}

void AYSFlowMode::BeginPlay()
{
	Super::BeginPlay();

	if (!StartIsland)
	{
		UE_LOG(LogTemp, Error, TEXT("[YSFlowMode] Pas d'île de départ (Start Island)"));
		return;
	}

	int32 Seed = 0;
	FParse::Value(FCommandLine::Get(), TEXT("YSIslandSeed="), Seed);

	const FTransform Transform = FTransform::Identity;
	Island = GetWorld()->SpawnActorDeferred<AYSIsland>(AYSIsland::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Island)
	{
		Island->InitIsland(StartIsland, Seed, true);
		Island->FinishSpawning(Transform);
		// îles suivantes générées dès maintenant, sous l'eau
		if (bPregenerateNextIslands && NextIslandsSubmergedDepth > 0.0f)
		{
			SpawnNextIslands(Island, true);
		}
	}
}

void AYSFlowMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// « go naviguer » : vagues en mer programmées
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState || PendingNavigationWaves.Num() == 0)
	{
		return;
	}
	const float Now = YSGameState->GetMatchElapsedTime();
	for (int32 Index = PendingNavigationWaves.Num() - 1; Index >= 0; --Index)
	{
		if (Now < PendingNavigationWaves[Index].Key)
		{
			continue;
		}
		const FYSNavigationWave Wave = PendingNavigationWaves[Index].Value;
		PendingNavigationWaves.RemoveAt(Index);
		const int32 JollyCount = Wave.bScaleWithPlayers ? ScaleEnemyCount(Wave.JollyShips) : Wave.JollyShips;
		const int32 GalionCount = Wave.bScaleWithPlayers ? ScaleEnemyCount(Wave.Galions) : Wave.Galions;
		int32 Jollies = 0;
		int32 Galions = 0;
		for (int32 K = 0; K < JollyCount; ++K)
		{
			Jollies += SpawnJollyShip(TEXT("go naviguer")) ? 1 : 0;
		}
		for (int32 K = 0; K < GalionCount; ++K)
		{
			Galions += SpawnGalion(TEXT("go naviguer")) ? 1 : 0;
		}
		UE_LOG(LogTemp, Log, TEXT("[YSNavigation] Vague à %.0f s : %d / %d chaloupe(s), %d / %d Galion(s)"), Wave.Delay, Jollies, JollyCount, Galions, GalionCount);
	}
}

void AYSFlowMode::OnIslandSinkStarted(AYSIsland* SinkingIsland)
{
	StartNavigationWaves();
	// objectifs jamais livrés : elles apparaissent quand même à la coulaison de l'île de départ (sans effet si déjà là)
	if (SinkingIsland && SinkingIsland == Island)
	{
		BringNextIslands(SinkingIsland);
	}
}

void AYSFlowMode::OnIslandMainObjectivesDone(AYSIsland* DoneIsland)
{
	UpgradeGameBoat();
	// demande de Paul : les îles suivantes arrivent dès que les objectifs de l'île de départ sont livrés (plus à sa coulaison)
	if (DoneIsland && DoneIsland == Island)
	{
		BringNextIslands(DoneIsland);
	}
}

void AYSFlowMode::BringNextIslands(const AYSIsland* From)
{
	if (bNextIslandsBrought)
	{
		return;
	}
	bNextIslandsBrought = true;
	// prégénérées : elles remontent ; sinon elles apparaissent maintenant
	SpawnNextIslands(From, false);
	for (AYSIsland* NextIsland : SpawnedIslands)
	{
		if (IsValid(NextIsland))
		{
			NextIsland->StartRise(NextIslandsRiseDuration);
		}
	}
	SpawnNavalBattles();
}

void AYSFlowMode::SpawnNavalBattles()
{
	if (bNavalBattlesSpawned || !NavalBattleTable || NavalBattleRows.Num() == 0)
	{
		if (!NavalBattleTable && !bNavalBattlesSpawned)
		{
			UE_LOG(LogTemp, Warning, TEXT("[YSNavalBattle] Pas de Naval Battle Table sur le FlowMode : aucune bataille navale"));
		}
		bNavalBattlesSpawned = true;
		return;
	}
	bNavalBattlesSpawned = true;

	// paires d'îles suivantes (toutes les paires, mélangées) : une bataille par paire, au milieu de l'espace entre leurs côtes
	TArray<TPair<AYSIsland*, AYSIsland*>> Pairs;
	for (int32 A = 0; A < SpawnedIslands.Num(); ++A)
	{
		for (int32 B = A + 1; B < SpawnedIslands.Num(); ++B)
		{
			if (IsValid(SpawnedIslands[A]) && IsValid(SpawnedIslands[B]))
			{
				Pairs.Emplace(SpawnedIslands[A], SpawnedIslands[B]);
			}
		}
	}
	for (int32 K = Pairs.Num() - 1; K > 0; --K)
	{
		Pairs.Swap(K, FMath::RandRange(0, K));
	}
	for (int32 Index = 0; Index < NavalBattleRows.Num() && Index < Pairs.Num(); ++Index)
	{
		const FYSNavalBattleRow* Row = NavalBattleTable->FindRow<FYSNavalBattleRow>(NavalBattleRows[Index], TEXT("SpawnNavalBattles"));
		if (!Row)
		{
			UE_LOG(LogTemp, Warning, TEXT("[YSNavalBattle] Ligne %s absente de %s"), *NavalBattleRows[Index].ToString(), *NavalBattleTable->GetName());
			continue;
		}
		// entre les deux îles sur le cercle des îles suivantes (le milieu en ligne droite tomberait près de l'île de départ, au centre du triangle)
		const AYSIsland* First = Pairs[Index].Key;
		const AYSIsland* Second = Pairs[Index].Value;
		const FVector Center = Island ? Island->GetActorLocation() * FVector(1.0f, 1.0f, 0.0f) : FVector::ZeroVector;
		const FVector A = First->GetActorLocation() * FVector(1.0f, 1.0f, 0.0f);
		const FVector B = Second->GetActorLocation() * FVector(1.0f, 1.0f, 0.0f);
		FVector MidDirection = ((A - Center).GetSafeNormal() + (B - Center).GetSafeNormal()).GetSafeNormal();
		if (MidDirection.IsNearlyZero())
		{
			MidDirection = (B - A).GetSafeNormal().RotateAngleAxis(90.0f, FVector::UpVector);
		}
		const FVector Location = Center + MidDirection * (FVector::Dist(A, Center) + FVector::Dist(B, Center)) * 0.5f;
		const float Gap = FMath::Min(FVector::Dist(Location, A) - First->GetRadius(), FVector::Dist(Location, B) - Second->GetRadius());
		const FTransform Transform(FRotator::ZeroRotator, Location);
		if (AYSNavalBattle* Battle = GetWorld()->SpawnActorDeferred<AYSNavalBattle>(AYSNavalBattle::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Battle->InitBattle(*Row, NavalBattleRows[Index]);
			Battle->FinishSpawning(Transform);
			NavalBattles.Add(Battle);
			UE_LOG(LogTemp, Log, TEXT("[YSNavalBattle] Bataille %s entre %s et %s, en %s (côte la plus proche à %.0f m)"), *NavalBattleRows[Index].ToString(), *First->GetData()->GetName(),
				*Second->GetData()->GetName(), *Location.ToCompactString(), Gap / 100.0f);
		}
	}
}

void AYSFlowMode::DebugActivateNavalBattles()
{
	for (AYSNavalBattle* Battle : NavalBattles)
	{
		if (IsValid(Battle))
		{
			Battle->Activate(TEXT("debug"));
		}
	}
}

void AYSFlowMode::StartNavigationWaves()
{
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	const float Now = YSGameState ? YSGameState->GetMatchElapsedTime() : 0.0f;
	for (const FYSNavigationWave& Wave : NavigationWaves)
	{
		PendingNavigationWaves.Emplace(Now + Wave.Delay, Wave);
	}
	UE_LOG(LogTemp, Log, TEXT("[YSNavigation] Go naviguer : %d vague(s) en mer programmée(s)"), NavigationWaves.Num());
}

void AYSFlowMode::SpawnNextIslands(const AYSIsland* From, bool bSubmerged)
{
	if (bNextIslandsSpawned || !From || NextIslands.Num() == 0)
	{
		return;
	}
	bNextIslandsSpawned = true;

	// places régulières autour de l'île de départ (triangle pour 3 îles), un peu décalées ; quelle île va où : au hasard
	// (les îles gardent l'ordre de Next Islands : YSIslandGoTo 1, 2, 3...)
	TArray<UYSIslandData*> Order;
	for (UYSIslandData* Data : NextIslands)
	{
		if (Data)
		{
			Order.Add(Data);
		}
	}
	TArray<int32> Slots;
	for (int32 Index = 0; Index < Order.Num(); ++Index)
	{
		Slots.Add(Index);
	}
	for (int32 K = Slots.Num() - 1; K > 0; --K)
	{
		Slots.Swap(K, FMath::RandRange(0, K));
	}
	const float BaseAngle = FMath::FRandRange(0.0f, 360.0f);
	const FVector Center = From->GetActorLocation() * FVector(1.0f, 1.0f, 0.0f);
	for (int32 Index = 0; Index < Order.Num(); ++Index)
	{
		UYSIslandData* Data = Order[Index];
		const float Angle = FMath::DegreesToRadians(BaseAngle + 360.0f * Slots[Index] / Order.Num() + FMath::FRandRange(-NextIslandsAngleJitter, NextIslandsAngleJitter));
		const float Distance = From->GetRadius() + NextIslandsGap + Data->Generation.Radius;
		const FVector Location = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance;
		const float Yaw = Data->bRandomRotation ? FMath::FRandRange(0.0f, 360.0f) : 0.0f;
		// prégénérée : sous l'eau, à sa place
		const float Depth = bSubmerged ? NextIslandsSubmergedDepth : 0.0f;
		const FTransform Transform(FRotator(0.0f, Yaw, 0.0f), Location - FVector(0.0f, 0.0f, Depth));
		AYSIsland* NewIsland = GetWorld()->SpawnActorDeferred<AYSIsland>(AYSIsland::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (NewIsland)
		{
			NewIsland->InitIsland(Data, 0, false);
			NewIsland->SetSubmerged(Depth);
			NewIsland->FinishSpawning(Transform);
			SpawnedIslands.Add(NewIsland);
			UE_LOG(LogTemp, Log, TEXT("[YSFlowMode] Île suivante %s en %s (yaw %.0f°, %.0f m du centre de départ%s)"), *Data->GetName(), *Location.ToCompactString(), Yaw, Distance / 100.0f,
				bSubmerged ? *FString::Printf(TEXT(", prégénérée %.0f cm sous l'eau"), Depth) : TEXT(""));
		}
	}
}

void AYSFlowMode::OnEliteKilled(AYSFoeCharacter* Elite)
{
	// l'île qui a fait apparaître cet élite pour sa nuit ; sinon la plus proche
	for (TActorIterator<AYSIsland> It(GetWorld()); It; ++It)
	{
		if (It->OwnsNightElite(Elite))
		{
			It->OnEliteKilled(Elite);
			return;
		}
	}
	if (AYSIsland* EliteIsland = Elite ? AYSIsland::FindAt(GetWorld(), Elite->GetActorLocation()) : nullptr)
	{
		EliteIsland->OnEliteKilled(Elite);
	}
}

void AYSFlowMode::OnDayStarted(int32 Day)
{
	Super::OnDayStarted(Day);

	for (TActorIterator<AYSIsland> It(GetWorld()); It; ++It)
	{
		It->OnGlobalDayChanged(Day);
	}
}

APawn* AYSFlowMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	// à leur place sur la chaloupe tant qu'elle n'a pas accosté (EnterStation les y pose exactement)
	const AYSPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AYSPlayerState>() : nullptr;
	if (const AYSGameBoatLongboat* Boat = PlayerState && PlayerState->GetBoatSeat() != INDEX_NONE ? GetArrivingBoat() : nullptr)
	{
		FTransform Seat = Boat->GetSeatFeetTransform(PlayerState->GetBoatSeat());
		Seat.AddToTranslation(FVector(0.0f, 0.0f, YSFlowMode::SeatSpawnHeight));
		Seat.SetScale3D(FVector::OneVector);
		return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, Seat);
	}

	// la plage change à chaque partie : les joueurs arrivent à côté du bateau de départ, pas sur les PlayerStart
	return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, Island ? Island->GetPlayerSpawnTransform(NextPlayerSpot++) : SpawnTransform);
}

AYSGameBoatLongboat* AYSFlowMode::GetArrivingBoat() const
{
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	AYSGameBoatLongboat* Boat = YSGameState ? Cast<AYSGameBoatLongboat>(YSGameState->GetGameBoat()) : nullptr;
	return Boat && !Boat->HasLanded() ? Boat : nullptr;
}

void AYSFlowMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// une place libre sur la chaloupe, dès l'arrivée
	AYSPlayerState* NewPlayerState = NewPlayer ? NewPlayer->GetPlayerState<AYSPlayerState>() : nullptr;
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (GameBoatData && NewPlayerState && YSGameState && NewPlayerState->GetBoatSeat() == INDEX_NONE)
	{
		for (int32 Seat = 0; Seat < AYSGameBoatLongboat::SeatCount; ++Seat)
		{
			const bool bTaken = YSGameState->PlayerArray.ContainsByPredicate([Seat](const APlayerState* Other)
			{
				const AYSPlayerState* YSOther = Cast<AYSPlayerState>(Other);
				return YSOther && YSOther->GetBoatSeat() == Seat;
			});
			if (!bTaken)
			{
				NewPlayerState->SetBoatSeat(Seat);
				UE_LOG(LogTemp, Log, TEXT("[YSGameBoat] %s : place %d"), *NewPlayerState->GetPlayerName(), Seat);
				break;
			}
		}
	}

	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

void AYSFlowMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	const AYSPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AYSPlayerState>() : nullptr;
	const AYSGameBoatLongboat* Boat = GetArrivingBoat();
	AYSPlayerCharacter* Character = NewPlayer ? Cast<AYSPlayerCharacter>(NewPlayer->GetPawn()) : nullptr;
	if (Boat && Character && PlayerState)
	{
		Character->EnterStation(Boat->GetSeat(PlayerState->GetBoatSeat()));
	}
}

void AYSFlowMode::OnAllPlayersReady()
{
	if (AYSGameBoatLongboat* Boat = GetArrivingBoat())
	{
		Boat->StartSailing();
	}
}

bool AYSFlowMode::SpawnGameBoat(AYSIsland* ForIsland)
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	UClass* BoatClass = GameBoatData && GameBoatData->Stages.IsValidIndex(0) ? GameBoatData->Stages[0].Get() : nullptr;
	if (!BoatClass || !YSGameState || !ForIsland)
	{
		if (GameBoatData)
		{
			UE_LOG(LogTemp, Error, TEXT("[YSGameBoat] %s : pas de première étape"), *GameBoatData->GetName());
		}
		return false;
	}

	const FTransform Transform(FRotator::ZeroRotator, ForIsland->GetGameBoatStartLocation(GameBoatData->StartDistanceFromShore));
	AYSPirateShip* Boat = GetWorld()->SpawnActorDeferred<AYSPirateShip>(BoatClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Boat)
	{
		return false;
	}
	Boat->FinishSpawning(Transform);
	GameBoatStage = 0;
	YSGameState->SetGameBoat(Boat);
	UE_LOG(LogTemp, Log, TEXT("[YSGameBoat] %s en %s"), *Boat->GetName(), *Transform.GetLocation().ToCompactString());
	return true;
}

void AYSFlowMode::OnGameBoatLanded(AYSGameBoatLongboat* Boat, const FVector& Landing)
{
	if (Island)
	{
		Island->SpawnStartBoat(Landing, Boat->GetActorRotation().Yaw);
	}
}

void AYSFlowMode::UpgradeGameBoat()
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	AYSPirateShip* OldBoat = YSGameState ? YSGameState->GetGameBoat() : nullptr;
	const int32 NextStage = GameBoatStage + 1;
	UClass* NextClass = GameBoatData && GameBoatData->Stages.IsValidIndex(NextStage) ? GameBoatData->Stages[NextStage].Get() : nullptr;
	if (!OldBoat || !NextClass)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSGameBoat] Pas d'étape %d (bateau %s)"), NextStage, OldBoat ? *OldBoat->GetName() : TEXT("-"));
		return;
	}

	// assez de fond pour sa coque : à reculons depuis la plage, le long de la direction d'arrivée
	const FVector From = OldBoat->GetActorLocation();
	const FVector Away = -OldBoat->GetActorForwardVector().GetSafeNormal2D();
	const float Yaw = OldBoat->GetActorRotation().Yaw + GameBoatData->UpgradeYawOffset;
	const FTransform SpawnTransform(FRotator(0.0f, Yaw, 0.0f), From);
	AYSPirateShip* NewBoat = GetWorld()->SpawnActorDeferred<AYSPirateShip>(NextClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!NewBoat)
	{
		return;
	}
	FVector Location = From + Away * GameBoatData->WaterSearchDistance;
	if (!NewBoat->FindWaterPlacement(From, Away, Yaw, GameBoatData->WaterSearchDistance, GameBoatData->WaterSearchStep, Location))
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSGameBoat] %s : pas assez de fond à moins de %.0f cm, posé au plus loin"), *NewBoat->GetName(), GameBoatData->WaterSearchDistance);
	}
	NewBoat->FinishSpawning(FTransform(FRotator(0.0f, Yaw, 0.0f), Location));
	UE_LOG(LogTemp, Log, TEXT("[YSGameBoat] Étape %d : %s remplace %s, en %s (%.0f cm de la chaloupe)"), NextStage, *NewBoat->GetName(), *OldBoat->GetName(), *Location.ToCompactString(), FVector::Dist2D(From, Location));
	GameBoatStage = NextStage;
	YSGameState->SetGameBoat(NewBoat);
	OldBoat->Destroy();
}
