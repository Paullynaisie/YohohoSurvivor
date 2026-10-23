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
}

void AYSPlayerBomb::BeginPlay()
{
	Super::BeginPlay();

	PlacedTime = GetWorld()->GetTimeSeconds();

	BombMaterialInstance = BombMaterial ? Mesh->CreateDynamicMaterialInstance(0, BombMaterial) : nullptr;
	SetFuseAlpha(0.0f);

	// rayon de l'explosion : disque posé sur le relief
	ExplosionZone->SetVisibility(bShowZone);
	if (bShowZone)
	{
		ExplosionZone->SetWorldLocation(GetActorLocation());
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

	UE_LOG(LogTemp, Verbose, TEXT("[YSPlayerBomb] %s (%s) posée en (%.0f, %.0f, %.0f) : mèche %.2f s, %.0f dégâts rayon %.0f"), *GetName(),
		HasAuthority() ? TEXT("serveur") : TEXT("client"), GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z, FuseDuration, Damage, ExplosionRadius);
}

void AYSPlayerBomb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ExplosionStartTime >= 0.0)
	{
		UpdateExplosion();
		return;
	}

	// mèche : du noir au rouge lumineux ; à la fin, le serveur fait exploser (les clients attendent l'explosion)
	const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - PlacedTime);
	SetFuseAlpha(FuseDuration > 0.0f ? Elapsed / FuseDuration : 1.0f);
	if (HasAuthority() && !bExploded && Elapsed >= FuseDuration)
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
	const FVector Center = GetActorLocation();
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
