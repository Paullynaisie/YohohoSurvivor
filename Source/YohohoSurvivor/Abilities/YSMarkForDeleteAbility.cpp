#include "Abilities/YSMarkForDeleteAbility.h"
#include "Enemies/YSFoeCharacter.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "Upgrades/YSUpgrade.h"
#include "Weapons/YSProjectile.h"
#include "Weapons/YSWeapon.h"

UYSMarkForDeleteAbility::UYSMarkForDeleteAbility()
{
	DisplayName = NSLOCTEXT("YS", "MarkForDelete", "Dashing Slash");
	Damage = 180.0f;
}

void UYSMarkForDeleteAbility::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UYSMarkForDeleteAbility, bMarkForDelete);
}

UYSMarkForDeleteAbility::EPhase UYSMarkForDeleteAbility::GetSequencePhase(float Elapsed, int32& OutIndex, float& OutAlpha) const
{
	OutIndex = 0;
	OutAlpha = 0.0f;
	if (!bMarkForDelete || bCancelled || Elapsed < 0.0f || !IsMovementDriver())
	{
		return EPhase::None;
	}
	// 1. roulades et visées
	if (Elapsed < GetRollsEnd())
	{
		OutIndex = FMath::FloorToInt(Elapsed / GetRollCycle());
		const float InCycle = Elapsed - OutIndex * GetRollCycle();
		if (InCycle < RollDuration)
		{
			OutAlpha = InCycle / RollDuration;
			return EPhase::Roll;
		}
		OutAlpha = AimDuration > 0.0f ? (InCycle - RollDuration) / AimDuration : 1.0f;
		return EPhase::Aim;
	}
	// 2. pas en arrière
	if (Elapsed < GetDashesStart())
	{
		OutAlpha = StepBackDuration > 0.0f ? (Elapsed - GetRollsEnd()) / StepBackDuration : 1.0f;
		return EPhase::StepBack;
	}
	// 3. dashes (nombre et directions donnés par le serveur)
	const float DashTime = GetDashPhaseDuration();
	if (DashCount < 0)
	{
		return Elapsed < GetMaxSequenceDuration() ? EPhase::Waiting : EPhase::None;
	}
	const float SinceDashes = Elapsed - GetDashesStart();
	OutIndex = FMath::FloorToInt(SinceDashes / DashTime);
	if (OutIndex < DashCount)
	{
		OutAlpha = (SinceDashes - OutIndex * DashTime) / DashTime;
		return DashDirections.IsValidIndex(OutIndex) && !DashDirections[OutIndex].IsNearlyZero() ? EPhase::Dash : EPhase::Waiting;
	}
	// retour de la coupe après le dernier dash
	const float SequenceEnd = GetDashesStart() + DashCount * DashTime;
	OutAlpha = Elapsed - SequenceEnd;
	return DashCount > 0 && Elapsed < SequenceEnd + GetRecoverTime() ? EPhase::Recover : EPhase::None;
}

void UYSMarkForDeleteAbility::ResetSequence()
{
	Targets.Reset();
	DashCount = -1;
	DashDirections.Reset();
	RollsStarted = 0;
	ShotsFired = 0;
	DashesStarted = 0;
	bStepBackStarted = false;
	bStepBackFinished = false;
	bCancelled = false;
	ForcedRollDirection = FVector::ZeroVector;
}

float UYSMarkForDeleteAbility::GetCooldownDuration() const
{
	// recharge comptée après la durée maximale de l'enchaînement (Shot Count dashes)
	return bMarkForDelete ? GetMaxSequenceDuration() + MarkForDeleteCooldown * GetCooldownMultiplier() : Super::GetCooldownDuration();
}

float UYSMarkForDeleteAbility::GetBaseCooldown() const
{
	return bMarkForDelete ? MarkForDeleteCooldown : Super::GetBaseCooldown();
}

