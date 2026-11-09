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
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Ship/YSPirateShip.h"
#include "Ship/YSShipHelm.h"

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
		// îles suivantes générées dès maintenant, sous l'eau
		if (bNextIslandsEnabled && bPregenerateNextIslands && NextIslandsSubmergedDepth > 0.0f)
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

	// île finale : quand toutes les îles suivantes et toutes les batailles navales sont faites
	if (FinalIsland && bNextIslandsBrought && !bFinalIslandBrought && AreNextIslandsAndBattlesDone())
	{
		BringFinalIsland(TEXT("îles suivantes et batailles navales faites"));
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
		if (!Actor || Actor->IsChildActor() || Actor->IsA<APawn>() || Actor->IsActorBeingDestroyed())
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
