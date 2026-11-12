#include "Summons/YSHiredCrewComponent.h"
#include "AIController.h"
#include "Abilities/YSAbilityComponent.h"
#include "Core/YSGameMode.h"
#include "Core/YSGameState.h"
#include "Enemies/YSFoeCharacter.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interaction/YSInteractiveObject.h"
#include "Interaction/YSPlayerCorpse.h"
#include "Net/UnrealNetwork.h"
#include "NPC/YSBrookerNPC.h"
#include "Player/YSPlayerCharacter.h"
#include "Ship/YSCombatPost.h"
#include "Ship/YSPirateShip.h"
#include "Ship/YSShipBoarding.h"
#include "Ship/YSShipCannon.h"
#include "Weapons/YSWeapon.h"

namespace YSHiredCrew
{
	/** Recherche d'un objet à prendre près du chef : toutes les ... */
	constexpr float PickupScanInterval = 0.25f;

	/** Bateau des joueurs (GameBoat) s'il est là */
	AYSPirateShip* GetPlayerBoat(const UWorld* World)
	{
		const AYSGameState* YSGameState = World ? World->GetGameState<AYSGameState>() : nullptr;
		AYSPirateShip* Boat = YSGameState ? YSGameState->GetGameBoat() : nullptr;
		return Boat && !Boat->IsFoeShip() && Boat->IsPresent() ? Boat : nullptr;
	}

	/** Réceptacle libre du bateau des joueurs le plus proche de From */
	AYSCombatPostReceptacle* FindFreeReceptacle(const UWorld* World, const FVector& From)
	{
		AYSPirateShip* Boat = GetPlayerBoat(World);
		if (!Boat || Boat->IsDisabled())
		{
			return nullptr;
		}
		AYSCombatPostReceptacle* Best = nullptr;
		float BestDistSq = TNumericLimits<float>::Max();
		for (AYSCombatPostReceptacle* Receptacle : Boat->GetCombatPostReceptacles())
		{
			if (!IsValid(Receptacle) || Receptacle->GetInstalledPost())
			{
				continue;
			}
			const float DistSq = FVector::DistSquared2D(Receptacle->GetActorLocation(), From);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Receptacle;
			}
		}
		return Best;
	}
}

UYSHiredCrewComponent::UYSHiredCrewComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UYSHiredCrewComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UYSHiredCrewComponent, HiredNumber);
	DOREPLIFETIME(UYSHiredCrewComponent, ColorIndex);
	DOREPLIFETIME(UYSHiredCrewComponent, FaceIndex);
}

UYSHiredCrewComponent* UYSHiredCrewComponent::Get(const AActor* Actor)
{
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(Actor);
	return Character ? Character->GetHiredCrew() : nullptr;
}

AYSPlayerCharacter* UYSHiredCrewComponent::GetCharacter() const
{
	return Cast<AYSPlayerCharacter>(GetOwner());
}