float UYSMarkForDeleteAbility::GetActionDuration() const
{
	if (!bMarkForDelete)
	{
		return Super::GetActionDuration();
	}
	const int32 Dashes = DashCount >= 0 ? DashCount : ShotCount;
	return GetDashesStart() + Dashes * GetDashPhaseDuration() + (Dashes > 0 ? GetRecoverTime() : 0.0f);
}

float UYSMarkForDeleteAbility::GetSwingElapsed() const
{
	if (!bMarkForDelete)
	{
		return Super::GetSwingElapsed();
	}
	// animation de la coupe : préparation pendant le pas en arrière, une coupe par dash, retour après le dernier
	int32 Index;
	float Alpha;
	switch (GetSequencePhase(GetTimeSinceActivation(), Index, Alpha))
	{
	case EPhase::StepBack:	return Alpha * StepBackDuration;
	case EPhase::Waiting:	return FMath::Max(0.0f, StepBackDuration - KINDA_SMALL_NUMBER);
	case EPhase::Dash:		return StepBackDuration + Alpha * GetDashPhaseDuration();
	case EPhase::Recover:	return StepBackDuration + GetDashPhaseDuration() + Alpha;
	default:				return -1.0f;
	}
}

bool UYSMarkForDeleteAbility::IsGrantingInvulnerability() const
{
	if (!bMarkForDelete)
	{
		return Super::IsGrantingInvulnerability();
	}
	int32 Index;
	float Alpha;
	const EPhase Phase = GetSequencePhase(GetTimeSinceActivation(), Index, Alpha);
	return Phase == EPhase::Roll || (Phase == EPhase::Dash && bDashInvulnerable);
}

float UYSMarkForDeleteAbility::GetMoveSpeedMultiplier() const
{
	if (!bMarkForDelete)
	{
		return Super::GetMoveSpeedMultiplier();
	}
	int32 Index;
	float Alpha;
	switch (GetSequencePhase(GetTimeSinceActivation(), Index, Alpha))
	{
	case EPhase::Roll:		return FMath::Lerp(RollSpeedMultiplier, AimMoveSpeedMultiplier, Alpha);
	case EPhase::Aim:		return AimMoveSpeedMultiplier;
	case EPhase::StepBack:	return FMath::Lerp(StepBackSpeedMultiplier, StepBackEndSpeedMultiplier, FMath::InterpEaseOut(0.0f, 1.0f, Alpha, StepBackDecayExponent));
	case EPhase::Dash:		return FMath::Lerp(DashSpeedMultiplier, DashEndSpeedMultiplier, FMath::InterpEaseOut(0.0f, 1.0f, Alpha, DashDecayExponent));
	case EPhase::Waiting:	return 0.0f;
	default:				return 1.0f;
	}
}

bool UYSMarkForDeleteAbility::GetForcedMoveDirection(FVector& OutDirection) const
{
	if (!bMarkForDelete)
	{
		return Super::GetForcedMoveDirection(OutDirection);
	}
	int32 Index;
	float Alpha;
	switch (GetSequencePhase(GetTimeSinceActivation(), Index, Alpha))
	{
	case EPhase::Roll:
		OutDirection = ForcedRollDirection;
		return !ForcedRollDirection.IsNearlyZero();
	case EPhase::StepBack:
		OutDirection = -LockedDirection;
		return true;
	case EPhase::Dash:
		OutDirection = LockedDirection;
		return true;
	default:
		return false;
	}
}

bool UYSMarkForDeleteAbility::OverridesMoveInput() const
{
	if (!bMarkForDelete)
	{
		return Super::OverridesMoveInput();
	}
	int32 Index;
	float Alpha;
	const EPhase Phase = GetSequencePhase(GetTimeSinceActivation(), Index, Alpha);
	return Phase == EPhase::StepBack || Phase == EPhase::Waiting || Phase == EPhase::Dash;
}

bool UYSMarkForDeleteAbility::GetLockedAimDirection(FVector& OutDirection) const
{
	if (!bMarkForDelete)
	{
		return Super::GetLockedAimDirection(OutDirection);
	}
	int32 Index;
	float Alpha;
	const EPhase Phase = GetSequencePhase(GetTimeSinceActivation(), Index, Alpha);
	if (Phase == EPhase::None || Phase == EPhase::Roll || Phase == EPhase::Aim)
	{
		return false;
	}
	OutDirection = LockedDirection;
	return true;
}

