#include "Summons/YSCaptainMinion.h"
#include "AIController.h"
#include "Abilities/YSAbilityComponent.h"
#include "Core/YSGameState.h"
#include "Core/YSPlayerState.h"
#include "Enemies/YSFoeCharacter.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Weapons/YSWeapon.h"

AYSCaptainMinion::AYSCaptainMinion()
{
	// auto-piloté : contrôleur d'IA créé à l'apparition (pas de PlayerState)
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// le corps entier (et les mains) prend la couleur du Captain
	CustomizationColorComponentName = TEXT("BodyPivot");
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

void AYSCaptainMinion::RefreshCustomization()
{
	const AYSPlayerState* CaptainState = Captain ? Captain->GetPlayerState<AYSPlayerState>() : nullptr;
	const AYSGameState* YSGameState = GetWorld() ? GetWorld()->GetGameState<AYSGameState>() : nullptr;
	if (CaptainState && YSGameState)
	{
		UMaterialInterface* ColorMaterial = YSGameState->GetCustomizationMaterial(CaptainState->GetColorIndex());
		UTexture2D* FaceTexture = YSGameState->GetCustomizationFace(FaceIndex);
		ApplyCustomization(ColorMaterial, FaceTexture);
		bCustomizationApplied = ColorMaterial && FaceTexture;
	}
}

float AYSCaptainMinion::GetWeaponDamageMultiplier(const AYSWeapon* Weapon, EYSActionSlot Slot) const
{
	return Captain ? Captain->GetSlotDamageMultiplier(EYSActionSlot::Ability) : 1.0f;
}

void AYSCaptainMinion::Die()
{
	UE_LOG(LogTemp, Verbose, TEXT("[YSMinion] %s meurt"), *GetName());
	Destroy();
}

void AYSCaptainMinion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || IsActorBeingDestroyed())
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

	AAIController* AIController = Cast<AAIController>(GetController());
	const FVector Location = GetActorLocation();
	const AYSFoeCharacter* Target = FindTarget();

	// cible : s'approche jusqu'à la portée de son arme, tourné vers elle (l'attaque auto part toute seule)
	FVector Goal = FVector::ZeroVector;
	float StopDistance = 0.0f;
	if (Target)
	{
		bReturningToCaptain = false;
		Goal = Target->GetActorLocation();
		StopDistance = HasRangedWeapon() ? RangedStopDistance : MeleeStopDistance;
		if (AIController)
		{
			AIController->SetFocalPoint(Goal);
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
