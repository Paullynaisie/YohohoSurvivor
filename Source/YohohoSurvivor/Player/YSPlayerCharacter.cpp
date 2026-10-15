#include "Player/YSPlayerCharacter.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "UObject/ConstructorHelpers.h"
#include "Abilities/YSAbilityComponent.h"
#include "Components/ChildActorComponent.h"
#include "Weapons/YSWeapon.h"
#include "Player/YSFaceDecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/YSGameplayTags.h"
#include "Player/YSPlayerController.h"

namespace
{
	/** En dessous de cette vitesse, le personnage est considéré à l'arrêt (pas d'inclinaison ni de sautillement) */
	constexpr float MovingSpeedThreshold = 50.0f;

	/** Au-delà de cet écart (téléportation, respawn), une main est replacée directement sur sa cible */
	constexpr float HandSnapDistance = 300.0f;
}

AYSPlayerCharacter::AYSPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	// le personnage suit le yaw du contrôleur (= direction du curseur), pas sa direction de déplacement
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->MaxWalkSpeed = MoveSpeed;
	Movement->bConstrainToPlane = true;
	Movement->bSnapToPlaneAtStart = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f));
	CameraBoom->TargetArmLength = 1400.0f;
	CameraBoom->bDoCollisionTest = false;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;

	// DA « primitives » : pas de mesh squelettique, le corps est un cylindre (BodyMesh)
	GetMesh()->SetSkeletalMeshAsset(nullptr);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -96.0f), FRotator(0.0f, -90.0f, 0.0f));

	// pivot à l'origine de la capsule : les positions relatives des visuels restent inchangées,
	// l'inclinaison est calculée autour des pieds dans UpdateLean
	BodyPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BodyPivot"));
	BodyPivot->SetupAttachment(RootComponent);

	// cylindre de la taille de la capsule (le cylindre moteur fait 100 x 100 x 100)
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(BodyPivot);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeScale3D(FVector(0.84f, 0.84f, 1.92f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultBody(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (DefaultBody.Succeeded())
	{
		BodyMesh->SetStaticMesh(DefaultBody.Object);
	}

	// visage : decal projeté sur l'avant du cylindre, vers l'arrière (axe X du decal = sens de projection)
	FaceDecal = CreateDefaultSubobject<UYSFaceDecalComponent>(TEXT("FaceDecal"));
	FaceDecal->SetupAttachment(BodyPivot);
	FaceDecal->SetRelativeLocationAndRotation(FVector(42.0f, 0.0f, 50.0f), FRotator(0.0f, 180.0f, 0.0f));
	FaceDecal->DecalSize = FVector(12.0f, 28.0f, 28.0f);

	// pistolet par défaut : un cube allongé sur le flanc gauche (remplaçable dans le Blueprint)
	// mains : un point d'attache par côté (servira aux armes) + une petite sphère visible
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultHand(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	auto CreateHand = [this](const TCHAR* SocketName, const TCHAR* MeshName, float Side, TObjectPtr<USceneComponent>& OutSocket, TObjectPtr<UStaticMeshComponent>& OutMesh)
	{
		OutSocket = CreateDefaultSubobject<USceneComponent>(SocketName);
		OutSocket->SetupAttachment(BodyPivot);
		// position monde calculée à chaque image par UpdateHands (retard + oscillation) ; la rotation suit le corps
		OutSocket->SetUsingAbsoluteLocation(true);

		OutMesh = CreateDefaultSubobject<UStaticMeshComponent>(MeshName);
		OutMesh->SetupAttachment(OutSocket);
		OutMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OutMesh->SetRelativeScale3D(FVector(0.25f));
		if (DefaultHand.Succeeded())
		{
			OutMesh->SetStaticMesh(DefaultHand.Object);
		}
	};
	CreateHand(TEXT("LeftHand"), TEXT("LeftHandMesh"), -1.0f, LeftHand, LeftHandMesh);
	CreateHand(TEXT("RightHand"), TEXT("RightHandMesh"), 1.0f, RightHand, RightHandMesh);

	// armes : acteurs enfants des mains (spawnés par le serveur et répliqués, visibles aussi dans l'éditeur)
	LeftHandWeapon = CreateDefaultSubobject<UChildActorComponent>(TEXT("LeftHandWeapon"));
	LeftHandWeapon->SetupAttachment(LeftHand);
	LeftHandWeapon->SetChildActorOwnerOnCreation(true);

	RightHandWeapon = CreateDefaultSubobject<UChildActorComponent>(TEXT("RightHandWeapon"));
	RightHandWeapon->SetupAttachment(RightHand);
	RightHandWeapon->SetChildActorOwnerOnCreation(true);

	GameplayTags.AddTag(YSTags::Team_Player);
}

AYSWeapon* AYSPlayerCharacter::GetLeftHandWeapon() const
{
	return Cast<AYSWeapon>(LeftHandWeapon->GetChildActor());
}

AYSWeapon* AYSPlayerCharacter::GetRightHandWeapon() const
{
	return Cast<AYSWeapon>(RightHandWeapon->GetChildActor());
}

void AYSPlayerCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// changer l'arme d'une main dans le Blueprint met à jour son visuel immédiatement
	for (const TPair<UChildActorComponent*, UClass*> Hand : { TPair<UChildActorComponent*, UClass*>(LeftHandWeapon, LeftHandWeaponClass), TPair<UChildActorComponent*, UClass*>(RightHandWeapon, RightHandWeaponClass) })
	{
		if (Hand.Key->GetChildActorClass() != Hand.Value)
		{
			Hand.Key->SetChildActorClass(Hand.Value);
		}
	}

	// exécuté aussi dans l'éditeur : changer le matériau du corps change celui des mains
	if (bHandsUseBodyMaterial)
	{
		for (UStaticMeshComponent* HandMesh : { LeftHandMesh.Get(), RightHandMesh.Get() })
		{
			for (int32 Index = 0; Index < HandMesh->GetNumMaterials(); ++Index)
			{
				HandMesh->SetMaterial(Index, BodyMesh->GetMaterial(0));
			}
		}
	}

	// mains à leur position cible (sans retard) : visible dans l'éditeur et point de départ en jeu
	bHandsInitialized = false;
	UpdateHands(0.0f);
}

void AYSPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// appliqué sur le serveur ET les clients pour que la prédiction de mouvement reste cohérente
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;

	// le strafe ne s'applique que si un mesh squelettique est renseigné
	if (StrafeBlendSpace && GetMesh()->GetSkeletalMeshAsset())
	{
		GetMesh()->PlayAnimation(StrafeBlendSpace, true);
	}
}

void AYSPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateStrafeAnimation(DeltaSeconds);
	UpdateBodyPivot(DeltaSeconds);
	UpdateHands(DeltaSeconds);
}

