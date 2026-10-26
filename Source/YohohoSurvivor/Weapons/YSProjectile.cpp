#include "Weapons/YSProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/YSGameplayTags.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayTagAssetInterface.h"
#include "Map/YSOcean.h"
#include "Weapons/YSPlayerBomb.h"
#include "Core/YSDamageTypes.h"
#include "Enemies/YSFoeCharacter.h"
#include "Player/YSPlayerCharacter.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace YSProjectile
{
	/** Suivi du sol : profondeur cherchée sous la balle au tir, puis sous le dernier sol trouvé */
	constexpr float InitialGroundSearch = 2000.0f;
	constexpr float GroundSearchDown = 1000.0f;

	/** Écart minimal entre le bas de la balle et le sol */
	constexpr float MinGroundClearance = 10.0f;

	/** Un choc contre une surface au moins aussi plate (pente < 45°) est le sol, pas un mur */
	constexpr float GroundNormalZ = 0.7f;
}

AYSProjectile::AYSProjectile()
{
	// Tick = suivi du relief (activé au BeginPlay si un sol est trouvé)
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

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
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &AYSProjectile::OnProjectileStopped);
}

void AYSProjectile::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(LifeTime);

	// relief : hauteur au-dessus du sol prise au tir, gardée pendant tout le vol (même calcul sur toutes les machines)
	float GroundZ = 0.0f;
	if (bFollowGround && FindGroundZ(FVector2D(GetActorLocation().X, GetActorLocation().Y), GetActorLocation().Z, 0.0f, YSProjectile::InitialGroundSearch, GroundZ))
	{
		// au moins un peu plus que le rayon de la balle, pour ne pas frotter le sol
		HeightAboveGround = FMath::Max(GetActorLocation().Z - GroundZ, Collision->GetScaledSphereRadius() + YSProjectile::MinGroundClearance);
		LastGroundZ = GroundZ;

		// vol à plat : la hauteur ne vient que du sol
		const FVector FlatDirection = FVector(ProjectileMovement->Velocity.X, ProjectileMovement->Velocity.Y, 0.0f).GetSafeNormal();
		if (!FlatDirection.IsNearlyZero())
		{
			ProjectileMovement->Velocity = FlatDirection * Speed;
		}
	}
	SetActorTickEnabled(HeightAboveGround >= 0.0f);
	UE_LOG(LogTemp, Verbose, TEXT("[YSProjectile] %s tirée en (%.0f, %.0f, %.0f) : %.0f cm au-dessus du sol (suivi du relief %d), %d NME max"), *GetName(),
		GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z, HeightAboveGround, HeightAboveGround >= 0.0f, MaxHits);
}

bool AYSProjectile::FindGroundZ(const FVector2D& Point, float ReferenceZ, float MaxStepUp, float MaxStepDown, float& OutGroundZ) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSProjectileGround), false, this);
	Params.AddIgnoredActor(GetInstigator());
	const FVector Start(Point.X, Point.Y, ReferenceZ + MaxStepUp);
	const FVector End(Point.X, Point.Y, ReferenceZ - MaxStepDown);
	if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return false;
	}
	// au-dessus de l'eau, la balle reste au-dessus de la surface au lieu de suivre le fond
	OutGroundZ = AYSOcean::ClampAboveSea(GetWorld(), Hit.ImpactPoint.Z);
	return true;
}

void AYSProjectile::CheckBombHits()
{
	const FVector Location = GetActorLocation();
	if (PreviousLocation.IsZero())
	{
		PreviousLocation = Location;
	}
	const FVector2D From(PreviousLocation.X, PreviousLocation.Y);
	const FVector2D To(Location.X, Location.Y);
	PreviousLocation = Location;

	// à plat : la bombe est au sol, la balle vole au-dessus
	for (TActorIterator<AYSPlayerBomb> It(GetWorld()); It; ++It)
	{
		AYSPlayerBomb* Bomb = *It;
		if (Bomb->GetInstigator() != GetInstigator() || !Bomb->CanDetonate())
		{
			continue;
		}
		const FVector2D BombLocation(Bomb->GetActorLocation().X, Bomb->GetActorLocation().Y);
		const FVector2D Closest = FMath::ClosestPointOnSegment2D(BombLocation, From, To);
		if (FVector2D::Distance(Closest, BombLocation) <= BombHitRadius)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[YSProjectile] %s touche la bombe %s : explosion critique"), *GetName(), *Bomb->GetName());
			Bomb->DetonateCritical();
			bHasHit = true;
			Destroy();
			return;
		}
	}
}

void AYSProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDetonatesBombs && HasAuthority() && !bHasHit && !bRicochetPaused)
	{
		CheckBombHits();
		if (bHasHit)
		{
			return;
		}
	}

	if (HeightAboveGround < 0.0f || bRicochetPaused)
	{
		return;
	}

	// sol sous la balle et un peu devant elle (pour monter avant la pente) ;
	// sol trop haut (mur) ou absent (bord du monde) : la balle continue tout droit et heurte le mur
	FVector Location = GetActorLocation();
	const FVector2D Here(Location.X, Location.Y);
	const FVector2D Ahead = Here + FVector2D(GetVelocity().X, GetVelocity().Y) * GroundFollowLookAhead;
	float GroundZ = 0.0f;
	if (FindGroundZ(Here, LastGroundZ, GroundFollowMaxStep, YSProjectile::GroundSearchDown, GroundZ))
	{
		LastGroundZ = GroundZ;
	}
	float AheadGroundZ = LastGroundZ;
	if (GroundFollowLookAhead > 0.0f && FindGroundZ(Ahead, LastGroundZ, GroundFollowMaxStep, YSProjectile::GroundSearchDown, GroundZ))
	{
		AheadGroundZ = GroundZ;
	}

	// montée immédiate (ne jamais toucher la pente), descente adoucie
	const float TargetZ = FMath::Max(LastGroundZ, AheadGroundZ) + HeightAboveGround;
	const float NewZ = TargetZ >= Location.Z || GroundFollowInterpSpeed <= 0.0f ? TargetZ : FMath::FInterpTo(Location.Z, TargetZ, DeltaSeconds, GroundFollowInterpSpeed);
	if (!FMath::IsNearlyEqual(NewZ, Location.Z, 0.1f))
	{
		Location.Z = NewZ;
		SetActorLocation(Location);
	}
}

void AYSProjectile::LifeSpanExpired()
{
	UE_LOG(LogTemp, Verbose, TEXT("[YSProjectile] %s en fin de vie en (%.0f, %.0f, %.0f), sol %.0f"), *GetName(),
		GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z, LastGroundZ);
	Super::LifeSpanExpired();
}

