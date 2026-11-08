#include "Enemies/YSFoeGalion.h"
#include "Components/CapsuleComponent.h"
#include "Components/ChildActorComponent.h"
#include "Enemies/YSEnemySpawnRule.h"
#include "Enemies/YSFoeCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Player/YSPlayerCharacter.h"
#include "Ship/YSBoardingPlank.h"
#include "Ship/YSLongboat.h"
#include "Ship/YSShipCannon.h"
#include "Ship/YSShipHelm.h"

namespace YSGalion
{
	/** Pendant le tour, le fond devant n'est vérifié que de temps en temps (un bateau = plusieurs traces) */
	constexpr double ShoreCheckInterval = 0.25;

	/** Évitement de la côte : pas de la recherche d'une direction libre, à partir de quel écart le tour change de sens */
	constexpr float ShoreAvoidStep = 30.0f;
	constexpr float ShoreFlipYaw = 90.0f;

	/** Apparition : profondeur en plus du tirant d'eau, emprise des autres bateaux, rayon de fond vérifié (longueur de la coque), recherche */
	constexpr float SpawnExtraDepth = 100.0f;
	constexpr float SpawnShipMargin = 6000.0f;
	constexpr float SpawnClearanceRadius = 3500.0f;
	constexpr float SpawnStep = 500.0f;
	constexpr int32 SpawnAttempts = 20;

	/** Apparition des matelots : essais par matelot, hauteur au-dessus du pont */
	constexpr int32 CrewPlacementAttempts = 12;
	constexpr float CrewSpawnHeight = 30.0f;

	/** Escalier : assez près d'un de ses bouts pour passer au suivant */
	constexpr float StairsReachDistance = 200.0f;
}

AYSFoeGalion::AYSFoeGalion(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ShipName = NSLOCTEXT("YS", "GalionName", "Galion");
	bObjectiveShip = false;
	bDrivable = true;
}

void AYSFoeGalion::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		SpawnCrew();
	}
}

void AYSFoeGalion::SpawnCrew()
{
	// capitaine à la barre (tiré au hasard s'il y a une liste)
	const TSubclassOf<AYSFoeCharacter> Chosen = CaptainClasses.Num() > 0 ? CaptainClasses[FMath::RandRange(0, CaptainClasses.Num() - 1)] : CaptainClass;
	if (Chosen)
	{
		const AYSShipStation* Helm = GetSeatStation(0);
		Captain = SpawnCrewMember(Chosen, GetSeatFeetLocation(0), Helm ? 0 : INDEX_NONE);
		bHadCaptain = IsValid(Captain);
		if (bHadCaptain)
		{
			Captain->SetShipCaptainBoss();
		}
		UE_LOG(LogTemp, Log, TEXT("[YSGalion] %s : capitaine %s"), *GetName(), *Chosen->GetName());
	}

	SpawnCrewGroups(CrewGroups, false);
	UE_LOG(LogTemp, Log, TEXT("[YSGalion] %s apparaît en (%.0f, %.0f) : capitaine %s, %d matelots"), *GetName(), GetActorLocation().X, GetActorLocation().Y, *GetNameSafe(Captain), Crew.Num());
}