FVector AYSPlayerCharacter::GetHandTargetLocation(bool bLeft) const
{
	// l'arme tenue peut imposer sa position de main ; valeurs exprimées pour la main droite
	FVector Location = HandLocation;
	const AYSWeapon* Weapon = bLeft ? GetLeftHandWeapon() : GetRightHandWeapon();
	FVector WeaponLocation;
	if (Weapon && Weapon->GetHandLocationOverride(WeaponLocation))
	{
		Location = WeaponLocation;
	}

	if (bLeft)
	{
		Location.Y = -Location.Y;
	}
	return Location;
}

FRotator AYSPlayerCharacter::GetHandTargetRotation(bool bLeft) const
{
	FRotator Rotation = HandRotation;
	const AYSWeapon* Weapon = bLeft ? GetLeftHandWeapon() : GetRightHandWeapon();
	FRotator WeaponRotation;
	if (Weapon && Weapon->GetHandRotationOverride(WeaponRotation))
	{
		Rotation = WeaponRotation;
	}

	// symétrie par rapport au plan avant/haut du corps : Yaw et Roll s'inversent
	if (bLeft)
	{
		Rotation.Yaw = -Rotation.Yaw;
		Rotation.Roll = -Rotation.Roll;
	}
	return Rotation;
}

void AYSPlayerCharacter::UpdateHands(float DeltaSeconds)
{
	HandSwayTime += DeltaSeconds;
	const FTransform& PivotTransform = BodyPivot->GetComponentTransform();

	auto UpdateHand = [this, DeltaSeconds, &PivotTransform](USceneComponent* Hand, FVector& WorldLocation, bool bLeft, float NoiseSeed)
	{
		// oscillation : bruit de Perlin sur chaque axe, limité à HandSwayAmplitude autour de la cible
		const float NoiseTime = HandSwayTime * HandSwaySpeed + NoiseSeed;
		const FVector Sway = FVector(
			FMath::PerlinNoise1D(NoiseTime),
			FMath::PerlinNoise1D(NoiseTime + 31.7f),
			FMath::PerlinNoise1D(NoiseTime + 67.3f)) * HandSwayAmplitude;

		const FVector Target = PivotTransform.TransformPosition(GetHandTargetLocation(bLeft) + Sway.GetClampedToMaxSize(HandSwayAmplitude));

		// retard : la main rejoint sa cible progressivement (téléportation / première image : directement)
		if (!bHandsInitialized || HandFollowSpeed <= 0.0f || FVector::DistSquared(WorldLocation, Target) > FMath::Square(HandSnapDistance))
		{
			WorldLocation = Target;
		}
		else
		{
			WorldLocation = FMath::VInterpTo(WorldLocation, Target, DeltaSeconds, HandFollowSpeed);
		}
		Hand->SetWorldLocation(WorldLocation);
		Hand->SetRelativeRotation(GetHandTargetRotation(bLeft));
	};

	UpdateHand(LeftHand, LeftHandWorldLocation, true, 0.0f);
	UpdateHand(RightHand, RightHandWorldLocation, false, 100.0f);
	bHandsInitialized = true;
}