void AYSProjectile::OnCollisionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// serveur autoritaire : la destruction est répliquée aux clients
	if (!HasAuthority() || bHasHit || bRicochetPaused || !OtherActor || OtherActor == GetInstigator() || !CanHit(OtherActor) || HitActors.Contains(OtherActor))
	{
		return;
	}

	HitActors.Add(OtherActor);
	// direction du coup = sens de vol de la balle (pushback)
	const FVector ShotDirection = GetVelocity().IsNearlyZero() ? GetActorForwardVector() : GetVelocity().GetSafeNormal();
	// Atk critique : le NME compte les balles reçues de ce tireur, une sur N est critique
	bool bCritical = false;
	float HitDamage = Damage;
	if (AYSFoeCharacter* Foe = CriticalBulletInterval > 0 ? Cast<AYSFoeCharacter>(OtherActor) : nullptr)
	{
		const AYSPlayerCharacter* Shooter = Cast<AYSPlayerCharacter>(GetInstigator());
		bCritical = Shooter && Foe->RegisterCriticalBullet(GetInstigator(), CriticalBulletInterval);
		HitDamage *= bCritical ? Shooter->GetCriticalDamageMultiplier() : 1.0f;
	}
	// Atk critique à la distance du point d'impact (bord du NME, gros NME compris) au tireur : Shotgun = proche, Sniper = loin
	const AYSPlayerCharacter* DistanceShooter = Cast<AYSPlayerCharacter>(GetInstigator());
	if (!bCritical && DistanceShooter && (CriticalWithinDistance > 0.0f || CriticalBeyondDistance > 0.0f) && Cast<AYSFoeCharacter>(OtherActor))
	{
		const float Distance = FVector::Dist2D(DistanceShooter->GetActorLocation(), GetActorLocation());
		if ((CriticalWithinDistance > 0.0f && Distance < CriticalWithinDistance) || (CriticalBeyondDistance > 0.0f && Distance > CriticalBeyondDistance))
		{
			bCritical = true;
			HitDamage *= DistanceShooter->GetCriticalDamageMultiplier();
		}
	}
	// minion du Captain sur le NME marqué par la Captain Sword : toujours critique
	if (!bCritical && DistanceShooter && DistanceShooter->HasForcedCriticalAgainst(OtherActor))
	{
		bCritical = true;
		HitDamage *= DistanceShooter->GetCriticalDamageMultiplier();
	}
	// balle qui traverse plusieurs NME (Sniper, Roll bomb shot) : moins de dégâts à chaque NME après le premier (pas après un ricochet)
	if (DistanceShooter && Cast<AYSFoeCharacter>(OtherActor))
	{
		HitDamage *= DistanceShooter->GetMultiHitDamageMultiplier(PierceHitIndex++);
	}
	const FPointDamageEvent DamageEvent(HitDamage, SweepResult, ShotDirection, bCritical ? UYSCriticalDamageType::StaticClass() : nullptr);
	OtherActor->TakeDamage(HitDamage, DamageEvent, GetInstigatorController(), this);

	// Captain AMMO : le dernier NME touché (encore en vie) devient le NME marqué du tireur
	AYSFoeCharacter* HitFoe = bMarksTarget ? Cast<AYSFoeCharacter>(OtherActor) : nullptr;
	AYSPlayerCharacter* Marker = Cast<AYSPlayerCharacter>(GetInstigator());
	if (HitFoe && Marker && HitFoe->GetHealth() > 0.0f && !HitFoe->IsActorBeingDestroyed())
	{
		Marker->SetMarkedFoe(HitFoe, bMarkCritsMinions, bMarkSpawnsMinion);
	}
	// Cursed Captain AMMO : un NME tué par la balle donne toujours un minion (en plus de celui de la marque s'il était marqué)
	else if (HitFoe && Marker && bMarkSpawnsMinion)
	{
		Marker->SpawnMinionAtFoe(HitFoe, TEXT("NME tué par la balle"));
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSProjectile] %s touche %s en (%.0f, %.0f, %.0f), sol %.0f : %.1f dégâts%s (ricochets restants %d)"), *GetName(), *OtherActor->GetName(),
		GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z, LastGroundZ, HitDamage, bCritical ? TEXT(" CRITIQUE") : TEXT(""), RemainingRicochets);

	// Sniper : la balle continue tout droit tant qu'elle peut encore toucher un NME (jamais deux fois le même)
	if (++HitCount < MaxHits)
	{
		return;
	}

	// ricochet vers une autre cible, sinon la balle disparaît
	if (RemainingRicochets > 0 && Ricochet())
	{
		--RemainingRicochets;
		PierceHitIndex = 0;
		Damage *= RicochetDamageMultiplier;
		return;
	}

	bHasHit = true;
	Destroy();
}

