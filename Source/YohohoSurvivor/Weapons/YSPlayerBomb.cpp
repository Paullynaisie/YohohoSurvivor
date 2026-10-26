#include "Weapons/YSPlayerBomb.h"
#include "Components/StaticMeshComponent.h"
#include "Core/YSCombat.h"
#include "Core/YSDamageTypes.h"
#include "Core/YSGameplayTags.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Enemies/YSFoeCharacter.h"
#include "Player/YSPlayerCharacter.h"
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
	DOREPLIFETIME_CONDITION(AYSPlayerBomb, RadiusMultiplier, COND_InitialOnly);
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
	// cloche : sommet ApexHeight au-dessus du plus haut des deux points, atteint en Duration au total
	return YSCombat::GetBellLocation(InFlight.Start, InFlight.Target, InFlight.Duration, InFlight.ApexHeight, Time);
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
		ExplosionZone->BuildZone(GetExplosionRadius(), YSPlayerBomb::ZoneHeight);
		if (ZoneMaterial)
		{
			UMaterialInstanceDynamic* Material = ExplosionZone->CreateDynamicMaterialInstance(0, ZoneMaterial);
			Material->SetVectorParameterValue(TEXT("Color"), ZoneColor);
			Material->SetScalarParameterValue(TEXT("Glow"), 0.3f);
			Material->SetScalarParameterValue(TEXT("FillOpacity"), ZoneFillOpacity);
			Material->SetScalarParameterValue(TEXT("EdgeOpacity"), ZoneEdgeOpacity);
			Material->SetScalarParameterValue(TEXT("Radius"), GetExplosionRadius());
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
		HasAuthority() ? TEXT("serveur") : TEXT("client"), Flight.bThrown ? TEXT("lancée vers") : TEXT("posée en"), GetExplosionCenter().X, GetExplosionCenter().Y, GetExplosionCenter().Z, GetTimeToExplode(), Damage, GetExplosionRadius());
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
	if (Flight.bThrown)
	{
		SetActorLocation(GetFlightLocation(Flight, Elapsed));
	}

	// mine : armée au lieu d'exploser (même instant sur toutes les machines), sa lueur pulse ; le serveur la fait exploser
	// dès qu'un NME entre dans son rayon
	if (bMine && Elapsed >= TimeToExplode)
	{
		// déclenchée : clignotement rapide jusqu'à l'explosion
		const float Frequency = bMineTriggered ? TriggeredPulseFrequency : ArmedPulseFrequency;
		const float Pulse = 0.5f + 0.5f * FMath::Cos(2.0f * PI * Frequency * (Elapsed - TimeToExplode));
		SetFuseAlpha(FMath::Lerp(1.0f, bMineTriggered ? 0.0f : ArmedPulseMin, Pulse));
		if (!bArmedLogged)
		{
			bArmedLogged = true;
			UE_LOG(LogTemp, Verbose, TEXT("[YSPlayerBomb] %s (%s) armée en (%.0f, %.0f)"), *GetName(), HasAuthority() ? TEXT("serveur") : TEXT("client"), GetExplosionCenter().X, GetExplosionCenter().Y);
		}
		if (HasAuthority() && !bExploded)
		{
			if (MineLifetime > 0.0f && Elapsed - TimeToExplode >= MineLifetime)
			{
				UE_LOG(LogTemp, Verbose, TEXT("[YSPlayerBomb] %s disparaît sans exploser"), *GetName());
				bExploded = true;
				Destroy();
				return;
			}
			// NME détecté : explosion après Mine Trigger Delay (les NME dans le rayon à ce moment-là sont touchés)
			const double Now = GetWorld()->GetTimeSeconds();
			if (MineExplodeTime >= 0.0)
			{
				if (Now >= MineExplodeTime)
				{
					Explode();
				}
			}
			else if (Now >= NextMineCheckTime)
			{
				NextMineCheckTime = Now + MineCheckInterval;
				if (FindFoesInRadius(GetExplosionCenter()).Num() > 0)
				{
					MineExplodeTime = Now + MineTriggerDelay;
					MulticastMineTriggered();
					UE_LOG(LogTemp, Verbose, TEXT("[YSPlayerBomb] %s (mine) déclenchée : explose dans %.2f s"), *GetName(), MineTriggerDelay);
				}
			}
		}
		return;
	}

	SetFuseAlpha(TimeToExplode > 0.0f ? Elapsed / TimeToExplode : 1.0f);
	if (HasAuthority() && !bExploded && Elapsed >= TimeToExplode)
	{
		Explode();
	}
}