void UYSHiredCrewComponent::InitHired(int32 InNumber, int32 InColorIndex, int32 InFaceIndex)
{
	HiredNumber = InNumber;
	ColorIndex = InColorIndex;
	FaceIndex = InFaceIndex;
	OnRep_Customization();
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void UYSHiredCrewComponent::OnRep_Customization()
{
	// le personnage réessaie à chaque image tant que ce n'est pas appliqué (GameState pas encore arrivé chez un client)
	if (AYSPlayerCharacter* Character = GetCharacter())
	{
		Character->bCustomizationApplied = false;
		Character->RefreshCustomization();
	}
}

bool UYSHiredCrewComponent::IsHaulable(const AYSInteractiveObject* Object)
{
	if (!IsValid(Object) || !Object->IsCarriable() || Object->IsDelivered() || Object->GetCarrier() || Object->IsA<AYSPlayerCorpse>() || Object->IsActorBeingDestroyed())
	{
		return false;
	}
	// poste de combat : seulement s'il reste un réceptacle libre sur le bateau des joueurs
	if (Object->IsA<AYSCombatPostItem>())
	{
		return YSHiredCrew::FindFreeReceptacle(Object->GetWorld(), Object->GetActorLocation()) != nullptr;
	}
	// coffre, membre d'équipage, boussole, core... : un PNJ (Brooker, core du bateau, zone de la plage) l'accepte
	return AYSBrookerNPC::FindNearest(Object->GetWorld(), Object->GetActorLocation(), Object) != nullptr;
}

FVector UYSHiredCrewComponent::GetAimLocation() const
{
	const AActor* Owner = GetOwner();
	const AYSFoeCharacter* Target = CurrentTarget.Get();
	return Target ? Target->GetActorLocation() : Owner->GetActorLocation() + Owner->GetActorForwardVector() * 1000.0f;
}

void UYSHiredCrewComponent::OnOwnerDies()
{
	bClimbing = false;
	AYSPlayerCharacter* Character = GetCharacter();
	if (AYSGameMode* GameMode = Character && Character->HasAuthority() ? GetWorld()->GetAuthGameMode<AYSGameMode>() : nullptr)
	{
		GameMode->OnHiredMoussDied(Character);
	}
}

void UYSHiredCrewComponent::OnMoveBlocked(const FHitResult& Impact)
{
	AYSPlayerCharacter* Character = GetCharacter();
	if (!bClimbWhenBlocked || !Character || !Character->HasAuthority())
	{
		return;
	}
	// personnages (joueurs, NME : canal Foe) : on ne leur grimpe pas dessus ; sol praticable ou plafond : rien à grimper
	const UPrimitiveComponent* HitComponent = Impact.GetComponent();
	const ECollisionChannel HitType = HitComponent ? HitComponent->GetCollisionObjectType() : ECC_WorldStatic;
	if (HitType == ECC_Pawn || HitType == ECC_GameTraceChannel2 || Cast<APawn>(Impact.GetActor()))
	{
		return;
	}
	if (Impact.Normal.Z >= Character->GetCharacterMovement()->GetWalkableFloorZ() || Impact.Normal.Z < -0.5f)
	{
		return;
	}
	LastClimbBlockTime = GetWorld()->GetTimeSeconds();
	bLastClimbBlockOnShipSwimming = bSwimmerFullClimbOnShips && Character->IsSwimming() && AYSPirateShip::GetShipOf(Impact.GetActor()) != nullptr;
}

void UYSHiredCrewComponent::UpdateClimb()
{
	AYSPlayerCharacter* Character = GetCharacter();
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const bool bMoved = bMoveInputThisFrame;
	bMoveInputThisFrame = false;

	// un autre mouvement a pris la main (pushback, roulade...) : plus d'escalade
	if (bClimbing && Movement->MovementMode != MOVE_Flying)
	{
		bClimbing = false;
		UE_LOG(LogTemp, Verbose, TEXT("[YSClimb] %s : escalade interrompue (mode %d)"), *Character->GetName(), static_cast<int32>(Movement->MovementMode.GetValue()));
	}

	const bool bBlocked = LastClimbBlockTime >= 0.0 && GetWorld()->GetTimeSeconds() - LastClimbBlockTime <= ClimbBlockMemory;
	if (!bClimbWhenBlocked || !bMoved || !bBlocked || Character->IsAtStation() || !Movement->IsActive())
	{
		if (bClimbing)
		{
			StopClimbing(bBlocked ? TEXT("plus de déplacement") : TEXT("plus rien devant"));
		}
		return;
	}

	// vitesse horizontale perdue contre la surface (Velocity = déplacement réel de l'image précédente)
	const float Speed = Movement->MaxWalkSpeed;
	const float KeptSpeed = FMath::Min(Movement->Velocity.Size2D(), Speed);
	const float LostSpeed = FMath::Max(Speed - KeptSpeed, 0.0f);
	if (!bClimbing)
	{
		bFullSpeedClimb = bLastClimbBlockOnShipSwimming;
		if ((Movement->MovementMode != MOVE_Walking && !bFullSpeedClimb) || (LostSpeed < Speed * ClimbMinLostSpeedRatio && !bFullSpeedClimb))
		{
			return;
		}
		bClimbing = true;
		Movement->SetMovementMode(MOVE_Flying);
		UE_LOG(LogTemp, Verbose, TEXT("[YSClimb] %s grimpe à %s : %.0f gardés%s"), *Character->GetName(), *Character->GetActorLocation().ToCompactString(), KeptSpeed,
			bFullSpeedClimb ? TEXT(" (nage contre un bateau : pleine vitesse)") : TEXT(""));
	}

	// vol sans gravité : horizontal plafonné à la vitesse de marche ; vers le haut : toute la vitesse (peu gardée, nageur contre un bateau), sinon la vitesse perdue
	const float VerticalSpeed = (bFullSpeedClimb || KeptSpeed < Speed * ClimbFullSpeedKeptRatio ? Speed : LostSpeed) * ClimbSpeedMultiplier;
	Movement->MaxFlySpeed = Speed + VerticalSpeed + 1.0f;
	const FVector Horizontal = FVector(Movement->Velocity.X, Movement->Velocity.Y, 0.0f).GetClampedToMaxSize(Speed);
	Movement->Velocity = FVector(Horizontal.X, Horizontal.Y, VerticalSpeed);
}

void UYSHiredCrewComponent::StopClimbing(const TCHAR* Reason)
{
	bClimbing = false;
	AYSPlayerCharacter* Character = GetCharacter();
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (Movement->MovementMode == MOVE_Flying)
	{
		// l'élan vers le haut est gardé (la gravité le reprend) : aide à passer le rebord
		Movement->SetMovementMode(MOVE_Falling);
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSClimb] %s arrête de grimper à %s : %s"), *Character->GetName(), *Character->GetActorLocation().ToCompactString(), Reason);
}

bool UYSHiredCrewComponent::IsBlockingActionInProgress() const
{
	for (const AYSWeapon* Weapon : GetCharacter()->GetHeldWeapons())
	{
		const UYSAbilityComponent* Ability = Weapon ? Weapon->GetActiveAction() : nullptr;
		if (Ability && Ability->IsActionInProgress() && Ability->BlocksAllWeapons())
		{
			return true;
		}
	}
	return false;
}

bool UYSHiredCrewComponent::HasOnlyRangedAutoAttack() const
{
	bool bRanged = false;
	for (const AYSWeapon* Weapon : GetCharacter()->GetHeldWeapons())
	{
		const UYSAbilityComponent* Auto = Weapon ? Weapon->GetAutoAction() : nullptr;
		if (Auto && Auto->WeaponType != EYSWeaponType::Gun)
		{
			return false;
		}
		bRanged |= Auto != nullptr;
	}
	return bRanged;
}

void UYSHiredCrewComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// IA : serveur seulement, personnage vivant
	AYSPlayerCharacter* Character = GetCharacter();
	if (!Character || !Character->HasAuthority() || Character->IsActorBeingDestroyed() || Character->IsDying() || Character->IsRising() || !Character->IsAlive())
	{
		return;
	}

	// escalade : d'après le déplacement de l'image précédente (avant celui de cette image)
	UpdateClimb();

	AAIController* AIController = Cast<AAIController>(Character->GetController());
	const AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	const AYSPlayerCharacter* Leader = GameMode ? GameMode->GetHiredCrewLeader() : nullptr;

	// à un poste qui n'est pas un poste de combat (place de la chaloupe de l'intro) : rien à faire
	if (Character->IsAtStation() && !Cast<AYSShipCannon>(Character->GetCurrentStation()))
	{
		if (AIController)
		{
			AIController->ClearFocus(EAIFocusPriority::Gameplay);
		}
		return;
	}

	// action en cours qui occupe tout le personnage (roulade, dash) : son déplacement
	if (IsBlockingActionInProgress())
	{
		return;
	}

	// 1. objet porté : le ramener
	if (UpdateDelivery(AIController, DeltaTime))
	{
		return;
	}

	if (!Leader)
	{
		if (Character->IsAtStation())
		{
			Character->LeaveStation(TEXT("plus de chef"), true);
		}
		return;
	}

	// resté trop loin du chef (bateau parti) : téléporté à côté de lui
	if (TeleportDistanceFromLeader > 0.0f && FVector::DistSquared2D(Character->GetActorLocation(), Leader->GetActorLocation()) > FMath::Square(TeleportDistanceFromLeader))
	{
		TeleportNear(Leader);
		return;
	}

	// 2. objet à ramener près du chef
	if (UpdatePickup(AIController, DeltaTime, Leader))
	{
		return;
	}

	// 3. chef à bord du bateau des joueurs : postes de combat, sinon centre du pont
	AYSPirateShip* LeaderShip = Leader->GetShipUnderFeet();
	if (LeaderShip && LeaderShip == YSHiredCrew::GetPlayerBoat(GetWorld()))
	{
		AYSFoeCharacter* ShipTarget = FindTarget(nullptr);
		CurrentTarget = ShipTarget;
		if (UpdateShipCrew(AIController, DeltaTime, Leader, ShipTarget))
		{
			return;
		}
	}
	else if (Character->IsAtStation())
	{
		Character->LeaveStation(TEXT("le chef a quitté le bateau"), true);
		AssignedPost = nullptr;
	}

	// 4. combat
	AYSFoeCharacter* Target = FindTarget(Leader);
	CurrentTarget = Target;
	if (Target)
	{
		bReturningToLeader = false;
		FightTarget(Target, AIController, DeltaTime, false);
		return;
	}

	// 5. suit le chef
	const float LeaderDistance = FVector::Dist2D(Character->GetActorLocation(), Leader->GetActorLocation());
	if (LeaderDistance > FollowDistance)
	{
		bReturningToLeader = true;
	}
	else if (LeaderDistance < FollowStopDistance)
	{
		bReturningToLeader = false;
	}
	if (bReturningToLeader)
	{
		WalkToward(Leader->GetActorLocation(), FollowStopDistance, AIController, DeltaTime);
	}
	else if (AIController)
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}
}

