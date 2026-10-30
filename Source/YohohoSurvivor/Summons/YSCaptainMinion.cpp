#include "Summons/YSCaptainMinion.h"
#include "Core/YSCombat.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include "Abilities/YSAbilityComponent.h"
#include "Core/YSGameState.h"
#include "Core/YSPlayerState.h"
#include "Enemies/YSFoeCharacter.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Ship/YSPirateShip.h"
#include "Ship/YSShipBoarding.h"
#include "Ship/YSShipCannon.h"
#include "Weapons/YSWeapon.h"

AYSCaptainMinion::AYSCaptainMinion()
{
	// auto-piloté : contrôleur d'IA créé à l'apparition (pas de PlayerState)
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// comme les joueurs : corps blanc, Head Feature (celle de Mouss) de la couleur du Captain, armes de leur couleur de base
	CustomizationColorComponentName = TEXT("Head Feature");
	bWeaponColoration = false;

	// critiques : seulement ceux donnés par le Captain (marque du Corsaire)
	bCriticalsFromStart = false;

	// même cycle que les NME : sort du sol à l'apparition, ragdoll 1 s puis rétrécit 1 s à la mort
	bSpawnRise = true;
	SpawnRiseDuration = 0.5f;
	DeathScaleDelay = 1.0f;
	DeathScaleDuration = 1.0f;
	// pas de cadavre : le minion disparaît
	bKeepCorpse = false;
}

void AYSCaptainMinion::BeginPlay()
{
	Super::BeginPlay();

	// apparition : la capsule ignore les autres personnages un court instant (toutes les machines)
	StartPassThrough();
}

void AYSCaptainMinion::StartPassThrough()
{
	if (SpawnPassThroughDuration > 0.0f)
	{
		UCapsuleComponent* Capsule = GetCapsuleComponent();
		YSCombat::SetPassThroughCharacters(Capsule, true);
		GetWorldTimerManager().SetTimer(PassThroughTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			YSCombat::SetPassThroughCharacters(GetCapsuleComponent(), false);
			UE_LOG(LogTemp, Verbose, TEXT("[YSMinion] %s : collisions avec les personnages rétablies"), *GetName());
		}), SpawnPassThroughDuration, false);
	}
}

void AYSCaptainMinion::SyncCaptainUpgrades()
{
	if (!IsValid(Captain))
	{
		return;
	}
	if (!FMath::IsNearlyEqual(GetMaxHealthMultiplier(), Captain->GetMaxHealthMultiplier()))
	{
		SetMaxHealthMultiplier(Captain->GetMaxHealthMultiplier());
		UE_LOG(LogTemp, Verbose, TEXT("[YSMinion] %s : PV max x%.3f (Captain) -> %.0f / %.0f"), *GetName(), GetMaxHealthMultiplier(), GetHealth(), GetMaxHealth());
	}
	if (!FMath::IsNearlyEqual(GetArmor(), Captain->GetArmor()))
	{
		SetArmor(Captain->GetArmor());
		UE_LOG(LogTemp, Verbose, TEXT("[YSMinion] %s : armure %.1f (Captain)"), *GetName(), GetArmor());
	}
}

void AYSCaptainMinion::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSCaptainMinion, Captain);
	DOREPLIFETIME(AYSCaptainMinion, FaceIndex);
}

void AYSCaptainMinion::InitMinion(AYSPlayerCharacter* InCaptain, UYSWeaponSet* InWeaponSet, int32 InFaceIndex)
{
	Captain = InCaptain;
	FaceIndex = InFaceIndex;
	if (InWeaponSet)
	{
		WeaponSet = InWeaponSet;
	}
}

void AYSCaptainMinion::SetCaptain(AYSPlayerCharacter* NewCaptain)
{
	if (!HasAuthority())
	{
		return;
	}
	Captain = NewCaptain;
	SetOwner(NewCaptain);
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSMinion] %s suit son nouveau Captain %s"), *GetName(), NewCaptain ? *NewCaptain->GetName() : TEXT("(aucun)"));
}

void AYSCaptainMinion::RefreshCustomization()
{
	const AYSPlayerState* CaptainState = Captain ? Captain->GetPlayerState<AYSPlayerState>() : nullptr;
	const AYSGameState* YSGameState = GetWorld() ? GetWorld()->GetGameState<AYSGameState>() : nullptr;
	if (CaptainState && YSGameState)
	{
		UMaterialInterface* ColorMaterial = YSGameState->GetCustomizationMaterial(CaptainState->GetColorIndex());
		// même visage que le Captain
		UTexture2D* FaceTexture = YSGameState->GetCustomizationFace(CaptainState->GetFaceIndex());
		ApplyCustomization(ColorMaterial, FaceTexture);
		bCustomizationApplied = ColorMaterial && FaceTexture;
	}
}