bool UYSMarkForDeleteAbility::GetBodyRoll(float& OutAngle, float& OutHeightOffset) const
{
	if (!bMarkForDelete)
	{
		return Super::GetBodyRoll(OutAngle, OutHeightOffset);
	}
	int32 Index;
	float Alpha;
	if (GetSequencePhase(GetTimeSinceActivation(), Index, Alpha) != EPhase::Roll)
	{
		return false;
	}
	OutAngle = 360.0f * RollAnimTurns * FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, RollAnimEaseExponent);
	OutHeightOffset = -RollAnimTuckHeight * FMath::Sin(PI * Alpha);
	return true;
}

void UYSMarkForDeleteAbility::OnActivationPredicted(const FVector& TargetLocation)
{
	if (!bMarkForDelete)
	{
		Super::OnActivationPredicted(TargetLocation);
		return;
	}
	ResetSequence();
}

void UYSMarkForDeleteAbility::ExecuteAbility(const FVector& TargetLocation)
{
	if (!bMarkForDelete)
	{
		Super::ExecuteAbility(TargetLocation);
		return;
	}
	ResetSequence();
	LockDirection(TargetLocation);
	ResetSwingHits();
	StartCriticalTracking(false, false);
	UE_LOG(LogTemp, Verbose, TEXT("[YSMarkForDelete] %s lancé à %.2f (%d tirs)"), *GetOwner()->GetName(), GetWorld()->GetTimeSeconds(), ShotCount);
}

void UYSMarkForDeleteAbility::OnActionCancelled()
{
	Super::OnActionCancelled();
	if (bMarkForDelete)
	{
		bCancelled = true;
		UE_LOG(LogTemp, Verbose, TEXT("[YSMarkForDelete] %s interrompu"), *GetOwner()->GetName());
	}
}

void UYSMarkForDeleteAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Golden Mark for Delete (propriété d'arme, serveur) répliquée
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bMarkForDelete = GetWeaponStat(UYSGunWasBetterUpgrade::MarkForDeleteStat, 0.0f) > 0.0f;
	}
	if (!bMarkForDelete)
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
		return;
	}

	// coupe (hitbox et dégâts côté serveur, traînée) sans le mouvement du Dashing Slash : celui de l'enchaînement à la place
	UYSAutoSwingAbility::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* Holder = GetHolderPawn();
	if (!Holder)
	{
		return;
	}
	const bool bAuthority = GetOwner()->HasAuthority();
	const float Elapsed = GetTimeSinceActivation();
	// nouvelle activation (toutes les machines) : enchaînement remis à zéro
	if (Elapsed < LastElapsed)
	{
		ResetSequence();
	}
	LastElapsed = Elapsed;
	if (bCancelled)
	{
		SetPassThroughPawns(false);
		return;
	}
	const bool bDriver = IsMovementDriver() && (bAuthority || Holder->IsLocallyControlled());

	// roulades (machines qui pilotent le mouvement)
	while (RollsStarted < ShotCount && Elapsed >= RollsStarted * GetRollCycle() && Elapsed < GetRollsEnd())
	{
		if (bDriver)
		{
			StartRoll(RollsStarted);
		}
		++RollsStarted;
	}
	// tirs (serveur), à la fin de chaque visée
	while (bAuthority && ShotsFired < ShotCount && Elapsed >= (ShotsFired + 1) * GetRollCycle() && Elapsed < GetMaxSequenceDuration())
	{
		FireShot(ShotsFired++);
	}
	// pas en arrière, à l'opposé du regard
	if (!bStepBackStarted && Elapsed >= GetRollsEnd() && Elapsed < GetDashesStart())
	{
		bStepBackStarted = true;
		LockedDirection = Holder->GetActorForwardVector().GetSafeNormal2D();
		if (bDriver)
		{
			ApplyPhaseKick(0);
		}
	}
	// fin du pas en arrière (serveur) : liste figée, puis un dash par NME
	if (bAuthority && !bStepBackFinished && Elapsed >= GetDashesStart() && Elapsed < GetMaxSequenceDuration())
	{
		bStepBackFinished = true;
		FinishStepBack();
	}
	while (bAuthority && bStepBackFinished && DashesStarted < DashCount && Elapsed >= GetDashesStart() + DashesStarted * GetDashPhaseDuration())
	{
		StartDash(DashesStarted++);
	}

	int32 Index;
	float Alpha;
	const EPhase Phase = GetSequencePhase(Elapsed, Index, Alpha);
	SetPassThroughPawns(bDriver && (Phase == EPhase::Roll || (Phase == EPhase::Dash && bDashThroughFoes)));
}

