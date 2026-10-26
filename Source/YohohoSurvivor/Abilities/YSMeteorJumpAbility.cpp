#include "Abilities/YSMeteorJumpAbility.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "Weapons/YSWeapon.h"

UYSMeteorJumpAbility::UYSMeteorJumpAbility()
{
	DisplayName = NSLOCTEXT("YS", "MeteorJump", "MeteorJump");
	// toujours invulnérable (IsGrantingInvulnerability) : la case, cachée, reste cochée pour ne pas griser Extra Invulnerability
	bDashInvulnerable = true;
}

UYSMeteorJumpAbility::EJumpStage UYSMeteorJumpAbility::GetJumpStage(float& OutAlpha) const
{
	OutAlpha = 0.0f;
	float Time = GetTimeSinceActivation();
	if (!IsMovementDriver() || Time < 0.0f)
	{
		return EJumpStage::None;
	}
	const TPair<EJumpStage, float> Stages[] = {
		{ EJumpStage::StepBack, StepBackDuration },
		{ EJumpStage::Ascent, AscentDuration },
		{ EJumpStage::Apex, ApexDuration },
		{ EJumpStage::Fall, FallDuration },
		{ EJumpStage::LandHold, LandHoldDuration },
		{ EJumpStage::LandRecover, LandRecoverDuration },
	};
	for (const TPair<EJumpStage, float>& Stage : Stages)
	{
		if (Time < Stage.Value)
		{
			OutAlpha = Time / Stage.Value;
			return Stage.Key;
		}
		Time -= Stage.Value;
	}
	return EJumpStage::None;
}

float UYSMeteorJumpAbility::GetSwingElapsed() const
{
	// temps réel -> temps de l'animation (préparation = montée, coupe = chute, retour = Land Recover Duration)
	const float Time = GetTimeSinceActivation();
	if (Time < 0.0f)
	{
		return Time;
	}
	if (Time < StepBackDuration)
	{
		return 0.0f;
	}
	const float JumpTime = Time - StepBackDuration;
	if (JumpTime < AscentDuration)
	{
		return JumpTime;
	}
	if (JumpTime < AscentDuration + ApexDuration)
	{
		// dernière pose de la préparation, juste avant la coupe (pas de hitbox)
		return FMath::Max(0.0f, AscentDuration - KINDA_SMALL_NUMBER);
	}
	if (JumpTime < AscentDuration + ApexDuration + FallDuration)
	{
		return JumpTime - ApexDuration;
	}
	if (JumpTime < AscentDuration + ApexDuration + FallDuration + LandHoldDuration)
	{
		// début du retour = dernière pose de la coupe (hitbox coupée)
		return AscentDuration + FallDuration;
	}
	return JumpTime - ApexDuration - LandHoldDuration;
}

float UYSMeteorJumpAbility::GetActionDuration() const
{
	return StepBackDuration + GetDashPhaseDuration() + LandHoldDuration + LandRecoverDuration;
}

void UYSMeteorJumpAbility::ApplyPhaseKick(int32 Phase)
{
	if (Phase != 1)
	{
		Super::ApplyPhaseKick(Phase);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetHolderPawn());
	if (!Character || !IsMovementDriver())
	{
		return;
	}

	// montée : chute du CharacterMovement qui s'arrête pile au sommet en Ascent Duration
	// => gravité g = 2 h / T², vitesse verticale de départ 2 h / T ; contrôle en l'air comme au sol
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const float Height = FMath::Max(JumpApexHeight, 1.0f);
	const float Duration = FMath::Max(AscentDuration, 0.05f);
	const float Gravity = 2.0f * Height / FMath::Square(Duration);
	const float WorldGravity = FMath::Abs(GetWorld()->GetGravityZ());

	if (!bJumping)
	{
		SavedGravityScale = Movement->GravityScale;
		SavedAirControl = Movement->AirControl;
		SavedBrakingDecelerationFalling = Movement->BrakingDecelerationFalling;
		SavedFallingLateralFriction = Movement->FallingLateralFriction;
	}
	bJumping = true;
	AppliedStage = EJumpStage::Ascent;
	Movement->GravityScale = WorldGravity > KINDA_SMALL_NUMBER ? Gravity / WorldGravity : 1.0f;
	Movement->AirControl = JumpAirControl;
	Movement->BrakingDecelerationFalling = Movement->BrakingDecelerationWalking;
	Movement->FallingLateralFriction = Movement->GroundFriction;
	Movement->SetMovementMode(MOVE_Falling);
	// élan du pas en arrière ramené à la vitesse de la montée
	const FVector Horizontal = (Movement->Velocity * FVector(1.0f, 1.0f, 0.0f)).GetClampedToMaxSize(Movement->MaxWalkSpeed);
	Movement->Velocity = Horizontal + FVector(0.0f, 0.0f, 2.0f * Height / Duration);

	UE_LOG(LogTemp, Verbose, TEXT("[YSMeteorJump] %s (%s) montée à %.2f depuis (%.0f, %.0f, %.0f) : %.0f cm en %.2f s, gravité x %.2f%s"), *GetOwner()->GetName(),
		GetOwner()->HasAuthority() ? TEXT("serveur") : TEXT("client"), GetWorld()->GetTimeSeconds(), Character->GetActorLocation().X, Character->GetActorLocation().Y,
		Character->GetActorLocation().Z, Height, Duration, Movement->GravityScale, bJumpThroughFoes ? TEXT(", traverse les NME") : TEXT(", ne traverse pas les NME"));
}