void AYSFoeGalion::SpawnCrewGroups(const TArray<FYSGalionCrewGroup>& Groups, bool bFree)
{
	// matelots sur le pont, à distance les uns des autres
	const FTransform& Transform = GetActorTransform();
	TArray<FVector2D> Placed;
	int32 Spawned = 0;
	for (const FYSGalionCrewGroup& Group : Groups)
	{
		TArray<TSubclassOf<AYSFoeCharacter>> Classes = Group.Classes;
		Classes.RemoveAll([](const TSubclassOf<AYSFoeCharacter>& Class) { return !Class; });
		if (Classes.Num() == 0)
		{
			continue;
		}
		for (int32 Index = 0; Index < Group.Count; ++Index)
		{
			FVector2D Point = DeckSpawnMin;
			for (int32 Attempt = 0; Attempt < YSGalion::CrewPlacementAttempts; ++Attempt)
			{
				Point = FVector2D(FMath::FRandRange(DeckSpawnMin.X, DeckSpawnMax.X), FMath::FRandRange(DeckSpawnMin.Y, DeckSpawnMax.Y));
				if (!Placed.ContainsByPredicate([this, &Point](const FVector2D& Other) { return FVector2D::DistSquared(Other, Point) < FMath::Square(CrewSpawnSpacing); }))
				{
					break;
				}
			}
			Placed.Add(Point);
			const FVector Feet = Transform.TransformPositionNoScale(FVector(Point.X, Point.Y, DeckHeight + YSGalion::CrewSpawnHeight));
			// apparus comme équipage (pas de sortie du sol sur le pont) ; libres : ils quittent tout de suite l'équipage
			if (AYSFoeCharacter* Foe = SpawnCrewMember(UYSEnemySpawnRule::PickEnemyClass(Classes), Feet, INDEX_NONE))
			{
				++Spawned;
				if (bFree)
				{
					Foe->ClearCrewShip();
				}
				else
				{
					Crew.Add(Foe);
				}
			}
		}
	}
	if (bFree)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSGalion] %s : second équipage de %d NME sur le pont"), *GetName(), Spawned);
	}
}

void AYSFoeGalion::ActivateBattle(float InRamDelay)
{
	if (!HasAuthority())
	{
		return;
	}
	SetDormant(false);
	BattleActivationTime = GetWorld()->GetTimeSeconds();
	if (InRamDelay >= 0.0f)
	{
		RamDelay = InRamDelay;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSGalion] %s : bataille activée, il fonce sur les joueurs dans %.0f s"), *GetName(), RamDelay);
}

bool AYSFoeGalion::IsDefeated() const
{
	return IsActorBeingDestroyed() || IsSinking() || !IsValid(Captain) || Captain->IsDying() || Captain->GetHealth() <= 0.0f;
}

void AYSFoeGalion::OnShipCollision(AYSPirateShip* Other)
{
	if (RamState == ERamState::Ramming && IsValid(Other) && !Other->IsFoeShip())
	{
		StartBoarding(Other);
		return;
	}
	OnHullBlocked();
}

void AYSFoeGalion::StartBoarding(AYSPirateShip* Other)
{
	if (!HasAuthority() || RamState == ERamState::Boarded || !IsValid(Other))
	{
		return;
	}
	RamState = ERamState::Boarded;
	BoardedShip = Other;
	SetGrappled(true);
	Other->SetGrappled(true);
	SpawnBoardingPlanks(Other);

	// tout l'équipage (capitaine compris) quitte ses postes et attaque les joueurs, où qu'ils soient
	TArray<AYSFoeCharacter*> All;
	All.Add(Captain);
	All.Append(Crew);
	int32 Released = 0;
	for (AYSFoeCharacter* Foe : All)
	{
		if (!IsValid(Foe) || Foe->IsDying())
		{
			continue;
		}
		Foe->SetCrewPost(nullptr);
		if (Foe->IsRidingShip())
		{
			UnseatCrewMember(Foe);
		}
		Foe->ClearCrewShip();
		++Released;
	}
	Crew.Reset();
	UE_LOG(LogTemp, Log, TEXT("[YSGalion] %s aborde %s : %d NME lâchent leurs postes"), *GetName(), *Other->GetName(), Released);

	// second équipage sur le pont (hors capitaine)
	SpawnCrewGroups(BoardingCrewGroups.Num() > 0 ? BoardingCrewGroups : CrewGroups, true);
}

void AYSFoeGalion::EndBoarding(const TCHAR* Reason)
{
	if (RamState != ERamState::Boarded)
	{
		return;
	}
	RamState = ERamState::Done;
	for (AActor* Plank : BoardingPlanks)
	{
		if (IsValid(Plank))
		{
			Plank->Destroy();
		}
	}
	BoardingPlanks.Reset();
	if (IsValid(BoardedShip))
	{
		BoardedShip->SetGrappled(false);
	}
	UE_LOG(LogTemp, Log, TEXT("[YSGalion] %s : fin de l'abordage (%s), planches retirées, %s repart"), *GetName(), Reason, *GetNameSafe(BoardedShip));
	BoardedShip = nullptr;
}

