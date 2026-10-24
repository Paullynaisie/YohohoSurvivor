#include "Abilities/YSRollAndBombAbility.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Upgrades/YSUpgrade.h"
#include "Weapons/YSPlayerBomb.h"

namespace YSRollAndBomb
{
	/** Recherche du sol sous les pieds du personnage */
	constexpr float GroundSearchUp = 50.0f;
	constexpr float GroundSearchDown = 500.0f;
}

UYSRollAndBombAbility::UYSRollAndBombAbility()
{
	DisplayName = NSLOCTEXT("YS", "RollAndBomb", "Roll and Bomb");
	Cooldown = 5.0f;
	// pas de pause après la roulade : elle retombe à la vitesse normale
	AimDuration = 0.0f;
	AimMoveSpeedMultiplier = 1.0f;
}

void UYSRollAndBombAbility::ExecuteAbility(const FVector& TargetLocation)
{
	DropBomb();

	// Golden Mine : la seconde mine est posée en cours de roulade (Second Mine Roll Fraction)
	if (MineClass && GetWeaponStat(UYSMineUpgrade::MineStat, 0.0f) > 0.0f && GetWeaponStat(UYSMineUpgrade::MineDoubleStat, 0.0f) > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(SecondMineTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { DropBomb(); }), FMath::Max(RollDuration * SecondMineRollFraction, KINDA_SMALL_NUMBER), false);
	}

	StartRoll();
	MulticastRollStarted();
	UE_LOG(LogTemp, Verbose, TEXT("[YSRollAndBomb] %s roulade à %.2f"), *GetOwner()->GetName(), GetWorld()->GetTimeSeconds());
}

void UYSRollAndBombAbility::DropBomb()
{
	// upgrade « Une Mine ! » : une mine à la place de la bombe
	const bool bMine = MineClass && GetWeaponStat(UYSMineUpgrade::MineStat, 0.0f) > 0.0f;
	const TSubclassOf<AYSPlayerBomb> SpawnClass = bMine ? MineClass : BombClass;
	ACharacter* Character = Cast<ACharacter>(GetHolderPawn());
	if (!SpawnClass || !IsValid(Character))
	{
		return;
	}

	// au sol sous les pieds (trace sur le décor), sinon au niveau des pieds
	const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FVector Location = Feet;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSRollAndBombGround), false, Character);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Feet + FVector(0.0f, 0.0f, YSRollAndBomb::GroundSearchUp), Feet - FVector(0.0f, 0.0f, YSRollAndBomb::GroundSearchDown),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		Location = Hit.ImpactPoint;
	}

	const FTransform SpawnTransform(FRotator(0.0f, Character->GetActorRotation().Yaw, 0.0f), Location);
	AYSPlayerBomb* Bomb = GetWorld()->SpawnActorDeferred<AYSPlayerBomb>(SpawnClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Bomb)
	{
		// mine : x Mine Damage Multiplier de l'upgrade
		const float BombHitDamage = BombDamage * GetDamageMultiplier() * (bMine ? GetWeaponStat(UYSMineUpgrade::MineDamageStat, 1.0f) : 1.0f);
		Bomb->SetDamage(BombHitDamage);
		Bomb->SetExtraPushback(bMine ? GetWeaponStat(UYSMineUpgrade::MinePushbackStat, 0.0f) : 0.0f);
		Bomb->FinishSpawning(SpawnTransform);
		UE_LOG(LogTemp, Verbose, TEXT("[YSRollAndBomb] %s pose %s en (%.0f, %.0f, %.0f) : %.0f dégâts"), *GetOwner()->GetName(), *Bomb->GetName(),
			Location.X, Location.Y, Location.Z, BombHitDamage);
	}
}

float UYSRollAndBombAbility::GetCooldownMultiplier() const
{
	// « Une Mine ! » : recharge x Mine Cooldown Multiplier
	const bool bMine = MineClass && GetWeaponStat(UYSMineUpgrade::MineStat, 0.0f) > 0.0f;
	return Super::GetCooldownMultiplier() * (bMine ? GetWeaponStat(UYSMineUpgrade::MineCooldownStat, 1.0f) : 1.0f);
}

void UYSRollAndBombAbility::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SecondMineTimer);
	}
	Super::EndPlay(EndPlayReason);
}
