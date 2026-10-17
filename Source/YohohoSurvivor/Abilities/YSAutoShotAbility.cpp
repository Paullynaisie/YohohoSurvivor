#include "Abilities/YSAutoShotAbility.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Player/YSPlayerCharacter.h"
#include "TimerManager.h"
#include "Weapons/YSWeapon.h"

UYSAutoShotAbility::UYSAutoShotAbility()
{
	PrimaryComponentTick.bCanEverTick = true;
	DisplayName = NSLOCTEXT("YS", "AutoShot", "Auto shot");
	Damage = 20.0f;
	Cooldown = 1.0f;
}

void UYSAutoShotAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// tir automatique : serveur uniquement ; en akimbo l'autre main est déclenchée par la principale
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Character || !IsReady() || !CanStartByPriority() || !FindAutoTarget())
	{
		return;
	}
	if (Character->IsAkimbo() && Character->GetOffHandWeapon() == GetOwner())
	{
		return;
	}

	FireAtTarget();

	if (UYSAutoShotAbility* Partner = GetAkimboPartner())
	{
		TWeakObjectPtr<UYSAutoShotAbility> WeakPartner = Partner;
		GetWorld()->GetTimerManager().SetTimer(AkimboTimer, FTimerDelegate::CreateWeakLambda(this, [WeakPartner]()
		{
			if (WeakPartner.IsValid())
			{
				WeakPartner->FireAtTarget();
			}
		}), FMath::Max(AkimboFireDelay, KINDA_SMALL_NUMBER), false);
	}
}

AActor* UYSAutoShotAbility::FindAutoTarget() const
{
	const APawn* Holder = GetHolderPawn();
	return Holder ? FindAssistTarget(Holder->GetActorLocation(), Holder->GetActorForwardVector()) : nullptr;
}

void UYSAutoShotAbility::FireAtTarget()
{
	// sans cible (elle est morte entre-temps) : tir droit devant
	const APawn* Holder = GetHolderPawn();
	const AActor* Target = FindAutoTarget();
	if (Holder)
	{
		ActivateOnServer(Target ? Target->GetActorLocation() : Holder->GetActorLocation() + Holder->GetActorForwardVector() * 1000.0f);
	}
}

UYSAutoShotAbility* UYSAutoShotAbility::GetAkimboPartner() const
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (!Character || !Character->IsAkimbo() || Character->GetMainHandWeapon() != GetOwner())
	{
		return nullptr;
	}
	const AYSWeapon* OffWeapon = Character->GetOffHandWeapon();
	return OffWeapon ? Cast<UYSAutoShotAbility>(OffWeapon->GetAutoAction()) : nullptr;
}
