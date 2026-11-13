#include "Weapons/YSLesserSpinningBlade.h"
#include "Player/YSPlayerCharacter.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

namespace YSLesserBlade
{
	const TCHAR* const BlueprintClassPath = TEXT("/Game/Placeable/Weapons/Player/BP_LesserSpinningBlade.BP_LesserSpinningBlade_C");

	FVector Bezier(const FVector& A, const FVector& Control, const FVector& B, float Alpha)
	{
		const float U = 1.0f - Alpha;
		return U * U * A + 2.0f * U * Alpha * Control + Alpha * Alpha * B;
	}
}

AYSLesserSpinningBlade::AYSLesserSpinningBlade()
{
	// trajet calculé sur chaque machine : pas de réplication du mouvement
	SetReplicatingMovement(false);
	bLevelDecay = false;
	MaxLevel = 1;
	RotationSpeed = 1080.0f;
	SwordLocation = FVector(-35.0f, 0.0f, 0.0f);
}

void AYSLesserSpinningBlade::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AYSLesserSpinningBlade, Path, COND_InitialOnly);
}

AYSLesserSpinningBlade* AYSLesserSpinningBlade::Launch(AYSPlayerCharacter* Character, const FYSBladePath& InPath)
{
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!World || !Character->HasAuthority())
	{
		return nullptr;
	}
	UClass* SpawnClass = LoadSpawnClass(YSLesserBlade::BlueprintClassPath, AYSLesserSpinningBlade::StaticClass());
	const FTransform Transform(FVector(InPath.Start));
	AYSLesserSpinningBlade* Blade = World->SpawnActorDeferred<AYSLesserSpinningBlade>(SpawnClass, Transform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Blade)
	{
		return nullptr;
	}
	Blade->Path = InPath;
	const AGameStateBase* GameState = World->GetGameState();
	Blade->SpawnServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
	Blade->FinishSpawning(Transform);
	Blade->AddLevel(1);
	UE_LOG(LogTemp, Verbose, TEXT("[YSLesserBlade] %s : %s de (%.0f, %.0f) vers (%.0f, %.0f), côté %.0f, aller %.2f s%s"), *Character->GetName(),
		InPath.Mode == EYSBladePathMode::Curve ? TEXT("courbe") : TEXT("ligne droite"), InPath.Start.X, InPath.Start.Y, InPath.Target.X, InPath.Target.Y,
		InPath.Side, InPath.OutDuration, InPath.bHomingReturn ? TEXT(", retour vers le joueur") : TEXT(""));
	return Blade;
}

bool AYSLesserSpinningBlade::EvaluatePath(float Time, FVector& OutLocation) const
{
	const FVector Start = Path.Start;
	const FVector Target = Path.Target;
	if (Path.Mode == EYSBladePathMode::Straight)
	{
		OutLocation = FMath::Lerp(Start, Target, FMath::Clamp(Time / FMath::Max(Path.OutDuration, 0.01f), 0.0f, 1.0f));
		return Time < Path.OutDuration;
	}

	// courbes symétriques : point de contrôle à 2 x Apex sur le côté (le milieu de la courbe est alors à Apex)
	const FVector Direction = (Target - Start).GetSafeNormal2D();
	const FVector Right(-Direction.Y, Direction.X, 0.0f);
	const FVector Middle = (Start + Target) * 0.5f;
	if (Time < Path.OutDuration)
	{
		OutLocation = YSLesserBlade::Bezier(Start, Middle + Right * Path.Side * 2.0f * Path.Apex, Target, Time / FMath::Max(Path.OutDuration, 0.01f));
		return true;
	}
	// retour au départ par la courbe de l'autre lame
	const float ReturnTime = Time - Path.OutDuration;
	OutLocation = YSLesserBlade::Bezier(Target, Middle - Right * Path.Side * 2.0f * Path.Apex, Start, FMath::Clamp(ReturnTime / FMath::Max(Path.ReturnDuration, 0.01f), 0.0f, 1.0f));
	return ReturnTime < Path.ReturnDuration;
}

void AYSLesserSpinningBlade::Tick(float DeltaSeconds)
{
	const float Time = static_cast<float>(GetServerTime() - SpawnServerTime);
	AYSPlayerCharacter* Character = GetOwnerCharacter();
	FVector Location;
	bool bAlive = true;

	if (Path.bHomingReturn && Path.Mode == EYSBladePathMode::Curve && Time >= Path.OutDuration)
	{
		// Swordsrang : depuis le point de croisement, vers le joueur à la vitesse moyenne de l'aller
		if (!bHoming)
		{
			bHoming = true;
			HomingLocation = Path.Target;
			float Length = 0.0f;
			FVector Previous = Path.Start;
			for (int32 Step = 1; Step <= 16; ++Step)
			{
				FVector Point;
				EvaluatePath(Path.OutDuration * Step / 16.0f, Point);
				Length += FVector::Dist2D(Previous, Point);
				Previous = Point;
			}
			HomingSpeed = Length / FMath::Max(Path.OutDuration, 0.01f);
		}
		const FVector Goal = IsValid(Character) ? Character->GetActorLocation() : HomingLocation;
		FVector Delta = Goal - HomingLocation;
		Delta.Z = 0.0f;
		const float Step = HomingSpeed * DeltaSeconds;
		HomingLocation += Delta.SizeSquared() <= FMath::Square(Step) ? Delta : Delta.GetSafeNormal() * Step;
		Location = HomingLocation;
		bAlive = FVector::Dist2D(HomingLocation, Goal) > HomingArriveDistance;
	}
	else
	{
		bAlive = EvaluatePath(Time, Location);
	}

	Location.Z = FindGroundZ(Location, GetActorLocation().Z);
	SetActorLocation(Location);
	if (!bAlive)
	{
		if (HasAuthority())
		{
			UE_LOG(LogTemp, Verbose, TEXT("[YSLesserBlade] %s : fin du trajet à %.2f s en (%.0f, %.0f)%s"), *GetName(), Time, Location.X, Location.Y, bHoming ? TEXT(", joueur atteint") : TEXT(""));
			Destroy();
		}
		return;
	}
	Super::Tick(DeltaSeconds);
}

bool AYSLesserSpinningBlade::IsCriticalAt(const FVector& TargetLocation) const
{
	return Path.CriticalRadius > 0.0f && FVector::Dist2D(TargetLocation, Path.Target) <= Path.CriticalRadius;
}