float AYSCaptainMinion::GetWeaponDamageMultiplier(const AYSWeapon* Weapon, EYSActionSlot Slot) const
{
	return Captain ? Captain->GetSlotDamageMultiplier(EYSActionSlot::Ability) : 1.0f;
}

bool AYSCaptainMinion::HasForcedCriticalAgainst(const AActor* Target) const
{
	return Captain && Target && Captain->GetMarkedFoe() == Target && Captain->IsMarkCriticalForMinions();
}

float AYSCaptainMinion::GetCriticalDamageMultiplier() const
{
	return Captain ? Captain->GetCriticalDamageMultiplier() : Super::GetCriticalDamageMultiplier();
}

FVector AYSCaptainMinion::GetAimLocation() const
{
	// pas encore de cible (Ability forcée pendant la sortie du sol : l'IA n'a pas encore tourné) : on la cherche
	const AYSFoeCharacter* Target = CurrentTarget.IsValid() ? CurrentTarget.Get() : FindTarget();
	return Target ? Target->GetActorLocation() : GetActorLocation() + GetActorForwardVector() * 1000.0f;
}

bool AYSCaptainMinion::IsAbilityInProgress() const
{
	for (const AYSWeapon* Weapon : GetHeldWeapons())
	{
		const UYSAbilityComponent* Ability = Weapon ? Weapon->GetActiveAction() : nullptr;
		if (Ability && Ability->IsActionInProgress())
		{
			return true;
		}
	}
	return false;
}

void AYSCaptainMinion::Die()
{
	if (!IsDying())
	{
		UE_LOG(LogTemp, Verbose, TEXT("[YSMinion] %s meurt"), *GetName());
	}
	// ragdoll comme les NME (détruit à la fin)
	Super::Die();
}

void AYSCaptainMinion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// en train de sortir du sol ou de mourir : pas d'IA
	if (!HasAuthority() || IsActorBeingDestroyed() || IsDying() || IsRising())
	{
		return;
	}

	// meurt avec son Captain
	if (!IsValid(Captain) || !Captain->IsAlive())
	{
		Die();
		return;
	}

	// même vitesse que le Captain (sans les bonus des actions : roulade, dash...)
	MoveSpeed = Captain->GetUnmodifiedWalkSpeed();

	// upgrades du Captain : Hps bonus (PV max du minion x le même multiplicateur, les PV gagnés s'ajoutent) et Armor
	SyncCaptainUpgrades();

	AAIController* AIController = Cast<AAIController>(GetController());
	const FVector Location = GetActorLocation();
	AYSFoeCharacter* Target = FindTarget();
	CurrentTarget = Target;

	// Ability forcée en cours (dash, roulade) : le mouvement est celui de l'action ; regard figé s'il l'est (dash)
	if (IsAbilityInProgress())
	{
		FVector LockedDirection;
		bool bLocked = false;
		ForEachHeldAction([&LockedDirection, &bLocked](const UYSAbilityComponent* Action)
		{
			bLocked = bLocked || Action->GetLockedAimDirection(LockedDirection);
		});
		if (AIController && bLocked)
		{
			AIController->SetFocalPoint(Location + LockedDirection * 500.0f);
		}
		return;
	}

	// Captain à bord d'un bateau : monter à bord, canons
	if (UpdateShipCrew(AIController, DeltaSeconds))
	{
		return;
	}

	// sur le même bateau que le Captain : il ne saute pas à l'eau pour poursuivre un NME (seulement ceux à bord)
	const AYSPirateShip* Deck = GetShipUnderFeet();
	const bool bStayAboard = Deck && Deck == Captain->GetShipUnderFeet();

	// cible : s'approche jusqu'à la portée de son arme, tourné vers elle (l'attaque auto part toute seule)
	FVector Goal = FVector::ZeroVector;
	float StopDistance = 0.0f;
	if (Target)
	{
		bReturningToCaptain = false;
		Goal = bStayAboard && !Deck->IsInsideHull(Target->GetActorLocation()) ? FVector::ZeroVector : Target->GetActorLocation();
		StopDistance = HasRangedWeapon() ? RangedStopDistance : MeleeStopDistance;
		if (AIController)
		{
			AIController->SetFocalPoint(Target->GetActorLocation());
		}
	}
	else
	{
		// sans cible : reste près du Captain
		const float CaptainDistance = FVector::Dist2D(Location, Captain->GetActorLocation());
		if (CaptainDistance > FollowDistance)
		{
			bReturningToCaptain = true;
		}
		else if (CaptainDistance < FollowStopDistance)
		{
			bReturningToCaptain = false;
		}
		if (bReturningToCaptain)
		{
			Goal = Captain->GetActorLocation();
			StopDistance = FollowStopDistance;
		}
	}

	FVector ToGoal = Goal - Location;
	ToGoal.Z = 0.0f;
	const bool bMoving = !Goal.IsZero() && ToGoal.SizeSquared() > FMath::Square(StopDistance);
	if (bMoving)
	{
		AddMovementInput(ToGoal.GetSafeNormal());
	}

	// sans cible : regarde là où il va (ou garde son orientation à l'arrêt)
	if (!Target && AIController)
	{
		if (bMoving)
		{
			AIController->SetFocalPoint(Location + ToGoal.GetSafeNormal() * 500.0f);
		}
		else
		{
			AIController->ClearFocus(EAIFocusPriority::Gameplay);
		}
	}
}

