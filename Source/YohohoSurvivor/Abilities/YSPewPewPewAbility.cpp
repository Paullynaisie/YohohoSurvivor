#include "Abilities/YSPewPewPewAbility.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "TimerManager.h"
#include "Upgrades/YSUpgrade.h"
#include "Weapons/YSProjectile.h"
#include "Weapons/YSWeapon.h"

namespace YSPewPewPew
{
	/** Tirs d'un cycle : nord, sud, est, ouest (décalage de yaw depuis le haut de l'écran) */
	constexpr float CardinalYaws[4] = { 0.0f, 180.0f, 90.0f, -90.0f };
}

UYSPewPewPewAbility::UYSPewPewPewAbility()
{
	PrimaryComponentTick.bCanEverTick = true;
	DisplayName = NSLOCTEXT("YS", "PewPewPew", "Pew pew pew");
	Cooldown = 20.0f;
	AssistAimAngle = 45.0f;
}

void UYSPewPewPewAbility::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UYSPewPewPewAbility, bPewPewPewActive);
	DOREPLIFETIME(UYSPewPewPewAbility, PewEndServerTime);
	DOREPLIFETIME(UYSPewPewPewAbility, LastShotYaw);
}

void UYSPewPewPewAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// serveur : réveillée par l'upgrade Pew Pew Pew (propriété d'arme, répliquée pour le clic et l'icône)
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		const bool bActive = GetWeaponStat(UYSPewPewPewUpgrade::PewPewPewStat, 0.0f) > 0.0f;
		if (bActive != bPewPewPewActive)
		{
			bPewPewPewActive = bActive;
			GetOwner()->ForceNetUpdate();
		}
	}
}

int32 UYSPewPewPewAbility::GetFreeCycleCount() const
{
	return FMath::Max(1, FreeCycles + FMath::RoundToInt(GetWeaponStat(UYSPewPewPewUpgrade::ExtraCyclesStat, 0.0f)));
}

float UYSPewPewPewAbility::GetCyclesDuration(int32 From, int32 To) const
{
	float Duration = 0.0f;
	// l'écart baisse à chaque tir : somme des écarts des tirs de ces cycles
	const int32 ShotsPerCycle = UE_ARRAY_COUNT(YSPewPewPew::CardinalYaws);
	for (int32 Shot = From * ShotsPerCycle; Shot < To * ShotsPerCycle; ++Shot)
	{
		Duration += GetShotInterval(Shot);
	}
	return Duration;
}

bool UYSPewPewPewAbility::IsPewing() const
{
	return GetTimeSinceActivation() >= 0.0f && GetServerTime() < PewEndServerTime;
}

bool UYSPewPewPewAbility::IsActionInProgress() const
{
	return Super::IsActionInProgress() || IsPewing();
}

bool UYSPewPewPewAbility::IsBlockingLowerPriority() const
{
	return Super::IsBlockingLowerPriority() || IsPewing();
}

bool UYSPewPewPewAbility::GetLockedAimDirection(FVector& OutDirection) const
{
	if (!IsPewing())
	{
		return false;
	}
	OutDirection = FRotator(0.0f, LastShotYaw, 0.0f).Vector();
	return true;
}

bool UYSPewPewPewAbility::GetMinMoveSpeedRatio(float& OutRatio) const
{
	if (!IsPewing())
	{
		return false;
	}
	OutRatio = MinMoveSpeedRatio;
	return true;
}

void UYSPewPewPewAbility::ExecuteAbility(const FVector& TargetLocation)
{
	TotalCycles = GetFreeCycleCount();
	CycleIndex = 0;
	ShotInCycle = 0;
	ShotCount = 0;
	bDeathBlossom = GetWeaponStat(UYSPewPewPewUpgrade::DeathBlossomStat, 0.0f) > 0.0f;
	PewEndServerTime = GetServerTime() + GetCyclesDuration(0, TotalCycles);
	GetOwner()->ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("[YSPewPewPew] %s : %d cycle(s) gratuit(s)%s, fin à %.2f"), *GetOwner()->GetName(), TotalCycles, bDeathBlossom ? TEXT(" (Death Blossom)") : TEXT(""), PewEndServerTime);
	FireNextPewShot();
}

