#include "Core/YSGameState.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "UI/YSDamageNumber.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "Core/YSPlayerState.h"
#include "NPC/YSBrookerNPC.h"
#include "Player/YSPlayerCharacter.h"

AYSGameState::AYSGameState()
{
	DamageNumberClass = AYSDamageNumber::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

bool AYSGameState::IsCharacterTaken(int32 CharacterIndex, const APlayerState* Except) const
{
	const UClass* CharacterClass = SelectableCharacters.IsValidIndex(CharacterIndex) ? SelectableCharacters[CharacterIndex].Get() : nullptr;
	const AYSPlayerCharacter* Default = CharacterClass ? CharacterClass->GetDefaultObject<AYSPlayerCharacter>() : nullptr;
	if (!Default || !Default->IsUniqueCharacter())
	{
		return false;
	}

	for (const APlayerState* Player : PlayerArray)
	{
		const AYSPlayerState* YSPlayer = Cast<AYSPlayerState>(Player);
		if (YSPlayer && YSPlayer != Except && YSPlayer->GetSelectedCharacterIndex() == CharacterIndex)
		{
			return true;
		}
	}
	return false;
}

void AYSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSGameState, bHarborPlaced);
	DOREPLIFETIME(AYSGameState, HarborPivot);
	DOREPLIFETIME(AYSGameState, HarborYawDelta);
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
	DOREPLIFETIME(AYSGameState, Doubloons);
	DOREPLIFETIME(AYSGameState, DoubloonGoal);
	DOREPLIFETIME(AYSGameState, ShipLevel);
	DOREPLIFETIME(AYSGameState, bShipPresent);
	DOREPLIFETIME(AYSGameState, ShipAboardPlayers);
	DOREPLIFETIME(AYSGameState, ShipTotalPlayers);
	DOREPLIFETIME(AYSGameState, bVictory);
	DOREPLIFETIME(AYSGameState, VictoryReturnDelay);
	DOREPLIFETIME(AYSGameState, bOvertime);
	DOREPLIFETIME(AYSGameState, IslandSinkStartServerTime);
	DOREPLIFETIME(AYSGameState, IslandSinkDuration);
	DOREPLIFETIME(AYSGameState, IslandSinkDepth);
	DOREPLIFETIME(AYSGameState, IslandSinkEaseExponent);
}

void AYSGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateHarborPlacement();
	UpdateIslandSink();
}

const FName AYSGameState::HarborTag(TEXT("Harbor"));
const FName AYSGameState::HarborPivotTag(TEXT("HarborPivot"));
const FName AYSGameState::HarborLandTag(TEXT("HarborLand"));
const FName AYSGameState::HarborSeaTag(TEXT("HarborSea"));

void AYSGameState::SetHarborPlacement(const FVector& Pivot, float YawDelta)
{
	HarborPivot = Pivot;
	HarborYawDelta = YawDelta;
	bHarborPlaced = true;
	ForceNetUpdate();
	UpdateHarborPlacement();
}

AActor* AYSGameState::FindHarborPivot(const UWorld* World)
{
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(HarborPivotTag))
		{
			return *It;
		}
	}
	return nullptr;
}

FTransform AYSGameState::GetHarborTransform(const FTransform& Original, const FVector& OldPivot, const FVector& NewPivot, float YawDelta)
{
	const FQuat Rotation(FRotator(0.0f, YawDelta, 0.0f));
	return FTransform(Rotation * Original.GetRotation(), NewPivot + Rotation.RotateVector(Original.GetLocation() - OldPivot), Original.GetScale3D());
}

