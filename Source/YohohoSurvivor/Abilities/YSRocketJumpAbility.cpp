#include "Abilities/YSRocketJumpAbility.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Weapons/YSPlayerBomb.h"

UYSRocketJumpAbility::UYSRocketJumpAbility()
{
	DisplayName = NSLOCTEXT("YS", "RocketJump", "Rocket jump");
	WeaponType = EYSWeaponType::Gun;
	Cooldown = 10.0f;
	// l'explosion lance le saut tout de suite
	StepBackDuration = 0.0f;
	bResetAutoAttackOnUse = false;
	bBumpElites = false;
	IconTint = FLinearColor(1.0f, 0.5f, 0.1f);
}

void UYSRocketJumpAbility::ExecuteAbility(const FVector& TargetLocation)
{
	ACharacter* Character = Cast<ACharacter>(GetHolderPawn());
	if (ExplosionClass && IsValid(Character))
	{
		// grosse explosion aux pieds (sol sous le personnage), immédiate
		const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		const FTransform SpawnTransform(FRotator::ZeroRotator, Feet);
		if (AYSPlayerBomb* Bomb = GetWorld()->SpawnActorDeferred<AYSPlayerBomb>(ExplosionClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Bomb->SetDamage(ExplosionDamage * GetDamageMultiplier());
			Bomb->SetInstant(false);
			Bomb->FinishSpawning(SpawnTransform);
			UE_LOG(LogTemp, Verbose, TEXT("[YSRocketJump] %s : explosion en (%.0f, %.0f, %.0f), %.0f dégâts"), *GetOwner()->GetName(), Feet.X, Feet.Y, Feet.Z, ExplosionDamage * GetDamageMultiplier());
		}
	}

	Super::ExecuteAbility(TargetLocation);
}

#if WITH_EDITOR
void UYSRocketJumpAbility::GetUnusedProperties(TArray<FName>& OutNames) const
{
	Super::GetUnusedProperties(OutNames);
	// pas de coupe : ni dégâts, ni hitbox, ni bump, ni poses de la main
	OutNames.Append({
		GET_MEMBER_NAME_CHECKED(UYSRocketJumpAbility, Damage),
		GET_MEMBER_NAME_CHECKED(UYSRocketJumpAbility, BumpDistance),
		GET_MEMBER_NAME_CHECKED(UYSRocketJumpAbility, BumpApexHeight),
		GET_MEMBER_NAME_CHECKED(UYSRocketJumpAbility, BumpDuration),
		GET_MEMBER_NAME_CHECKED(UYSRocketJumpAbility, BumpSpinTurns),
		GET_MEMBER_NAME_CHECKED(UYSRocketJumpAbility, bBumpElites),
		GET_MEMBER_NAME_CHECKED(UYSRocketJumpAbility, HitboxExtentMultiplier),
		GET_MEMBER_NAME_CHECKED(UYSRocketJumpAbility, JumpSwingEaseExponent),
	});
}
#endif