float AYSFoeGalion::FindDeckHeight(const AYSPirateShip* Ship, const FVector& Point, float Fallback) const
{
	const float Top = Ship->GetActorLocation().Z + 1500.0f;
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSGalionDeck), false);
	GetWorld()->LineTraceMultiByObjectType(Hits, FVector(Point.X, Point.Y, Top), FVector(Point.X, Point.Y, Ship->GetActorLocation().Z - 300.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params);
	for (const FHitResult& Hit : Hits)
	{
		if (GetShipOf(Hit.GetActor()) == Ship)
		{
			return Hit.ImpactPoint.Z;
		}
	}
	return Fallback;
}

void AYSFoeGalion::SpawnBoardingPlanks(AYSPirateShip* Other)
{
	// point de contact : les deux points les plus proches des deux coques (projections alternées, coques convexes), là où il a percuté ;
	// la planche entre dans chaque pont de Boarding Plank Inset
	FVector OtherEdge = Other->GetClosestHullPoint(GetActorLocation());
	FVector OwnEdge = GetClosestHullPoint(OtherEdge);
	for (int32 Iteration = 0; Iteration < 6; ++Iteration)
	{
		OtherEdge = Other->GetClosestHullPoint(OwnEdge);
		OwnEdge = GetClosestHullPoint(OtherEdge);
	}
	FVector Direction = FVector(OtherEdge.X - OwnEdge.X, OtherEdge.Y - OwnEdge.Y, 0.0f);
	if (Direction.SizeSquared() < 100.0f)
	{
		Direction = (Other->GetActorLocation() - GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
	}
	Direction = Direction.GetSafeNormal();
	const FVector OwnIn = OwnEdge - Direction * BoardingPlankInset;
	const FVector OtherIn = OtherEdge + Direction * BoardingPlankInset;

	FYSBoardingPlankSpan Span;
	Span.Start = FVector(OwnIn.X, OwnIn.Y, FindDeckHeight(this, OwnIn, GetActorLocation().Z + DeckHeight) + BoardingPlankHeight);
	Span.End = FVector(OtherIn.X, OtherIn.Y, FindDeckHeight(Other, OtherIn, Other->GetActorLocation().Z + DeckHeight) + BoardingPlankHeight);
	Span.Width = BoardingPlankWidth;
	Span.Thickness = BoardingPlankThickness;
	Span.Material = BoardingPlankMaterial;

	const FTransform Transform(FRotator::ZeroRotator, Span.Start);
	if (AYSBoardingPlank* Plank = GetWorld()->SpawnActorDeferred<AYSBoardingPlank>(AYSBoardingPlank::StaticClass(), Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Plank->InitPlank(Span);
		Plank->FinishSpawning(Transform);
		BoardingPlanks.Add(Plank);
	}
	UE_LOG(LogTemp, Log, TEXT("[YSGalion] %s : planche d'abordage de %s à %s (%.0f cm de large, %.0f de long)"), *GetName(), *FVector(Span.Start).ToCompactString(), *FVector(Span.End).ToCompactString(),
		Span.Width, FVector::Dist(Span.Start, Span.End));
}

AYSFoeCharacter* AYSFoeGalion::SpawnCrewMember(TSubclassOf<AYSFoeCharacter> FoeClass, const FVector& FeetLocation, int32 Seat)
{
	const float HalfHeight = FoeClass->GetDefaultObject<AYSFoeCharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FTransform SpawnTransform(GetActorRotation(), FeetLocation + FVector(0.0f, 0.0f, HalfHeight));
	AYSFoeCharacter* Foe = GetWorld()->SpawnActorDeferred<AYSFoeCharacter>(FoeClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Foe)
	{
		return nullptr;
	}
	Foe->SetCrewShip(this);
	if (Seat != INDEX_NONE)
	{
		Foe->StartShipRide(this, Seat, false, 1.0f, 1.0f);
		if (AYSShipStation* Station = GetSeatStation(Seat))
		{
			Station->SetFoeOccupant(Foe);
		}
	}
	Foe->FinishSpawning(SpawnTransform);
	return Foe;
}

void AYSFoeGalion::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && EndPlayReason == EEndPlayReason::Destroyed)
	{
		EndBoarding(TEXT("Galion coulé"));
		ReleaseCrew();
	}
	Super::EndPlay(EndPlayReason);
}