void AYSGameState::UpdateHarborPlacement()
{
	if (!bHarborPlaced || bHarborApplied)
	{
		return;
	}
	bHarborApplied = true;

	// même calcul sur toutes les machines, depuis les positions de la map (le port n'a pas encore bougé ici)
	const AActor* Pivot = FindHarborPivot(GetWorld());
	if (!Pivot)
	{
		return;
	}
	const FVector OldPivot = Pivot->GetActorLocation();
	TArray<AActor*> HarborActors;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(HarborTag))
		{
			HarborActors.Add(*It);
		}
	}
	for (AActor* Actor : HarborActors)
	{
		// props statiques (ponton) : rendus mobiles pour être déplacés
		TInlineComponentArray<USceneComponent*> Components(Actor);
		for (USceneComponent* Component : Components)
		{
			Component->SetMobility(EComponentMobility::Movable);
		}
		const FTransform Placed = GetHarborTransform(Actor->GetActorTransform(), OldPivot, HarborPivot, HarborYawDelta);
		Actor->SetActorLocationAndRotation(Placed.GetLocation(), Placed.GetRotation());

		// Brooker : reposé au sol, zone de rachat reconstruite sur le relief
		if (AYSBrookerNPC* Brooker = Cast<AYSBrookerNPC>(Actor))
		{
			Brooker->Resettle();
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[YSHarbor] %s : port déplacé (%d acteur(s)), ponton en (%.0f, %.0f), rotation %.0f°"),
		HasAuthority() ? TEXT("serveur") : TEXT("client"), HarborActors.Num(), HarborPivot.X, HarborPivot.Y, HarborYawDelta);
}

void AYSGameState::StartIslandSink(float Duration, float Depth, float EaseExponent)
{
	IslandSinkDuration = FMath::Max(Duration, 0.1f);
	IslandSinkDepth = Depth;
	IslandSinkEaseExponent = EaseExponent;
	IslandSinkStartServerTime = FMath::Max(GetServerWorldTimeSeconds(), 0.001);
	ForceNetUpdate();
}

void AYSGameState::UpdateIslandSink()
{
	if (!IsIslandSinking())
	{
		return;
	}

	// une fois : l'île (Landscape) et tout ce qui y est accroché (props, ponton, Brooker), rendus mobiles
	if (!bSinkingActorsGathered)
	{
		bSinkingActorsGathered = true;
		TArray<AActor*> Roots;
		for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
		{
			Roots.Add(*It);
		}
		for (AActor* Root : Roots)
		{
			TArray<AActor*> Attached;
			Root->GetAttachedActors(Attached, false, true);
			Attached.Insert(Root, 0);
			for (AActor* Actor : Attached)
			{
				TInlineComponentArray<USceneComponent*> Components(Actor);
				for (USceneComponent* Component : Components)
				{
					Component->SetMobility(EComponentMobility::Movable);
				}
				SinkingActors.Emplace(Actor, Actor->GetActorLocation());
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[YSOvertime] L'île coule : %d acteur(s) (Landscape et acteurs accrochés)"), SinkingActors.Num());
	}

	// même descente sur toutes les machines (temps serveur)
	const float Alpha = FMath::Clamp(static_cast<float>(GetServerWorldTimeSeconds() - IslandSinkStartServerTime) / IslandSinkDuration, 0.0f, 1.0f);
	const float Offset = -IslandSinkDepth * FMath::Pow(Alpha, IslandSinkEaseExponent);
	for (const TPair<TWeakObjectPtr<AActor>, FVector>& Entry : SinkingActors)
	{
		AActor* Actor = Entry.Key.Get();
		// les enfants suivent leur parent : seules les racines sont déplacées
		if (Actor && !Actor->GetAttachParentActor())
		{
			const FVector Target = Entry.Value + FVector(0.0f, 0.0f, Offset);
			if (!Actor->GetActorLocation().Equals(Target, 0.1f))
			{
				Actor->SetActorLocation(Target);
			}
		}
	}
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

void AYSGameState::ReportDamage(const AActor* Victim, float Amount, const FVector& ImpactLocation, const AController* InstigatorController, bool bCritical)
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
		VictimPawn ? VictimPawn->GetPlayerState() : nullptr,
		bCritical);
}

void AYSGameState::MulticastDamageNumber_Implementation(float Amount, FVector_NetQuantize Location, APlayerState* InstigatorPlayer, APlayerState* VictimPlayer, bool bCritical)
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
		Number->Init(Amount, Kind, bCritical);
	}
}
