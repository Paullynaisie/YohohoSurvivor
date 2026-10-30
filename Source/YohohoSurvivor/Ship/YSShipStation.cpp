#include "Ship/YSShipStation.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/YSCombat.h"
#include "Core/YSDamageTypes.h"
#include "Core/YSGameplayTags.h"
#include "Core/YSGameState.h"
#include "Enemies/YSFoeCharacter.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "Ship/YSPirateShip.h"
#include "Enemies/YSFoeGalion.h"
#include "Summons/YSCaptainMinion.h"
#include "UI/YSHealthBarWidget.h"
#include "UObject/ConstructorHelpers.h"

AYSShipStation::AYSShipStation()
{
	// vérifie que l'occupant est toujours là (serveur), animations des postes
	PrimaryActorTick.bCanEverTick = true;
	bCarriable = false;
	DisplayName = NSLOCTEXT("YS", "ShipStationName", "Poste");

	OccupantPoint = CreateDefaultSubobject<USceneComponent>(TEXT("OccupantPoint"));
	OccupantPoint->SetupAttachment(GetRootComponent());

	ExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ExitPoint"));
	ExitPoint->SetupAttachment(GetRootComponent());
	ExitPoint->SetRelativeLocation(FVector(-120.0f, 0.0f, 0.0f));

	// du niveau de la mer (pont à +210) jusqu'au-dessus du poste ; sans effet sans Takes Damage (BeginPlay)
	ModuleHurtbox = CreateDefaultSubobject<UBoxComponent>(TEXT("ModuleHurtbox"));
	ModuleHurtbox->SetupAttachment(GetRootComponent());
	ModuleHurtbox->InitBoxExtent(FVector(120.0f, 120.0f, 250.0f));
	ModuleHurtbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ModuleHurtbox->SetGenerateOverlapEvents(false);
	ModuleHurtbox->SetCanEverAffectNavigation(false);

	ModuleHealthBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("ModuleHealthBar"));
	ModuleHealthBar->SetupAttachment(GetRootComponent());
	ModuleHealthBar->SetRelativeLocation(FVector(0.0f, 0.0f, 260.0f));
	ModuleHealthBar->SetWidgetSpace(EWidgetSpace::Screen);
	ModuleHealthBar->SetDrawAtDesiredSize(true);
	ModuleHealthBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FClassFinder<UUserWidget> HealthBarFinder(TEXT("/Game/Core/UI/WBP_FoeHealthBar"));
	ModuleHealthBar->SetWidgetClass(HealthBarFinder.Succeeded() ? HealthBarFinder.Class : TSubclassOf<UUserWidget>(UYSHealthBarWidget::StaticClass()));
}

void AYSShipStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSShipStation, Occupant);
	DOREPLIFETIME(AYSShipStation, Health);
	DOREPLIFETIME(AYSShipStation, FoeOccupant);
	DOREPLIFETIME(AYSShipStation, RepairEndServerTime);
}

void AYSShipStation::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		Health = MaxHealth;
		bHealthInitialized = true;
	}

	if (bTakesDamage)
	{
		YSCombat::ConfigureHurtbox(ModuleHurtbox);
		// la hurtbox ne suit que les requêtes de combat : aucun overlap entretenu
		ModuleHurtbox->SetGenerateOverlapEvents(false);
		ModuleHealthBar->InitWidget();
	}
	else
	{
		ModuleHealthBar->SetVisibility(false);
	}
}

void AYSShipStation::OnRep_Health()
{
	bHealthInitialized = true;
}

void AYSShipStation::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	const AYSPirateShip* Ship = GetShip();
	TagContainer.AddTag(Ship && Ship->IsFoeShip() ? YSTags::Team_Foe : YSTags::Team_Player);
}

float AYSShipStation::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	AYSPirateShip* Ship = GetShip();
	if (!HasAuthority() || !bTakesDamage || DamageAmount <= 0.0f || !Ship || Ship->IsActorBeingDestroyed())
	{
		return 0.0f;
	}
	const FVector ImpactLocation = DamageCauser ? DamageCauser->GetActorLocation() : GetActorLocation();

	// à 0 PV (ou bateau hors d'usage) : le core prend le coup, et le joueur qui répare aussi
	if (Health <= 0.0f || Ship->IsDisabled())
	{
		if (AYSPlayerCharacter* Repairer = IsRepairing() ? GetOccupant() : nullptr)
		{
			UE_LOG(LogTemp, Log, TEXT("%s %s : coup de %.0f, %s qui répare le prend aussi"), GetLogTag(), *GetName(), DamageAmount, *Repairer->GetName());
			Repairer->TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
		}
		Ship->TakeCoreDamage(DamageAmount, EventInstigator, ImpactLocation);
		return DamageAmount;
	}

	Health = FMath::Max(0.0f, Health - DamageAmount);
	AYSGameState::ReportDamage(this, DamageAmount, ImpactLocation, EventInstigator, UYSCriticalDamageType::IsCritical(DamageEvent));
	UE_LOG(LogTemp, Log, TEXT("%s %s : -%.0f -> %.0f / %.0f"), GetLogTag(), *GetName(), DamageAmount, Health, MaxHealth);
	ForceNetUpdate();

	// inutilisable : celui qui y était s'en va
	if (Health <= 0.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("%s %s : à 0 PV, inutilisable (E pour réparer)"), GetLogTag(), *GetName());
		if (Occupant)
		{
			Occupant->LeaveStation(TEXT("module à 0 PV"), true);
		}
		if (AYSFoeGalion* Galion = Cast<AYSFoeGalion>(Ship))
		{
			Galion->OnModuleBroken(this);
		}
	}
	return DamageAmount;
}