void UYSMarkForDeleteAbility::StartRoll(int32 Index)
{
	AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (!Character)
	{
		return;
	}
	// direction des touches, sinon à l'opposé du curseur (du regard)
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const FVector NoInputDirection = -Character->GetActorForwardVector().GetSafeNormal2D();
	const bool bNoInput = Movement->GetCurrentAcceleration().IsNearlyZero();
	const FVector Direction = bNoInput ? NoInputDirection : Movement->GetCurrentAcceleration().GetSafeNormal2D();
	ForcedRollDirection = bNoInput ? NoInputDirection : FVector::ZeroVector;
	const float Speed = Character->GetUnmodifiedWalkSpeed() * RollSpeedMultiplier;
	Movement->Velocity = Direction * Speed + FVector(0.0f, 0.0f, Movement->Velocity.Z);
	Movement->MaxWalkSpeed = Speed;
	UE_LOG(LogTemp, Verbose, TEXT("[YSMarkForDelete] %s (%s) : roulade %d vers (%.2f, %.2f)"), *GetOwner()->GetName(), GetOwner()->HasAuthority() ? TEXT("serveur") : TEXT("client"),
		Index + 1, Direction.X, Direction.Y);
}

void UYSMarkForDeleteAbility::FireShot(int32 Index)
{
	AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	UWorld* World = GetWorld();
	if (!Character || !World || !ShotProjectileClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSMarkForDelete] %s : tir %d impossible (Shot Projectile Class vide ?)"), *GetOwner()->GetName(), Index + 1);
		return;
	}
	// depuis la bouche de l'arme de l'autre main (le Pistolet), droit devant avec visée assistée
	const AYSWeapon* OffHand = Character->GetOffHandWeapon();
	const FVector Muzzle = OffHand && OffHand != GetOwner() ? OffHand->GetMuzzleLocation() : Character->GetActorLocation();
	const FVector Forward = Character->GetActorForwardVector().GetSafeNormal2D();
	FVector Direction = Forward;
	if (const AActor* Target = FindTargetInCone(World, Character->GetActorLocation(), Forward, ShotAssistAimDistance, ShotAssistAimAngle, HitTags, Character))
	{
		const FVector ToTarget = (Target->GetActorLocation() - Muzzle).GetSafeNormal2D();
		Direction = ToTarget.IsNearlyZero() ? Forward : ToTarget;
	}

	const FTransform SpawnTransform(Direction.Rotation(), Muzzle);
	AYSProjectile* Projectile = World->SpawnActorDeferred<AYSProjectile>(ShotProjectileClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		return;
	}
	Projectile->SetDamage(ShotDamage * GetDamageMultiplier());
	Projectile->SetMaxHits(1);
	Projectile->OnFoeHit.AddUObject(this, &UYSMarkForDeleteAbility::OnShotHitFoe);
	Projectile->FinishSpawning(SpawnTransform);
	UE_LOG(LogTemp, Verbose, TEXT("[YSMarkForDelete] %s : tir %d à %.2f, %.1f dégâts, vers (%.2f, %.2f)"), *GetOwner()->GetName(), Index + 1, World->GetTimeSeconds(),
		ShotDamage * GetDamageMultiplier(), Direction.X, Direction.Y);
}

