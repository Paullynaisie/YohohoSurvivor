#include "Weapons/YSPlayerBomb.h"
#include "Ship/YSShipStation.h"
#include "Components/StaticMeshComponent.h"
#include "Core/YSCombat.h"
#include "Core/YSDamageTypes.h"
#include "Core/YSGameplayTags.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
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
	DOREPLIFETIME_CONDITION(AYSPlayerBomb, bInstant, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AYSPlayerBomb, StuckActor, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AYSPlayerBomb, StuckOffset, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AYSPlayerBomb, StuckFuse, COND_InitialOnly);
	DOREPLIFETIME(AYSPlayerBomb, StuckExplodeServerTime);
}

double AYSPlayerBomb::GetServerWorldTime() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
}

void AYSPlayerBomb::SetStuckTo(AActor* Target, float Fuse, float BonusPerMine)
{
	StuckActor = Target;
	StuckFuse = FMath::Max(Fuse, 0.0f);
	StuckBonusPerMine = BonusPerMine;

	// endroit au hasard sur le corps : autour du cylindre de collision, entre le bas et le haut du torse
	float Radius = 40.0f;
	float HalfHeight = 80.0f;
	if (Target)
	{
		Target->GetSimpleCollisionCylinder(Radius, HalfHeight);
	}
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	StuckOffset = FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, FMath::FRandRange(-0.3f, 0.6f) * HalfHeight);
	if (Target)
	{
		StuckOffset = Target->GetActorQuat().UnrotateVector(StuckOffset);
	}
}

void AYSPlayerBomb::AttachToStuckActor()
{
	if (!StuckActor || GetAttachParentActor() == StuckActor)
	{
		return;
	}
	// décalage dans le repère du porteur sans son échelle (élite x3), placé en coordonnées monde puis accroché tel quel
	SetActorLocationAndRotation(StuckActor->GetActorLocation() + StuckActor->GetActorQuat().RotateVector(StuckOffset),
		FRotator(FMath::FRandRange(-40.0f, 40.0f), FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(-40.0f, 40.0f)));
	AttachToActor(StuckActor, FAttachmentTransformRules::KeepWorldTransform);
}

TArray<AYSPlayerBomb*> AYSPlayerBomb::GetStuckGroup() const
{
	TArray<AYSPlayerBomb*> Group;
	if (!StuckActor)
	{
		return Group;
	}
	TArray<AActor*> Attached;
	StuckActor->GetAttachedActors(Attached, true, false);
	for (AActor* Actor : Attached)
	{
		AYSPlayerBomb* Mine = Cast<AYSPlayerBomb>(Actor);
		if (Mine && Mine->IsStuckOn(StuckActor) && Mine->GetInstigator() == GetInstigator())
		{
			Group.AddUnique(Mine);
		}
	}
	Group.AddUnique(const_cast<AYSPlayerBomb*>(this));
	return Group;
}

void AYSPlayerBomb::ExplodeStuckGroup()
{
	TArray<AYSPlayerBomb*> Group = GetStuckGroup();
	const float Multiplier = 1.0f + StuckBonusPerMine * Group.Num();
	UE_LOG(LogTemp, Verbose, TEXT("[YSPlayerBomb] %s : %d mine(s) collée(s) sur %s explosent, dégâts x%.2f"), *GetName(), Group.Num(),
		StuckActor ? *StuckActor->GetName() : TEXT("?"), Multiplier);
	for (AYSPlayerBomb* Mine : Group)
	{
		Mine->Damage *= Multiplier;
		// le porteur peut être détruit pendant l'explosion : on décroche la mine d'abord
		Mine->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Mine->Explode();
	}
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

	// mine collée : accrochée au NME (sur chaque machine) ; explosion à la fin du délai, repoussé à chaque mine collée sur lui
	if (IsStuck())
	{
		AttachToStuckActor();
		if (HasAuthority())
		{
			StuckExplodeServerTime = GetServerWorldTime() + StuckFuse;
			for (AYSPlayerBomb* Mine : GetStuckGroup())
			{
				Mine->StuckExplodeServerTime = StuckExplodeServerTime;
				Mine->StuckBonusPerMine = StuckBonusPerMine;
				Mine->ForceNetUpdate();
			}
		}
	}

	// explosion immédiate (balle explosive, Rocket jump) : la bombe elle-même n'apparaît pas
	if (bInstant)
	{
		TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			Primitive->SetVisibility(false);
		}
	}

	// rayon de l'explosion : disque posé sur le relief (pas pour une explosion immédiate ni une mine collée, qui bouge)
	const bool bZone = bShowZone && !bInstant && !IsStuck();
	ExplosionZone->SetVisibility(bZone);
	if (bZone)
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

	// mine collée : lueur jusqu'à l'explosion ; le serveur fait exploser tout le groupe à la fin du délai ou à la mort du porteur
	if (IsStuck() || (HasAuthority() && StuckFuse > 0.0f && StuckActor == nullptr && StuckExplodeServerTime > 0.0))
	{
		AttachToStuckActor();
		const float Remaining = static_cast<float>(StuckExplodeServerTime - GetServerWorldTime());
		SetFuseAlpha(StuckFuse > 0.0f ? 1.0f - Remaining / StuckFuse : 1.0f);
		if (HasAuthority() && !bExploded)
		{
			const AYSFoeCharacter* Foe = Cast<AYSFoeCharacter>(StuckActor);
			if (!IsValid(StuckActor) || (Foe && (Foe->IsDying() || Foe->GetHealth() <= 0.0f)) || Remaining <= 0.0f)
			{
				ExplodeStuckGroup();
			}
		}
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
	AYSShipStation::DamageModulesInRadius(GetWorld(), Center, GetExplosionRadius(), HitTags, HitDamage, GetInstigatorController());
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
