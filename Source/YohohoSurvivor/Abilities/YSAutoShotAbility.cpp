#include "Abilities/YSAutoShotAbility.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Player/YSPlayerCharacter.h"
#include "Weapons/YSWeapon.h"

UYSAutoShotAbility::UYSAutoShotAbility()
{
	PrimaryComponentTick.bCanEverTick = true;
	DisplayName = NSLOCTEXT("YS", "AutoShot", "Auto shot");
	Damage = 20.0f;
	Cooldown = 1.0f;
	BulletsPerBurst = 3;
	BurstDuration = 1.0f;
}

void UYSAutoShotAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// tir automatique : serveur uniquement ; en akimbo l'autre main tire dans la rafale de la principale
	// joueur : seulement en maintenant le clic gauche (une rafale commencée va jusqu'au bout)
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Character || !IsAutoAttackAllowedByHolder() || !IsReady() || !CanStartByPriority())
	{
		return;
	}
	if (Character->IsAkimbo() && Character->GetOffHandWeapon() == GetOwner())
	{
		return;
	}

	if (const AActor* Target = FindAutoTarget())
	{
		ActivateOnServer(Target->GetActorLocation());
	}
	else if (IsAutoAttackTargetOptional())
	{
		// clic gauche maintenu sans cible : droit devant (le PJ regarde le curseur)
		ActivateOnServer(Character->GetActorLocation() + Character->GetActorForwardVector() * 1000.0f);
	}
}

bool UYSAutoShotAbility::GetAutoAttackSlowdown(float& OutMultiplier) const
{
	if (!IsBurstInProgressLocally())
	{
		return false;
	}
	OutMultiplier = AutoAtkMoveSpeedMultiplier;
	return true;
}

AActor* UYSAutoShotAbility::FindAutoTarget() const
{
	const APawn* Holder = GetHolderPawn();
	return Holder ? FindAssistTarget(Holder->GetActorLocation(), Holder->GetActorForwardVector()) : nullptr;
}

FVector UYSAutoShotAbility::GetBurstTargetLocation(int32 BulletIndex) const
{
	if (const AActor* Target = FindAutoTarget())
	{
		return Target->GetActorLocation();
	}
	const APawn* Holder = GetHolderPawn();
	return Holder ? Holder->GetActorLocation() + Holder->GetActorForwardVector() * 1000.0f : Super::GetBurstTargetLocation(BulletIndex);
}

UYSCoupDPetoirAbility* UYSAutoShotAbility::GetBulletShooter(int32 BulletIndex) const
{
	UYSAutoShotAbility* Partner = GetAkimboPartner();
	return Partner && BulletIndex % 2 == 1 ? Partner : const_cast<UYSAutoShotAbility*>(this);
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
