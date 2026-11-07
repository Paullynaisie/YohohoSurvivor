#include "Enemies/YSNavalBattle.h"
#include "Core/YSGameMode.h"
#include "Enemies/YSFoeGalion.h"
#include "Enemies/YSFoeJollyShip.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Ship/YSPirateShip.h"

namespace YSNavalBattle
{
	/** Apparition : fond en plus du tirant d'eau, emprise des autres bateaux, fond vérifié autour (Galion : longueur de la coque), essais */
	constexpr float SpawnExtraDepth = 100.0f;
	constexpr float SpawnShipMargin = 1500.0f;
	constexpr float GalionClearance = 3500.0f;
	constexpr float JollyClearance = 600.0f;
	constexpr int32 SpawnAttempts = 40;
}

AYSNavalBattle::AYSNavalBattle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	// serveur seulement : les bateaux sont répliqués eux-mêmes
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AYSNavalBattle::InitBattle(const FYSNavalBattleRow& InSettings, FName InRowName)
{
	Settings = InSettings;
	RowName = InRowName;
}

void AYSNavalBattle::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	const AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	UClass* GalionClass = Settings.GalionClass ? Settings.GalionClass.Get() : (GameMode ? GameMode->GetGalionClass() : nullptr);
	UClass* JollyClass = Settings.JollyShipClass ? Settings.JollyShipClass.Get() : (GameMode ? GameMode->GetJollyShipClass() : nullptr);
	for (int32 Index = 0; Index < Settings.GalionCount; ++Index)
	{
		if (AYSFoeGalion* Galion = Cast<AYSFoeGalion>(SpawnShip(GalionClass, true)))
		{
			Galions.Add(Galion);
		}
	}
	for (int32 Index = 0; Index < Settings.InitialJollyShips; ++Index)
	{
		if (AYSFoeJollyShip* Jolly = Cast<AYSFoeJollyShip>(SpawnShip(JollyClass, true)))
		{
			JollyShips.Add(Jolly);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[YSNavalBattle] %s (%s) en %s : %d Galion(s), %d Jolly Ship(s) en sommeil, activation à %.0f m"), *GetName(), *RowName.ToString(),
		*GetActorLocation().ToCompactString(), Galions.Num(), JollyShips.Num(), Settings.ActivationDistance / 100.0f);
}

AYSPirateShip* AYSNavalBattle::SpawnShip(UClass* ShipClass, bool bDormant)
{
	const AYSPirateShip* Defaults = ShipClass ? Cast<AYSPirateShip>(ShipClass->GetDefaultObject()) : nullptr;
	if (!Defaults)
	{
		return nullptr;
	}
	const bool bGalion = ShipClass->IsChildOf(AYSFoeGalion::StaticClass());
	FVector Point;
	if (!AYSPirateShip::FindSeaPointNear(GetWorld(), GetActorLocation(), Settings.SpawnRadius, Defaults->GetMinWaterDepth() + YSNavalBattle::SpawnExtraDepth, YSNavalBattle::SpawnShipMargin,
		bGalion ? YSNavalBattle::GalionClearance : YSNavalBattle::JollyClearance, YSNavalBattle::SpawnAttempts, Point))
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSNavalBattle] %s : pas de place sur l'eau pour %s"), *GetName(), *ShipClass->GetName());
		return nullptr;
	}
	const FTransform Transform(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), Point);
	AYSPirateShip* Ship = GetWorld()->SpawnActorDeferred<AYSPirateShip>(ShipClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Ship)
	{
		return nullptr;
	}
	Ship->SetDormant(bDormant);
	Ship->FinishSpawning(Transform);
	return Ship;
}

bool AYSNavalBattle::IsPlayerShipNear() const
{
	for (TActorIterator<AYSPirateShip> It(GetWorld()); It; ++It)
	{
		if (!It->IsFoeShip() && It->IsPresent() && !It->IsActorBeingDestroyed()
			&& FVector::DistSquared2D(It->GetClosestHullPoint(GetActorLocation()), GetActorLocation()) <= FMath::Square(Settings.ActivationDistance))
		{
			return true;
		}
	}
	return false;
}

void AYSNavalBattle::Activate(const TCHAR* Reason)
{
	if (bActive)
	{
		return;
	}
	bActive = true;
	ReinforcementTimer = Settings.ReinforcementInterval;
	for (AYSFoeGalion* Galion : Galions)
	{
		if (IsValid(Galion))
		{
			Galion->ActivateBattle(Settings.GalionRamDelay);
		}
	}
	for (AYSFoeJollyShip* Jolly : JollyShips)
	{
		if (IsValid(Jolly))
		{
			Jolly->SetDormant(false);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[YSNavalBattle] %s activée (%s) : renforts de %d Jolly Ship(s) toutes les %.0f s"), *GetName(), Reason, Settings.ReinforcementJollyShips, Settings.ReinforcementInterval);
}

void AYSNavalBattle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	if (!bActive)
	{
		if (IsPlayerShipNear())
		{
			Activate(TEXT("bateau des joueurs à portée"));
		}
		return;
	}
	if (bReinforcementsStopped)
	{
		return;
	}

	// tous les Galions vaincus : plus de renforts (choix de Claude : sinon la bataille ne finit jamais)
	Galions.RemoveAll([](const AYSFoeGalion* Galion) { return !IsValid(Galion); });
	if (Settings.bStopReinforcementsWhenGalionsDefeated && !Galions.ContainsByPredicate([](const AYSFoeGalion* Galion) { return !Galion->IsDefeated(); }))
	{
		bReinforcementsStopped = true;
		UE_LOG(LogTemp, Log, TEXT("[YSNavalBattle] %s : Galion(s) vaincu(s), plus de renforts"), *GetName());
		return;
	}

	ReinforcementTimer -= DeltaSeconds;
	if (ReinforcementTimer > 0.0f)
	{
		return;
	}
	ReinforcementTimer += Settings.ReinforcementInterval;
	const AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	UClass* JollyClass = Settings.JollyShipClass ? Settings.JollyShipClass.Get() : (GameMode ? GameMode->GetJollyShipClass() : nullptr);
	int32 Spawned = 0;
	for (int32 Index = 0; Index < Settings.ReinforcementJollyShips; ++Index)
	{
		Spawned += SpawnShip(JollyClass, false) ? 1 : 0;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSNavalBattle] %s : renforts, %d / %d Jolly Ship(s)"), *GetName(), Spawned, Settings.ReinforcementJollyShips);
}