void UYSPewPewPewAbility::FireNextPewShot()
{
	AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (!Character || !Character->IsAlive() || Character->IsOverheated())
	{
		EndPew();
		return;
	}

	// au bout des cycles : un cycle de plus pour de la stamina, touche maintenue (pas avec Death Blossom)
	if (CycleIndex >= TotalCycles)
	{
		if (bDeathBlossom || !Character->IsAbilityHeld() || Character->GetStamina() < ExtraCycleStaminaCost)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[YSPewPewPew] %s : fin après %d cycle(s) (maintenu %d, stamina %.1f)"), *GetOwner()->GetName(), TotalCycles,
				Character->IsAbilityHeld(), Character->GetStamina());
			EndPew();
			return;
		}
		Character->ConsumeStamina(ExtraCycleStaminaCost);
		++TotalCycles;
		const float Extra = GetCyclesDuration(CycleIndex, CycleIndex + 1);
		PewEndServerTime += Extra;
		ReadyServerTime += Extra;
		GetOwner()->ForceNetUpdate();
		UE_LOG(LogTemp, Verbose, TEXT("[YSPewPewPew] %s : cycle %d pour %.0f stamina"), *GetOwner()->GetName(), TotalCycles, ExtraCycleStaminaCost);
	}

	// direction du tir (haut de l'écran hors bateau = nord), visée assistée autour
	// chaque cycle tourne de Cycle Yaw Offset par rapport au précédent (rotation progressive)
	const float Yaw = Character->GetDefaultCameraYaw() + YSPewPewPew::CardinalYaws[ShotInCycle] + CycleYawOffset * CycleIndex;
	FVector Direction = FRotator(0.0f, Yaw, 0.0f).Vector();
	const AYSProjectile* ProjectileCDO = ProjectileClass ? ProjectileClass->GetDefaultObject<AYSProjectile>() : nullptr;
	if (const AActor* Target = ProjectileCDO ? FindTargetInCone(GetWorld(), Character->GetActorLocation(), Direction, AssistAimDistance, CardinalAssistAngle, ProjectileCDO->GetHitTags(), Character) : nullptr)
	{
		const FVector ToTarget = (Target->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal2D();
		Direction = ToTarget.IsNearlyZero() ? Direction : ToTarget;
	}
	const int32 CircleLevel = bDeathBlossom ? FMath::Min(CycleIndex + 1, Character->GetCircleBulletMaxLevel()) : CircleBulletOnImpact;
	// le personnage se tourne vers le tir
	LastShotYaw = Direction.Rotation().Yaw;
	GetOwner()->ForceNetUpdate();
	if (UYSPewPewPewAbility* Shooter = GetPewShooter(ShotCount))
	{
		Shooter->FireInDirection(Direction, CircleLevel);
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSPewPewPew] %s : cycle %d, tir %d (yaw %.0f), CircleBullet %d à %.2f"), *GetOwner()->GetName(), CycleIndex + 1, ShotInCycle + 1,
		Direction.Rotation().Yaw, CircleLevel, GetWorld()->GetTimeSeconds());

	// tir suivant : l'écart baisse à chaque tir
	const float Delay = GetShotInterval(ShotCount);
	++ShotCount;
	if (++ShotInCycle >= UE_ARRAY_COUNT(YSPewPewPew::CardinalYaws))
	{
		ShotInCycle = 0;
		++CycleIndex;
	}
	GetWorld()->GetTimerManager().SetTimer(PewTimer, this, &UYSPewPewPewAbility::FireNextPewShot, FMath::Max(Delay, KINDA_SMALL_NUMBER), false);
}

void UYSPewPewPewAbility::EndPew()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PewTimer);
	}
	// fin plus tôt que prévu (surchauffe, mort) : la caméra et la vitesse reviennent tout de suite
	PewEndServerTime = FMath::Min(PewEndServerTime, GetServerTime());
	// rechargement des pistolets qui ont tiré
	UYSPewPewPewAbility* MainShooter = GetPewShooter(0);
	UYSPewPewPewAbility* OtherShooter = GetPewShooter(1);
	for (UYSPewPewPewAbility* Shooter : { MainShooter, OtherShooter != MainShooter ? OtherShooter : nullptr })
	{
		if (Shooter && Shooter->bPlayReloadAnim)
		{
			Shooter->MulticastReloadStarted();
		}
	}
	GetOwner()->ForceNetUpdate();
}

void UYSPewPewPewAbility::OnActionCancelled()
{
	Super::OnActionCancelled();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PewTimer);
	}
	PewEndServerTime = 0.0;
}

UYSPewPewPewAbility* UYSPewPewPewAbility::GetPewShooter(int32 ShotIndex) const
{
	// akimbo : main principale, autre main, main principale...
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (ShotIndex % 2 == 1 && Character && Character->IsAkimbo() && Character->GetMainHandWeapon() == GetOwner())
	{
		if (const AYSWeapon* OffWeapon = Character->GetOffHandWeapon())
		{
			if (UYSPewPewPewAbility* Partner = OffWeapon->FindComponentByClass<UYSPewPewPewAbility>())
			{
				return Partner;
			}
		}
	}
	return const_cast<UYSPewPewPewAbility*>(this);
}
