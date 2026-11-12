#include "Interaction/YSObjectiveTreasureMap.h"
#include "Core/YSGameMode.h"
#include "Enemies/YSEnemySpawnRule.h"
#include "Enemies/YSFoeCharacter.h"
#include "Engine/World.h"
#include "Island/YSIsland.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"

namespace YSObjectiveMap
{
	/** Sol vérifié tous les ... entre deux points d'un tracé */
	constexpr float GroundCheckStep = 200.0f;

	/** Embuscade : essais d'emplacement par NME */
	constexpr int32 AmbushPlacementAttempts = 6;
}

AYSObjectiveTreasureMap::AYSObjectiveTreasureMap()
{
	DisplayName = NSLOCTEXT("YS", "ObjectiveTreasureMap", "Carte au trésor de la boussole");
	DashColor = FLinearColor(0.1f, 1.0f, 0.2f);
	GuideIndicatorColor = FLinearColor(0.1f, 1.0f, 0.2f);
	// pas de vague bonus de la carte au trésor : ce sont les embuscades des fins de tracé
	WaveAtPoint = 1000;
	WaveEnemiesPerPlayer = 0;
}

void AYSObjectiveTreasureMap::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AYSObjectiveTreasureMap, LegEnds);
}

void AYSObjectiveTreasureMap::BuildLeg(const UWorld* World, const FVector& From, const FVector& To, TArray<FVector>& OutPoints) const
{
	const float Length = FVector::Dist2D(From, To);
	const float Spacing = FMath::Max(LegPointSpacing, 200.0f);
	const int32 Segments = FMath::Max(1, FMath::RoundToInt(Length / Spacing));

	// segment au sol de A à B (pas de vide, pas de massif, pas de marche trop haute)
	const auto IsSegmentGrounded = [this, World](const FVector& A, const FVector& B)
	{
		const int32 Checks = FMath::CeilToInt(FVector::Dist2D(A, B) / YSObjectiveMap::GroundCheckStep);
		for (int32 Check = 1; Check < Checks; ++Check)
		{
			const FVector Sample = FMath::Lerp(A, B, static_cast<float>(Check) / Checks);
			FVector SampleGround;
			if (!FindPathGround(World, Sample, Sample.Z, LegMaxHeightDifference, SampleGround))
			{
				return false;
			}
		}
		return true;
	};

	// marche guidée : à chaque pas, parmi des directions vers la cible (écart au hasard, plus large si ça bloque), le point au sol valide
	// qui rapproche le plus de la cible ; contourne les massifs au lieu de les traverser
	const int32 MaxSteps = Segments * 3 + 4;
	for (int32 Attempt = 0; Attempt < LegAttempts; ++Attempt)
	{
		OutPoints.Reset();
		FVector Previous = From;
		bool bArrived = false;
		for (int32 Step = 0; Step < MaxSteps && !bArrived; ++Step)
		{
			// assez près : dernier segment jusqu'à la cible
			if (FVector::Dist2D(Previous, To) <= Spacing * 1.3f && IsSegmentGrounded(Previous, To))
			{
				OutPoints.Add(To);
				bArrived = true;
				break;
			}
			const FVector ToTarget = (To - Previous).GetSafeNormal2D();
			const float BaseYaw = ToTarget.Rotation().Yaw;
			FVector Best;
			float BestDistance = TNumericLimits<float>::Max();
			for (int32 Candidate = 0; Candidate < 12; ++Candidate)
			{
				// premiers essais proches de la ligne droite (petit décalage = look de carte), puis de plus en plus de côté
				const float Spread = Candidate < 4 ? 25.0f : Candidate < 8 ? 60.0f : 100.0f;
				const float Yaw = BaseYaw + FMath::FRandRange(-Spread, Spread);
				const float StepLength = FMath::Min(Spacing, FVector::Dist2D(Previous, To)) * FMath::FRandRange(0.8f, 1.1f);
				FVector Point;
				if (!FindPathGround(World, Previous + FRotator(0.0f, Yaw, 0.0f).Vector() * StepLength, Previous.Z, LegMaxHeightDifference, Point)
					|| !IsSegmentGrounded(Previous, Point))
				{
					continue;
				}
				// pas de retour sur ses pas
				bool bTooClose = false;
				for (int32 Index = 0; Index + 1 < OutPoints.Num() && !bTooClose; ++Index)
				{
					bTooClose = FVector::Dist2D(Point, OutPoints[Index]) < Spacing * 0.5f;
				}
				const float Remaining = FVector::Dist2D(Point, To);
				if (!bTooClose && Remaining < BestDistance)
				{
					BestDistance = Remaining;
					Best = Point;
				}
			}
			if (BestDistance == TNumericLimits<float>::Max())
			{
				break;
			}
			OutPoints.Add(Best);
			Previous = Best;
		}
		if (bArrived)
		{
			return;
		}
	}

	// aucun tracé valide : ligne droite, chaque point posé sur ce qu'il y a dessous (le tracé ne doit jamais bloquer l'objectif)
	OutPoints.Reset();
	for (int32 Index = 1; Index <= Segments; ++Index)
	{
		const FVector Candidate = FMath::Lerp(From, To, static_cast<float>(Index) / Segments);
		FVector Point = Index == Segments ? To : Candidate;
		FVector Ground;
		if (FindPathGround(World, Candidate, Candidate.Z, 5000.0f, Ground))
		{
			Point = Index == Segments ? To : Ground;
		}
		OutPoints.Add(Point);
	}
	UE_LOG(LogTemp, Log, TEXT("[YSObjectiveMap] Tracé de %s à %s : aucun tracé valide après %d essais, ligne droite"), *From.ToCompactString(), *To.ToCompactString(), LegAttempts);
}