bool UYSHiredCrewComponent::UpdateDelivery(AAIController* AIController, float DeltaSeconds)
{
	AYSPlayerCharacter* Character = GetCharacter();
	AYSInteractiveObject* Object = Character->GetCarriedObject();
	if (!IsValid(Object))
	{
		return false;
	}
	ClaimedObject = nullptr;

	// poste de combat : sur un réceptacle libre du bateau des joueurs
	if (Object->IsA<AYSCombatPostItem>())
	{
		if (AYSCombatPostReceptacle* Receptacle = YSHiredCrew::FindFreeReceptacle(GetWorld(), Character->GetActorLocation()))
		{
			if (BoardShip(Receptacle->GetShip(), AIController, DeltaSeconds))
			{
				return true;
			}
			if (WalkToward(Receptacle->GetActorLocation(), PostReachDistance, AIController, DeltaSeconds) && Receptacle->CanInteract(Character))
			{
				UE_LOG(LogTemp, Log, TEXT("[YSHiredCrew] %s pose %s sur %s"), *Character->GetName(), *Object->GetName(), *Receptacle->GetName());
				Receptacle->Interact(Character);
			}
			return true;
		}
	}

	// sinon : la zone du PNJ qui l'accepte (Brooker de l'île, core du bateau, zone de la plage)
	if (AYSBrookerNPC* Zone = AYSBrookerNPC::FindNearest(GetWorld(), Character->GetActorLocation(), Object))
	{
		AYSPirateShip* ZoneShip = AYSPirateShip::GetShipOf(Zone);
		if (ZoneShip && ZoneShip->IsPresent() && BoardShip(ZoneShip, AIController, DeltaSeconds))
		{
			return true;
		}
		if (WalkToward(Zone->GetActorLocation(), Zone->GetAcceptRange() * DeliverRangeRatio, AIController, DeltaSeconds))
		{
			UE_LOG(LogTemp, Log, TEXT("[YSHiredCrew] %s ramène %s à %s"), *Character->GetName(), *Object->GetName(), *Zone->GetName());
			Character->DropCarriedObject(TEXT("ramené par le Hired crew"));
		}
		return true;
	}

	// plus aucun endroit où le ramener : reposé (il ne le reprend pas tant qu'il n'y a pas de destination)
	UE_LOG(LogTemp, Log, TEXT("[YSHiredCrew] %s : plus de destination pour %s, reposé"), *Character->GetName(), *Object->GetName());
	Character->DropCarriedObject(TEXT("Hired crew : pas de destination"));
	return false;
}