void AYSFoeGalion::ReleaseCrew()
{
	TArray<AYSFoeCharacter*> All;
	All.Add(Captain);
	All.Append(Crew);
	for (AYSFoeCharacter* Foe : All)
	{
		if (IsValid(Foe) && !Foe->IsActorBeingDestroyed())
		{
			Foe->ClearCrewShip();
			if (Foe->IsRidingShip())
			{
				Foe->EndShipRide(Foe->GetActorLocation() - FVector(0.0f, 0.0f, Foe->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), true);
			}
		}
	}
	Crew.Reset();
	Captain = nullptr;
}

void AYSFoeGalion::OnShipDestroyedByDamage()
{
	EndBoarding(TEXT("Galion coulé"));
	ReleaseCrew();
}

AYSShipStation* AYSFoeGalion::GetSeatStation(int32 Seat) const
{
	const UChildActorComponent* Component = GetHelmStationComponent();
	if (Seat > 0)
	{
		const TArray<UChildActorComponent*, TInlineAllocator<8>> Cannons = GetCannonComponents();
		Component = Cannons.IsValidIndex(Seat - 1) ? Cannons[Seat - 1] : nullptr;
	}
	return Component ? Cast<AYSShipStation>(Component->GetChildActor()) : nullptr;
}

int32 AYSFoeGalion::GetSeatOf(const AYSShipStation* Station) const
{
	for (int32 Seat = 0; Seat <= GetCannonComponents().Num(); ++Seat)
	{
		if (Station && GetSeatStation(Seat) == Station)
		{
			return Seat;
		}
	}
	return INDEX_NONE;
}

FVector AYSFoeGalion::GetSeatFeetLocation(int32 Seat) const
{
	const AYSShipStation* Station = GetSeatStation(Seat);
	return Station ? Station->GetOccupantFeetLocation() : GetActorLocation();
}

float AYSFoeGalion::GetSeatYaw(int32 Seat) const
{
	const AYSShipStation* Station = GetSeatStation(Seat);
	return Station ? Station->GetOccupantForward().Rotation().Yaw : GetActorRotation().Yaw;
}

void AYSFoeGalion::SeatCrewMember(AYSFoeCharacter* Foe, AYSShipStation* Post)
{
	const int32 Seat = GetSeatOf(Post);
	if (!HasAuthority() || !IsValid(Foe) || Seat == INDEX_NONE || !Post->IsFree() || !Post->IsUsable())
	{
		return;
	}
	Foe->SetCrewPost(nullptr);
	Foe->StartShipRide(this, Seat, false, 1.0f, 1.0f);
	Post->SetFoeOccupant(Foe);
	UE_LOG(LogTemp, Log, TEXT("[YSGalion] %s : %s s'installe à %s"), *GetName(), *Foe->GetName(), *Post->GetName());
}

