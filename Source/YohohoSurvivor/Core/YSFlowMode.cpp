#include "Core/YSFlowMode.h"
#include "Components/CapsuleComponent.h"
#include "Core/YSGameState.h"
#include "Core/YSPlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Enemies/YSFoeCharacter.h"
#include "Enemies/YSNavalBattle.h"
#include "Enemies/YSFoeGalion.h"
#include "Enemies/YSKrakenArena.h"
#include "Enemies/YSDavyJonesArena.h"
#include "Interaction/YSObjectiveTreasureMap.h"
#include "Map/YSOcean.h"
#include "TimerManager.h"
#include "Island/YSIsland.h"
#include "Island/YSIslandData.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Player/YSPlayerCharacter.h"
#include "Ship/YSGameBoat.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Ship/YSPirateShip.h"
#include "Ship/YSShipHelm.h"
#include "Ship/YSCombatPost.h"
#include "Ship/YSShipStation.h"
#include "Interaction/YSInteractiveObject.h"

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
	// demande de Paul (2026-11-09) : 2 relances par sélection d'upgrade
	MaxUpgradeRerolls = 2;
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
		// partie 2 : îles et batailles navales générées dès maintenant, à la surface, en polygone autour de l'île de départ
		if (bNextIslandsEnabled && bZonesOfInterest)
		{
			SpawnZonesOfInterest();
			SpawnKrakenArena();
			SpawnDavyJonesArena();
		}
		// îles suivantes générées dès maintenant, sous l'eau
		else if (bNextIslandsEnabled && bPregenerateNextIslands && NextIslandsSubmergedDepth > 0.0f)
		{
			SpawnNextIslands(Island, true);
			SpawnFinalIsland(true);
		}
	}
}

void AYSFlowMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// GameBoat : étapes demandées (core du bateau livré)
	if (PendingGameBoatUpgrades > 0)
	{
		--PendingGameBoatUpgrades;
		UpgradeGameBoat();
	}
	else if (StocksCoreBoats())
	{
		UpdateCoreBoatStock();
	}

	// île finale : quand toutes les îles suivantes et toutes les batailles navales sont faites (pas avec les zones d'intérêt : partie 3 à venir)
	if (FinalIsland && !bZonesOfInterest && bNextIslandsBrought && !bFinalIslandBrought && AreNextIslandsAndBattlesDone())
	{
		BringFinalIsland(TEXT("îles suivantes et batailles navales faites"));
	}

	// partie 2 écoulée : overtime, un Galion Nemesis tout de suite puis toutes les Part 2 Overtime Interval
	const AYSGameState* Part2State = GetGameState<AYSGameState>();
	if (bPart2Started && !bPart3Started && Part2State && Part2OvertimeGalionClass && NextPart2GalionElapsed >= 0.0f)
	{
		const float Elapsed = Part2State->GetPart2Elapsed();
		if (Elapsed >= NextPart2GalionElapsed)
		{
			if (NextPart2GalionElapsed <= Part2Duration)
			{
				UE_LOG(LogTemp, Log, TEXT("[YSFlowMode] Partie 2 : OVERTIME (%.0f s écoulées), un Galion Nemesis toutes les %.0f s"), Elapsed, Part2OvertimeInterval);
			}
			// pas de place : réessayé dans 2 s
			NextPart2GalionElapsed = SpawnPart2Galion() ? NextPart2GalionElapsed + Part2OvertimeInterval : Elapsed + 2.0f;
		}
	}

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
		// fin de la partie 1 : l'objectif en mer remplace celui de l'île (demande de Paul : plus de « Victoire »)
		if (bZonesOfInterest)
		{
			StartSeaObjectives(TEXT("l'île de départ coule"));
		}
	}
}

void AYSFlowMode::OnIslandMainObjectivesDone(AYSIsland* DoneIsland)
{
	// demande de Paul : les îles suivantes arrivent dès que les objectifs de l'île de départ sont livrés (plus à sa coulaison) ;
	// le GameBoat passe au sloop. Îles suivantes : il change à chaque core du bateau livré (AYSIsland::NotifyObjectDelivered)
	if (DoneIsland && DoneIsland == Island)
	{
		RequestGameBoatUpgrade(TEXT("objectifs de l'île de départ livrés"));
		BringNextIslands(DoneIsland);
	}
}

void AYSFlowMode::RequestGameBoatUpgrade(const TCHAR* Reason)
{
	// à l'image suivante : l'objet vient d'être livré dans la zone du core du bateau actuel, qui va être détruit
	++PendingGameBoatUpgrades;
	UE_LOG(LogTemp, Log, TEXT("[YSGameBoat] Étape suivante demandée : %s"), Reason);
}