void AYSShipStation::DamageModulesInRadius(UWorld* World, const FVector& Center, float Radius, const FGameplayTagContainer& HitTags, float Damage, AController* EventInstigator)
{
	if (!World)
	{
		return;
	}
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius));
	TSet<AYSShipStation*> Damaged;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AYSShipStation* Station = Cast<AYSShipStation>(Overlap.GetActor());
		if (Station && !Damaged.Contains(Station) && Station->HasAnyMatchingGameplayTags(HitTags))
		{
			Damaged.Add(Station);
			Station->TakeDamage(Damage, FDamageEvent(), EventInstigator, nullptr);
		}
	}
}

bool AYSShipStation::IsUsable() const
{
	const AYSPirateShip* Ship = GetShip();
	return !IsBroken() && Ship && !Ship->IsDisabled();
}

bool AYSShipStation::IsUsableForRepair() const
{
	const AYSPirateShip* Ship = GetShip();
	return Ship && !Ship->IsDisabled();
}

void AYSShipStation::SetFoeOccupant(AYSFoeCharacter* NewOccupant)
{
	if (HasAuthority() && FoeOccupant != NewOccupant)
	{
		FoeOccupant = NewOccupant;
		SetPromptVisible(false);
		ForceNetUpdate();
	}
}

float AYSShipStation::GetRepairRemaining() const
{
	if (!IsRepairing())
	{
		return 0.0f;
	}
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	return FMath::Max(0.0f, static_cast<float>(RepairEndServerTime - Now));
}

bool AYSShipStation::GetActionIcon(UTexture2D*& OutIcon, FLinearColor& OutTint, float& OutRemaining, float& OutDuration) const
{
	if (!IsRepairing())
	{
		return false;
	}
	OutIcon = RepairIcon;
	OutTint = FLinearColor::White;
	OutRemaining = GetRepairRemaining();
	OutDuration = RepairDuration;
	return true;
}

void AYSShipStation::FinishRepair()
{
	Health = MaxHealth;
	RepairEndServerTime = 0.0;
	UE_LOG(LogTemp, Log, TEXT("%s %s : réparé par %s (%.0f PV)"), GetLogTag(), *GetName(), *GetNameSafe(Occupant), Health);
	ForceNetUpdate();
	if (Occupant)
	{
		Occupant->LeaveStation(TEXT("réparation terminée"), true);
	}
}

void AYSShipStation::UpdateModuleHealthBar()
{
	if (!bTakesDamage || !bHealthInitialized)
	{
		return;
	}
	UYSHealthBarWidget* Bar = Cast<UYSHealthBarWidget>(ModuleHealthBar->GetUserWidgetObject());
	if (!Bar)
	{
		return;
	}
	// réparation : la barre se remplit ; sinon PV du module (affichée dès qu'il est abîmé, à 0 PV comprise)
	const float RepairProgress = IsRepairing() && RepairDuration > 0.0f ? 1.0f - GetRepairRemaining() / RepairDuration : -1.0f;
	if (RepairProgress >= 0.0f)
	{
		if (!FMath::IsNearlyEqual(RepairProgress, DisplayedRepairProgress, 0.01f))
		{
			DisplayedRepairProgress = RepairProgress;
			Bar->SetHealth(RepairProgress * MaxHealth, MaxHealth, true);
		}
		return;
	}
	if (DisplayedRepairProgress >= 0.0f || !FMath::IsNearlyEqual(Health, DisplayedHealth))
	{
		DisplayedRepairProgress = -1.0f;
		DisplayedHealth = Health;
		Bar->SetHealth(Health, MaxHealth, Health <= 0.0f);
	}
}

void AYSShipStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// poste détruit (chaloupe) : l'occupant n'y est plus
	if (HasAuthority() && IsValid(Occupant) && Occupant->GetCurrentStation() == this)
	{
		Occupant->LeaveStation(TEXT("poste détruit"), true);
	}

	Super::EndPlay(EndPlayReason);
}

void AYSShipStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// occupant parti (déconnexion, changement de classe, mort) : le poste est libre
	if (HasAuthority() && Occupant && (!IsValid(Occupant) || !Occupant->IsAlive() || Occupant->GetCurrentStation() != this))
	{
		UE_LOG(LogTemp, Log, TEXT("%s %s : occupant parti, poste libre"), GetLogTag(), *GetName());
		SetOccupant(nullptr);
	}
	if (HasAuthority() && FoeOccupant && (!IsValid(FoeOccupant) || FoeOccupant->IsDying()))
	{
		UE_LOG(LogTemp, Log, TEXT("%s %s : NME au poste mort, poste libre"), GetLogTag(), *GetName());
		SetFoeOccupant(nullptr);
	}

	// réparation (serveur) ; le bateau hors d'usage l'arrête
	if (HasAuthority() && IsRepairing())
	{
		if (!Occupant || !IsUsableForRepair())
		{
			RepairEndServerTime = 0.0;
			ForceNetUpdate();
		}
		else if (GetRepairRemaining() <= 0.0f)
		{
			FinishRepair();
		}
	}

	if (bTakesDamage)
	{
		UpdateModuleHealthBar();
		const bool bBroken = IsBroken();
		if (bBroken != bAppliedBroken)
		{
			bAppliedBroken = bBroken;
			OnBrokenChanged(bBroken);
		}
	}
}

bool AYSShipStation::IsShipAvailable() const
{
	const AYSPirateShip* Ship = GetShip();
	return Ship && Ship->IsPresent() && !Ship->IsActorBeingDestroyed();
}

bool AYSShipStation::CanInteract(const AYSPlayerCharacter* Character) const
{
	const AYSPirateShip* Ship = GetShip();
	// bateau de NME : pas pour les joueurs ; bateau hors d'usage : plus rien ne marche ; module à 0 PV : un joueur peut le réparer
	if (!Ship || Ship->IsFoeShip() || Ship->IsDisabled() || FoeOccupant || (IsBroken() && Occupant))
	{
		return false;
	}
	return IsShipAvailable() && (!Occupant || CanTakeOverFrom(Character)) && Character && Character->IsAlive() && !Character->IsCarrying() && (bAllowSwimmingEntry || !Character->IsSwimming()) && !Character->IsAtStation();
}

bool AYSShipStation::CanTakeOverFrom(const AYSPlayerCharacter* Character) const
{
	return Occupant && Occupant->IsA<AYSCaptainMinion>() && Character && !Character->IsA<AYSCaptainMinion>();
}

void AYSShipStation::Interact(AYSPlayerCharacter* Character)
{
	if (!HasAuthority() || !CanInteract(Character))
	{
		return;
	}
	// place volée à un minion : il est reposé sur le côté et ira voir ailleurs
	if (AYSPlayerCharacter* Replaced = Occupant)
	{
		UE_LOG(LogTemp, Log, TEXT("%s %s prend la place de %s à %s"), GetLogTag(), *Character->GetName(), *Replaced->GetName(), *GetName());
		Replaced->LeaveStation(TEXT("place prise par un joueur"), true);
		Replaced->TeleportToFeetLocation(GetOccupantFeetLocation() + OccupantPoint->GetRightVector().GetSafeNormal2D() * ReplacedOccupantOffset, GetOccupantForward());
	}
	const bool bRepair = IsBroken();
	Character->EnterStation(this);
	if (bRepair && GetOccupant() == Character)
	{
		const AGameStateBase* GameState = GetWorld()->GetGameState();
		RepairEndServerTime = (GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds()) + RepairDuration;
		ForceNetUpdate();
		UE_LOG(LogTemp, Log, TEXT("%s %s : %s le répare (%.0f s)"), GetLogTag(), *GetName(), *Character->GetName(), RepairDuration);
	}
}

void AYSShipStation::SetOccupant(AYSPlayerCharacter* NewOccupant)
{
	if (!HasAuthority() || Occupant == NewOccupant)
	{
		return;
	}
	AYSPlayerCharacter* Previous = Occupant;
	Occupant = NewOccupant;
	// le réparateur est parti : à refaire depuis le début
	RepairEndServerTime = 0.0;
	SetPromptVisible(false);
	OnOccupantChanged(Previous);
	ForceNetUpdate();
}

void AYSShipStation::OnRep_Occupant()
{
	SetPromptVisible(false);
	OnOccupantChanged(nullptr);
}

AYSPirateShip* AYSShipStation::GetShip() const
{
	return AYSPirateShip::GetShipOf(this);
}

FVector AYSShipStation::GetOccupantFeetLocation() const
{
	return OccupantPoint->GetComponentLocation();
}

FVector AYSShipStation::GetOccupantForward() const
{
	return OccupantPoint->GetForwardVector().GetSafeNormal2D();
}

FVector AYSShipStation::GetExitFeetLocation() const
{
	return ExitPoint->GetComponentLocation();
}
