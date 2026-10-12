#include "Enemies/YSFoeCharacter.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Core/YSGameplayTags.h"

AYSFoeCharacter::AYSFoeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// un contrôleur IA est nécessaire pour que le CharacterMovementComponent consomme l'input
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	// la rotation est pilotée à la main vers la cible, pas par le mouvement ni le contrôleur
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->MaxWalkSpeed = MoveSpeed;
	Movement->bConstrainToPlane = true;
	Movement->bSnapToPlaneAtStart = true;

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

	GameplayTags.AddTag(YSTags::Team_Foe);
}

void AYSFoeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSFoeCharacter, Health);
}

float AYSFoeCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Damage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (!HasAuthority() || Health <= 0.0f || Damage <= 0.0f)
	{
		return 0.0f;
	}

	Health = FMath::Max(0.0f, Health - Damage);
	if (Health <= 0.0f)
	{
		Die();
	}
	return Damage;
}

void AYSFoeCharacter::Die()
{
	Destroy();
}

void AYSFoeCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// exécuté aussi dans l'éditeur : la couleur est visible dans le viewport du Blueprint
	ApplyBodyColor();
}

void AYSFoeCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;

	if (HasAuthority())
	{
		Health = MaxHealth;
	}
}

void AYSFoeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// serveur autoritaire : les clients reçoivent position et rotation par réplication
	if (!HasAuthority())
	{
		return;
	}

	Target = FindNearestPlayer();
	if (!Target)
	{
		return;
	}

	FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	AddMovementInput(ToTarget.GetSafeNormal());

	const FRotator Desired(0.0f, ToTarget.Rotation().Yaw, 0.0f);
	SetActorRotation(TurnSpeed > 0.0f ? FMath::RInterpTo(GetActorRotation(), Desired, DeltaSeconds, TurnSpeed) : Desired);
}

APawn* AYSFoeCharacter::FindNearestPlayer() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!GameState)
	{
		return nullptr;
	}

	APawn* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();

	for (const APlayerState* Player : GameState->PlayerArray)
	{
		APawn* Pawn = Player ? Player->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared2D(Pawn->GetActorLocation(), GetActorLocation());
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Pawn;
		}
	}

	return Nearest;
}

void AYSFoeCharacter::ApplyBodyColor()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp || !MeshComp->GetSkeletalMeshAsset())
	{
		return;
	}

	for (int32 Index = 0; Index < MeshComp->GetNumMaterials(); ++Index)
	{
		if (UMaterialInstanceDynamic* Material = MeshComp->CreateAndSetMaterialInstanceDynamic(Index))
		{
			Material->SetVectorParameterValue(ColorParameterName, BodyColor);
		}
	}
}