void AYSFoeGalion::UnseatCrewMember(AYSFoeCharacter* Foe)
{
	const AYSShipStation* Station = nullptr;
	for (int32 Seat = 0; Seat <= GetCannonComponents().Num() && !Station; ++Seat)
	{
		AYSShipStation* Candidate = GetSeatStation(Seat);
		if (Candidate && Candidate->GetFoeOccupant() == Foe)
		{
			Candidate->SetFoeOccupant(nullptr);
			Station = Candidate;
		}
	}
	const FVector Feet = Station ? Station->GetExitFeetLocation() : Foe->GetActorLocation() - FVector(0.0f, 0.0f, Foe->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	Foe->EndShipRide(Feet + FVector(0.0f, 0.0f, YSGalion::CrewSpawnHeight), true);
	UE_LOG(LogTemp, Log, TEXT("[YSGalion] %s : %s quitte %s"), *GetName(), *Foe->GetName(), *GetNameSafe(Station));
}

void AYSFoeGalion::OnModuleBroken(AYSShipStation* Station)
{
	if (AYSFoeCharacter* Foe = Station->GetFoeOccupant())
	{
		UnseatCrewMember(Foe);
	}
}

void AYSFoeGalion::OnShipDisabled()
{
	Super::OnShipDisabled();

	TArray<AYSFoeCharacter*> All;
	All.Add(Captain);
	All.Append(Crew);
	for (AYSFoeCharacter* Foe : All)
	{
		if (IsValid(Foe) && Foe->IsRidingShip())
		{
			UnseatCrewMember(Foe);
		}
	}
}

void AYSFoeGalion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// barre de vie du capitaine dans le HUD seulement quand la bataille navale est active (navire réveillé)
	if (HasAuthority() && IsValid(Captain))
	{
		Captain->SetHudHealthBar(!IsDormant() && !IsSinking());
	}

	if (!HasAuthority() || IsActorBeingDestroyed() || IsSinking())
	{
		return;
	}
	// capitaine tué : le Galion coule (demande de Paul) ; l'abordage se termine avec lui (EndBoarding dans OnShipDestroyedByDamage)
	if (bSinkWhenCaptainKilled && bHadCaptain && IsDefeated())
	{
		UE_LOG(LogTemp, Log, TEXT("[YSGalion] %s : capitaine tué, le Galion coule"), *GetName());
		SinkAtSea(TEXT("capitaine tué"));
		return;
	}
	// abordage : il dure jusqu'à la mort du capitaine ; plus d'équipage à répartir
	if (RamState == ERamState::Boarded || RamState == ERamState::Done)
	{
		if (RamState == ERamState::Boarded && (IsDefeated() || !IsValid(BoardedShip)))
		{
			EndBoarding(IsValid(BoardedShip) ? TEXT("capitaine tué") : TEXT("bateau abordé disparu"));
		}
		return;
	}
	CrewUpdateTimer -= DeltaSeconds;
	if (CrewUpdateTimer <= 0.0f)
	{
		CrewUpdateTimer = CrewUpdateInterval;
		UpdateCrew();
	}
}

void AYSFoeGalion::UpdateCrew()
{
	Crew.RemoveAll([](const AYSFoeCharacter* Foe) { return !IsValid(Foe) || Foe->IsDying(); });
	AYSFoeCharacter* LiveCaptain = IsValid(Captain) && !Captain->IsDying() ? Captain.Get() : nullptr;

	// joueurs (minions compris) debout sur ce pont
	bool bBoarded = false;
	for (TActorIterator<AYSPlayerCharacter> It(GetWorld()); It && !bBoarded; ++It)
	{
		bBoarded = It->IsAlive() && It->GetShipUnderFeet() == this;
	}

	TArray<AYSFoeCharacter*> All;
	if (LiveCaptain)
	{
		All.Add(LiveCaptain);
	}
	All.Append(Crew);

	if (bBoarded)
	{
		// ceux qui ne sont pas à un poste attaquent ; s'il n'y en a plus, tout le monde quitte son poste
		const bool bAnyFree = All.ContainsByPredicate([](const AYSFoeCharacter* Foe) { return !Foe->IsRidingShip(); });
		if (!bAnyFree && !bAllHands)
		{
			bAllHands = true;
			UE_LOG(LogTemp, Log, TEXT("[YSGalion] %s : joueur à bord et plus personne hors des postes, tout l'équipage quitte les postes"), *GetName());
		}
		for (AYSFoeCharacter* Foe : All)
		{
			Foe->SetCrewPost(nullptr);
			if (bAllHands && Foe->IsRidingShip())
			{
				UnseatCrewMember(Foe);
			}
		}
		return;
	}

	bAllHands = false;
	if (bDisabled)
	{
		return;
	}

	// personne à bord : capitaine à la barre, matelots aux canons libres
	if (LiveCaptain && !LiveCaptain->IsRidingShip())
	{
		AYSShipStation* Helm = GetSeatStation(0);
		LiveCaptain->SetCrewPost(Helm && Helm->IsFree() ? Helm : nullptr);
	}
	for (AYSFoeCharacter* Foe : Crew)
	{
		if (!Foe->IsRidingShip())
		{
			const AYSShipStation* Current = Foe->GetCrewPost();
			if (!Current || !Current->IsFree() || !Current->IsUsable())
			{
				Foe->SetCrewPost(FindFreeCannon(Foe));
			}
		}
	}
}

