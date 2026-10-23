#include "Weapons/YSPlayerBomb.h"
#include "Components/StaticMeshComponent.h"
#include "Core/YSGameplayTags.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Enemies/YSFoeCharacter.h"
#include "Map/YSGroundZoneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace YSPlayerBomb
{
	/** Sphère moteur de 100 cm (rayon 50) */
	constexpr float ShapeRadius = 50.0f;

	/** Hauteur du disque au-dessus du sol */
	constexpr float ZoneHeight = 5.0f;

	/** Largeur du bord lumineux du disque (paramètre EdgeWidth de M_BrookerZone) */
	constexpr float ZoneEdgeWidth = 40.0f;
}

AYSPlayerBomb::AYSPlayerBomb()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ExplosionZone = CreateDefaultSubobject<UYSGroundZoneComponent>(TEXT("ExplosionZone"));
	ExplosionZone->SetupAttachment(Root);
	ExplosionZone->SetUsingAbsoluteLocation(true);
	ExplosionZone->SetUsingAbsoluteRotation(true);

	ExplosionSphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExplosionSphere"));
	ExplosionSphere->SetupAttachment(Root);
	ExplosionSphere->SetUsingAbsoluteScale(true);
	ExplosionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ExplosionSphere->SetCastShadow(false);
	ExplosionSphere->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Mesh->SetStaticMesh(Sphere.Object);
		ExplosionSphere->SetStaticMesh(Sphere.Object);
	}

	HitTags.AddTag(YSTags::Team_Foe);
}

void AYSPlayerBomb::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AYSPlayerBomb, Damage, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AYSPlayerBomb, Flight, COND_InitialOnly);
}

void AYSPlayerBomb::SetThrow(const FVector& Start, const FVector& Target, float Duration, float ApexHeight)
{
	Flight.bThrown = true;
	Flight.Start = Start;
	Flight.Target = Target;
	Flight.Duration = Duration;
	Flight.ApexHeight = ApexHeight;
}

FVector AYSPlayerBomb::GetFlightLocation(const FYSPlayerBombFlight& InFlight, float Time)
{
	const float Duration = FMath::Max(InFlight.Duration, 0.05f);
	const float Clamped = FMath::Clamp(Time, 0.0f, Duration);

	// cloche : sommet ApexHeight au-dessus du plus haut des deux points, atteint en Duration au total.
	// Montée sqrt(2 Up / g) + descente sqrt(2 Down / g) = Duration  =>  g = ((sqrt(2 Up) + sqrt(2 Down)) / Duration)²
	const float ApexZ = FMath::Max(InFlight.Start.Z, InFlight.Target.Z) + FMath::Max(InFlight.ApexHeight, 1.0f);
	const float Rise = FMath::Sqrt(2.0f * (ApexZ - InFlight.Start.Z)) + FMath::Sqrt(2.0f * (ApexZ - InFlight.Target.Z));
	const float Gravity = FMath::Square(Rise / Duration);
	const float VerticalSpeed = FMath::Sqrt(2.0f * Gravity * (ApexZ - InFlight.Start.Z));

	FVector Location = FMath::Lerp(InFlight.Start, InFlight.Target, Clamped / Duration);
	Location.Z = InFlight.Start.Z + VerticalSpeed * Clamped - 0.5f * Gravity * Clamped * Clamped;
	return Location;
}

void AYSPlayerBomb::BeginPlay()
{
	Super::BeginPlay();

	PlacedTime = GetWorld()->GetTimeSeconds();
	if (Flight.bThrown)
	{
		SetActorLocation(GetFlightLocation(Flight, 0.0f));
	}

	BombMaterialInstance = BombMaterial ? Mesh->CreateDynamicMaterialInstance(0, BombMaterial) : nullptr;
	SetFuseAlpha(0.0f);

	// rayon de l'explosion : disque posé sur le relief
	ExplosionZone->SetVisibility(bShowZone);
	if (bShowZone)
	{
		ExplosionZone->SetWorldLocation(GetExplosionCenter());
		ExplosionZone->BuildZone(ExplosionRadius, YSPlayerBomb::ZoneHeight);
		if (ZoneMaterial)
		{
			UMaterialInstanceDynamic* Material = ExplosionZone->CreateDynamicMaterialInstance(0, ZoneMaterial);
			Material->SetVectorParameterValue(TEXT("Color"), ZoneColor);
			Material->SetScalarParameterValue(TEXT("Glow"), 0.3f);
			Material->SetScalarParameterValue(TEXT("FillOpacity"), ZoneFillOpacity);
			Material->SetScalarParameterValue(TEXT("EdgeOpacity"), ZoneEdgeOpacity);
			Material->SetScalarParameterValue(TEXT("Radius"), ExplosionRadius);
			Material->SetScalarParameterValue(TEXT("EdgeWidth"), YSPlayerBomb::ZoneEdgeWidth);
		}
	}

	if (ExplosionMaterial)
	{
		ExplosionMaterialInstance = ExplosionSphere->CreateDynamicMaterialInstance(0, ExplosionMaterial);
		ExplosionMaterialInstance->SetVectorParameterValue(TEXT("Color"), ExplosionColor);
		ExplosionMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), ExplosionColor.A);
	}

	UE_LOG(LogTemp, Verbose, TEXT("[YSPlayerBomb] %s (%s) %s (%.0f, %.0f, %.0f) : explose dans %.2f s, %.0f dégâts rayon %.0f"), *GetName(),
		HasAuthority() ? TEXT("serveur") : TEXT("client"), Flight.bThrown ? TEXT("lancée vers") : TEXT("posée en"), GetExplosionCenter().X, GetExplosionCenter().Y, GetExplosionCenter().Z, GetTimeToExplode(), Damage, ExplosionRadius);
}

