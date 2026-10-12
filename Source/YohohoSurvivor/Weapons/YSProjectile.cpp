#include "Weapons/YSProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/YSGameplayTags.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayTagAssetInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AYSProjectile::AYSProjectile()
{
	bReplicates = true;
	SetReplicatingMovement(true);

	// requête seulement : bloque le décor, chevauche les pawns (le filtrage se fait par tags)
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(15.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.3f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (DefaultMesh.Succeeded())
	{
		Mesh->SetStaticMesh(DefaultMesh.Object);
	}

	// trajectoire rectiligne, sans gravité
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(Collision);
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;

	HitTags.AddTag(YSTags::Team_Foe);
}

void AYSProjectile::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(ColorParameterName, Color);
	}
}

void AYSProjectile::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Speed est réglable dans le Blueprint : il remplace les vitesses du composant de mouvement
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->Velocity = GetActorForwardVector() * Speed;

	Collision->OnComponentBeginOverlap.AddDynamic(this, &AYSProjectile::OnCollisionOverlap);
	Collision->OnComponentHit.AddDynamic(this, &AYSProjectile::OnCollisionHit);
}

void AYSProjectile::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(LifeTime);
}

void AYSProjectile::OnCollisionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// serveur autoritaire : la destruction est répliquée aux clients
	if (!HasAuthority() || bHasHit || !OtherActor || OtherActor == GetInstigator() || !CanHit(OtherActor))
	{
		return;
	}

	bHasHit = true;
	OtherActor->TakeDamage(Damage, FDamageEvent(), GetInstigatorController(), this);
	Destroy();
}

void AYSProjectile::OnCollisionHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (HasAuthority() && bDestroyOnWorldHit)
	{
		Destroy();
	}
}

bool AYSProjectile::CanHit(const AActor* Target) const
{
	const IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Target);
	return TagInterface && TagInterface->HasAnyMatchingGameplayTags(HitTags);
}