bool AYSCaptainMinion::UpdateShipCrew(AAIController* AIController, float DeltaSeconds)
{
	AYSPirateShip* Ship = Captain->GetShipUnderFeet();
	if (Ship && Ship->IsFoeShip())
	{
		Ship = nullptr;
	}
	const AYSShipStation* CaptainStation = Captain->GetCurrentStation();
	const bool bCaptainPiloting = Ship && IsValid(CaptainStation) && CaptainStation->UsesPilotInput();

	// au canon : il tire tant que le Captain pilote ce bateau, sinon il le lâche
	if (IsAtStation())
	{
		AYSShipCannon* Cannon = Cast<AYSShipCannon>(GetCurrentStation());
		if (Cannon && bCaptainPiloting && Cannon->GetShip() == Ship)
		{
			UpdateCannonFire(Cannon, AIController);
			return true;
		}
		LeaveStation(TEXT("le Captain lâche la barre"), true);
		AssignedCannon = nullptr;
	}

	if (!Ship)
	{
		AssignedCannon = nullptr;
		return false;
	}

	// pas encore à bord : accès du bateau, à pied ou à la nage (le tour de la coque si l'accès est de l'autre côté)
	// (au-dessus de la coque : il est à bord même sans le pont pour base de mouvement, en retombant ou debout sur un autre minion ; sinon il remonte à chaque image)
	const FVector Location = GetActorLocation();
	const bool bAboveDeck = Ship->IsInsideHull(Location) && Location.Z > Ship->GetActorLocation().Z;
	if (GetShipUnderFeet() != Ship && !bAboveDeck)
	{
		AYSShipBoarding* Boarding = Ship->GetBoardingAccess();
		if (!Boarding)
		{
			return false;
		}
		const FVector Access = Boarding->GetInteractLocation(Location);
		if (FVector::Dist2D(Location, Access) <= InteractionDistance && Boarding->CanInteract(this))
		{
			// tous reposés au même point : ils se traversent le temps de s'en éloigner
			Boarding->Interact(this);
			StartPassThrough();
			return true;
		}
		WalkToward(Ship->GetRouteAroundHull(Location, Access, BoardingHullMargin), AIController, DeltaSeconds);
		return true;
	}

	// à bord, Captain à la barre : à pied jusqu'à un canon libre (aucun : il reste à bord, IA normale)
	AYSShipCannon* Cannon = bCaptainPiloting ? FindFreeCannon(Ship) : nullptr;
	AssignedCannon = Cannon;
	if (!Cannon)
	{
		return false;
	}
	const FVector Seat = Cannon->GetOccupantFeetLocation();
	if (FVector::Dist2D(Location, Seat) <= CannonReachDistance)
	{
		EnterStation(Cannon);
		return true;
	}
	WalkToward(Seat, AIController, DeltaSeconds);
	return true;
}

