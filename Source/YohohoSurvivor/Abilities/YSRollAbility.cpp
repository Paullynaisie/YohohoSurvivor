#include "Abilities/YSRollAbility.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"

UYSRollAbility::UYSRollAbility()
{
	DisplayName = NSLOCTEXT("YS", "Roll", "Roll");
	Cooldown = 0.0f;
	AkimboFireDelay = 0.0f;
	BombDamage = 50.0f;
}

void UYSRollAbility::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UYSRollAbility, RollCount, COND_OwnerOnly);
}

float UYSRollAbility::GetMoveActionStaminaCost() const
{
	// BunnyHop : la dernière roulade de chaque série est gratuite
	if (UsesRollUpgrades() && GetMoveActionGolden() == GoldenBunnyHop && BunnyHopInterval > 1 && (RollCount + 1) % BunnyHopInterval == 0)
	{
		return 0.0f;
	}
	return FMath::Max(0.0f, StaminaCost - (UsesRollUpgrades() ? GetMoveActionBonus() : 0.0f));
}

bool UYSRollAbility::CanActivateNow() const
{
	// Shoulder bump prolongé (Buldozer) : pas de nouvelle roulade avant la fin de la précédente
	const float SinceRoll = GetTimeSinceRoll();
	if (SinceRoll >= 0.0f && SinceRoll < GetRollDuration())
	{
		return false;
	}
	return CanCharacterUseMoveAction(Cast<AYSPlayerCharacter>(GetHolderPawn()), GetMoveActionStaminaCost());
}

bool UYSRollAbility::CanCharacterUseMoveAction(const AYSPlayerCharacter* Character, float Cost)
{
	return Character && !Character->IsCarrying() && !Character->IsSwimming() && !Character->IsAtStation() && Character->GetStamina() >= Cost;
}

void UYSRollAbility::OnActivationPredicted(const FVector& TargetLocation)
{
	Super::OnActivationPredicted(TargetLocation);
	if (AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn()))
	{
		Character->PredictStaminaCost(GetMoveActionStaminaCost());
	}
}

void UYSRollAbility::ExecuteAbility(const FVector& TargetLocation)
{
	AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	const float Cost = GetMoveActionStaminaCost();
	if (Character && Cost > 0.0f)
	{
		Character->ConsumeStamina(Cost);
	}
	++RollCount;

	// Golden
	switch (UsesRollUpgrades() ? GetMoveActionGolden() : INDEX_NONE)
	{
	case GoldenSleightOfHands:
		if (Character)
		{
			Character->ResetAbilityCooldowns();
		}
		break;
	case GoldenStuntsArtist:
		if (Character)
		{
			Character->SetNextAutoAttackCritical();
		}
		break;
	case GoldenLittleGift:
		// « Bomb and Roll » : bombe aux pieds, puis la roulade
		DropBomb();
		break;
	default:
		break;
	}

	StartRoll();
	MulticastRollStarted();
	UE_LOG(LogTemp, Verbose, TEXT("[YSRoll] %s roulade n°%d à %.2f (%.0f de stamina, Golden %d)"), *GetOwner()->GetName(), RollCount, GetWorld()->GetTimeSeconds(), Cost, GetMoveActionGolden());
}

void UYSRollAbility::OnActionCancelled()
{
	Super::OnActionCancelled();
	// activation prédite refusée par le serveur : la stamina prédite est rendue
	AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (Character && !Character->HasAuthority())
	{
		Character->ClearPredictedStamina();
	}
}

#if WITH_EDITOR
void UYSRollAbility::GetUnusedProperties(TArray<FName>& OutNames) const
{
	Super::GetUnusedProperties(OutNames);
	// pas d'arme ; la bombe ne sert qu'au Golden Little Gift (Bomb Class et Bomb Damage restent visibles)
	OutNames.Append({
		GET_MEMBER_NAME_CHECKED(UYSRollAbility, MineClass),
		GET_MEMBER_NAME_CHECKED(UYSRollAbility, SecondMineRollFraction),
		GET_MEMBER_NAME_CHECKED(UYSRollAbility, bAlwaysDropSecondBomb),
		GET_MEMBER_NAME_CHECKED(UYSRollAbility, AkimboFireDelay),
		GET_MEMBER_NAME_CHECKED(UYSRollAbility, bResetAutoAttackOnUse),
		GET_MEMBER_NAME_CHECKED(UYSRollAbility, AutoAttackResetDelay),
	});
}
#endif