void AYSPlayerCharacter::UpdateBodyPivot(float DeltaSeconds)
{
	const FVector Velocity = GetVelocity();
	const FQuat LeanQuat = UpdateLean(DeltaSeconds, Velocity);
	const float Hop = UpdateHop(DeltaSeconds, Velocity.Size2D());

	// pivot autour des pieds (bas de la capsule) pour que le personnage reste posé au sol, puis sautillement vertical
	const FVector ActorLocation = GetActorLocation();
	const FVector Feet = ActorLocation - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector PivotLocation = Feet + LeanQuat.RotateVector(ActorLocation - Feet) + FVector(0.0f, 0.0f, Hop);
	BodyPivot->SetWorldLocationAndRotation(PivotLocation, LeanQuat * GetActorQuat());
}

float AYSPlayerCharacter::UpdateHop(float DeltaSeconds, float Speed)
{
	const bool bMoving = Speed > MovingSpeedThreshold;

	// à l'arrêt : on termine le saut en cours (à la cadence de référence) puis on reste au sol
	if (!bMoving && HopPhase <= 0.0f)
	{
		return 0.0f;
	}

	// cadence proportionnelle à la vitesse réelle, MoveSpeed servant de référence
	const float SpeedRatio = (bMoving && MoveSpeed > 0.0f) ? Speed / MoveSpeed : 1.0f;
	HopPhase += DeltaSeconds * SpeedRatio / HopDuration;
	if (HopPhase >= 1.0f)
	{
		HopPhase = bMoving ? FMath::Fmod(HopPhase, 1.0f) : 0.0f;
	}

	// montée sur la première moitié, retour sur la seconde
	const float Triangle = HopPhase < 0.5f ? HopPhase * 2.0f : (1.0f - HopPhase) * 2.0f;
	return HopHeight * UKismetMathLibrary::Ease(0.0f, 1.0f, Triangle, HopEasing, HopEaseExponent);
}

FQuat AYSPlayerCharacter::UpdateLean(float DeltaSeconds, const FVector& Velocity)
{
	// cible : MaxAngle dans la direction du déplacement (monde), indépendamment du regard
	FVector2D Target = FVector2D::ZeroVector;
	if (Velocity.SizeSquared2D() > FMath::Square(MovingSpeedThreshold))
	{
		Target = FVector2D(Velocity.X, Velocity.Y).GetSafeNormal() * LeanMaxAngle;
	}

	// nouvelle transition seulement si la cible change vraiment (sinon on la suit sans relancer la courbe)
	if (FVector2D::Distance(Target, LeanTarget) > 1.0f)
	{
		LeanStart = CurrentLean;
		LeanAlpha = 0.0f;
	}
	LeanTarget = Target;

	LeanAlpha = LeanDuration > 0.0f ? FMath::Min(1.0f, LeanAlpha + DeltaSeconds / LeanDuration) : 1.0f;
	const float EasedAlpha = UKismetMathLibrary::Ease(0.0f, 1.0f, LeanAlpha, LeanEasing, LeanEaseExponent);
	CurrentLean = FMath::Lerp(LeanStart, LeanTarget, EasedAlpha);

	// rotation monde : on bascule le haut du corps vers CurrentLean, autour d'un axe horizontal
	const float Angle = CurrentLean.Size();
	FQuat LeanQuat = FQuat::Identity;
	if (Angle > KINDA_SMALL_NUMBER)
	{
		const FVector LeanDirection(CurrentLean.X / Angle, CurrentLean.Y / Angle, 0.0f);
		LeanQuat = FQuat(FVector::CrossProduct(FVector::UpVector, LeanDirection), FMath::DegreesToRadians(Angle));
	}
	return LeanQuat;
}