void AYSPlayerBomb::MulticastMineTriggered_Implementation()
{
	bMineTriggered = true;
}

TArray<AYSFoeCharacter*> AYSPlayerBomb::FindFoesInRadius(const FVector& Center) const
{
	// NME dont la hurtbox (le corps) touche la sphère ; jamais les joueurs
	TArray<FOverlapResult> Overlaps;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(YSPlayerBombExplosion), false, this);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(GetExplosionRadius()), Params);

	TArray<AYSFoeCharacter*> Foes;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AYSFoeCharacter* Foe = Cast<AYSFoeCharacter>(Overlap.GetActor());
		if (Foe && Foe->GetHealth() > 0.0f && Overlap.GetComponent() == Foe->GetHurtbox() && Foe->HasAnyMatchingGameplayTags(HitTags))
		{
			Foes.AddUnique(Foe);
		}
	}
	return Foes;
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

void AYSPlayerBomb::DetonateCritical()
{
	if (!HasAuthority() || bExploded)
	{
		return;
	}
	bCriticalExplosion = true;
	Explode();
}

void AYSPlayerBomb::Explode()
{
	bExploded = true;

	// explosion critique : dégâts critiques du personnage qui a posé la bombe
	const AYSPlayerCharacter* BombOwner = Cast<AYSPlayerCharacter>(GetInstigator());
	const float HitDamage = bCriticalExplosion && BombOwner ? Damage * BombOwner->GetCriticalDamageMultiplier() : Damage;

	// dégâts aux NME dont la hurtbox (le corps) touche la sphère ; jamais aux joueurs
	const FVector Center = GetExplosionCenter();
	SetActorLocation(Center);
	TArray<AYSFoeCharacter*> Foes = FindFoesInRadius(Center);
	// plusieurs NME touchés : le plus proche prend tous les dégâts, les suivants de moins en moins (perte multi-cibles du poseur)
	Foes.Sort([&Center](const AYSFoeCharacter& A, const AYSFoeCharacter& B) { return FVector::DistSquared(A.GetActorLocation(), Center) < FVector::DistSquared(B.GetActorLocation(), Center); });
	for (int32 Index = 0; Index < Foes.Num(); ++Index)
	{
		AYSFoeCharacter* Foe = Foes[Index];
		const float FoeDamage = BombOwner ? HitDamage * BombOwner->GetMultiHitDamageMultiplier(Index) : HitDamage;
		// direction du coup = depuis la bombe (pushback vers l'extérieur, + Extra Pushback de la Mine repulsante) ;
		// pas de DamageCauser : le chiffre de dégâts s'affiche sur le NME
		const FVector ShotDirection = (Foe->GetActorLocation() - Center).GetSafeNormal2D();
		FHitResult Hit(Foe, Foe->GetHurtbox(), Foe->GetActorLocation(), -ShotDirection);
		FYSPushbackDamageEvent DamageEvent(FoeDamage, Hit, ShotDirection, ExtraPushback);
		if (bCriticalExplosion)
		{
			DamageEvent.DamageTypeClass = UYSCriticalDamageType::StaticClass();
		}
		Foe->TakeDamage(FoeDamage, DamageEvent, GetInstigatorController(), nullptr);
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSPlayerBomb] %s %s%s en (%.0f, %.0f) : %d NME touché(s), %.0f dégâts, pushback en plus %.0f"), *GetName(), bMine ? TEXT("(mine) explose") : TEXT("explose"),
		bCriticalExplosion ? TEXT(" (CRITIQUE)") : TEXT(""), Center.X, Center.Y, Foes.Num(), HitDamage, ExtraPushback);

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

	ExplosionSphere->SetWorldScale3D(FVector(FMath::Max(GetExplosionRadius() * Grow / YSPlayerBomb::ShapeRadius, KINDA_SMALL_NUMBER)));
	if (ExplosionMaterialInstance)
	{
		ExplosionMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), ExplosionColor.A * (1.0f - Fade));
	}
	if (Fade >= 1.0f)
	{
		ExplosionSphere->SetVisibility(false);
	}
}
