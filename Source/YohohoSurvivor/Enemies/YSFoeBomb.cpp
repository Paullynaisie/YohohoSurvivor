#include "Enemies/YSFoeBomb.h"
#include "Components/StaticMeshComponent.h"
#include "Core/YSGameplayTags.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"

namespace YSFoeBomb
{
	/** Formes moteur : sphère et cylindre de 100 cm (rayon 50) */
	constexpr float ShapeRadius = 50.0f;

	/** Épaisseur du disque d'impact (échelle Z du cylindre) et hauteur au-dessus du sol */
	constexpr float ZoneThickness = 0.01f;
	constexpr float ZoneHeight = 2.0f;

	/** Rayon de la bombe en vol */
	constexpr float BombRadius = 20.0f;

	/** Largeur du bord lumineux du disque d'impact (paramètre EdgeWidth de M_BrookerZone) */
	constexpr float ZoneEdgeWidth = 40.0f;
}

AYSFoeBomb::AYSFoeBomb()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);

	BombMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BombMesh"));
	RootComponent = BombMesh;
	BombMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BombMesh->SetRelativeScale3D(FVector(YSFoeBomb::BombRadius / YSFoeBomb::ShapeRadius));

	LandingZone = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LandingZone"));
	LandingZone->SetupAttachment(BombMesh);
	LandingZone->SetUsingAbsoluteLocation(true);
	LandingZone->SetUsingAbsoluteRotation(true);
	LandingZone->SetUsingAbsoluteScale(true);
	LandingZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LandingZone->SetCastShadow(false);

	ExplosionSphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExplosionSphere"));
	ExplosionSphere->SetupAttachment(BombMesh);
	ExplosionSphere->SetUsingAbsoluteLocation(true);
	ExplosionSphere->SetUsingAbsoluteRotation(true);
	ExplosionSphere->SetUsingAbsoluteScale(true);
	ExplosionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ExplosionSphere->SetCastShadow(false);
	ExplosionSphere->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Sphere.Succeeded())
	{
		BombMesh->SetStaticMesh(Sphere.Object);
		ExplosionSphere->SetStaticMesh(Sphere.Object);
	}
	if (Cylinder.Succeeded())
	{
		LandingZone->SetStaticMesh(Cylinder.Object);
	}

	HitTags.AddTag(YSTags::Team_Player);
}

void AYSFoeBomb::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AYSFoeBomb, Throw, COND_InitialOnly);
}

float AYSFoeBomb::GetFlightDuration(const FYSBombThrow& Throw)
{
	return FMath::Max(Throw.FlightDuration, 0.05f);
}

FVector AYSFoeBomb::GetFlightLocation(const FYSBombThrow& Throw, float Time)
{
	// la bombe finit posée sur le sol (son centre à un rayon au-dessus du point d'impact)
	const FVector End = Throw.Target + FVector(0.0f, 0.0f, YSFoeBomb::BombRadius);
	const float Duration = GetFlightDuration(Throw);
	const float Clamped = FMath::Clamp(Time, 0.0f, Duration);

	// cloche : sommet ApexHeight au-dessus du plus haut des deux points, atteint en Duration au total.
	// Montée sqrt(2 Up / g) + descente sqrt(2 Down / g) = Duration  =>  g = ((sqrt(2 Up) + sqrt(2 Down)) / Duration)²
	const float ApexZ = FMath::Max(Throw.Start.Z, End.Z) + FMath::Max(Throw.ApexHeight, 1.0f);
	const float Rise = FMath::Sqrt(2.0f * (ApexZ - Throw.Start.Z)) + FMath::Sqrt(2.0f * (ApexZ - End.Z));
	const float Gravity = FMath::Square(Rise / Duration);
	const float VerticalSpeed = FMath::Sqrt(2.0f * Gravity * (ApexZ - Throw.Start.Z));

	FVector Location = FMath::Lerp(Throw.Start, End, Clamped / Duration);
	Location.Z = Throw.Start.Z + VerticalSpeed * Clamped - 0.5f * Gravity * Clamped * Clamped;
	return Location;
}

void AYSFoeBomb::BeginPlay()
{
	Super::BeginPlay();

	FlightStartTime = GetWorld()->GetTimeSeconds();
	ApplyThrowVisuals();
	SetActorLocation(GetFlightLocation(Throw, 0.0f));

	UE_LOG(LogTemp, Verbose, TEXT("[YSFoeBomb] %s (%s) : vol %.2f s + mèche %.2f s vers (%.0f, %.0f), sommet +%.0f, explosion %.0f dégâts rayon %.0f"), *GetName(),
		HasAuthority() ? TEXT("serveur") : TEXT("client"), GetFlightDuration(Throw), Throw.FuseDuration, Throw.Target.X, Throw.Target.Y, Throw.ApexHeight, Throw.Damage, Throw.ExplosionRadius);
}