bool UYSHiredCrewComponent::UpdatePickup(AAIController* AIController, float DeltaSeconds, const AYSPlayerCharacter* Leader)
{
	AYSPlayerCharacter* Character = GetCharacter();

	// objet réservé : encore à prendre ?
	AYSInteractiveObject* Object = ClaimedObject.Get();
	if (Object && !IsHaulable(Object))
	{
		ClaimedObject = nullptr;
		Object = nullptr;
	}

	// nouvel objet près du chef (pas à chaque image), pas déjà réservé par un autre HiredMouss
	PickupScanTimer -= DeltaSeconds;
	if (!Object && PickupScanTimer <= 0.0f)
	{
		PickupScanTimer = YSHiredCrew::PickupScanInterval;
		float BestDistSq = FMath::Square(PickupTriggerDistance);
		for (TActorIterator<AYSInteractiveObject> It(GetWorld()); It; ++It)
		{
			AYSInteractiveObject* Candidate = *It;
			const float DistSq = FVector::DistSquared2D(Candidate->GetActorLocation(), Leader->GetActorLocation());
			if (DistSq > BestDistSq || !IsHaulable(Candidate))
			{
				continue;
			}
			bool bClaimed = false;
			for (TActorIterator<AYSPlayerCharacter> Other(GetWorld()); Other && !bClaimed; ++Other)
			{
				const UYSHiredCrewComponent* OtherCrew = Other->GetHiredCrew();
				bClaimed = OtherCrew && OtherCrew != this && OtherCrew->ClaimedObject == Candidate;
			}
			if (!bClaimed)
			{
				BestDistSq = DistSq;
				Object = Candidate;
			}
		}
		if (Object)
		{
			ClaimedObject = Object;
			UE_LOG(LogTemp, Log, TEXT("[YSHiredCrew] %s va chercher %s (chef à %.0f cm)"), *Character->GetName(), *Object->GetName(), FMath::Sqrt(BestDistSq));
		}
	}
	if (!Object)
	{
		return false;
	}

	if (Character->IsAtStation())
	{
		Character->LeaveStation(TEXT("va chercher un objet"), true);
		AssignedPost = nullptr;
	}
	if (WalkToward(Object->GetActorLocation(), Character->InteractionDistance * 0.5f, AIController, DeltaSeconds) && Object->CanInteract(Character))
	{
		Object->Interact(Character);
		if (Character->GetCarriedObject() == Object)
		{
			ClaimedObject = nullptr;
		}
	}
	return true;
}