void AYSFlowMode::BringNextIslands(const AYSIsland* From)
{
	if (bNextIslandsBrought || !bNextIslandsEnabled)
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
	// l'espace entre les deux îles de l'île finale en dernier (pas de bataille navale là-bas tant qu'il reste d'autres paires)
	if (FinalIslandPair.Num() == 2)
	{
		auto IsFinalPair = [this](const TPair<AYSIsland*, AYSIsland*>& Pair) { return FinalIslandPair.Contains(Pair.Key) && FinalIslandPair.Contains(Pair.Value); };
		Pairs.StableSort([&IsFinalPair](const TPair<AYSIsland*, AYSIsland*>& P, const TPair<AYSIsland*, AYSIsland*>& Q) { return !IsFinalPair(P) && IsFinalPair(Q); });
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

void AYSFlowMode::SpawnFinalIsland(bool bSubmerged)
{
	if (FinalIslandActor || !FinalIsland || !Island)
	{
		return;
	}
	const FVector Center = Island->GetActorLocation() * FVector(1.0f, 1.0f, 0.0f);
	const float FinalRadius = FinalIsland->Generation.Radius;

	// direction : entre deux îles suivantes tirées au hasard (cet espace n'aura pas de bataille navale), à défaut à l'opposé de la seule île suivante
	TArray<AYSIsland*> Others;
	for (AYSIsland* Other : SpawnedIslands)
	{
		if (IsValid(Other))
		{
			Others.Add(Other);
		}
	}
	const float RandomAngle = FMath::FRandRange(0.0f, 2.0f * PI);
	FVector Direction(FMath::Cos(RandomAngle), FMath::Sin(RandomAngle), 0.0f);
	if (Others.Num() >= 2)
	{
		const int32 First = FMath::RandRange(0, Others.Num() - 1);
		int32 Second = FMath::RandRange(0, Others.Num() - 2);
		Second += Second >= First ? 1 : 0;
		FinalIslandPair = { Others[First], Others[Second] };
		Direction = ((Others[First]->GetActorLocation() - Center).GetSafeNormal2D() + (Others[Second]->GetActorLocation() - Center).GetSafeNormal2D()).GetSafeNormal2D();
	}
	else if (Others.Num() == 1)
	{
		Direction = -(Others[0]->GetActorLocation() - Center).GetSafeNormal2D();
	}
	if (Direction.IsNearlyZero())
	{
		Direction = FVector::ForwardVector;
	}

	// assez loin pour laisser Final Island Gap entre sa côte et celle de chaque autre île
	Others.Add(Island);
	const float Gap = FinalIslandGap * NextIslandsDistanceMultiplier;
	float Distance = Island->GetRadius() + Gap + FinalRadius;
	FVector Location = Center + Direction * Distance;
	for (int32 Step = 0; Step < 1000; ++Step)
	{
		Location = Center + Direction * Distance;
		const bool bClear = !Others.ContainsByPredicate([&](const AYSIsland* Other)
		{
			return FVector::Dist2D(Other->GetActorLocation(), Location) < Other->GetRadius() + Gap + FinalRadius;
		});
		if (bClear)
		{
			break;
		}
		Distance += 1000.0f;
	}

	const float Yaw = FinalIsland->bRandomRotation ? FMath::FRandRange(0.0f, 360.0f) : 0.0f;
	const float Depth = bSubmerged ? NextIslandsSubmergedDepth : 0.0f;
	const FTransform Transform(FRotator(0.0f, Yaw, 0.0f), Location - FVector(0.0f, 0.0f, Depth));
	FinalIslandActor = GetWorld()->SpawnActorDeferred<AYSIsland>(AYSIsland::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (FinalIslandActor)
	{
		FinalIslandActor->InitIsland(FinalIsland, 0, false);
		FinalIslandActor->SetSubmerged(Depth);
		FinalIslandActor->FinishSpawning(Transform);
		UE_LOG(LogTemp, Log, TEXT("[YSFlowMode] Île finale %s en %s (yaw %.0f°, %.0f m du centre de départ, entre %s et %s%s)"), *FinalIsland->GetName(), *Location.ToCompactString(), Yaw, Distance / 100.0f,
			FinalIslandPair.Num() == 2 ? *FinalIslandPair[0]->GetData()->GetName() : TEXT("-"), FinalIslandPair.Num() == 2 ? *FinalIslandPair[1]->GetData()->GetName() : TEXT("-"),
			bSubmerged ? *FString::Printf(TEXT(", prégénérée %.0f cm sous l'eau"), Depth) : TEXT(""));
	}
}

void AYSFlowMode::BringFinalIsland(const TCHAR* Reason)
{
	if (bFinalIslandBrought || !FinalIsland)
	{
		return;
	}
	bFinalIslandBrought = true;
	SpawnFinalIsland(false);
	if (IsValid(FinalIslandActor))
	{
		FinalIslandActor->StartRise(FinalIslandRiseDuration);
		UE_LOG(LogTemp, Log, TEXT("[YSFlowMode] L'île finale %s arrive (%s)"), *FinalIsland->GetName(), Reason);
	}
}

bool AYSFlowMode::AreNextIslandsAndBattlesDone() const
{
	if (!bNextIslandsBrought || !bNavalBattlesSpawned)
	{
		return false;
	}
	for (const AYSIsland* NextIsland : SpawnedIslands)
	{
		if (IsValid(NextIsland) && !NextIsland->AreMainObjectivesDone() && !NextIsland->HasSinkStarted())
		{
			return false;
		}
	}
	for (const AYSNavalBattle* Battle : NavalBattles)
	{
		if (IsValid(Battle) && !Battle->IsWon())
		{
			return false;
		}
	}
	return true;
}

AYSIsland* AYSFlowMode::GetIslandByIndex(int32 Index) const
{
	if (Index == 0)
	{
		return Island;
	}
	if (SpawnedIslands.IsValidIndex(Index - 1))
	{
		return SpawnedIslands[Index - 1];
	}
	return Index == SpawnedIslands.Num() + 1 ? FinalIslandActor.Get() : nullptr;
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
		// jamais au point que les côtes se touchent
		const float Distance = FMath::Max((From->GetRadius() + NextIslandsGap + Data->Generation.Radius) * NextIslandsDistanceMultiplier, From->GetRadius() + Data->Generation.Radius + 2000.0f);
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

AYSShipStation* AYSFlowMode::FindHiredIntroSeat(FTransform& OutFeet) const
{
	const AYSGameBoatLongboat* Boat = GetArrivingBoat();
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!Boat || !YSGameState)
	{
		return nullptr;
	}
	for (int32 Seat = 0; Seat < AYSGameBoatLongboat::SeatCount; ++Seat)
	{
		AYSGameBoatSeat* Station = Boat->GetSeat(Seat);
		const bool bPlayerSeat = YSGameState->PlayerArray.ContainsByPredicate([Seat](const APlayerState* Other)
		{
			const AYSPlayerState* YSOther = Cast<AYSPlayerState>(Other);
			return YSOther && YSOther->GetBoatSeat() == Seat;
		});
		if (Station && !bPlayerSeat && !Station->GetOccupant())
		{
			OutFeet = Boat->GetSeatFeetTransform(Seat);
			return Station;
		}
	}
	return nullptr;
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
	// joueur « mort » pour l'hôte (apparition ratée au lancement) : il apparaît sur la plage ; les autres reçoivent leur état à nouveau
	RespawnMissingPlayers(TEXT("débarquement"), true);
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

	// chaloupe échouée : à reculons depuis la plage, le long de la direction d'arrivée, tourné (accès côté plage) ;
	// bateau en mer : à sa place, même cap, éloigné de l'île la plus proche s'il n'y a pas assez de fond pour la coque plus grande
	const bool bFromLongboat = OldBoat->IsA<AYSGameBoatLongboat>();
	const FVector From = OldBoat->GetActorLocation();
	FVector Away = -OldBoat->GetActorForwardVector().GetSafeNormal2D();
	float Yaw = OldBoat->GetActorRotation().Yaw + GameBoatData->UpgradeYawOffset;
	if (!bFromLongboat)
	{
		Yaw = OldBoat->GetActorRotation().Yaw;
		const AYSIsland* NearestIsland = AYSIsland::FindAt(GetWorld(), From);
		Away = NearestIsland ? (From - NearestIsland->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
		if (Away.IsNearlyZero())
		{
			Away = -OldBoat->GetActorRightVector().GetSafeNormal2D();
		}
	}
	const FTransform SpawnTransform(FRotator(0.0f, Yaw, 0.0f), From);
	AYSPirateShip* NewBoat = GetWorld()->SpawnActorDeferred<AYSPirateShip>(NextClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!NewBoat)
	{
		return;
	}
	// chaloupe garée entre les pontons de la plage : le bateau y apparaît, coque centrée sur la place, tourné vers le sud
	FTransform Berth;
	const bool bAtBerth = bFromLongboat && Island && Island->GetBerthTransform(Berth);
	FVector Location = From + Away * GameBoatData->WaterSearchDistance;
	if (!bAtBerth && !NewBoat->FindWaterPlacement(From, Away, Yaw, GameBoatData->WaterSearchDistance, GameBoatData->WaterSearchStep, Location))
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSGameBoat] %s : pas assez de fond à moins de %.0f cm, posé au plus loin"), *NewBoat->GetName(), GameBoatData->WaterSearchDistance);
	}
	if (bAtBerth)
	{
		Yaw = Berth.Rotator().Yaw;
		Location = Berth.GetLocation();
	}
	NewBoat->FinishSpawning(FTransform(FRotator(0.0f, Yaw, 0.0f), Location));
	if (bAtBerth)
	{
		TArray<FVector> HullPoints;
		NewBoat->GetWorldHullPoints(HullPoints);
		FBox HullBox(HullPoints);
		if (HullBox.IsValid)
		{
			const FVector Shift = (Berth.GetLocation() - HullBox.GetCenter()) * FVector(1.0f, 1.0f, 0.0f);
			NewBoat->SetActorLocation(NewBoat->GetActorLocation() + Shift, false, nullptr, ETeleportType::TeleportPhysics);
		}
		Location = NewBoat->GetActorLocation();
		UE_LOG(LogTemp, Log, TEXT("[YSGameBoat] %s posé entre les pontons de la plage (yaw %.0f°)"), *NewBoat->GetName(), Yaw);
	}
	NewBoat->SyncMotionToActor();
	UE_LOG(LogTemp, Log, TEXT("[YSGameBoat] Étape %d : %s remplace %s, en %s (%.0f cm de l'ancien)"), NextStage, *NewBoat->GetName(), *OldBoat->GetName(), *Location.ToCompactString(), FVector::Dist2D(From, Location));
	GameBoatStage = NextStage;
	YSGameState->SetGameBoat(NewBoat);
	if (!bFromLongboat)
	{
		TransferAboard(OldBoat, NewBoat);
		TransferCombatPosts(OldBoat, NewBoat);
	}
	OldBoat->Destroy();
}

void AYSFlowMode::TransferAboard(AYSPirateShip* OldBoat, AYSPirateShip* NewBoat)
{
	// emprise des deux coques (repère local) : une place à bord garde la même position proportionnelle, hauteur gardée (les étapes ont le même pont)
	const auto LocalBounds = [](const AYSPirateShip* Ship)
	{
		TArray<FVector> Points;
		Ship->GetWorldHullPoints(Points);
		FBox2D Box(ForceInit);
		for (const FVector& Point : Points)
		{
			const FVector Local = Ship->GetActorTransform().InverseTransformPosition(Point);
			Box += FVector2D(Local.X, Local.Y);
		}
		return Box;
	};
	const FBox2D OldBox = LocalBounds(OldBoat);
	const FBox2D NewBox = LocalBounds(NewBoat);
	const FVector2D Scale(OldBox.GetSize().X > 1.0 ? NewBox.GetSize().X / OldBox.GetSize().X : 1.0, OldBox.GetSize().Y > 1.0 ? NewBox.GetSize().Y / OldBox.GetSize().Y : 1.0);
	const auto MapLocation = [&](const FVector& World)
	{
		const FVector Local = OldBoat->GetActorTransform().InverseTransformPosition(World);
		const FVector2D Mapped = (FVector2D(Local.X, Local.Y) - OldBox.GetCenter()) * Scale + NewBox.GetCenter();
		return NewBoat->GetActorTransform().TransformPosition(FVector(Mapped.X, Mapped.Y, Local.Z));
	};
	const float DeltaYaw = NewBoat->GetActorRotation().Yaw - OldBoat->GetActorRotation().Yaw;

	// personnages à bord (postes compris) : joueurs, minions, NME montés sur le pont
	OldBoat->SetActorEnableCollision(false);
	int32 Characters = 0;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Character = *It;
		AYSPlayerCharacter* Player = Cast<AYSPlayerCharacter>(Character);
		const UPrimitiveComponent* Base = Character->GetMovementBase();
		const bool bAboard = Player ? Player->GetShipUnderFeet() == OldBoat : (Base && AYSPirateShip::GetShipOf(Base->GetOwner()) == OldBoat);
		if (!bAboard || Character->IsActorBeingDestroyed())
		{
			continue;
		}
		const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Feet = MapLocation(Character->GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight));
		const FVector Facing = FRotator(0.0f, Character->GetActorRotation().Yaw + DeltaYaw, 0.0f).Vector();
		if (Player)
		{
			const bool bWasPiloting = Player->GetCurrentStation() && Player->GetCurrentStation()->UsesPilotInput();
			Player->LeaveStation(TEXT("le bateau change"), true);
			Player->TeleportToFeetLocation(Feet, Facing);
			if (bWasPiloting && NewBoat->GetHelm())
			{
				Player->EnterStation(NewBoat->GetHelm());
			}
		}
		else
		{
			Character->SetActorLocationAndRotation(Feet + FVector(0.0f, 0.0f, HalfHeight + 2.0f), Facing.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
			Character->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		}
		++Characters;
	}

	// posé sur le pont (objets, cadavres, PNJ livrés) ; pas les postes, acteurs enfants du bateau
	TArray<AActor*> Attached;
	OldBoat->GetAttachedActors(Attached, true, false);
	int32 Objects = 0;
	for (AActor* Actor : Attached)
	{
		// pas les postes (acteurs enfants, ou postes de combat posés sur un réceptacle : TransferCombatPosts)
		if (!Actor || Actor->IsChildActor() || Actor->IsA<APawn>() || Actor->IsA<AYSShipStation>() || Actor->IsActorBeingDestroyed())
		{
			continue;
		}
		Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Actor->SetActorLocationAndRotation(MapLocation(Actor->GetActorLocation()), Actor->GetActorRotation() + FRotator(0.0f, DeltaYaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		Actor->AttachToActor(NewBoat, FAttachmentTransformRules::KeepWorldTransform);
		++Objects;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSGameBoat] %s -> %s : %d personnage(s) et %d objet(s) à bord passés sur le nouveau bateau (échelle %.2f x %.2f)"), *OldBoat->GetName(), *NewBoat->GetName(),
		Characters, Objects, Scale.X, Scale.Y);
}

void AYSFlowMode::TransferCombatPosts(AYSPirateShip* OldBoat, AYSPirateShip* NewBoat)
{
	// postes de combat posés : sur le réceptacle de même rang du nouveau bateau, sinon le premier libre, sinon rendus en objet sur le pont
	TArray<TSubclassOf<AYSShipStation>> Posts;
	for (AYSCombatPostReceptacle* Receptacle : OldBoat->GetCombatPostReceptacles())
	{
		Posts.Add(Receptacle->RemovePost(TEXT("le bateau change")));
	}
	TArray<AYSCombatPostReceptacle*> NewReceptacles = NewBoat->GetCombatPostReceptacles();
	int32 Moved = 0;
	int32 Returned = 0;
	for (int32 Index = 0; Index < Posts.Num(); ++Index)
	{
		if (!Posts[Index])
		{
			continue;
		}
		AYSCombatPostReceptacle* Target = NewReceptacles.IsValidIndex(Index) && !NewReceptacles[Index]->GetInstalledPost() ? NewReceptacles[Index] : nullptr;
		for (int32 K = 0; !Target && K < NewReceptacles.Num(); ++K)
		{
			Target = NewReceptacles[K]->GetInstalledPost() ? nullptr : NewReceptacles[K];
		}
		if (Target && Target->InstallPost(Posts[Index], TEXT("transféré de l'ancien bateau")))
		{
			++Moved;
			continue;
		}
		const AYSShipStation* PostDefaults = Posts[Index]->GetDefaultObject<AYSShipStation>();
		if (PostDefaults && PostDefaults->GetCombatPostItemClass())
		{
			AYSInteractiveObject* Item = AYSInteractiveObject::SpawnOnGround(GetWorld(), PostDefaults->GetCombatPostItemClass(), NewBoat->GetActorLocation() + FVector(0.0f, 0.0f, 400.0f), 0.0f);
			if (Item)
			{
				Item->AttachToShipBelow();
				++Returned;
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[YSGameBoat] Postes de combat : %d reposé(s) sur %s, %d rendu(s) en objet"), Moved, *NewBoat->GetName(), Returned);
}

void AYSFlowMode::SpawnZonesOfInterest()
{
	if (bNextIslandsSpawned || !Island)
	{
		return;
	}
	// tout est là dès le lancement : rien n'apparaît ni ne remonte plus tard
	bNextIslandsSpawned = true;
	bNextIslandsBrought = true;
	bNavalBattlesSpawned = true;

	// les ZI : îles suivantes puis batailles navales
	struct FZone
	{
		UYSIslandData* IslandData = nullptr;
		const FYSNavalBattleRow* Battle = nullptr;
		FName BattleRow;
		TArray<TSubclassOf<AYSInteractiveObject>> MainItems;
		TArray<TSubclassOf<AYSInteractiveObject>> SecondaryItems;
	};
	TArray<FZone> Zones;
	for (UYSIslandData* Data : NextIslands)
	{
		if (Data)
		{
			Zones.AddDefaulted_GetRef().IslandData = Data;
		}
	}
	for (const FName& RowName : NavalBattleRows)
	{
		const FYSNavalBattleRow* Row = NavalBattleTable ? NavalBattleTable->FindRow<FYSNavalBattleRow>(RowName, TEXT("SpawnZonesOfInterest")) : nullptr;
		if (Row)
		{
			FZone& Zone = Zones.AddDefaulted_GetRef();
			Zone.Battle = Row;
			Zone.BattleRow = RowName;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[YSFlowMode] Zones d'intérêt : ligne %s absente de %s"), *RowName.ToString(), *GetNameSafe(NavalBattleTable));
		}
	}
	if (Zones.Num() == 0)
	{
		return;
	}

	// objets : un CoreBoat par ZI, des boussoles (une au plus par ZI), l'équipage sur les ZI sans boussole, la Super Treasure Map n'importe où
	TArray<int32> Order;
	for (int32 Index = 0; Index < Zones.Num(); ++Index)
	{
		Order.Add(Index);
	}
	for (int32 K = Order.Num() - 1; K > 0; --K)
	{
		Order.Swap(K, FMath::RandRange(0, K));
	}
	for (FZone& Zone : Zones)
	{
		if (BoatCoreItemClass)
		{
			// îles : le CoreBoat est secondaire, l'objectif du jour 1 est la boussole ou le membre d'équipage (demande de Paul)
			(Zone.IslandData ? Zone.SecondaryItems : Zone.MainItems).Add(BoatCoreItemClass);
		}
	}
	// répartition (demande de Paul du 2026-11-12) : les boussoles sur les îles, l'équipage sur les batailles navales ;
	// s'il y en a plus que de ZI du bon type, les suivantes vont sur les autres ZI (une boussole au plus par ZI)
	TArray<int32> IslandZones;
	TArray<int32> BattleZones;
	for (const int32 Index : Order)
	{
		(Zones[Index].IslandData ? IslandZones : BattleZones).Add(Index);
	}
	TArray<int32> CompassSlots = IslandZones;
	CompassSlots.Append(BattleZones);
	const int32 Compasses = CompassItemClass ? FMath::Min(CompassCount, Zones.Num()) : 0;
	CompassTotal = Compasses;
	for (int32 K = 0; K < Compasses; ++K)
	{
		Zones[CompassSlots[K]].MainItems.Add(CompassItemClass);
	}
	// équipage : batailles navales d'abord, puis les ZI sans boussole, puis n'importe où
	TArray<int32> CrewSlots = BattleZones;
	for (const int32 Index : IslandZones)
	{
		if (!Zones[Index].MainItems.Contains(CompassItemClass))
		{
			CrewSlots.Add(Index);
		}
	}
	CrewSlots.StableSort([&Zones, this](int32 A, int32 B) { return !Zones[A].MainItems.Contains(CompassItemClass) && Zones[B].MainItems.Contains(CompassItemClass); });
	if (CrewSlots.Num() == 0)
	{
		CrewSlots = Order;
	}
	TArray<TSubclassOf<AYSInteractiveObject>> Crew = CrewItemClasses;
	Crew.RemoveAll([](const TSubclassOf<AYSInteractiveObject>& Class) { return !Class; });
	// Crew Count membres d'équipage tirés au hasard parmi Crew Item Classes (demande de Paul du 2026-11-10)
	for (int32 K = Crew.Num() - 1; K > 0; --K)
	{
		Crew.Swap(K, FMath::RandRange(0, K));
	}
	if (CrewCount >= 0 && Crew.Num() > CrewCount)
	{
		Crew.SetNum(CrewCount);
	}
	ZoneCrewClasses = Crew;
	FString CrewNames;
	for (const TSubclassOf<AYSInteractiveObject>& Class : Crew)
	{
		CrewNames += (CrewNames.IsEmpty() ? TEXT("") : TEXT(", ")) + Class->GetName();
	}
	UE_LOG(LogTemp, Log, TEXT("[YSFlowMode] Zones d'intérêt : équipage tiré au hasard (%d sur %d) : %s"), Crew.Num(), CrewItemClasses.Num(), CrewNames.IsEmpty() ? TEXT("-") : *CrewNames);
	for (int32 K = 0; K < Crew.Num(); ++K)
	{
		// s'il y a plus de membres d'équipage que de places prévues, les suivants vont n'importe où (choix de Claude)
		const int32 Slot = K < CrewSlots.Num() ? CrewSlots[K] : Order[K % Order.Num()];
		Zones[Slot].MainItems.Add(Crew[K]);
	}
	if (SuperTreasureMapClass)
	{
		Zones[FMath::RandRange(0, Zones.Num() - 1)].SecondaryItems.Add(SuperTreasureMapClass);
	}

	// sommets du polygone (pentagone pour 5 ZI), quelle ZI va où : au hasard
	TArray<int32> Slots = Order;
	for (int32 K = Slots.Num() - 1; K > 0; --K)
	{
		Slots.Swap(K, FMath::RandRange(0, K));
	}
	float MeanIslandRadius = 0.0f;
	int32 IslandCount = 0;
	for (const FZone& Zone : Zones)
	{
		if (Zone.IslandData)
		{
			MeanIslandRadius += Zone.IslandData->Generation.Radius;
			++IslandCount;
		}
	}
	MeanIslandRadius = IslandCount > 0 ? MeanIslandRadius / IslandCount : 7500.0f;
	const float BaseAngle = FMath::FRandRange(0.0f, 360.0f);
	const FVector Center = Island->GetActorLocation() * FVector(1.0f, 1.0f, 0.0f);
	const float StartRadius = Island->GetRadius();
	// même distance pour tous les sommets (polygone régulier), jamais au point que les côtes se touchent
	const float Distance = FMath::Max((StartRadius + NextIslandsGap + MeanIslandRadius) * NextIslandsDistanceMultiplier, StartRadius + MeanIslandRadius + 2000.0f);

	const auto ItemNames = [](const TArray<TSubclassOf<AYSInteractiveObject>>& Items)
	{
		FString Names;
		for (const TSubclassOf<AYSInteractiveObject>& Item : Items)
		{
			Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + Item->GetName();
		}
		return Names.IsEmpty() ? FString(TEXT("-")) : Names;
	};

	for (int32 Index = 0; Index < Zones.Num(); ++Index)
	{
		FZone& Zone = Zones[Index];
		const float Angle = FMath::DegreesToRadians(BaseAngle + 360.0f * Slots[Index] / Zones.Num());
		const FVector Location = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance;
		if (Zone.IslandData)
		{
			const float Yaw = Zone.IslandData->bRandomRotation ? FMath::FRandRange(0.0f, 360.0f) : 0.0f;
			const FTransform Transform(FRotator(0.0f, Yaw, 0.0f), Location);
			AYSIsland* NewIsland = GetWorld()->SpawnActorDeferred<AYSIsland>(AYSIsland::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!NewIsland)
			{
				continue;
			}
			NewIsland->InitIsland(Zone.IslandData, 0, false);
			for (int32 Pass = 0; Pass < 2; ++Pass)
			{
				for (const TSubclassOf<AYSInteractiveObject>& Item : Pass == 0 ? Zone.MainItems : Zone.SecondaryItems)
				{
					FYSIslandObjectiveDef Def;
					Def.DisplayName = Item->GetDefaultObject<AYSInteractiveObject>()->GetDisplayName();
					Def.ItemClass = Item;
					Def.DistanceFromBoat = ZoneItemDistanceFromBeach;
					NewIsland->AddExtraObjective(Def, Pass == 0);
				}
			}
			NewIsland->FinishSpawning(Transform);
			SpawnedIslands.Add(NewIsland);
			UE_LOG(LogTemp, Log, TEXT("[YSFlowMode] ZI %d (sommet %d) : île %s en %s (yaw %.0f°, %.0f m du centre de départ) ; objets : %s ; secondaires en plus : %s"), Index, Slots[Index],
				*Zone.IslandData->GetName(), *Location.ToCompactString(), Yaw, Distance / 100.0f, *ItemNames(Zone.MainItems), *ItemNames(Zone.SecondaryItems));
		}
		else if (Zone.Battle)
		{
			const FTransform Transform(FRotator::ZeroRotator, Location);
			AYSNavalBattle* Battle = GetWorld()->SpawnActorDeferred<AYSNavalBattle>(AYSNavalBattle::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Battle)
			{
				continue;
			}
			Battle->InitBattle(*Zone.Battle, Zone.BattleRow);
			Battle->FinishSpawning(Transform);
			NavalBattles.Add(Battle);
			TArray<TSubclassOf<AYSInteractiveObject>> Items = Zone.MainItems;
			Items.Append(Zone.SecondaryItems);
			for (AYSInteractiveObject* Spawned : Battle->SpawnObjectiveItems(Items))
			{
				ZoneItems.Add(Spawned);
			}
			UE_LOG(LogTemp, Log, TEXT("[YSFlowMode] ZI %d (sommet %d) : bataille navale %s en %s (%.0f m du centre de départ) ; objets : %s"), Index, Slots[Index], *Zone.BattleRow.ToString(),
				*Location.ToCompactString(), Distance / 100.0f, *ItemNames(Items));
		}
	}
}

void AYSFlowMode::OnIslandSunk(AYSIsland* SunkIsland)
{
	if (SunkIsland && SunkIsland == Island)
	{
		StartPart2(TEXT("l'île de départ a fini de couler"));
	}
}

void AYSFlowMode::StartPart2(const TCHAR* Reason)
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (bPart2Started || !YSGameState)
	{
		return;
	}
	bPart2Started = true;
	// Peaceful : pas de 20 min (ni timer affiché, ni overtime de Galions Nemesis)
	const bool bPeaceful = YSGameState->IsPeaceful();
	YSGameState->StartFlowPart2(YSGameState->GetMatchElapsedTime(), bPeaceful ? 0.0f : Part2Duration);
	// « Victoire » de l'île de départ effacé ; les îles de la partie 2 affichent le leur en s'activant
	YSGameState->SetFlowStatus(FYSFlowStatus());
	NextPart2GalionElapsed = bPeaceful ? -1.0f : Part2Duration;
	// la nuit de l'île de départ compte comme la nuit 1 de la séquence, même sautée
	FlowNightsStarted = FMath::Max(FlowNightsStarted, 1);
	if (bZonesOfInterest)
	{
		StartSeaObjectives(Reason);
	}
	if (bPeaceful)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSFlowMode] PARTIE 2 (%s) : Peaceful, pas de limite de temps ni d'overtime"), Reason);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[YSFlowMode] PARTIE 2 (%s) : %.0f s avant l'overtime (Galion Nemesis toutes les %.0f s ensuite)"), Reason, Part2Duration, Part2OvertimeInterval);
	}
}

void AYSFlowMode::StartPart3(const TCHAR* Reason)
{
	if (bPart3Started)
	{
		return;
	}
	bPart3Started = true;
	// la partie 2 peut avoir été sautée (debug) : elle est marquée commencée
	bPart2Started = true;
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (YSGameState)
	{
		YSGameState->StartFlowPart3();
		// plus d'objectif d'île (ni barre verrouillée) : « Affrontez le Kraken », sans timer
		YSGameState->SetFlowStatus(FYSFlowStatus());
	}
	// toutes les îles coulent
	int32 Sunk = 0;
	TArray<AYSIsland*> AllIslands;
	for (TActorIterator<AYSIsland> It(GetWorld()); It; ++It)
	{
		AllIslands.Add(*It);
	}
	for (AYSIsland* Other : AllIslands)
	{
		if (IsValid(Other) && !Other->HasSunk() && !Other->IsSinking())
		{
			Other->ForceSinkNow(TEXT("partie 3"));
			++Sunk;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[YSFlowMode] PARTIE 3 (%s) : %d île(s) coulent (les autres coulaient déjà), « Affrontez le Kraken »"), Reason, Sunk);
}

void AYSFlowMode::SpawnKrakenArena()
{
	if (!KrakenArenaClass || KrakenArena || !Island)
	{
		return;
	}
	// en mer, à Kraken Arena Distance du centre de l'île de départ, direction au hasard
	const AYSOcean* Ocean = AYSOcean::Find(GetWorld());
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	FVector Location = Island->GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * KrakenArenaDistance;
	Location.Z = Ocean ? Ocean->GetSeaLevel() : 0.0f;
	KrakenArena = GetWorld()->SpawnActor<AYSKrakenArena>(KrakenArenaClass, Location, FRotator::ZeroRotator);
	UE_LOG(LogTemp, Log, TEXT("[YSFlowMode] Zone du Kraken en %s (%.0f m du centre, %.0f°)"), *Location.ToCompactString(), KrakenArenaDistance / 100.0f, FMath::RadiansToDegrees(Angle));
}

void AYSFlowMode::SpawnDavyJonesArena()
{
	if (!DavyJonesArenaClass || DavyJonesArena || !Island)
	{
		return;
	}
	// à l'opposé de la zone du Kraken par rapport au centre de l'île de départ (même distance), décalée au hasard dans un disque
	const FVector Center = Island->GetActorLocation() * FVector(1.0f, 1.0f, 0.0f);
	const FVector KrakenOffset = KrakenArena ? (KrakenArena->GetActorLocation() - Center) * FVector(1.0f, 1.0f, 0.0f)
		: FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f).Vector() * KrakenArenaDistance;
	const float JitterAngle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float JitterRadius = WorldBossRandomOffset * FMath::Sqrt(FMath::FRand());
	FVector Location = Center - KrakenOffset * WorldBossDistanceMultiplier + FVector(FMath::Cos(JitterAngle), FMath::Sin(JitterAngle), 0.0f) * JitterRadius;
	const AYSOcean* Ocean = AYSOcean::Find(GetWorld());
	Location.Z = Ocean ? Ocean->GetSeaLevel() : 0.0f;
	DavyJonesArena = GetWorld()->SpawnActor<AYSDavyJonesArena>(DavyJonesArenaClass, Location, FRotator::ZeroRotator);
	UE_LOG(LogTemp, Log, TEXT("[YSFlowMode] Zone du world boss (Davy Jones) en %s : %.0f m du centre, à l'opposé du Kraken décalé de %.0f m"), *Location.ToCompactString(),
		FVector::Dist2D(Location, Center) / 100.0f, JitterRadius / 100.0f);
}

void AYSFlowMode::DebugGalionWithCompasses()
{
	// GameBoat jusqu'à la dernière étape (galion) : à sa place, ceux qui sont à bord passent dessus
	PendingGameBoatUpgrades = 0;
	const int32 LastStage = GameBoatData ? GameBoatData->Stages.Num() - 1 : GameBoatStage;
	while (GameBoatStage < LastStage)
	{
		const int32 Before = GameBoatStage;
		UpgradeGameBoat();
		if (GameBoatStage == Before)
		{
			break;
		}
	}
	// étapes enchaînées dans la même image : ceux qui étaient à bord ne sont pas toujours repérés au 2e passage (base de mouvement pas
	// encore mise à jour) -> joueurs et minions vivants reposés sur le pont du galion, en file le long du bateau
	if (const AYSGameState* BoatState = GetGameState<AYSGameState>(); BoatState && BoatState->GetGameBoat())
	{
		AYSPirateShip* Boat = BoatState->GetGameBoat();
		TArray<FVector> HullPoints;
		Boat->GetWorldHullPoints(HullPoints);
		const FBox HullBox(HullPoints);
		const FVector HullCenter = (HullBox.IsValid ? HullBox.GetCenter() : Boat->GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
		const FVector Forward = Boat->GetActorForwardVector().GetSafeNormal2D();
		TArray<AYSPlayerCharacter*> Aboard;
		for (TActorIterator<AYSPlayerCharacter> It(GetWorld()); It; ++It)
		{
			if (It->IsAlive() && !It->IsDying() && It->GetShipUnderFeet() != Boat && !It->IsAtStation())
			{
				Aboard.Add(*It);
			}
		}
		for (int32 Index = 0; Index < Aboard.Num(); ++Index)
		{
			const float Offset = (Index - (Aboard.Num() - 1) * 0.5f) * 180.0f;
			Aboard[Index]->TeleportTo(HullCenter + Forward * Offset + FVector(0.0f, 0.0f, Boat->GetActorLocation().Z + 330.0f), Forward.Rotation());
		}
		UE_LOG(LogTemp, Log, TEXT("[YSCheat] Partie 2 : %d personnage(s) reposé(s) sur le pont de %s"), Aboard.Num(), *Boat->GetName());
	}

	// toutes les boussoles comptées à bord (celles encore dans le monde ne changent plus rien)
	StartSeaObjectives(TEXT("triche : flèche bas"));
	DebugFindAllCompasses();
	const AYSGameState* YSGameState = GetGameState<AYSGameState>();
	UE_LOG(LogTemp, Log, TEXT("[YSCheat] Partie 2 : %s (étape %d), %d / %d boussole(s) à bord"),
		YSGameState ? *GetNameSafe(YSGameState->GetGameBoat()) : TEXT("?"), GameBoatStage, CompassesFound, CompassTotal);
}

void AYSFlowMode::DebugFindAllCompasses()
{
	CompassesFound = FMath::Max(CompassesFound, CompassTotal);
	if (AYSGameState* YSGameState = GetGameState<AYSGameState>())
	{
		YSGameState->SetSeaObjectives(true, CompassTotal, CompassesFound);
	}
	UE_LOG(LogTemp, Log, TEXT("[YSFlowMode] Debug : boussoles %d / %d"), CompassesFound, CompassTotal);
}

bool AYSFlowMode::TakeNightSetup(const AYSIsland* NightIsland, FRandomStream& Rng, TArray<UClass*>& OutClasses)
{
	if (NightSequence.Num() == 0)
	{
		return false;
	}
	const int32 Night = FlowNightsStarted++;
	const FYSFlowNightSetup& Setup = NightSequence[FMath::Min(Night, NightSequence.Num() - 1)];
	for (const FYSIslandFoeEntry& Entry : Setup.Foes)
	{
		for (int32 K = 0; K < Entry.Count; ++K)
		{
			// duo d'élites ou de boss : un de chaque (Skull et Gun) tant que possible
			UClass* Class = Entry.PickClass(Rng, &OutClasses);
			if (Class)
			{
				OutClasses.Add(Class);
			}
		}
	}
	FString Names;
	for (const UClass* Class : OutClasses)
	{
		Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + Class->GetName();
	}
	UE_LOG(LogTemp, Log, TEXT("[YSFlowMode] Nuit %d de la partie (%s) : %s"), Night + 1, NightIsland && NightIsland->GetData() ? *NightIsland->GetData()->GetName() : TEXT("?"),
		Names.IsEmpty() ? TEXT("personne") : *Names);
	return true;
}

bool AYSFlowMode::SpawnPart2Galion()
{
	AYSFoeGalion* Galion = AYSFoeGalion::SpawnAtSea(GetWorld(), Part2OvertimeGalionClass, Part2OvertimeGalionMinDistance, Part2OvertimeGalionMaxDistance);
	if (!Galion)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSFlowMode] Overtime de la partie 2 : pas de place pour %s, réessayé"), *GetNameSafe(Part2OvertimeGalionClass));
		return false;
	}
	Galion->TriggerRam(TEXT("overtime de la partie 2"));
	UE_LOG(LogTemp, Log, TEXT("[YSFlowMode] Overtime de la partie 2 : %s en %s"), *Galion->GetName(), *Galion->GetActorLocation().ToCompactString());
	return true;
}

void AYSFlowMode::DebugSpawnPart2Galion()
{
	SpawnPart2Galion();
}

void AYSFlowMode::DebugSkipToPart2()
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (bPart2Started || !YSGameState || !YSGameState->IsMatchStarted() || !Island)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSCheat] Partie 2 : refusé (%s)"), bPart2Started ? TEXT("déjà en partie 2") : TEXT("partie pas commencée"));
		return;
	}

	// joueurs vivants : plus à un poste (places de la chaloupe), plus d'objet en main
	TArray<AYSPlayerCharacter*> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AYSPlayerCharacter* Character = It->Get() ? Cast<AYSPlayerCharacter>(It->Get()->GetPawn()) : nullptr;
		if (Character && Character->IsAlive())
		{
			Character->LeaveStation(TEXT("triche : partie 2"), true);
			Character->DropCarriedObject(TEXT("triche : partie 2"));
			Players.Add(Character);
		}
	}

	// le GameBoat jusqu'au sloop (étape 1)
	PendingGameBoatUpgrades = 0;
	while (GameBoatStage < 1)
	{
		const int32 Before = GameBoatStage;
		UpgradeGameBoat();
		if (GameBoatStage == Before)
		{
			break;
		}
	}
	AYSPirateShip* Boat = YSGameState->GetGameBoat();
	if (!Boat)
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSCheat] Partie 2 : pas de bateau des joueurs"));
		return;
	}

	// un poste de combat de chaque sorte, sur les réceptacles libres
	TArray<AYSCombatPostReceptacle*> Receptacles = Boat->GetCombatPostReceptacles();
	int32 Installed = 0;
	for (const TSubclassOf<AYSInteractiveObject>& ItemClass : GetCombatPostItemClasses())
	{
		const AYSCombatPostItem* Item = ItemClass ? Cast<AYSCombatPostItem>(ItemClass->GetDefaultObject()) : nullptr;
		AYSCombatPostReceptacle** Free = Receptacles.FindByPredicate([](const AYSCombatPostReceptacle* Receptacle) { return Receptacle && !Receptacle->GetInstalledPost(); });
		if (Item && Item->GetPostClass() && Free && (*Free)->InstallPost(Item->GetPostClass(), TEXT("triche : partie 2")))
		{
			++Installed;
		}
	}

	// joueurs sur le pont, au milieu de la coque, en file le long du bateau
	TArray<FVector> HullPoints;
	Boat->GetWorldHullPoints(HullPoints);
	const FBox HullBox(HullPoints);
	const FVector HullCenter = (HullBox.IsValid ? HullBox.GetCenter() : Boat->GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
	const FVector Forward = Boat->GetActorForwardVector().GetSafeNormal2D();
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		const float Offset = (Index - (Players.Num() - 1) * 0.5f) * 180.0f;
		const FVector Target = HullCenter + Forward * Offset + FVector(0.0f, 0.0f, Boat->GetActorLocation().Z + 330.0f);
		const bool bTeleported = Players[Index]->TeleportTo(Target, Forward.Rotation());
		UE_LOG(LogTemp, Log, TEXT("[YSCheat] Partie 2 : %s sur %s en %s%s"), *Players[Index]->GetName(), *Boat->GetName(), *Target.ToCompactString(), bTeleported ? TEXT("") : TEXT(" : REFUSÉ"));
	}

	// l'île de départ : évacuation finie tout de suite (bateau pilotable, elle coule), partie 2 tout de suite
	Island->DebugFinishEvacuation();
	UE_LOG(LogTemp, Log, TEXT("[YSCheat] Partie 2 : %s, %d poste(s) de combat posé(s), %d joueur(s) à bord, l'île de départ coule"), *Boat->GetName(), Installed, Players.Num());
	StartPart2(TEXT("triche : flèche bas"));
}