void UYSMeteorJumpAbility::StartApex()
{
	AppliedStage = EJumpStage::Apex;
	const ACharacter* Character = Cast<ACharacter>(GetHolderPawn());
	if (!Character)
	{
		return;
	}
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Movement->GravityScale = 0.0f;
	Movement->Velocity.Z = 0.0f;
	UE_LOG(LogTemp, Verbose, TEXT("[YSMeteorJump] %s (%s) apex à %.2f en (%.0f, %.0f, %.0f)"), *GetOwner()->GetName(), GetOwner()->HasAuthority() ? TEXT("serveur") : TEXT("client"),
		GetWorld()->GetTimeSeconds(), Character->GetActorLocation().X, Character->GetActorLocation().Y, Character->GetActorLocation().Z);
}

void UYSMeteorJumpAbility::StartFall()
{
	AppliedStage = EJumpStage::Fall;
	const ACharacter* Character = Cast<ACharacter>(GetHolderPawn());
	if (!Character)
	{
		return;
	}

	// sol sous le personnage (décor) : chute depuis l'arrêt qui l'atteint en Fall Duration => gravité g = 2 d / T²
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight);
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(MeteorJumpFall), false, Character);
	const bool bGround = GetWorld()->LineTraceSingleByObjectType(Hit, Feet, Feet - FVector(0.0f, 0.0f, 100000.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params);
	const float Drop = FMath::Max(bGround ? Feet.Z - Hit.ImpactPoint.Z : JumpApexHeight, 1.0f);
	const float Gravity = 2.0f * Drop / FMath::Square(FMath::Max(FallDuration, 0.05f));
	const float WorldGravity = FMath::Abs(GetWorld()->GetGravityZ());

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Movement->GravityScale = WorldGravity > KINDA_SMALL_NUMBER ? Gravity / WorldGravity : 1.0f;
	Movement->Velocity = FVector::ZeroVector;
	// vitesse atteinte au sol (2 d / T) au-delà de la vitesse terminale du volume physique (4000 par défaut) : relevée,
	// sinon la chute plafonne et dure plus que Fall Duration (sans effet ailleurs : rien d'autre ne tombe aussi vite)
	APhysicsVolume* Volume = Movement->GetPhysicsVolume();
	const float FallSpeed = 2.0f * Drop / FMath::Max(FallDuration, 0.05f);
	if (Volume && Volume->TerminalVelocity < FallSpeed * 1.05f)
	{
		Volume->TerminalVelocity = FallSpeed * 1.05f;
	}

	// la coupe part dans la direction du regard au début de la chute
	const FVector Forward = Character->GetActorForwardVector().GetSafeNormal2D();
	if (!Forward.IsNearlyZero())
	{
		LockedDirection = Forward;
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSMeteorJump] %s (%s) chute à %.2f de (%.0f, %.0f, %.0f) : %.0f cm en %.2f s, gravité x %.2f"), *GetOwner()->GetName(),
		GetOwner()->HasAuthority() ? TEXT("serveur") : TEXT("client"), GetWorld()->GetTimeSeconds(), Character->GetActorLocation().X, Character->GetActorLocation().Y,
		Character->GetActorLocation().Z, Drop, FallDuration, Movement->GravityScale);
}

