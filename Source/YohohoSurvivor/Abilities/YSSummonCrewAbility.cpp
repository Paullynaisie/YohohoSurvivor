#include "Abilities/YSSummonCrewAbility.h"
#include "Components/CapsuleComponent.h"
#include "Core/YSGameState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Map/YSOcean.h"
#include "Player/YSPlayerCharacter.h"
#include "Summons/YSCaptainMinion.h"
#include "TimerManager.h"
#include "Weapons/YSWeaponSet.h"

UYSSummonCrewAbility::UYSSummonCrewAbility()
{
	WeaponType = EYSWeaponType::Summon;
	Cooldown = 10.0f;
	DisplayName = NSLOCTEXT("YS", "SummonTheCrew", "Summon the crew !");
}

bool UYSSummonCrewAbility::EvaluateHandPose(float& OutIdleWeight, FVector& OutLocation, FQuat& OutRotation) const
{
	// depuis l'activation (prédite chez le Captain, multicast chez les autres)
	const float Elapsed = GetTimeSinceActivation();
	if (Elapsed < 0.0f || Elapsed >= WindupDuration + StrikeDuration + RecoverDuration)
	{
		return false;
	}

	if (Elapsed < WindupDuration)
	{
		// préparation : la main descend et recule
		const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, Elapsed / WindupDuration, 2.0f);
		OutIdleWeight = 1.0f - Alpha;
		OutLocation = WindupHandLocation;
		OutRotation = WindupHandRotation.Quaternion();
	}
	else if (Elapsed < WindupDuration + StrikeDuration)
	{
		// crochet vers le haut, rapide
		const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, (Elapsed - WindupDuration) / StrikeDuration, StrikeEaseExponent);
		OutIdleWeight = 0.0f;
		OutLocation = FMath::Lerp(WindupHandLocation, StrikeHandLocation, Alpha);
		OutRotation = FQuat::Slerp(WindupHandRotation.Quaternion(), StrikeHandRotation.Quaternion(), Alpha);
	}
	else
	{
		// retour à la pose normale
		OutIdleWeight = FMath::InterpEaseInOut(0.0f, 1.0f, (Elapsed - WindupDuration - StrikeDuration) / RecoverDuration, 2.0f);
		OutLocation = StrikeHandLocation;
		OutRotation = StrikeHandRotation.Quaternion();
	}
	return true;
}

int32 UYSSummonCrewAbility::GetMaxMinions() const
{
	const AYSPlayerCharacter* Captain = Cast<AYSPlayerCharacter>(GetHolderPawn());
	return BaseMaxMinions + (Captain ? FMath::RoundToInt(Captain->GetSummonSystemBonus()) : 0);
}

int32 UYSSummonCrewAbility::CountMinions(const UWorld* World, const AYSPlayerCharacter* Captain)
{
	int32 Count = 0;
	for (TActorIterator<AYSCaptainMinion> It(World); It && Captain; ++It)
	{
		if (It->GetCaptain() == Captain && It->IsAlive() && !It->IsActorBeingDestroyed())
		{
			++Count;
		}
	}
	return Count;
}

bool UYSSummonCrewAbility::CanActivateNow() const
{
	const AYSPlayerCharacter* Captain = Cast<AYSPlayerCharacter>(GetHolderPawn());
	return MinionClass && Captain && CountMinions(GetWorld(), Captain) < GetMaxMinions();
}

void UYSSummonCrewAbility::ExecuteAbility(const FVector& TargetLocation)
{
	GetWorld()->GetTimerManager().SetTimer(SummonTimer, FTimerDelegate::CreateUObject(this, &UYSSummonCrewAbility::OnHookStrike), FMath::Max(WindupDuration + StrikeDuration, KINDA_SMALL_NUMBER), false);
}

void UYSSummonCrewAbility::OnHookStrike()
{
	SpawnMinions();
}

void UYSSummonCrewAbility::OnActionCancelled()
{
	GetWorld()->GetTimerManager().ClearTimer(SummonTimer);
}