int32 AYSFlowMode::GetNextStageCoreBoatCost() const
{
	const int32 NextStage = GameBoatStage + 1;
	const bool bStageExists = GameBoatData && GameBoatData->Stages.IsValidIndex(NextStage);
	return bStageExists && CoreBoatCostPerStage.IsValidIndex(NextStage) ? FMath::Max(CoreBoatCostPerStage[NextStage], 0) : 0;
}

void AYSFlowMode::UpdateCoreBoatStock()
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (!YSGameState)
	{
		return;
	}
	// assez de CoreBoats pour l'étape suivante : consommés, le bateau change (une étape par image)
	const int32 Cost = GetNextStageCoreBoatCost();
	if (Cost > 0 && CoreBoatStock >= Cost && YSGameState->GetGameBoat())
	{
		CoreBoatStock -= Cost;
		UE_LOG(LogTemp, Log, TEXT("[YSGameBoat] %d CoreBoat(s) consommé(s) pour l'étape %d (%d en stock)"), Cost, GameBoatStage + 1, CoreBoatStock);
		RequestGameBoatUpgrade(TEXT("CoreBoats du stock"));
	}
	const int32 NextCost = PendingGameBoatUpgrades > 0 ? CoreBoatCostPerStage.IsValidIndex(GameBoatStage + 2) ? CoreBoatCostPerStage[GameBoatStage + 2] : 0 : GetNextStageCoreBoatCost();
	// inventaire affiché à partir de la partie 2 (ou dès le premier CoreBoat rangé)
	const bool bShown = bPart2Started || CoreBoatStock > 0;
	if (YSGameState->UsesCoreBoatStock() != bShown || YSGameState->GetCoreBoatStock() != CoreBoatStock || YSGameState->GetNextStageCoreBoatCost() != NextCost)
	{
		YSGameState->SetCoreBoatStock(bShown, CoreBoatStock, NextCost);
	}
}

