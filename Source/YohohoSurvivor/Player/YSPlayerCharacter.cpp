#include "Player/YSPlayerCharacter.h"
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

AYSPlayerCharacter::AYSPlayerCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	// le personnage s'oriente dans sa direction de déplacement, pas selon le contrôleur
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.0f, 640.0f, 0.0f);
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
}

void AYSPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// appliqué sur le serveur ET les clients pour que la prédiction de mouvement reste cohérente
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
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
	}
}

void AYSPlayerCharacter::CreateDefaultInput()
{
	if (MoveAction && DefaultMappingContext)
	{
		return;
	}

	MoveAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;

	DefaultMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Default"));

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
