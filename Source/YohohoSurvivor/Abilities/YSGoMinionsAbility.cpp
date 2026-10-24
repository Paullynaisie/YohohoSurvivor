#include "Abilities/YSGoMinionsAbility.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "Summons/YSCaptainMinion.h"
#include "Upgrades/YSUpgrade.h"

UYSGoMinionsAbility::UYSGoMinionsAbility()
{
	PrimaryComponentTick.bCanEverTick = true;
	DisplayName = NSLOCTEXT("YS", "GoMinions", "Allez-y mes minions !");
}

void UYSGoMinionsAbility::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UYSGoMinionsAbility, bForcesPlayers);
}

void UYSGoMinionsAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// propriété d'arme (serveur seulement) recopiée pour le client
	if (GetOwner()->HasAuthority())
	{
		bForcesPlayers = GetWeaponStat(UYSAllMyMinionsUpgrade::AllMyMinionsStat, 0.0f) > 0.0f;
	}
}

void UYSGoMinionsAbility::GetForcedCharacters(TArray<AYSPlayerCharacter*>& OutCharacters) const
{
	const AYSPlayerCharacter* Captain = Cast<AYSPlayerCharacter>(GetHolderPawn());
	if (!Captain)
	{
		return;
	}
	for (TActorIterator<AYSPlayerCharacter> It(GetWorld()); It; ++It)
	{
		AYSPlayerCharacter* Character = *It;
		if (Character == Captain || !Character->IsAlive() || Character->IsActorBeingDestroyed())
		{
			continue;
		}
		const AYSCaptainMinion* Minion = Cast<AYSCaptainMinion>(Character);
		const bool bOwnMinion = Minion && Minion->GetCaptain() == Captain;
		// joueur (pas un minion) à portée, avec You're all my minions
		const bool bPlayerInRange = !Minion && bForcesPlayers && Character->GetPlayerState()
			&& FVector::Dist2D(Character->GetActorLocation(), Captain->GetActorLocation()) <= AllMyMinionsRange;
		if (bOwnMinion || bPlayerInRange)
		{
			OutCharacters.Add(Character);
		}
	}
}

bool UYSGoMinionsAbility::CanActivateNow() const
{
	TArray<AYSPlayerCharacter*> Characters;
	GetForcedCharacters(Characters);
	return Characters.Num() > 0;
}

void UYSGoMinionsAbility::OnHookStrike()
{
	FYSForcedActivation Forced;
	Forced.DamageMultiplier = 1.0f + GetWeaponStat(UYSAllMyMinionsUpgrade::ForcedDamageStat, 0.0f);
	Forced.bResetCooldown = GetWeaponStat(UYSAllMyMinionsUpgrade::ForcedCooldownResetStat, 0.0f) > 0.0f;

	TArray<AYSPlayerCharacter*> Characters;
	GetForcedCharacters(Characters);
	UE_LOG(LogTemp, Verbose, TEXT("[YSGoMinions] %s force %d Ability(s) (dégâts x %.2f%s)"), *GetOwner()->GetName(), Characters.Num(),
		Forced.DamageMultiplier, Forced.bResetCooldown ? TEXT(", recharge rendue") : TEXT(""));
	for (AYSPlayerCharacter* Character : Characters)
	{
		Character->ForceAbility(Forced);
	}
}
