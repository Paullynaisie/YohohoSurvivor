#include "Enemies/YSFoeCharacter.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/WidgetComponent.h"
#include "Core/YSCombat.h"
#include "Core/YSGameplayTags.h"
#include "Core/YSGameState.h"
#include "Player/YSFaceDecalComponent.h"
#include "Player/YSPlayerCharacter.h"
#include "UI/YSHealthBarWidget.h"

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

	// DA « primitives » comme le joueur : pas de mesh squelettique, le corps est un cylindre (BodyMesh), sans inclinaison
	GetMesh()->SetSkeletalMeshAsset(nullptr);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -96.0f), FRotator(0.0f, -90.0f, 0.0f));

	// mêmes proportions que le corps de PlayerCharacter_mouss (cylindre moteur 100 x 100 x 100)
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -20.0f));
	BodyMesh->SetRelativeScale3D(FVector(0.84f, 0.84f, 1.44f));
	YSCombat::ConfigureHurtbox(BodyMesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultBody(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (DefaultBody.Succeeded())
	{
		BodyMesh->SetStaticMesh(DefaultBody.Object);
	}

	// visage : decal projeté sur l'avant du cylindre (même placement que le joueur)
	FaceDecal = CreateDefaultSubobject<UYSFaceDecalComponent>(TEXT("FaceDecal"));
	FaceDecal->SetupAttachment(RootComponent);
	FaceDecal->SetRelativeLocationAndRotation(FVector(42.0f, 0.0f, 10.0f), FRotator(0.0f, 180.0f, 90.0f));
	FaceDecal->DecalSize = FVector(12.0f, 28.0f, 28.0f);

	// contact : un peu plus large que le corps (rayon 42), sinon les capsules empêchent tout contact
	Hitbox = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Hitbox"));
	Hitbox->SetupAttachment(RootComponent);
	Hitbox->SetRelativeLocation(FVector(0.0f, 0.0f, -20.0f));
	Hitbox->InitCapsuleSize(55.0f, 72.0f);
	YSCombat::ConfigureHitbox(Hitbox);

	GameplayTags.AddTag(YSTags::Team_Foe);
	ContactHitTags.AddTag(YSTags::Team_Player);

	// barre de vie en espace écran, juste au-dessus de la tête
	HealthBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(RootComponent);
	HealthBar->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	HealthBar->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBar->SetDrawAtDesiredSize(true);
	HealthBar->SetWidgetClass(UYSHealthBarWidget::StaticClass());
	HealthBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AYSFoeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSFoeCharacter, MaxHealth);
	DOREPLIFETIME(AYSFoeCharacter, Health);
}

void AYSFoeCharacter::OnRep_Health()
{
	UpdateHealthBar();
}

void AYSFoeCharacter::UpdateHealthBar()
{
	if (UYSHealthBarWidget* Bar = Cast<UYSHealthBarWidget>(HealthBar->GetUserWidgetObject()))
	{
		Bar->SetHealth(Health, MaxHealth);
	}
}

float AYSFoeCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Damage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (!HasAuthority() || Health <= 0.0f || Damage <= 0.0f)
	{
		return 0.0f;
	}

	Health = FMath::Max(0.0f, Health - Damage);

	// chiffre de dégâts chez tous les joueurs, au point d'impact (position du projectile)
	AYSGameState::ReportDamage(this, Damage, DamageCauser ? DamageCauser->GetActorLocation() : GetActorLocation(), EventInstigator);

	// OnRep n'est pas appelé sur le serveur : mise à jour directe pour le joueur hôte
	UpdateHealthBar();

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

UPrimitiveComponent* AYSFoeCharacter::GetHurtbox() const
{
	return BodyMesh;
}

void AYSFoeCharacter::BeginPlay()
{
	Super::BeginPlay();

	// réappliqué en jeu : les collisions de combat ne dépendent pas de réglages sauvegardés dans le Blueprint
	YSCombat::ConfigureHurtbox(BodyMesh);
	YSCombat::ConfigureHitbox(Hitbox);

	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;

	if (HasAuthority())
	{
		Health = MaxHealth;
	}

	HealthBar->InitWidget();
	UpdateHealthBar();
}

void AYSFoeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// serveur autoritaire : les clients reçoivent position et rotation par réplication
	if (!HasAuthority())
	{
		return;
	}

	ApplyContactDamage();

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

void AYSFoeCharacter::ApplyContactDamage()
{
	if (ContactDamage <= 0.0f || Health <= 0.0f)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	TArray<UPrimitiveComponent*> Touching;
	Hitbox->GetOverlappingComponents(Touching);

	for (UPrimitiveComponent* Component : Touching)
	{
		// seule la hurtbox (le corps) d'un joueur compte, pas sa capsule de mouvement
		AYSPlayerCharacter* Player = Component ? Cast<AYSPlayerCharacter>(Component->GetOwner()) : nullptr;
		if (!Player || Component != Player->GetHurtbox() || !Player->IsAlive() || !Player->HasAnyMatchingGameplayTags(ContactHitTags))
		{
			continue;
		}

		double& NextTime = NextContactDamageTime.FindOrAdd(Player, 0.0);
		if (Now < NextTime)
		{
			continue;
		}

		NextTime = Now + ContactDamageInterval;
		// pas de DamageCauser : le chiffre de dégâts s'affiche sur le joueur
		Player->TakeDamage(ContactDamage, FDamageEvent(), GetController(), nullptr);
	}
}