AYSShipCannon* AYSCaptainMinion::FindFreeCannon(const AYSPirateShip* Ship) const
{
	AYSShipCannon* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (AYSShipStation* Station : Ship->GetStations())
	{
		AYSShipCannon* Cannon = Cast<AYSShipCannon>(Station);
		if (!Cannon || Cannon->GetOccupant() || !Cannon->CanInteract(this))
		{
			continue;
		}
		// déjà visé par un autre minion : pour lui
		bool bReserved = false;
		for (TActorIterator<AYSCaptainMinion> It(GetWorld()); It && !bReserved; ++It)
		{
			bReserved = *It != this && !It->IsDying() && It->AssignedCannon == Cannon;
		}
		if (bReserved)
		{
			continue;
		}
		if (AssignedCannon == Cannon)
		{
			return Cannon;
		}
		const float DistSq = FVector::DistSquared2D(Cannon->GetOccupantFeetLocation(), GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Cannon;
		}
	}
	return Best;
}

void AYSCaptainMinion::UpdateCannonFire(AYSShipCannon* Cannon, AAIController* AIController)
{
	const AYSFoeCharacter* CannonTarget = Cannon->FindAutoFireTarget();
	if (!CannonTarget)
	{
		// sans cible : droit devant le canon
		if (AIController)
		{
			AIController->SetFocalPoint(Cannon->GetActorLocation() + Cannon->GetActorForwardVector() * 1000.0f);
		}
		return;
	}
	if (AIController)
	{
		AIController->SetFocalPoint(CannonTarget->GetActorLocation());
	}
	if (Cannon->GetCooldownRemaining() <= 0.0f && Cannon->IsAimedAt(CannonTarget))
	{
		Cannon->ServerHandleAction(this, EYSStationAction::Fire, CannonTarget->GetActorLocation());
	}
}

void AYSCaptainMinion::WalkToward(const FVector& Goal, AAIController* AIController, float DeltaSeconds)
{
	const FVector Location = GetActorLocation();
	FVector Direction = FVector(Goal.X - Location.X, Goal.Y - Location.Y, 0.0f).GetSafeNormal();

	// bloqué (mât, marche, bastingage) : un moment sur le côté, de l'autre côté à chaque fois
	if (CrewSidestepTime > 0.0f)
	{
		CrewSidestepTime -= DeltaSeconds;
		Direction = (Direction + FVector::CrossProduct(FVector::UpVector, Direction) * CrewSidestepSide * 2.0f).GetSafeNormal();
	}
	else if (GetVelocity().Size2D() < CrewStuckSpeed)
	{
		CrewStuckTime += DeltaSeconds;
		if (CrewStuckTime >= CrewStuckDelay)
		{
			CrewStuckTime = 0.0f;
			CrewSidestepTime = CrewSidestepDuration;
			CrewSidestepSide = -CrewSidestepSide;
			UE_LOG(LogTemp, Verbose, TEXT("[YSMinion] %s bloqué en (%.0f, %.0f) : contourne"), *GetName(), Location.X, Location.Y);
		}
	}
	else
	{
		CrewStuckTime = 0.0f;
	}

	AddMovementInput(Direction);
	if (AIController)
	{
		AIController->SetFocalPoint(Location + Direction * 500.0f);
	}
}

AYSFoeCharacter* AYSCaptainMinion::FindTarget() const
{
	const AYSFoeCharacter* Marked = Captain ? Captain->GetMarkedFoe() : nullptr;
	const FVector Location = GetActorLocation();
	const FVector CaptainLocation = Captain ? Captain->GetActorLocation() : Location;

	AYSFoeCharacter* Best = nullptr;
	float BestDistance = AggroRange;
	for (TActorIterator<AYSFoeCharacter> It(GetWorld()); It; ++It)
	{
		AYSFoeCharacter* Foe = *It;
		if (!Foe || Foe->GetHealth() <= 0.0f || Foe->IsActorBeingDestroyed())
		{
			continue;
		}
		// NME marqué : compte N fois plus près (pour la portée et pour la distance au Captain)
		const float Attraction = Foe == Marked ? FMath::Max(MarkedFoeAttraction, 1.0f) : 1.0f;
		if (FVector::Dist2D(CaptainLocation, Foe->GetActorLocation()) / Attraction > MaxChaseDistanceFromCaptain)
		{
			continue;
		}
		const float Distance = FVector::Dist2D(Location, Foe->GetActorLocation()) / Attraction;
		if (Distance <= BestDistance)
		{
			BestDistance = Distance;
			Best = Foe;
		}
	}
	return Best;
}

bool AYSCaptainMinion::HasRangedWeapon() const
{
	for (const AYSWeapon* Weapon : GetHeldWeapons())
	{
		const UYSAbilityComponent* Auto = Weapon ? Weapon->GetAutoAction() : nullptr;
		if (Auto && Auto->WeaponType == EYSWeaponType::Gun)
		{
			return true;
		}
	}
	return false;
}
