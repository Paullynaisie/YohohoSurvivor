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
#include "Abilities/YSCoupDPetoirAbility.h"
#include "Components/StaticMeshComponent.h"
#include "Core/YSGameplayTags.h"
#include "Player/YSPlayerController.h"

const FName AYSPlayerCharacter::PistolSocketName(TEXT("PistolSocket_L"));

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

	// mesh et animations par défaut (mannequin du template), remplaçables dans le Blueprint
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> DefaultMesh(TEXT("/Game/PresetProject/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> DefaultAnim(TEXT("/Game/PresetProject/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));

	if (DefaultMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMeshAsset(DefaultMesh.Object);
	}

	if (DefaultAnim.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(DefaultAnim.Class);
	}

	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -96.0f), FRotator(0.0f, -90.0f, 0.0f));

	// os mis à jour même hors écran : le serveur a besoin de la vraie position du pistolet pour tirer
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// pistolet par défaut : un cube allongé (remplaçable dans le Blueprint)
	PistolMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PistolMesh"));
	PistolMesh->SetupAttachment(GetMesh(), PistolSocketName);
	PistolMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PistolMesh->SetRelativeScale3D(FVector(0.25f, 0.06f, 0.1f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultPistol(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DefaultPistol.Succeeded())
	{
		PistolMesh->SetStaticMesh(DefaultPistol.Object);
	}

	// au bout du cube (le cube fait 100 unités, soit ±50 en local)
	MuzzlePoint = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzlePoint"));
	MuzzlePoint->SetupAttachment(PistolMesh);
	MuzzlePoint->SetRelativeLocation(FVector(60.0f, 0.0f, 0.0f));

	CoupDPetoir = CreateDefaultSubobject<UYSCoupDPetoirAbility>(TEXT("CoupDPetoir"));

	GameplayTags.AddTag(YSTags::Team_Player);
}

FVector AYSPlayerCharacter::GetMuzzleLocation() const
{
	return MuzzlePoint->GetComponentLocation();
}

void AYSPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// appliqué sur le serveur ET les clients pour que la prédiction de mouvement reste cohérente
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;

	if (StrafeBlendSpace)
	{
		GetMesh()->PlayAnimation(StrafeBlendSpace, true);
	}
}

void AYSPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateStrafeAnimation(DeltaSeconds);
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
	if (const AYSPlayerController* PC = Cast<AYSPlayerController>(GetController()))
	{
		CoupDPetoir->TryActivate(PC->GetAimLocation());
	}
}