AYSShipStation* AYSFoeGalion::FindFreeCannon(const AYSFoeCharacter* Foe) const
{
	AYSShipStation* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (int32 Seat = 1; Seat <= GetCannonComponents().Num(); ++Seat)
	{
		AYSShipStation* Cannon = GetSeatStation(Seat);
		if (!Cannon || !Cannon->IsFree() || !Cannon->IsUsable() || Crew.ContainsByPredicate([Foe, Cannon](const AYSFoeCharacter* Other) { return Other != Foe && IsValid(Other) && Other->GetCrewPost() == Cannon; }))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(Cannon->GetOccupantFeetLocation(), Foe->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Cannon;
		}
	}
	return Best;
}

bool AYSFoeGalion::IsOnDeck(const FVector& WorldLocation) const
{
	return IsInsideHull(WorldLocation, -DeckEdgeInset);
}

FVector AYSFoeGalion::GetDeckRoute(const FVector& From, const FVector& To) const
{
	const FTransform& Transform = GetActorTransform();
	const FVector LocalFrom = Transform.InverseTransformPositionNoScale(From);
	const FVector LocalTo = Transform.InverseTransformPositionNoScale(To);
	const bool bFromCastle = LocalFrom.X < CastleFrontX;
	const bool bToCastle = LocalTo.X < CastleFrontX;
	if (bFromCastle == bToCastle)
	{
		return To;
	}

	const auto ToWorld = [&Transform, &LocalFrom](const FVector2D& Point) { return Transform.TransformPositionNoScale(FVector(Point.X, Point.Y, LocalFrom.Z)); };
	if (bFromCastle)
	{
		// descend : d'abord le haut de l'escalier, puis en bas
		return FVector2D::Distance(FVector2D(LocalFrom.X, LocalFrom.Y), StairsTop) > YSGalion::StairsReachDistance ? ToWorld(StairsTop) : ToWorld(StairsBottom);
	}
	// monte : d'abord le bas de l'escalier, une fois dessus vers le haut
	const bool bOnStairs = LocalFrom.X <= StairsBottom.X + YSGalion::StairsReachDistance && FMath::Abs(LocalFrom.Y - StairsBottom.Y) <= StairsHalfWidth;
	return bOnStairs ? ToWorld(StairsTop) : ToWorld(StairsBottom);
}

AYSPirateShip* AYSFoeGalion::FindTargetShip() const
{
	AYSPirateShip* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<AYSPirateShip> It(GetWorld()); It; ++It)
	{
		AYSPirateShip* Ship = *It;
		if (Ship->IsFoeShip() || Ship->IsA<AYSLongboat>() || !Ship->IsPresent() || Ship->IsActorBeingDestroyed())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(Ship->GetActorLocation(), GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Ship;
		}
	}
	return Best;
}

const APawn* AYSFoeGalion::FindNearestPlayerPawn() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const APawn* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();
	for (const APlayerState* Player : GameState ? GameState->PlayerArray : TArray<TObjectPtr<APlayerState>>())
	{
		const AYSPlayerCharacter* Character = Player ? Cast<AYSPlayerCharacter>(Player->GetPawn()) : nullptr;
		if (Character && Character->IsAlive())
		{
			const float DistSq = FVector::DistSquared2D(Character->GetActorLocation(), GetActorLocation());
			if (DistSq < NearestDistSq)
			{
				NearestDistSq = DistSq;
				Nearest = Character;
			}
		}
	}
	return Nearest;
}

