#include "Weapons/YSProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/YSGameplayTags.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
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
	if (!HasAuthority() || bHasHit || !OtherActor || OtherActor == GetInstigator() || !CanHit(OtherActor) || HitActors.Contains(OtherActor))
	{
		return;
	}

	HitActors.Add(OtherActor);
	// direction du coup = sens de vol de la balle (pushback)
	const FVector ShotDirection = GetVelocity().IsNearlyZero() ? GetActorForwardVector() : GetVelocity().GetSafeNormal();
	const FPointDamageEvent DamageEvent(Damage, SweepResult, ShotDirection, nullptr);
	OtherActor->TakeDamage(Damage, DamageEvent, GetInstigatorController(), this);
	UE_LOG(LogTemp, Verbose, TEXT("[YSProjectile] %s touche %s : %.1f dégâts (ricochets restants %d)"), *GetName(), *OtherActor->GetName(), Damage, RemainingRicochets);

	// ricochet vers une autre cible, sinon la balle disparaît
	if (RemainingRicochets > 0 && Ricochet())
	{
		--RemainingRicochets;
		Damage *= RicochetDamageMultiplier;
		return;
	}

	bHasHit = true;
	Destroy();
}

bool AYSProjectile::Ricochet()
{
	const FVector From = GetActorLocation();
	AActor* BestTarget = nullptr;
	float BestDistanceSquared = FMath::Square(RicochetRange);
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == GetInstigator() || !CanHit(Candidate) || HitActors.Contains(Candidate))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(From, Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = Candidate;
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("[YSProjectile] %s ricochet : %s (%.0f cm)"), *GetName(), BestTarget ? *BestTarget->GetName() : TEXT("aucune cible à portée"), FMath::Sqrt(BestDistanceSquared));
	if (!BestTarget)
	{
		return false;
	}

	// trajectoire parallèle au sol vers la nouvelle cible ; la durée de vie repart de zéro
	FVector Direction = BestTarget->GetActorLocation() - From;
	Direction.Z = 0.0f;
	Direction = Direction.GetSafeNormal();
	SetActorRotation(Direction.Rotation());
	ProjectileMovement->Velocity = Direction * Speed;
	SetLifeSpan(LifeTime);
	return true;
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