void UYSMeteorJumpAbility::GetCurrentEases(float& OutWindup, float& OutSwing, float& OutRecover) const
{
	Super::GetCurrentEases(OutWindup, OutSwing, OutRecover);
	OutSwing = JumpSwingEaseExponent;
}

void UYSMeteorJumpAbility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// en l'air (saut, ou chute qui le prolonge) : la traversée des NME dure jusqu'à Pass Through After Landing après l'atterrissage
	float Alpha;
	const int32 Phase = GetPhase(Alpha);
	const ACharacter* Character = Cast<ACharacter>(GetHolderPawn());
	if (Phase == 1)
	{
		bAwaitingLanding = true;
	}
	if (bAwaitingLanding)
	{
		if (Phase == 1 || (Character && Character->GetCharacterMovement()->IsFalling()))
		{
			PassThroughUntil = GetWorld()->GetTimeSeconds() + PassThroughAfterLanding;
		}
		else
		{
			bAwaitingLanding = false;
			UE_LOG(LogTemp, Verbose, TEXT("[YSMeteorJump] %s (%s) atterrit à %.2f, traverse les NME jusqu'à %.2f"), *GetOwner()->GetName(),
				GetOwner()->HasAuthority() ? TEXT("serveur") : TEXT("client"), GetWorld()->GetTimeSeconds(), PassThroughUntil);
		}
	}

	// hitbox agrandie avant les coups de cette image
	UpdateHitboxExtent();

	// départ du saut (ApplyPhaseKick), coupe et dégâts
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// étapes du saut (machine qui a lancé le saut : serveur et client qui contrôle) ; une image longue peut en sauter une
	if (!bJumping)
	{
		return;
	}
	const EJumpStage Stage = GetJumpStage(Alpha);
	if (Stage == EJumpStage::Ascent)
	{
		return;
	}
	if (Stage != EJumpStage::Apex && Stage != EJumpStage::Fall)
	{
		// fin de la chute : gravité normale (atterrissage géré par le CharacterMovement)
		EndJump();
		return;
	}
	if (AppliedStage == EJumpStage::Ascent)
	{
		StartApex();
	}
	if (Stage == EJumpStage::Fall && AppliedStage == EJumpStage::Apex)
	{
		StartFall();
	}
}

void UYSMeteorJumpAbility::UpdateHitboxExtent()
{
	const AYSWeapon* Weapon = Cast<AYSWeapon>(GetOwner());
	UBoxComponent* Hitbox = Weapon ? Weapon->FindComponentByClass<UBoxComponent>() : nullptr;
	if (!Hitbox)
	{
		return;
	}
	const float Elapsed = GetTimeSinceActivation();
	const bool bEnlarge = Elapsed >= 0.0f && Elapsed < StepBackDuration + GetDashPhaseDuration();
	if (bEnlarge == bHitboxEnlarged)
	{
		return;
	}
	if (!bHitboxEnlarged)
	{
		BaseHitboxExtent = Hitbox->GetUnscaledBoxExtent();
	}
	bHitboxEnlarged = bEnlarge;
	Hitbox->SetBoxExtent(bEnlarge ? BaseHitboxExtent * HitboxExtentMultiplier : BaseHitboxExtent);
	UE_LOG(LogTemp, Verbose, TEXT("[YSMeteorJump] %s : hitbox %s (%.0f, %.0f, %.0f)"), *GetOwner()->GetName(), bEnlarge ? TEXT("agrandie") : TEXT("normale"),
		Hitbox->GetUnscaledBoxExtent().X, Hitbox->GetUnscaledBoxExtent().Y, Hitbox->GetUnscaledBoxExtent().Z);
}

bool UYSMeteorJumpAbility::PassesThroughPawns(int32 Phase) const
{
	return bJumpThroughFoes && (Phase >= 0 || GetWorld()->GetTimeSeconds() < PassThroughUntil);
}