bool AYSFlowMode::IsStockedCoreBoat(const AYSInteractiveObject* Object) const
{
	// (pas le CoreBoat de l'île de départ : il fait passer au sloop directement)
	return StocksCoreBoats() && Object && Object->IsA(BoatCoreItemClass) && !Object->IsDelivered() && !(Island && Island->IsObjectiveItem(Object));
}

bool AYSFlowMode::IsZoneItem(const AYSInteractiveObject* Object) const
{
	return Object && !Object->IsDelivered() && ZoneItems.ContainsByPredicate([Object](const TWeakObjectPtr<AYSInteractiveObject>& Item) { return Item.Get() == Object; });
}

void AYSFlowMode::StoreCoreBoat(AYSInteractiveObject* Object)
{
	if (!IsValid(Object))
	{
		return;
	}
	++CoreBoatStock;
	ZoneItems.RemoveAll([Object](const TWeakObjectPtr<AYSInteractiveObject>& Item) { return !Item.IsValid() || Item.Get() == Object; });
	UE_LOG(LogTemp, Log, TEXT("[YSGameBoat] %s rangé dans le stock : %d CoreBoat(s) (%d pour l'étape suivante)"), *Object->GetName(), CoreBoatStock, GetNextStageCoreBoatCost());
	Object->Destroy();
}