void AYSFoeGalion::OnHullBlocked()
{
	const double Now = GetWorld()->GetTimeSeconds();
	ReverseUntilTime = Now + ReverseDuration;
	if (CircleSide != 0.0f)
	{
		CircleSide = -CircleSide;
		LastFlipTime = Now;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSGalion] %s échoué : marche arrière, repart dans l'autre sens"), *GetName());
}

bool AYSFoeGalion::ComputeAutoPilot(float& OutThrottle, float& OutSteer)
{
	// le capitaine a quitté la barre (ou est mort) : le galion n'avance plus
	const AYSShipStation* Helm = GetSeatStation(0);
	if (!IsValid(Captain) || !Helm || Helm->GetFoeOccupant() != Captain)
	{
		return false;
	}

	const FVector Location = GetActorLocation();
	AYSPirateShip* TargetShip = FindTargetShip();

	// bataille navale : Ram Delay après l'activation, il fonce droit sur le bateau des joueurs ; contact = abordage
	if (RamState == ERamState::Boarded || RamState == ERamState::Done)
	{
		OutThrottle = 0.0f;
		OutSteer = 0.0f;
		return true;
	}
	const double RamNow = GetWorld()->GetTimeSeconds();
	if (RamState == ERamState::None && BattleActivationTime >= 0.0 && RamDelay > 0.0f && RamNow - BattleActivationTime >= RamDelay && TargetShip)
	{
		RamState = ERamState::Ramming;
		UE_LOG(LogTemp, Log, TEXT("[YSGalion] %s fonce sur %s"), *GetName(), *TargetShip->GetName());
	}
	if (RamState == ERamState::Ramming && TargetShip)
	{
		const FVector TargetEdge = TargetShip->GetClosestHullPoint(Location);
		const FVector OwnEdge = GetClosestHullPoint(TargetEdge);
		if (FVector::Dist2D(TargetEdge, OwnEdge) <= RamContactDistance)
		{
			StartBoarding(TargetShip);
			OutThrottle = 0.0f;
			OutSteer = 0.0f;
			return true;
		}
		const FVector ToTarget = ((TargetShip->GetActorLocation() - Location) * FVector(1.0f, 1.0f, 0.0f)).GetSafeNormal();
		OutThrottle = 1.0f;
		OutSteer = ComputeSteerTowards(AvoidShore(ToTarget, ToTarget, false), SteerFullAngle);
		return true;
	}

	const APawn* TargetPawn = TargetShip ? nullptr : FindNearestPlayerPawn();
	if (!TargetShip && !TargetPawn)
	{
		OutThrottle = 0.0f;
		OutSteer = 0.0f;
		return true;
	}
	const FVector Point = TargetShip ? TargetShip->GetClosestHullPoint(Location) : TargetPawn->GetActorLocation();
	const FVector ToPoint(Point.X - Location.X, Point.Y - Location.Y, 0.0f);
	const float Distance = ToPoint.Size();
	const FVector Inward = ToPoint.GetSafeNormal();
	const double Now = GetWorld()->GetTimeSeconds();

	// échoué : il recule un moment
	if (Now < ReverseUntilTime)
	{
		OutThrottle = -1.0f;
		OutSteer = 0.0f;
		return true;
	}

	// distance mesurée de son centre : flanc à Engage Distance de la coque visée
	const FBox2D Hull = GetHullBounds();
	const float Radius = EngageDistance + (Hull.bIsValid ? Hull.Max.Y : 0.0f);
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();

	FVector Desired = Inward;
	if (Distance <= Radius + CircleBand)
	{
		// sens du tour : celui où il va déjà
		const FVector Tangent = FVector::CrossProduct(FVector::UpVector, Inward);
		if (CircleSide == 0.0f)
		{
			CircleSide = FVector::DotProduct(Tangent, Forward) >= 0.0f ? 1.0f : -1.0f;
			UE_LOG(LogTemp, Log, TEXT("[YSGalion] %s : à %.0f cm de sa cible, commence le tour (sens %.0f)"), *GetName(), Distance, CircleSide);
		}
		const float RadialError = FMath::Clamp((Distance - Radius) / CircleBand, -1.0f, 1.0f);
		Desired = Tangent * CircleSide + Inward * RadialError;
		// beaucoup trop près : il s'éloigne
		if (Distance < Radius * TooCloseRatio)
		{
			Desired = Tangent * CircleSide * 0.3f - Inward;
		}
	}
	else
	{
		CircleSide = 0.0f;
	}

	if (Now - LastRoutineLogTime >= 2.0)
	{
		LastRoutineLogTime = Now;
		UE_LOG(LogTemp, Verbose, TEXT("[YSGalion] %s en (%.0f, %.0f) : à %.0f cm de sa cible (rayon %.0f), sens %.0f, vitesse %.0f"), *GetName(), Location.X, Location.Y, Distance, Radius, CircleSide, GetSpeed());
	}

	OutThrottle = 1.0f;
	OutSteer = ComputeSteerTowards(AvoidShore(Desired.GetSafeNormal2D(), Inward, CircleSide != 0.0f), SteerFullAngle);
	return true;
}