void AYSObjectiveTreasureMap::InitObjectiveMap(const TArray<FVector>& Points, const TArray<int32>& InLegEnds, AYSIsland* InIsland, int32 InObjectiveIndex)
{
	LegEnds = InLegEnds;
	OwnerIsland = InIsland;
	ObjectiveIndex = InObjectiveIndex;
	SetPath(Points);
}

int32 AYSObjectiveTreasureMap::GetCurrentLeg() const
{
	for (int32 Leg = 0; Leg < LegEnds.Num(); ++Leg)
	{
		if (LegEnds[Leg] > ReachedPointIndex)
		{
			return Leg;
		}
	}
	return LegEnds.Num();
}

int32 AYSObjectiveTreasureMap::GetFirstVisiblePointIndex() const
{
	const int32 Leg = GetCurrentLeg();
	const int32 LegStart = Leg > 0 && LegEnds.IsValidIndex(Leg - 1) ? LegEnds[Leg - 1] : 0;
	return FMath::Max(Super::GetFirstVisiblePointIndex(), LegStart);
}

int32 AYSObjectiveTreasureMap::GetMaxReachablePointIndex() const
{
	// seule la fin du tracé en cours compte (et seul lui est affiché)
	const int32 Leg = GetCurrentLeg();
	return LegEnds.IsValidIndex(Leg) ? LegEnds[Leg] : PathPoints.Num() - 1;
}

void AYSObjectiveTreasureMap::OnPointsReached(int32 PreviousReached)
{
	for (int32 Leg = 0; Leg < LegEnds.Num(); ++Leg)
	{
		if (LegEnds[Leg] <= PreviousReached || LegEnds[Leg] > ReachedPointIndex)
		{
			continue;
		}
		if (Leg == LegEnds.Num() - 1)
		{
			UE_LOG(LogTemp, Log, TEXT("[YSObjectiveMap] %s : fin du tracé %d / %d, la boussole apparaît"), *GetName(), Leg + 1, LegEnds.Num());
			Complete();
			return;
		}
		UE_LOG(LogTemp, Log, TEXT("[YSObjectiveMap] %s : fin du tracé %d / %d, embuscade, tracé %d visible"), *GetName(), Leg + 1, LegEnds.Num(), Leg + 2);
		SpawnAmbush(PathPoints[LegEnds[Leg]], Leg);
	}
}

void AYSObjectiveTreasureMap::SpawnAmbush(const FVector& Center, int32 Leg)
{
	AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	if (!GameMode)
	{
		return;
	}
	int32 Wanted = 0;
	int32 Spawned = 0;
	for (const FYSGalionCrewGroup& Group : AmbushGroups)
	{
		TArray<TSubclassOf<AYSFoeCharacter>> Classes = Group.Classes;
		Classes.RemoveAll([](const TSubclassOf<AYSFoeCharacter>& Class) { return !Class; });
		const int32 Count = Classes.Num() > 0 ? GameMode->ScaleEnemyCount(Group.Count) : 0;
		Wanted += Count;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			for (int32 Attempt = 0; Attempt < YSObjectiveMap::AmbushPlacementAttempts; ++Attempt)
			{
				const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
				const float Radius = FMath::FRandRange(AmbushMinRadius, FMath::Max(AmbushMinRadius, AmbushMaxRadius));
				const FVector Candidate = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Radius;
				FVector Ground;
				if (!GameMode->FindIslandGround(Candidate, Ground) || AYSIsland::IsBlockedAt(GetWorld(), Ground, 150.0f))
				{
					continue;
				}
				if (AYSFoeCharacter* Foe = GameMode->SpawnFoeOnGround(UYSEnemySpawnRule::PickEnemyClass(Classes), Ground, Center))
				{
					++Spawned;
					break;
				}
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[YSObjectiveMap] %s : embuscade de la fin du tracé %d en %s : %d / %d NME"), *GetName(), Leg + 1, *Center.ToCompactString(), Spawned, Wanted);
}

void AYSObjectiveTreasureMap::Complete()
{
	// la boussole (objectif de l'île) au dernier point, les coffres (Reward Classes) en cercle autour
	if (AYSIsland* Island = OwnerIsland.Get())
	{
		Island->OnObjectiveMapCompleted(this, ObjectiveIndex, PathPoints.Last());
	}
	Super::Complete();
}

void AYSObjectiveTreasureMap::DebugCompleteLeg()
{
	AYSPlayerCharacter* Character = GetCarrier();
	const int32 Target = GetMaxReachablePointIndex();
	if (Character && PathPoints.IsValidIndex(Target))
	{
		Character->TeleportTo(PathPoints[Target] + FVector(0.0f, 0.0f, 120.0f), Character->GetActorRotation());
		UE_LOG(LogTemp, Log, TEXT("[YSObjectiveMap] Debug : %s posé à la fin du tracé %d"), *Character->GetName(), GetCurrentLeg() + 1);
	}
}
