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
#include "Ship/YSShipCore.h"
#include "Ship/YSShipHelm.h"
#include "Ship/YSCombatPost.h"
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
	DOREPLIFETIME(AYSShipStation, HostReceptacle);
	DOREPLIFETIME(AYSShipStation, DismountEndServerTime);
	DOREPLIFETIME(AYSShipStation, Health);
	DOREPLIFETIME(AYSShipStation, MaxHealth);
	DOREPLIFETIME(AYSShipStation, FoeOccupant);
	DOREPLIFETIME(AYSShipStation, Work);
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
	// poste d'un bateau de NME : dégâts d'un personnage (Player Dmg Multiplier) ou d'un poste du bateau des joueurs (Boat Dmg Multiplier)
	if (Ship->IsFoeShip())
	{
		const float SourceMultiplier = YSCombat::GetDamageTakenMultiplier(EventInstigator, PlayerDmgMultiplier, BoatDmgMultiplier);
		if (SourceMultiplier != 1.0f)
		{
			UE_LOG(LogTemp, Verbose, TEXT("%s %s : coup de %.1f x%.2f (%s)"), GetLogTag(), *GetName(), DamageAmount, SourceMultiplier, YSCombat::IsBoatDamage() ? TEXT("bateau") : TEXT("personnage"));
			DamageAmount = FMath::RoundToFloat(DamageAmount * SourceMultiplier);
			if (DamageAmount <= 0.0f)
			{
				return 0.0f;
			}
		}
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
		// (le joueur au poste y reste : il peut le réparer avec Espace)
		UE_LOG(LogTemp, Log, TEXT("%s %s : à 0 PV, inutilisable (Espace maintenu au poste pour réparer)"), GetLogTag(), *GetName());
		if (Occupant && Occupant->IsA<AYSCaptainMinion>())
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
	TSet<AYSShipCore*> DamagedCores;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AYSShipStation* Station = Cast<AYSShipStation>(Overlap.GetActor());
		if (Station && !Damaged.Contains(Station) && Station->HasAnyMatchingGameplayTags(HitTags))
		{
			Damaged.Add(Station);
			Station->TakeDamage(Damage, FDamageEvent(), EventInstigator, nullptr);
		}
		// core du bateau des joueurs (explosion d'un NME)
		AYSShipCore* Core = Cast<AYSShipCore>(Overlap.GetActor());
		if (Core && !DamagedCores.Contains(Core) && Core->HasAnyMatchingGameplayTags(HitTags))
		{
			DamagedCores.Add(Core);
			Core->TakeDamage(Damage, FDamageEvent(), EventInstigator, nullptr);
		}
	}
}

bool AYSShipStation::IsUsable() const
{
	const AYSPirateShip* Ship = GetShip();
	return !IsBroken() && Ship && !Ship->IsDisabled();
}

void AYSShipStation::RestoreFullHealth()
{
	if (!HasAuthority() || !bTakesDamage || MaxHealth <= 0.0f || Health >= MaxHealth)
	{
		return;
	}
	Health = MaxHealth;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("%s %s : PV remis au max (%.0f)"), GetLogTag(), *GetName(), MaxHealth);
}

void AYSShipStation::AddRepairHealth(float Amount)
{
	if (!HasAuthority() || !bTakesDamage || MaxHealth <= 0.0f || Health >= MaxHealth || Amount <= 0.0f || !IsUsableForRepair())
	{
		return;
	}
	const bool bWasBroken = Health <= 0.0f;
	Health = FMath::Min(MaxHealth, Health + Amount);
	ForceNetUpdate();
	if (bWasBroken)
	{
		UE_LOG(LogTemp, Verbose, TEXT("%s %s : réparé par l'équipage, de nouveau utilisable"), GetLogTag(), *GetName());
	}
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
	return MaxHealth > 0.0f ? FMath::Max(0.0f, (MaxHealth - Health) / MaxHealth * GetFullRepairDuration()) : 0.0f;
}