FVector AYSFoeGalion::AvoidShore(const FVector& Desired, const FVector& Inward, bool bOrbiting)
{
	if (ShoreLookAhead <= 0.0f)
	{
		return Desired;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextShoreCheckTime)
	{
		NextShoreCheckTime = Now + YSGalion::ShoreCheckInterval;
		const FVector Location = GetActorLocation();
		const auto IsClear = [this, &Desired, &Location](float Offset)
		{
			const FVector Direction = Desired.RotateAngleAxis(Offset, FVector::UpVector);
			const float Yaw = Direction.Rotation().Yaw;
			return GetHullMinDepth(Location + Direction * ShoreLookAhead, Yaw) >= MinWaterDepth && GetHullMinDepth(Location + Direction * ShoreLookAhead * 0.5f, Yaw) >= MinWaterDepth;
		};

		// le côté déjà pris, sinon celui qui l'éloigne de sa cible
		const float Away = FVector::DotProduct(Desired.RotateAngleAxis(YSGalion::ShoreAvoidStep, FVector::UpVector), Inward) < FVector::DotProduct(Desired.RotateAngleAxis(-YSGalion::ShoreAvoidStep, FVector::UpVector), Inward) ? 1.0f : -1.0f;
		const float Side = ShoreAvoidYaw != 0.0f ? FMath::Sign(ShoreAvoidYaw) : Away;
		float Found = 0.0f;
		bool bClear = IsClear(0.0f);
		for (float Offset = YSGalion::ShoreAvoidStep; !bClear && Offset <= 180.0f; Offset += YSGalion::ShoreAvoidStep)
		{
			for (const float Sign : { Side, -Side })
			{
				if (!bClear && IsClear(Sign * Offset))
				{
					Found = Sign * Offset;
					bClear = true;
				}
			}
		}
		ShoreAvoidYaw = Found;

		if (bOrbiting && FMath::Abs(Found) >= YSGalion::ShoreFlipYaw && Now - LastFlipTime >= FlipCooldown)
		{
			LastFlipTime = Now;
			CircleSide = -CircleSide;
			UE_LOG(LogTemp, Log, TEXT("[YSGalion] %s : la côte barre le passage, change de sens"), *GetName());
		}
	}
	return Desired.RotateAngleAxis(ShoreAvoidYaw, FVector::UpVector);
}

AYSFoeGalion* AYSFoeGalion::SpawnAtSea(UWorld* World, TSubclassOf<AYSFoeGalion> ShipClass, float MinDistance, float MaxDistance)
{
	FVector Point;
	float Yaw = 0.0f;
	if (!World || !ShipClass || !FindSeaSpawnPoint(World, ShipClass->GetDefaultObject<AYSFoeGalion>()->MinWaterDepth + YSGalion::SpawnExtraDepth, MinDistance, MaxDistance, YSGalion::SpawnStep, YSGalion::SpawnAttempts, YSGalion::SpawnShipMargin, YSGalion::SpawnClearanceRadius, Point, Yaw))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AYSFoeGalion>(ShipClass, Point, FRotator(0.0f, Yaw, 0.0f), SpawnParams);
}