bool UYSHiredCrewComponent::BoardShip(AYSPirateShip* Ship, AAIController* AIController, float DeltaSeconds)
{
	AYSPlayerCharacter* Character = GetCharacter();
	if (!Ship)
	{
		return false;
	}
	// déjà à bord (au-dessus de la coque : même en retombant ou debout sur un autre personnage)
	const FVector Location = Character->GetActorLocation();
	const bool bAboveDeck = Ship->IsInsideHull(Location) && Location.Z > Ship->GetActorLocation().Z;
	if (Character->GetShipUnderFeet() == Ship || bAboveDeck)
	{
		return false;
	}
	AYSShipBoarding* Boarding = Ship->GetBoardingAccess(Location);
	if (!Boarding)
	{
		return false;
	}
	const FVector Access = Boarding->GetInteractLocation(Location);
	if (FVector::Dist2D(Location, Access) <= Character->InteractionDistance * Boarding->GetInteractRangeMultiplier() && Boarding->CanInteract(Character))
	{
		if (bClimbing)
		{
			StopClimbing(TEXT("monte à bord"));
		}
		Boarding->Interact(Character);
		return true;
	}
	// accès de l'autre côté : le tour de la coque
	WalkToward(Ship->GetRouteAroundHull(Location, Access, BoardingHullMargin), 0.0f, AIController, DeltaSeconds);
	return true;
}