void AYSShipStation::GetStationActionIcons(FYSStationActionIcon& OutLeft, FYSStationActionIcon& OutRight, FYSStationActionIcon& OutSpace) const
{
	// clic droit : démontage d'un poste de combat posé (barre de progression pendant le démontage), sinon l'icône du poste
	if (HostReceptacle && CombatPostItemClass)
	{
		OutRight.bShow = true;
		OutRight.Icon = DismountIcon;
		OutRight.Name = DismountActionName;
		OutRight.bBlocked = IsBroken() || IsRepairing();
		if (IsBeingDismounted())
		{
			const AGameStateBase* GameState = GetWorld()->GetGameState();
			const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
			OutRight.Duration = HostReceptacle->GetDismountHoldDuration();
			OutRight.Remaining = FMath::Max(0.0f, static_cast<float>(DismountEndServerTime - Now));
		}
	}
	else
	{
		OutRight.bShow = GetActionIcon(OutRight.Icon, OutRight.Tint, OutRight.Remaining, OutRight.Duration);
	}
	// Espace : réparation (secondes pour finir pendant qu'on répare ; grisée aux PV max ou pendant un démontage)
	if (bTakesDamage)
	{
		OutSpace.bShow = true;
		OutSpace.Icon = RepairIcon;
		OutSpace.Name = RepairActionName;
		OutSpace.bBlocked = Health >= MaxHealth || IsBeingDismounted();
		if (IsRepairing())
		{
			OutSpace.Remaining = GetRepairRemaining();
			OutSpace.Duration = GetFullRepairDuration();
		}
	}
}

bool AYSShipStation::GetActionIcon(UTexture2D*& OutIcon, FLinearColor& OutTint, float& OutRemaining, float& OutDuration) const
{
	// (la réparation a son icône à Espace : GetStationActionIcons)
	return false;
}

void AYSShipStation::UpdateWork(float DeltaSeconds)
{
	// minions : jamais de touche maintenue
	AYSPlayerCharacter* User = Occupant && !Occupant->IsA<AYSCaptainMinion>() ? Occupant.Get() : nullptr;
	const uint8 Held = User ? User->GetStationHeldInputs() : 0;
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const bool bCanRepair = bTakesDamage && MaxHealth > 0.0f && Health < MaxHealth && IsUsableForRepair();
	const bool bCanDismount = User && HostReceptacle && HostReceptacle->CanDismount(User);

	switch (Work)
	{
	case EYSStationWork::None:
		// la touche déjà maintenue en arrivant au poste compte aussi ; démontage d'abord (choix de Claude), une seule chose à la fois
		if ((Held & YSStationInput::Right) && bCanDismount)
		{
			Work = EYSStationWork::Dismount;
			DismountEndServerTime = Now + HostReceptacle->GetDismountHoldDuration();
			ForceNetUpdate();
			UE_LOG(LogTemp, Log, TEXT("[YSCombatPost] %s commence à démonter %s (clic droit maintenu %.1f s)"), *User->GetName(), *GetName(), HostReceptacle->GetDismountHoldDuration());
		}
		else if ((Held & YSStationInput::Space) && bCanRepair)
		{
			Work = EYSStationWork::Repair;
			ForceNetUpdate();
			UE_LOG(LogTemp, Log, TEXT("%s %s : %s le répare (%.0f / %.0f PV)"), GetLogTag(), *GetName(), *User->GetName(), Health, MaxHealth);
		}
		break;

	case EYSStationWork::Repair:
		if (!(Held & YSStationInput::Space))
		{
			StopWork(TEXT("Espace relâché"));
		}
		else if (!bCanRepair)
		{
			StopWork(Health >= MaxHealth ? TEXT("PV au max") : TEXT("plus réparable"));
		}
		else
		{
			Health = FMath::Min(MaxHealth, Health + MaxHealth * RepairPercentPerSecond / 100.0f * DeltaSeconds);
			ForceNetUpdate();
			if (Health >= MaxHealth)
			{
				StopWork(TEXT("PV au max"));
			}
		}
		break;

	case EYSStationWork::Dismount:
		if (!(Held & YSStationInput::Right) || !bCanDismount)
		{
			StopWork(!(Held & YSStationInput::Right) ? TEXT("clic droit relâché") : TEXT("plus démontable"));
		}
		else if (Now >= DismountEndServerTime)
		{
			// démonté : le joueur quitte le poste et l'emporte (le poste est détruit)
			Work = EYSStationWork::None;
			DismountEndServerTime = 0.0;
			HostReceptacle->Dismount(User);
		}
		break;
	}
}