void UYSMeteorJumpAbility::EndJump()
{
	if (!bJumping)
	{
		return;
	}
	bJumping = false;
	AppliedStage = EJumpStage::None;
	if (const ACharacter* Character = Cast<ACharacter>(GetHolderPawn()))
	{
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		Movement->GravityScale = SavedGravityScale;
		Movement->AirControl = SavedAirControl;
		Movement->BrakingDecelerationFalling = SavedBrakingDecelerationFalling;
		Movement->FallingLateralFriction = SavedFallingLateralFriction;
		UE_LOG(LogTemp, Verbose, TEXT("[YSMeteorJump] %s (%s) fin du saut à %.2f en (%.0f, %.0f, %.0f)"), *GetOwner()->GetName(), GetOwner()->HasAuthority() ? TEXT("serveur") : TEXT("client"),
			GetWorld()->GetTimeSeconds(), Character->GetActorLocation().X, Character->GetActorLocation().Y, Character->GetActorLocation().Z);
	}
}

void UYSMeteorJumpAbility::OnActionCancelled()
{
	Super::OnActionCancelled();
	EndJump();
}

void UYSMeteorJumpAbility::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndJump();
	Super::EndPlay(EndPlayReason);
}

bool UYSMeteorJumpAbility::IsGrantingInvulnerability() const
{
	const float Elapsed = GetTimeSinceActivation();
	return IsMovementDriver() && Elapsed >= 0.0f && Elapsed < StepBackDuration + GetDashPhaseDuration() + ExtraInvulnerability;
}

float UYSMeteorJumpAbility::GetMoveSpeedMultiplier() const
{
	float Alpha;
	switch (GetJumpStage(Alpha))
	{
	case EJumpStage::StepBack:
		return Super::GetMoveSpeedMultiplier();
	case EJumpStage::Ascent:
		return AscentMoveSpeedMultiplier;
	case EJumpStage::Apex:
		return FMath::Lerp(ApexStartMoveSpeedMultiplier, ApexEndMoveSpeedMultiplier, Alpha);
	case EJumpStage::Fall:
	case EJumpStage::LandHold:
		return 0.0f;
	case EJumpStage::LandRecover:
		return Alpha;
	default:
		return 1.0f;
	}
}

bool UYSMeteorJumpAbility::GetForcedMoveDirection(FVector& OutDirection) const
{
	float Alpha;
	return GetJumpStage(Alpha) == EJumpStage::StepBack && Super::GetForcedMoveDirection(OutDirection);
}

bool UYSMeteorJumpAbility::OverridesMoveInput() const
{
	float Alpha;
	const EJumpStage Stage = GetJumpStage(Alpha);
	return Stage == EJumpStage::StepBack || Stage == EJumpStage::Fall || Stage == EJumpStage::LandHold;
}

bool UYSMeteorJumpAbility::GetLockedAimDirection(FVector& OutDirection) const
{
	float Alpha;
	const EJumpStage Stage = GetJumpStage(Alpha);
	const bool bLocked = Stage == EJumpStage::StepBack || Stage == EJumpStage::Fall || Stage == EJumpStage::LandHold
		|| (!bAimDuringJump && (Stage == EJumpStage::Ascent || Stage == EJumpStage::Apex));
	if (bLocked)
	{
		OutDirection = LockedDirection;
	}
	return bLocked;
}

#if WITH_EDITOR
void UYSMeteorJumpAbility::GetUnusedProperties(TArray<FName>& OutNames) const
{
	Super::GetUnusedProperties(OutNames);
	// le saut remplace le dash (durée, vitesse, traversée, invulnérabilité), sa courbe de coupe (Jump Swing Ease Exponent)
	// et le retour (Land Recover Duration)
	OutNames.Append({
		GET_MEMBER_NAME_CHECKED(UYSMeteorJumpAbility, DashDuration),
		GET_MEMBER_NAME_CHECKED(UYSMeteorJumpAbility, DashSpeedMultiplier),
		GET_MEMBER_NAME_CHECKED(UYSMeteorJumpAbility, DashEndSpeedMultiplier),
		GET_MEMBER_NAME_CHECKED(UYSMeteorJumpAbility, DashDecayExponent),
		GET_MEMBER_NAME_CHECKED(UYSMeteorJumpAbility, bDashThroughFoes),
		GET_MEMBER_NAME_CHECKED(UYSMeteorJumpAbility, bDashInvulnerable),
		GET_MEMBER_NAME_CHECKED(UYSMeteorJumpAbility, SwingEaseExponent),
		GET_MEMBER_NAME_CHECKED(UYSMeteorJumpAbility, RecoverDuration),
	});
}
#endif