void AYSPlayerCharacter::UpdateStrafeAnimation(float DeltaSeconds)
{
	UAnimSingleNodeInstance* AnimInstance = StrafeBlendSpace ? GetMesh()->GetSingleNodeInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	// la vélocité est disponible sur toutes les machines (répliquée pour les autres joueurs)
	const FVector Velocity = GetVelocity();
	const float TargetSpeed = Velocity.Size2D();

	// angle du mouvement par rapport au regard : 0 = avant, 90 = droite, +-180 = arrière
	float TargetDirection = StrafeBlendPosition.X;
	if (TargetSpeed > 10.0f)
	{
		TargetDirection = FRotator::NormalizeAxis(Velocity.Rotation().Yaw - GetActorRotation().Yaw);
	}

	if (StrafeBlendSmoothing > 0.0f)
	{
		// interpolation de l'angle par le plus court chemin pour éviter un tour complet entre -180 et 180
		const float DeltaAngle = FRotator::NormalizeAxis(TargetDirection - StrafeBlendPosition.X);
		const float Alpha = FMath::Clamp(DeltaSeconds * StrafeBlendSmoothing, 0.0f, 1.0f);
		StrafeBlendPosition.X = FRotator::NormalizeAxis(StrafeBlendPosition.X + DeltaAngle * Alpha);
		StrafeBlendPosition.Y = FMath::FInterpTo(StrafeBlendPosition.Y, TargetSpeed, DeltaSeconds, StrafeBlendSmoothing);
	}
	else
	{
		StrafeBlendPosition = FVector2D(TargetDirection, TargetSpeed);
	}

	AnimInstance->SetBlendSpacePosition(FVector(StrafeBlendPosition.X, StrafeBlendPosition.Y, 0.0f));
}

void AYSPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// appelé uniquement pour le pawn contrôlé localement
	CreateDefaultInput();

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AYSPlayerCharacter::Move);
		EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &AYSPlayerCharacter::Fire);
	}
}

void AYSPlayerCharacter::CreateDefaultInput()
{
	if (MoveAction && FireAction && DefaultMappingContext)
	{
		return;
	}

	MoveAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;

	FireAction = NewObject<UInputAction>(this, TEXT("IA_Fire"));
	FireAction->ValueType = EInputActionValueType::Boolean;

	DefaultMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Default"));
	DefaultMappingContext->MapKey(FireAction, EKeys::LeftMouseButton);

	// une touche renvoie (1,0) : Swizzle la passe sur Y, Negate l'inverse
	auto MapKey = [this](const FKey& Key, bool bSwizzle, bool bNegate)
	{
		FEnhancedActionKeyMapping& Mapping = DefaultMappingContext->MapKey(MoveAction, Key);
		if (bSwizzle)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(DefaultMappingContext));
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(DefaultMappingContext));
		}
	};

	// avant : Z (AZERTY), W (QWERTY), flèche haut
	MapKey(EKeys::Z, true, false);
	MapKey(EKeys::W, true, false);
	MapKey(EKeys::Up, true, false);

	// arrière
	MapKey(EKeys::S, true, true);
	MapKey(EKeys::Down, true, true);

	// gauche : Q (AZERTY), A (QWERTY)
	MapKey(EKeys::Q, false, true);
	MapKey(EKeys::A, false, true);
	MapKey(EKeys::Left, false, true);

	// droite
	MapKey(EKeys::D, false, false);
	MapKey(EKeys::Right, false, false);
}

void AYSPlayerCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();

	// directions relatives à la caméra (seulement le yaw) : "haut" = vers le haut de l'écran
	const FRotator CameraYaw(0.0f, CameraBoom->GetComponentRotation().Yaw, 0.0f);
	const FRotationMatrix CameraMatrix(CameraYaw);

	AddMovementInput(CameraMatrix.GetUnitAxis(EAxis::X), Input.Y);
	AddMovementInput(CameraMatrix.GetUnitAxis(EAxis::Y), Input.X);
}

void AYSPlayerCharacter::Fire()
{
	// clic gauche : action de l'arme de la main gauche
	const AYSPlayerController* PC = Cast<AYSPlayerController>(GetController());
	const AYSWeapon* Weapon = GetLeftHandWeapon();
	UYSAbilityComponent* Action = Weapon ? Weapon->GetAction() : nullptr;
	if (PC && Action)
	{
		Action->TryActivate(PC->GetAimLocation());
	}
}