void AYSFlowMode::DebugAddCoreBoats(int32 Count)
{
	CoreBoatStock = FMath::Max(CoreBoatStock + Count, 0);
	UE_LOG(LogTemp, Log, TEXT("[YSGameBoat] Debug : %d CoreBoat(s) en stock"), CoreBoatStock);
}

void AYSFlowMode::StartSeaObjectives(const TCHAR* Reason)
{
	AYSGameState* YSGameState = GetGameState<AYSGameState>();
	if (bSeaObjectivesStarted || !YSGameState)
	{
		return;
	}
	bSeaObjectivesStarted = true;
	YSGameState->SetSeaObjectives(true, CompassTotal, CompassesFound);
	UE_LOG(LogTemp, Log, TEXT("[YSFlowMode] Objectif en mer (%s) : %d / %d boussole(s) trouvée(s)"), Reason, CompassesFound, CompassTotal);
}

bool AYSFlowMode::IsCompass(const AYSInteractiveObject* Object) const
{
	return Object && CompassItemClass && Object->IsA(CompassItemClass);
}

void AYSFlowMode::NotifyCompassDelivered(AYSInteractiveObject* Compass)
{
	++CompassesFound;
	if (AYSGameState* YSGameState = GetGameState<AYSGameState>())
	{
		YSGameState->SetSeaObjectives(bSeaObjectivesStarted, CompassTotal, CompassesFound);
	}
	UE_LOG(LogTemp, Log, TEXT("[YSFlowMode] Boussole %s ramenée au bateau : %d / %d"), *GetNameSafe(Compass), CompassesFound, CompassTotal);
}