bool UYSHiredCrewComponent::UpdateShipCrew(AAIController* AIController, float DeltaSeconds, const AYSPlayerCharacter* Leader, AYSFoeCharacter* Target)
{
	AYSPlayerCharacter* Character = GetCharacter();
	AYSPirateShip* Ship = Leader->GetShipUnderFeet();

	// au poste de combat : il tire tant que le chef est sur ce bateau
	if (Character->IsAtStation())
	{
		AYSShipCannon* Cannon = Cast<AYSShipCannon>(Character->GetCurrentStation());
		if (Cannon && Cannon->GetShip() == Ship && Cannon->IsUsable())
		{
			UpdateCannonFire(Cannon, AIController);
			return true;
		}
		Character->LeaveStation(TEXT("poste inutilisable"), true);
		AssignedPost = nullptr;
	}

	// monter à bord
	if (BoardShip(Ship, AIController, DeltaSeconds))
	{
		return true;
	}

	// un poste de combat libre
	AYSShipCannon* Post = FindFreeCombatPost(Ship);
	AssignedPost = Post;
	if (Post)
	{
		if (WalkToward(Post->GetOccupantFeetLocation(), PostReachDistance, AIController, DeltaSeconds) && Post->CanInteract(Character))
		{
			Character->EnterStation(Post);
		}
		return true;
	}

	// aucun : au centre du bateau, il attaque les NME qui arrivent (ceux à bord : il va les chercher ; les autres : à distance, sans quitter le pont)
	if (Target)
	{
		const bool bTargetAboard = Ship->IsInsideHull(Target->GetActorLocation());
		if (!bTargetAboard)
		{
			WalkToward(Ship->GetHullCenter(), DeckCenterStopDistance, AIController, DeltaSeconds);
		}
		FightTarget(Target, AIController, DeltaSeconds, !bTargetAboard);
		return true;
	}
	if (WalkToward(Ship->GetHullCenter(), DeckCenterStopDistance, AIController, DeltaSeconds) && AIController)
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}
	return true;
}

AYSShipCannon* UYSHiredCrewComponent::FindFreeCombatPost(const AYSPirateShip* Ship) const
{
	AYSPlayerCharacter* Character = GetCharacter();
	AYSShipCannon* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (AYSShipStation* Station : Ship->GetStations())
	{
		AYSShipCannon* Cannon = Cast<AYSShipCannon>(Station);
		if (!Cannon || !Cannon->IsFree() || !Cannon->IsUsable() || !Cannon->CanInteract(Character))
		{
			continue;
		}
		// déjà visé par un autre HiredMouss : pour lui
		bool bReserved = false;
		for (TActorIterator<AYSPlayerCharacter> It(GetWorld()); It && !bReserved; ++It)
		{
			const UYSHiredCrewComponent* OtherCrew = It->GetHiredCrew();
			bReserved = OtherCrew && OtherCrew != this && !It->IsDying() && OtherCrew->AssignedPost == Cannon;
		}
		if (bReserved)
		{
			continue;
		}
		if (AssignedPost == Cannon)
		{
			return Cannon;
		}
		const float DistSq = FVector::DistSquared2D(Cannon->GetOccupantFeetLocation(), Character->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Cannon;
		}
	}
	return Best;
}

void UYSHiredCrewComponent::UpdateCannonFire(AYSShipCannon* Cannon, AAIController* AIController)
{
	const AActor* CannonTarget = Cannon->FindAutoFireTarget(false);
	if (!CannonTarget)
	{
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
		Cannon->ServerHandleAction(GetCharacter(), EYSStationAction::Primary, CannonTarget->GetActorLocation());
	}
}

AYSFoeCharacter* UYSHiredCrewComponent::FindTarget(const AYSPlayerCharacter* Leader) const
{
	const FVector Location = GetOwner()->GetActorLocation();
	AYSFoeCharacter* Best = nullptr;
	float BestDistance = AggroRange;
	for (TActorIterator<AYSFoeCharacter> It(GetWorld()); It; ++It)
	{
		AYSFoeCharacter* Foe = *It;
		if (!Foe || Foe->GetHealth() <= 0.0f || Foe->IsActorBeingDestroyed() || Foe->IsHidden())
		{
			continue;
		}
		if (Leader && FVector::Dist2D(Leader->GetActorLocation(), Foe->GetActorLocation()) > MaxChaseDistanceFromLeader)
		{
			continue;
		}
		const float Distance = FVector::Dist2D(Location, Foe->GetActorLocation());
		if (Distance <= BestDistance)
		{
			BestDistance = Distance;
			Best = Foe;
		}
	}
	return Best;
}