bool AYSProjectile::Ricochet()
{
	AActor* Target = FindRicochetTarget();
	UE_LOG(LogTemp, Verbose, TEXT("[YSProjectile] %s ricochet : %s"), *GetName(), Target ? *Target->GetName() : TEXT("aucune cible à portée"));
	if (!Target)
	{
		return false;
	}

	RicochetTarget = Target;
	if (RicochetPauseDuration <= 0.0f)
	{
		LaunchRicochet();
		return true;
	}

	// gamefeel : la balle s'arrête un instant sur l'impact, puis repart
	bRicochetPaused = true;
	ProjectileMovement->StopMovementImmediately();
	SetLifeSpan(LifeTime + RicochetPauseDuration);
	GetWorldTimerManager().SetTimer(RicochetPauseTimer, this, &AYSProjectile::LaunchRicochet, RicochetPauseDuration, false);
	ForceNetUpdate();
	return true;
}

void AYSProjectile::LaunchRicochet()
{
	bRicochetPaused = false;

	// la cible a pu mourir pendant l'arrêt : on en cherche une autre
	AActor* Target = RicochetTarget.IsValid() ? RicochetTarget.Get() : FindRicochetTarget();
	RicochetTarget.Reset();
	if (!Target)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[YSProjectile] %s ricochet annulé : plus de cible"), *GetName());
		bHasHit = true;
		Destroy();
		return;
	}

	// trajectoire parallèle au sol vers la nouvelle cible ; la durée de vie repart de zéro
	FVector Direction = Target->GetActorLocation() - GetActorLocation();
	Direction.Z = 0.0f;
	Direction = Direction.GetSafeNormal();
	SetActorRotation(Direction.Rotation());
	ProjectileMovement->Velocity = Direction * Speed;
	SetLifeSpan(LifeTime);
	ForceNetUpdate();
}

AActor* AYSProjectile::FindRicochetTarget() const
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

	return BestTarget;
}

bool AYSProjectile::IsGroundHit(const FHitResult& Hit) const
{
	return HeightAboveGround >= 0.0f && Hit.ImpactNormal.Z >= YSProjectile::GroundNormalZ;
}

void AYSProjectile::OnProjectileStopped(const FHitResult& Hit)
{
	// heurt d'une pente praticable (grosse image : la balle a avancé d'un coup dans la colline) : elle reprend sa hauteur et repart
	if (!IsGroundHit(Hit) || bHasHit || bRicochetPaused || IsActorBeingDestroyed())
	{
		return;
	}

	const FVector Direction = FVector(Hit.TraceEnd.X - Hit.TraceStart.X, Hit.TraceEnd.Y - Hit.TraceStart.Y, 0.0f).GetSafeNormal();
	LastGroundZ = AYSOcean::ClampAboveSea(GetWorld(), Hit.ImpactPoint.Z);
	FVector Location = GetActorLocation();
	Location.Z = LastGroundZ + HeightAboveGround;
	SetActorLocation(Location);
	ProjectileMovement->SetUpdatedComponent(Collision);
	ProjectileMovement->Velocity = (Direction.IsNearlyZero() ? GetActorForwardVector() : Direction) * Speed;
	ProjectileMovement->UpdateComponentVelocity();
	UE_LOG(LogTemp, Verbose, TEXT("[YSProjectile] %s a touché la pente en (%.0f, %.0f, %.0f) : remise à %.0f cm au-dessus du sol"), *GetName(),
		Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z, HeightAboveGround);
}

void AYSProjectile::OnCollisionHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// une pente de sol n'est pas un mur : la balle repart (OnProjectileStopped)
	if (IsGroundHit(Hit))
	{
		return;
	}

	if (HasAuthority() && bDestroyOnWorldHit)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[YSProjectile] %s arrêtée par %s en (%.0f, %.0f, %.0f), sol %.0f"), *GetName(), OtherActor ? *OtherActor->GetName() : TEXT("?"),
			Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z, LastGroundZ);
		Destroy();
	}
}

bool AYSProjectile::CanHit(const AActor* Target) const
{
	// joueur qui traverse les NME (roulade, dash, saut) : la balle passe à travers sans disparaître
	if (const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(Target); Character && Character->IsPassingThroughPawns())
	{
		return false;
	}
	const IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Target);
	return TagInterface && TagInterface->HasAnyMatchingGameplayTags(HitTags);
}
