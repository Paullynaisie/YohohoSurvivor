#include "Abilities/YSAutoBombThrowAbility.h"
#include "Core/YSGameplayTags.h"
#include "Engine/World.h"
#include "Player/YSPlayerCharacter.h"
#include "TimerManager.h"
#include "Weapons/YSWeapon.h"

UYSAutoBombThrowAbility::UYSAutoBombThrowAbility()
{
	DisplayName = NSLOCTEXT("YS", "AutoBombThrow", "BombThrow");
	Cooldown = 2.0f;
	BombDamage = 50.0f;
	AkimboFireDelay = 0.25f;
	// la Battle Bomb est déjà une bombe dans la main
	bShowHeldBomb = false;
}

void UYSAutoBombThrowAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// lancer automatique : serveur ; en akimbo l'autre main est lancée par la principale
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Character || !IsAutoAttackAllowedByHolder() || !IsReady() || !CanStartByPriority())
	{
		return;
	}
	if (Character->IsAkimbo() && Character->GetOffHandWeapon() == GetOwner())
	{
		return;
	}

	FVector Target;
	if (const AActor* Foe = FindAutoTarget())
	{
		Target = Foe->GetActorLocation();
	}
	else if (IsAutoAttackTargetOptional())
	{
		// clic gauche maintenu sans cible : droit devant, à la distance max
		Target = Character->GetActorLocation() + Character->GetActorForwardVector() * MaxThrowDistance;
	}
	else
	{
		return;
	}

	if (!ActivateOnServer(Target))
	{
		return;
	}
	if (UYSAutoBombThrowAbility* Partner = GetAkimboPartner())
	{
		TWeakObjectPtr<UYSAutoBombThrowAbility> WeakPartner = Partner;
		GetWorld()->GetTimerManager().SetTimer(AkimboTimer, FTimerDelegate::CreateWeakLambda(this, [WeakPartner, Target]()
		{
			if (WeakPartner.IsValid())
			{
				WeakPartner->ActivateOnServer(Target);
			}
		}), FMath::Max(AkimboFireDelay, KINDA_SMALL_NUMBER), false);
	}
}

bool UYSAutoBombThrowAbility::GetAutoAttackSlowdown(float& OutMultiplier) const
{
	const float Elapsed = GetTimeSinceActivation();
	if (Elapsed < 0.0f || Elapsed >= GetActionDuration())
	{
		return false;
	}
	OutMultiplier = AutoAtkMoveSpeedMultiplier;
	return true;
}

AActor* UYSAutoBombThrowAbility::FindAutoTarget() const
{
	const APawn* Holder = GetHolderPawn();
	return Holder ? FindTargetInCone(GetWorld(), Holder->GetActorLocation(), Holder->GetActorForwardVector(), MaxThrowDistance, AutoTargetAngle,
		FGameplayTagContainer(YSTags::Team_Foe), Holder) : nullptr;
}

UYSAutoBombThrowAbility* UYSAutoBombThrowAbility::GetAkimboPartner() const
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (!Character || !Character->IsAkimbo() || Character->GetMainHandWeapon() != GetOwner())
	{
		return nullptr;
	}
	const AYSWeapon* OffWeapon = Character->GetOffHandWeapon();
	return OffWeapon ? Cast<UYSAutoBombThrowAbility>(OffWeapon->GetAutoAction()) : nullptr;
}