void UYSHiredCrewComponent::FightTarget(AYSFoeCharacter* Target, AAIController* AIController, float DeltaSeconds, bool bHoldPosition)
{
	AYSPlayerCharacter* Character = GetCharacter();
	const FVector TargetLocation = Target->GetActorLocation();
	if (!bHoldPosition)
	{
		WalkToward(TargetLocation, HasOnlyRangedAutoAttack() ? RangedStopDistance : MeleeStopDistance, AIController, DeltaSeconds);
	}
	// tourné vers la cible : l'attaque auto part toute seule
	if (AIController)
	{
		AIController->SetFocalPoint(TargetLocation);
	}

	// Ability dès qu'elle est prête et la cible à sa portée
	if (Character->AreWeaponsStowed())
	{
		return;
	}
	float AbilityRange = -1.0f;
	for (const AYSWeapon* Weapon : Character->GetHeldWeapons())
	{
		const UYSAbilityComponent* Ability = Weapon ? Weapon->GetActiveAction() : nullptr;
		if (Ability && Ability->IsReady() && !Ability->IsActionInProgress())
		{
			AbilityRange = FMath::Max(AbilityRange, Ability->WeaponType == EYSWeaponType::Gun ? AbilityRangeGun : AbilityRangeMelee);
		}
	}
	const float EdgeDistance = FVector::Dist2D(Character->GetActorLocation(), TargetLocation) - Target->GetSimpleCollisionRadius();
	if (AbilityRange >= 0.0f && EdgeDistance <= AbilityRange)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[YSHiredCrew] %s : Ability vers %s (%.0f cm)"), *Character->GetName(), *Target->GetName(), EdgeDistance);
		Character->Fire();
	}
}

bool UYSHiredCrewComponent::WalkToward(const FVector& Goal, float StopDistance, AAIController* AIController, float DeltaSeconds)
{
	AYSPlayerCharacter* Character = GetCharacter();
	const FVector Location = Character->GetActorLocation();
	const FVector ToGoal(Goal.X - Location.X, Goal.Y - Location.Y, 0.0f);
	if (ToGoal.SizeSquared() <= FMath::Square(StopDistance))
	{
		StuckTime = 0.0f;
		return true;
	}
	FVector Direction = ToGoal.GetSafeNormal();

	// bloqué (mât, marche, bastingage) : un moment sur le côté, de l'autre côté à chaque fois (l'escalade passe d'abord)
	if (SidestepTime > 0.0f)
	{
		SidestepTime -= DeltaSeconds;
		Direction = (Direction + FVector::CrossProduct(FVector::UpVector, Direction) * SidestepSide * 2.0f).GetSafeNormal();
	}
	else if (!bClimbing && Character->GetVelocity().Size2D() < StuckSpeed)
	{
		StuckTime += DeltaSeconds;
		if (StuckTime >= StuckDelay)
		{
			StuckTime = 0.0f;
			SidestepTime = SidestepDuration;
			SidestepSide = -SidestepSide;
			UE_LOG(LogTemp, Verbose, TEXT("[YSHiredCrew] %s bloqué en %s : contourne"), *Character->GetName(), *Location.ToCompactString());
		}
	}
	else
	{
		StuckTime = 0.0f;
	}

	Character->AddMovementInput(Direction);
	bMoveInputThisFrame = true;
	if (AIController)
	{
		AIController->SetFocalPoint(Location + Direction * 500.0f);
	}
	return false;
}

void UYSHiredCrewComponent::TeleportNear(const AYSPlayerCharacter* Leader)
{
	AYSPlayerCharacter* Character = GetCharacter();
	if (bClimbing)
	{
		StopClimbing(TEXT("téléporté"));
	}
	if (Character->IsAtStation())
	{
		Character->LeaveStation(TEXT("téléporté"), true);
	}
	// chef sur un bateau : au centre du pont ; sinon derrière lui
	const AYSPirateShip* Ship = Leader->GetShipUnderFeet();
	const FVector From = Ship ? Ship->GetHullCenter() + FVector(0.0f, 0.0f, 600.0f) : Leader->GetActorLocation() - Leader->GetActorForwardVector() * FollowStopDistance;
	const FVector Feet = AYSInteractiveObject::FindGround(GetWorld(), From, Character);
	UE_LOG(LogTemp, Log, TEXT("[YSHiredCrew] %s trop loin du chef : téléporté en %s"), *Character->GetName(), *Feet.ToCompactString());
	Character->TeleportToFeetLocation(Feet, Leader->GetActorForwardVector());
}