void AYSPlayerBomb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ExplosionStartTime >= 0.0)
	{
		UpdateExplosion();
		return;
	}

	// mèche (ou vol du lancer) : du noir au rouge lumineux ; à la fin, le serveur fait exploser (les clients attendent l'explosion)
	const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - PlacedTime);
	const float TimeToExplode = GetTimeToExplode();
	SetFuseAlpha(TimeToExplode > 0.0f ? Elapsed / TimeToExplode : 1.0f);
	if (Flight.bThrown)
	{
		SetActorLocation(GetFlightLocation(Flight, Elapsed));
	}
	if (HasAuthority() && !bExploded && Elapsed >= TimeToExplode)
	{
		Explode();
	}
}

void AYSPlayerBomb::SetFuseAlpha(float Alpha)
{
	if (!BombMaterialInstance)
	{
		return;
	}
	const float Clamped = FMath::Clamp(Alpha, 0.0f, 1.0f);
	BombMaterialInstance->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(BombColor, FuseColor, Clamped));
	BombMaterialInstance->SetVectorParameterValue(TEXT("Emissive"), FuseColor * (FuseGlow * Clamped));
}

void AYSPlayerBomb::Explode()
{
	bExploded = true;

	// dégâts aux NME dont la hurtbox (le corps) touche la sphère ; jamais aux joueurs
	TArray<FOverlapResult> Overlaps;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(YSPlayerBombExplosion), false, this);
	const FVector Center = GetExplosionCenter();
	SetActorLocation(Center);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(ExplosionRadius), Params);

	TSet<AYSFoeCharacter*> Damaged;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AYSFoeCharacter* Foe = Cast<AYSFoeCharacter>(Overlap.GetActor());
		if (!Foe || Damaged.Contains(Foe) || Overlap.GetComponent() != Foe->GetHurtbox() || !Foe->HasAnyMatchingGameplayTags(HitTags))
		{
			continue;
		}
		Damaged.Add(Foe);
		// direction du coup = depuis la bombe (pushback vers l'extérieur) ; pas de DamageCauser : le chiffre de dégâts s'affiche sur le NME
		const FVector ShotDirection = (Foe->GetActorLocation() - Center).GetSafeNormal2D();
		FHitResult Hit(Foe, Overlap.GetComponent(), Foe->GetActorLocation(), -ShotDirection);
		Foe->TakeDamage(Damage, FPointDamageEvent(Damage, Hit, ShotDirection, nullptr), GetInstigatorController(), nullptr);
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSPlayerBomb] %s explose en (%.0f, %.0f) : %d NME touché(s), %.0f dégâts"), *GetName(), Center.X, Center.Y, Damaged.Num(), Damage);

	MulticastExploded();
	SetLifeSpan(ExplosionGrowDuration + ExplosionFadeDuration + 0.5f);
}

void AYSPlayerBomb::MulticastExploded_Implementation()
{
	ExplosionStartTime = GetWorld()->GetTimeSeconds();
	SetActorLocation(GetExplosionCenter());

	// la bombe disparaît (tous ses composants visuels, ceux du Blueprint compris), la sphère d'explosion apparaît
	TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		Primitive->SetVisibility(false);
	}
	ExplosionSphere->SetVisibility(true);
	UpdateExplosion();
}

void AYSPlayerBomb::UpdateExplosion()
{
	// grossit de 0 au rayon, puis s'efface
	const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - ExplosionStartTime);
	const float Grow = ExplosionGrowDuration > 0.0f ? FMath::Clamp(Elapsed / ExplosionGrowDuration, 0.0f, 1.0f) : 1.0f;
	const float Fade = ExplosionFadeDuration > 0.0f ? FMath::Clamp((Elapsed - ExplosionGrowDuration) / ExplosionFadeDuration, 0.0f, 1.0f) : (Grow >= 1.0f ? 1.0f : 0.0f);

	ExplosionSphere->SetWorldScale3D(FVector(FMath::Max(ExplosionRadius * Grow / YSPlayerBomb::ShapeRadius, KINDA_SMALL_NUMBER)));
	if (ExplosionMaterialInstance)
	{
		ExplosionMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), ExplosionColor.A * (1.0f - Fade));
	}
	if (Fade >= 1.0f)
	{
		ExplosionSphere->SetVisibility(false);
	}
}