void AYSShipStation::StopWork(const TCHAR* Reason)
{
	const EYSStationWork Stopped = Work;
	if (Stopped == EYSStationWork::None)
	{
		return;
	}
	Work = EYSStationWork::None;
	DismountEndServerTime = 0.0;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("%s %s : %s arrêté%s (%s), %.0f / %.0f PV"), GetLogTag(), *GetName(), Stopped == EYSStationWork::Repair ? TEXT("réparation") : TEXT("démontage"),
		Stopped == EYSStationWork::Repair ? TEXT("e") : TEXT(""), Reason, Health, MaxHealth);
	OnWorkStopped(Stopped);
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
	// démontage d'un poste de combat (clic droit maintenu) : la barre se remplit ; sinon PV du module (affichée dès qu'il est abîmé, à 0 PV
	// comprise, et pendant la réparation)
	float RepairProgress = -1.0f;
	if (IsBeingDismounted() && HostReceptacle)
	{
		const AGameStateBase* GameState = GetWorld()->GetGameState();
		const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
		const float Hold = FMath::Max(HostReceptacle->GetDismountHoldDuration(), 0.01f);
		RepairProgress = FMath::Clamp(1.0f - static_cast<float>(DismountEndServerTime - Now) / Hold, 0.0f, 1.0f);
	}
	if (RepairProgress >= 0.0f)
	{
		if (!FMath::IsNearlyEqual(RepairProgress, DisplayedRepairProgress, 0.01f))
		{
			DisplayedRepairProgress = RepairProgress;
			Bar->SetHealth(RepairProgress * MaxHealth, MaxHealth, true);
		}
		return;
	}
	if (DisplayedRepairProgress >= 0.0f || !FMath::IsNearlyEqual(Health, DisplayedHealth) || IsRepairing() != bDisplayedRepairing)
	{
		DisplayedRepairProgress = -1.0f;
		DisplayedHealth = Health;
		bDisplayedRepairing = IsRepairing();
		Bar->SetHealth(Health, MaxHealth, Health <= 0.0f || IsRepairing());
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

	// réparation (Espace) et démontage (clic droit) de l'occupant (serveur)
	if (HasAuthority())
	{
		// poste de combat : PV max = PV max du bateau / diviseur (le bateau change d'étape, le Galion de NME se réveille un autre jour...)
		const AYSPirateShip* PostShip = IsCombatPost() && bTakesDamage ? GetShip() : nullptr;
		const float WantedMax = PostShip ? PostShip->GetCombatPostMaxHealth() : 0.0f;
		if (WantedMax > 0.0f && !FMath::IsNearlyEqual(WantedMax, MaxHealth, 0.5f))
		{
			Health = MaxHealth > 0.0f ? Health / MaxHealth * WantedMax : WantedMax;
			UE_LOG(LogTemp, Log, TEXT("%s %s : PV max %.0f -> %.0f (bateau %s), %.0f PV"), GetLogTag(), *GetName(), MaxHealth, WantedMax, *PostShip->GetName(), Health);
			MaxHealth = WantedMax;
			ForceNetUpdate();
		}
		UpdateWork(DeltaSeconds);
		if (!IsValid(this) || IsActorBeingDestroyed())
		{
			return;
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
	// (module à 0 PV : le joueur s'y installe quand même, Espace maintenu le répare)
	Character->EnterStation(this);
}

void AYSShipStation::SetOccupant(AYSPlayerCharacter* NewOccupant)
{
	if (!HasAuthority() || Occupant == NewOccupant)
	{
		return;
	}
	// le joueur part : réparation / démontage arrêtés (recharge de l'Ability comme si on relâchait)
	StopWork(TEXT("occupant parti"));
	AYSPlayerCharacter* Previous = Occupant;
	Occupant = NewOccupant;
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

float AYSShipStation::GetCameraArmLength() const
{
	// barre : réglage du bateau s'il en a un (le gabarit de la barre d'un Blueprint enfant, ex. BP_GameBoat_Galion, ne garde pas ses retouches)
	const AYSPirateShip* Ship = GetShip();
	if (IsA<AYSShipHelm>())
	{
		return Ship && Ship->GetHelmCameraArmLength() > 0.0f ? Ship->GetHelmCameraArmLength() : CameraArmLength;
	}
	// postes de combat : dézoom à la taille du bateau, comme la barre (demande de Paul, 2026-11-11)
	return IsCombatPost() && Ship ? CameraArmLength * Ship->GetStationCameraScale() : CameraArmLength;
}

float AYSShipStation::GetCameraOffsetDistance() const
{
	const AYSPirateShip* Ship = IsCombatPost() ? GetShip() : nullptr;
	return Ship ? CameraOffsetDistance * Ship->GetStationCameraScale() : CameraOffsetDistance;
}