void UYSMarkForDeleteAbility::OnShotHitFoe(AYSProjectile* Projectile, AYSFoeCharacter* Foe, bool bKilled)
{
	// liste figée à la fin du pas en arrière ; un même NME peut y être plusieurs fois (un dash par balle qui l'a touché) ;
	// tué : l'endroit de sa mort ; une seule entrée par balle (elle ne touche qu'un NME)
	if (!Foe || bStepBackFinished || Targets.Num() >= ShotCount)
	{
		return;
	}
	FTarget& Entry = Targets.AddDefaulted_GetRef();
	Entry.Actor = Foe;
	Entry.Location = Foe->GetActorLocation();
	UE_LOG(LogTemp, Verbose, TEXT("[YSMarkForDelete] %s : NME %d = %s%s"), *GetOwner()->GetName(), Targets.Num(), *Foe->GetName(), bKilled ? TEXT(" (tué : endroit de sa mort)") : TEXT(""));
}

void UYSMarkForDeleteAbility::FinishStepBack()
{
	if (AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn()))
	{
		// Blood Swords (Golden du Bloodlust)
		Character->OnAbilityStepBackFinished();
	}
	DashCount = Targets.Num();
	UE_LOG(LogTemp, Verbose, TEXT("[YSMarkForDelete] %s : fin du pas en arrière, %d dash(es)"), *GetOwner()->GetName(), DashCount);
	if (DashCount == 0)
	{
		MulticastDash(INDEX_NONE, 0, FVector::ZeroVector);
	}
}

void UYSMarkForDeleteAbility::StartDash(int32 Index)
{
	const APawn* Holder = GetHolderPawn();
	if (!Holder || !Targets.IsValidIndex(Index))
	{
		return;
	}
	// vers le NME s'il est encore en vie, sinon vers l'endroit de sa mort
	const FTarget& Target = Targets[Index];
	const AYSFoeCharacter* Foe = Cast<AYSFoeCharacter>(Target.Actor.Get());
	const bool bAlive = Foe && Foe->GetHealth() > 0.0f && !Foe->IsActorBeingDestroyed();
	const FVector Destination = bAlive ? Foe->GetActorLocation() : Target.Location;
	FVector Direction = (Destination - Holder->GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Holder->GetActorForwardVector().GetSafeNormal2D();
	}
	if (DashDirections.Num() <= Index)
	{
		DashDirections.SetNum(Index + 1);
	}
	DashDirections[Index] = Direction;
	LockedDirection = Direction;
	// chaque dash est une nouvelle coupe : les NME peuvent être retouchés
	ResetSwingHits();
	ApplyPhaseKick(1);
	MulticastDash(Index, DashCount, Direction);
	UE_LOG(LogTemp, Verbose, TEXT("[YSMarkForDelete] %s : dash %d / %d vers %s (%.0f, %.0f)%s"), *GetOwner()->GetName(), Index + 1, DashCount,
		Foe ? *Foe->GetName() : TEXT("l'endroit d'une mort"), Destination.X, Destination.Y, bAlive ? TEXT("") : TEXT(", NME mort"));
}

void UYSMarkForDeleteAbility::MulticastDash_Implementation(int32 Index, int32 Count, FVector_NetQuantizeNormal Direction)
{
	// serveur : déjà fait ; clients : nombre de dashes et direction (le client qui contrôle donne l'élan)
	if (GetOwner()->HasAuthority())
	{
		return;
	}
	DashCount = Count;
	if (Index < 0)
	{
		return;
	}
	if (DashDirections.Num() <= Index)
	{
		DashDirections.SetNum(Index + 1);
	}
	DashDirections[Index] = Direction;
	LockedDirection = Direction;
	const APawn* Holder = GetHolderPawn();
	if (Holder && Holder->IsLocallyControlled())
	{
		ApplyPhaseKick(1);
	}
}