void AYSFoeBomb::ApplyThrowVisuals()
{
	BombMaterialInstance = Throw.BombMaterial ? BombMesh->CreateDynamicMaterialInstance(0, Throw.BombMaterial) : BombMesh->CreateDynamicMaterialInstance(0);
	SetFuseAlpha(0.0f);

	// prévisualisation de l'impact : disque rouge au sol, de la taille de l'explosion
	const float ZoneScale = FMath::Max(Throw.ExplosionRadius, 1.0f) / YSFoeBomb::ShapeRadius;
	LandingZone->SetWorldLocationAndRotation(Throw.Target + FVector(0.0f, 0.0f, YSFoeBomb::ZoneHeight), FRotator::ZeroRotator);
	LandingZone->SetWorldScale3D(FVector(ZoneScale, ZoneScale, YSFoeBomb::ZoneThickness));
	if (Throw.LandingZoneMaterial)
	{
		UMaterialInstanceDynamic* ZoneMaterial = LandingZone->CreateDynamicMaterialInstance(0, Throw.LandingZoneMaterial);
		ZoneMaterial->SetVectorParameterValue(TEXT("Color"), Throw.LandingZoneColor);
		ZoneMaterial->SetScalarParameterValue(TEXT("Glow"), 0.3f);
		ZoneMaterial->SetScalarParameterValue(TEXT("FillOpacity"), Throw.LandingZoneFillOpacity);
		ZoneMaterial->SetScalarParameterValue(TEXT("EdgeOpacity"), Throw.LandingZoneEdgeOpacity);
		ZoneMaterial->SetScalarParameterValue(TEXT("Radius"), Throw.ExplosionRadius);
		ZoneMaterial->SetScalarParameterValue(TEXT("EdgeWidth"), YSFoeBomb::ZoneEdgeWidth);
	}

	ExplosionSphere->SetWorldLocation(Throw.Target);
	if (Throw.ExplosionMaterial)
	{
		ExplosionMaterialInstance = ExplosionSphere->CreateDynamicMaterialInstance(0, Throw.ExplosionMaterial);
		ExplosionMaterialInstance->SetVectorParameterValue(TEXT("Color"), Throw.ExplosionColor);
		ExplosionMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), Throw.ExplosionColor.A);
	}
}

void AYSFoeBomb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ExplosionStartTime >= 0.0)
	{
		UpdateExplosion();
		return;
	}

	const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - FlightStartTime);
	const float FlightDuration = GetFlightDuration(Throw);
	SetActorLocation(GetFlightLocation(Throw, Elapsed));

	// au sol : la mèche, du noir au rouge lumineux
	if (Elapsed >= FlightDuration)
	{
		SetFuseAlpha(Throw.FuseDuration > 0.0f ? (Elapsed - FlightDuration) / Throw.FuseDuration : 1.0f);
	}

	// fin de la mèche : le serveur fait exploser (les clients attendent l'explosion)
	if (HasAuthority() && !bExploded && Elapsed >= FlightDuration + FMath::Max(Throw.FuseDuration, 0.0f))
	{
		Explode();
	}
}

void AYSFoeBomb::SetFuseAlpha(float Alpha)
{
	if (!BombMaterialInstance)
	{
		return;
	}
	const float Clamped = FMath::Clamp(Alpha, 0.0f, 1.0f);
	BombMaterialInstance->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(Throw.BombColor, Throw.FuseColor, Clamped));
	BombMaterialInstance->SetVectorParameterValue(TEXT("Emissive"), Throw.FuseColor * (Throw.FuseGlow * Clamped));
}

void AYSFoeBomb::Explode()
{
	bExploded = true;

	// dégâts aux joueurs dont la hurtbox (le corps) touche la sphère ; jamais aux NME
	TArray<FOverlapResult> Overlaps;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(YSFoeBombExplosion), false, this);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Throw.Target, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Throw.ExplosionRadius), Params);

	TSet<AYSPlayerCharacter*> Damaged;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AYSPlayerCharacter* Player = Cast<AYSPlayerCharacter>(Overlap.GetActor());
		if (!Player || Damaged.Contains(Player) || Overlap.GetComponent() != Player->GetHurtbox() || !Player->IsAlive() || !Player->HasAnyMatchingGameplayTags(HitTags))
		{
			continue;
		}
		Damaged.Add(Player);
		// pas de DamageCauser : le chiffre de dégâts s'affiche sur le joueur
		Player->TakeDamage(Throw.Damage, FDamageEvent(), GetInstigatorController(), nullptr);
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSFoeBomb] %s explose en (%.0f, %.0f) : %d joueur(s) touché(s), %.0f dégâts"), *GetName(), Throw.Target.X, Throw.Target.Y, Damaged.Num(), Throw.Damage);
	for (TActorIterator<AYSPlayerCharacter> It(GetWorld()); It; ++It)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[YSFoeBomb]   %s à %.0f cm de l'impact%s"), *It->GetName(), FVector::Dist(It->GetActorLocation(), Throw.Target), Damaged.Contains(*It) ? TEXT(" (touché)") : TEXT(""));
	}

	MulticastExploded();
	SetLifeSpan(Throw.ExplosionGrowDuration + Throw.ExplosionFadeDuration + 0.5f);
}

void AYSFoeBomb::MulticastExploded_Implementation()
{
	ExplosionStartTime = GetWorld()->GetTimeSeconds();
	SetActorLocation(Throw.Target);
	BombMesh->SetVisibility(false);
	LandingZone->SetVisibility(false);
	ExplosionSphere->SetVisibility(true);
	UpdateExplosion();
}

void AYSFoeBomb::UpdateExplosion()
{
	// grossit de 0 au rayon, puis s'efface
	const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - ExplosionStartTime);
	const float Grow = Throw.ExplosionGrowDuration > 0.0f ? FMath::Clamp(Elapsed / Throw.ExplosionGrowDuration, 0.0f, 1.0f) : 1.0f;
	const float Fade = Throw.ExplosionFadeDuration > 0.0f ? FMath::Clamp((Elapsed - Throw.ExplosionGrowDuration) / Throw.ExplosionFadeDuration, 0.0f, 1.0f) : (Grow >= 1.0f ? 1.0f : 0.0f);

	ExplosionSphere->SetWorldScale3D(FVector(FMath::Max(Throw.ExplosionRadius * Grow / YSFoeBomb::ShapeRadius, KINDA_SMALL_NUMBER)));
	if (ExplosionMaterialInstance)
	{
		ExplosionMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), Throw.ExplosionColor.A * (1.0f - Fade));
	}
	if (Fade >= 1.0f)
	{
		ExplosionSphere->SetVisibility(false);
	}
}