void UYSSummonCrewAbility::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SummonTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UYSSummonCrewAbility::SpawnMinions()
{
	AYSPlayerCharacter* Captain = Cast<AYSPlayerCharacter>(GetHolderPawn());
	UWorld* World = GetWorld();
	if (!Captain || !World || !MinionClass || !Captain->IsAlive())
	{
		return;
	}

	const int32 ToSpawn = FMath::Clamp(GetMaxMinions() - CountMinions(World, Captain), 0, MinionsPerSummon);
	for (int32 Index = 0; Index < ToSpawn; ++Index)
	{
		// sur les côtés du Captain, un peu derrière (alternés droite / gauche, puis de plus en plus derrière)
		const float Side = Index % 2 == 0 ? 1.0f : -1.0f;
		const float Yaw = Captain->GetActorRotation().Yaw + Side * (110.0f + 25.0f * (Index / 2));
		SpawnMinionAt(Captain->GetActorLocation() + FRotator(0.0f, Yaw, 0.0f).Vector() * SpawnDistance, Captain->GetActorRotation().Yaw);
	}
}

AYSCaptainMinion* UYSSummonCrewAbility::SpawnMinionAt(const FVector& Location, float Yaw)
{
	AYSPlayerCharacter* Captain = Cast<AYSPlayerCharacter>(GetHolderPawn());
	UWorld* World = GetWorld();
	if (!Captain || !World || !MinionClass || !Captain->IsAlive() || !Captain->HasAuthority())
	{
		return nullptr;
	}
	const int32 Alive = CountMinions(World, Captain);
	if (Alive >= GetMaxMinions())
	{
		return nullptr;
	}

	// posé sur le sol (pas dans la mer : il tomberait dans la deadzone) ; toujours à cet endroit : il traverse les personnages à son apparition
	const AYSCaptainMinion* DefaultMinion = MinionClass->GetDefaultObject<AYSCaptainMinion>();
	const float HalfHeight = DefaultMinion && DefaultMinion->GetCapsuleComponent() ? DefaultMinion->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 96.0f;
	FVector SpawnLocation = Location;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SummonCrew), false, Captain);
	if (World->LineTraceSingleByChannel(Hit, Location + FVector(0.0f, 0.0f, 300.0f), Location - FVector(0.0f, 0.0f, 1000.0f), ECC_WorldStatic, Params))
	{
		if (AYSOcean::IsUnderwater(World, Hit.ImpactPoint))
		{
			UE_LOG(LogTemp, Verbose, TEXT("[YSSummon] %s : pas de minion en (%.0f, %.0f), sous l'eau"), *Captain->GetName(), Location.X, Location.Y);
			return nullptr;
		}
		SpawnLocation.Z = Hit.ImpactPoint.Z + HalfHeight;
	}

	const FTransform SpawnTransform(FRotator(0.0f, Yaw, 0.0f), SpawnLocation);
	AYSCaptainMinion* Minion = World->SpawnActorDeferred<AYSCaptainMinion>(MinionClass, SpawnTransform, Captain, Captain, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Minion)
	{
		return nullptr;
	}
	const AYSGameState* YSGameState = World->GetGameState<AYSGameState>();
	const int32 FaceCount = YSGameState ? YSGameState->GetCustomizationFaces().Num() : 0;
	UYSWeaponSet* Set = MinionWeaponSets.Num() > 0 ? MinionWeaponSets[FMath::RandRange(0, MinionWeaponSets.Num() - 1)].Get() : nullptr;
	Minion->InitMinion(Captain, Set, FaceCount > 0 ? FMath::RandRange(0, FaceCount - 1) : 0);
	Minion->FinishSpawning(SpawnTransform);
	UE_LOG(LogTemp, Verbose, TEXT("[YSSummon] %s invoque %s (set %s) en (%.0f, %.0f, %.0f) : %d / %d minions"), *Captain->GetName(), *Minion->GetName(),
		Set ? *Set->GetName() : TEXT("par défaut"), SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z, Alive + 1, GetMaxMinions());
	return Minion;
}
