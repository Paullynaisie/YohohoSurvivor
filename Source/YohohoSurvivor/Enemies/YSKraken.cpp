#include "Enemies/YSKraken.h"
#include "Enemies/YSKrakenArena.h"
#include "Components/CapsuleComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/YSCombat.h"
#include "Engine/DamageEvents.h"
#include "Core/YSGameState.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Map/YSGroundZoneComponent.h"
#include "Map/YSOcean.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Ship/YSPirateShip.h"
#include "Ship/YSShipStation.h"
#include "UObject/ConstructorHelpers.h"

namespace YSKraken
{
	/** Matériaux de base (chemins du projet) */
	const TCHAR* PreviewMaterialPath = TEXT("/Game/Core/Ship/M_ShipPreview.M_ShipPreview");
	const TCHAR* ZoneMaterialPath = TEXT("/Game/Art/Materials/M_BrookerZone.M_BrookerZone");
	const TCHAR* InkMaterialPath = TEXT("/Game/Art/Materials/PH_materials/m_PH_monochrome_Black.m_PH_monochrome_Black");

	/** Haut d'une tentacule (départ de la boule d'encre), dans son repère (vers le bateau = +X) */
	const FVector TentacleTip(900.0f, 0.0f, 2400.0f);

	constexpr float ZoneHeight = 8.0f;

	float EaseInOut(float Alpha) { return FMath::InterpEaseInOut(0.0f, 1.0f, FMath::Clamp(Alpha, 0.0f, 1.0f), 2.0f); }

	/** Anneau (rayons Inner -> Outer) ajouté au maillage, couleur unie */
	void AddRing(TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FLinearColor>& Colors, const FVector& Center, float Inner, float Outer,
		const FLinearColor& InnerColor, const FLinearColor& OuterColor, int32 Segments)
	{
		const int32 Base = Vertices.Num();
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const float Angle = 2.0f * PI * Index / Segments;
			const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
			Vertices.Add(Center + Dir * Inner);
			Colors.Add(InnerColor);
			Vertices.Add(Center + Dir * Outer);
			Colors.Add(OuterColor);
		}
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const int32 A = Base + Index * 2;
			Triangles.Append({ A, A + 2, A + 1, A + 1, A + 2, A + 3 });
		}
	}
}

// ==================================================================== AYSKrakenPart

AYSKrakenPart::AYSKrakenPart()
{
	PrimaryActorTick.bCanEverTick = true;

	// gros morceau du boss : ni niveau, ni XP, ni attaque de contact, ni apparition, ni ragdoll, ni nage, ni arme
	BaseHealth = 8000.0f;
	HealthPerLevel = 0.0f;
	bLevelFromDay = false;
	BaseExperience = 0.0f;
	ExperiencePerLevel = 0.0f;
	BaseContactDamage = 0.0f;
	ContactDamagePerLevel = 0.0f;
	bSpawnRise = false;
	bDeathRagdoll = false;
	bCanSwim = false;
	LeftHandedChance = 0.0f;
	// dégâts subis (réglages de Paul, 2026-11-11) : x0.65 depuis les personnages, x1 depuis les postes de combat du bateau
	PlayerDmgMultiplier = 0.65f;
	BoatDmgMultiplier = 1.0f;

	// placé sur chaque machine d'après son état (pas de mouvement répliqué), toujours connu des clients (énorme, loin)
	SetReplicateMovement(false);
	bAlwaysRelevant = true;

	GetCapsuleComponent()->InitCapsuleSize(50.0f, 50.0f);

	VisualPivot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualPivot"));
	VisualPivot->SetupAttachment(GetRootComponent());
	Visual = CreateDefaultSubobject<UChildActorComponent>(TEXT("Visual"));
	Visual->SetupAttachment(VisualPivot);
	// hurtbox (cylindre caché) : suit le visuel (inclinaison, rotation)
	GetHurtbox()->SetupAttachment(VisualPivot);
}

void AYSKrakenPart::BeginPlay()
{
	Super::BeginPlay();

	// immobile : pas de CharacterMovement (pas de chute en mer), capsule sans collision (seule la hurtbox compte)
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->SetComponentTickEnabled(false);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// seul le visuel se voit : corps, mains, visage, hitbox cachés (la hurtbox reste touchable) ; barre de vie en option
	TInlineComponentArray<USceneComponent*> Components(this);
	for (USceneComponent* Component : Components)
	{
		if (Component == VisualPivot || Component == Visual || Component->IsA<UYSGroundZoneComponent>() || Component->IsA<UProceduralMeshComponent>())
		{
			continue;
		}
		if (UWidgetComponent* Widget = Cast<UWidgetComponent>(Component))
		{
			Widget->SetRelativeLocation(FVector(0.0f, 0.0f, OverheadHealthBarHeight));
			Widget->SetHiddenInGame(!bShowOverheadHealthBar);
			continue;
		}
		Component->SetHiddenInGame(true);
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component); Primitive && Primitive != GetHurtbox())
		{
			Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	TargetableTags = GameplayTags;
	UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s en %s : %.0f PV (x%.2f pour le nombre de joueurs, %s)"), *GetName(), *GetActorLocation().ToCompactString(), MaxHealth, PlayerCountHealthMultiplier, HasAuthority() ? TEXT("serveur") : TEXT("client"));
}

void AYSKrakenPart::Tick(float DeltaSeconds)
{
	// pas de comportement de NME (poursuite, attaques, nage, mains...) : seulement celui du morceau
	ACharacter::Tick(DeltaSeconds);
	TickKraken(DeltaSeconds);
}

void AYSKrakenPart::SetTargetable(bool bTargetable)
{
	if (bTargetable == bTargetableApplied)
	{
		return;
	}
	bTargetableApplied = bTargetable;
	// sous l'eau : ni ciblé (attaques auto, canons, visée assistée), ni touché
	if (bTargetable)
	{
		GameplayTags = TargetableTags;
	}
	else
	{
		GameplayTags.Reset();
	}
	GetHurtbox()->SetCollisionEnabled(bTargetable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (UWidgetComponent* Widget = FindComponentByClass<UWidgetComponent>())
	{
		Widget->SetHiddenInGame(!bShowOverheadHealthBar || !bTargetable);
	}
	if (AActor* VisualActor = Visual->GetChildActor())
	{
		VisualActor->SetActorHiddenInGame(!bTargetable);
	}
}

void AYSKrakenPart::GatherHitColorMeshes(TArray<UMeshComponent*>& OutMeshes) const
{
	if (const AActor* VisualActor = Visual ? Visual->GetChildActor() : nullptr)
	{
		TInlineComponentArray<UMeshComponent*> Meshes(VisualActor);
		for (UMeshComponent* PropMesh : Meshes)
		{
			if (PropMesh->IsA<UStaticMeshComponent>() || PropMesh->IsA<USkeletalMeshComponent>())
			{
				OutMeshes.Add(PropMesh);
			}
		}
	}
}

void AYSKrakenPart::Die()
{
	UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s meurt"), *GetName());
	if (AYSKrakenArena* KrakenArena = Arena.Get())
	{
		KrakenArena->OnPartKilled(this);
	}
	Super::Die();
}

void AYSKrakenPart::Vanish(const TCHAR* Reason)
{
	UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s disparaît (%s)"), *GetName(), Reason);
	Destroy();
}

AYSPirateShip* AYSKrakenPart::GetGameBoat() const
{
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	AYSPirateShip* Boat = YSGameState ? YSGameState->GetGameBoat() : nullptr;
	return IsValid(Boat) && !Boat->IsSinking() ? Boat : nullptr;
}

double AYSKrakenPart::GetServerTime() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

float AYSKrakenPart::GetSeaLevel() const
{
	const AYSOcean* Ocean = AYSOcean::Find(GetWorld());
	return Ocean ? Ocean->GetSeaLevel() : 0.0f;
}

// ==================================================================== AYSKrakenHead

AYSKrakenHead::AYSKrakenHead()
{
	FoeName = NSLOCTEXT("YS", "KrakenHeadName", "Kraken");
	BaseHealth = 25000.0f;
	bShowOverheadHealthBar = false;

	// hurtbox : gros cylindre de la mer au sommet du manteau
	GetHurtbox()->SetRelativeLocation(FVector(0.0f, 0.0f, 2200.0f));
	GetHurtbox()->SetRelativeScale3D(FVector(36.0f, 36.0f, 52.0f));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PreviewMaterial(YSKraken::PreviewMaterialPath);
	if (PreviewMaterial.Succeeded())
	{
		VfxMaterial = PreviewMaterial.Object;
	}
}

void AYSKrakenHead::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AYSKrakenHead, SuctionStartServerTime);
	DOREPLIFETIME(AYSKrakenHead, SmashServerTime);
}

void AYSKrakenHead::BeginPlay()
{
	Super::BeginPlay();

	SuctionVfx = NewObject<UProceduralMeshComponent>(this, TEXT("SuctionVfx"));
	SuctionVfx->SetupAttachment(GetRootComponent());
	SuctionVfx->SetUsingAbsoluteRotation(true);
	SuctionVfx->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SuctionVfx->SetCastShadow(false);
	SuctionVfx->RegisterComponent();
	SuctionVfx->SetVisibility(false);
}

void AYSKrakenHead::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// plus de dérive sur le bateau si la tête disparaît pendant l'aspiration
	if (HasAuthority() && SuctionStartServerTime > 0.0)
	{
		if (AYSPirateShip* Boat = GetGameBoat())
		{
			Boat->SetExternalDrift(FVector::ZeroVector);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AYSKrakenHead::TickKraken(float DeltaSeconds)
{
	UpdateFacing(DeltaSeconds);
	VisualPivot->SetRelativeRotation(FRotator(-GetSmashPitch(), CurrentYaw, 0.0f));
	if (HasAuthority())
	{
		TickHeadServer(DeltaSeconds);
	}
	UpdateSuctionVfx();
}

void AYSKrakenHead::UpdateFacing(float DeltaSeconds)
{
	// regarde le bateau des joueurs (sinon le joueur vivant le plus proche) ; même calcul sur toutes les machines
	FVector LookTarget = FVector::ZeroVector;
	bool bHasTarget = false;
	if (const AYSPirateShip* Boat = GetGameBoat())
	{
		LookTarget = Boat->GetHullCenter();
		bHasTarget = true;
	}
	else
	{
		float BestDistance = TNumericLimits<float>::Max();
		for (TActorIterator<AYSPlayerCharacter> It(GetWorld()); It; ++It)
		{
			const float Distance = FVector::DistSquared2D(It->GetActorLocation(), GetActorLocation());
			if (!It->IsDying() && It->IsPlayerControlled() && Distance < BestDistance)
			{
				BestDistance = Distance;
				LookTarget = It->GetActorLocation();
				bHasTarget = true;
			}
		}
	}
	if (!bHasTarget)
	{
		return;
	}
	const float WantedYaw = (LookTarget - GetActorLocation()).GetSafeNormal2D().Rotation().Yaw;
	if (!bYawInitialized || HeadTurnSpeed <= 0.0f)
	{
		CurrentYaw = WantedYaw;
		bYawInitialized = true;
		return;
	}
	CurrentYaw = FMath::RInterpTo(FRotator(0.0f, CurrentYaw, 0.0f), FRotator(0.0f, WantedYaw, 0.0f), DeltaSeconds, HeadTurnSpeed).Yaw;
}

float AYSKrakenHead::GetSmashPitch() const
{
	if (SmashServerTime <= 0.0)
	{
		return 0.0f;
	}
	const float Elapsed = static_cast<float>(GetServerTime() - SmashServerTime);
	if (Elapsed < 0.0f || Elapsed > SmashAnimDuration)
	{
		return 0.0f;
	}
	// s'abat vite, se relève plus lentement
	const float Alpha = Elapsed / SmashAnimDuration;
	const float Curve = Alpha < 0.3f ? FMath::InterpEaseOut(0.0f, 1.0f, Alpha / 0.3f, 2.0f) : FMath::InterpEaseInOut(1.0f, 0.0f, (Alpha - 0.3f) / 0.7f, 2.0f);
	return SmashAnimPitch * Curve;
}

bool AYSKrakenHead::IsTouching(const AYSPirateShip* Boat) const
{
	return Boat && Boat->IsInsideHull(GetActorLocation(), ContactRadius);
}

void AYSKrakenHead::TickHeadServer(float DeltaSeconds)
{
	AYSPirateShip* Boat = GetGameBoat();
	const double Now = GetServerTime();
	if (NextSuctionServerTime < 0.0)
	{
		NextSuctionServerTime = Now + FirstSuctionDelay;
	}

	// aspiration toutes les Suction Cooldown : le bateau est tiré vers la tête à sa vitesse de base pendant Suction Duration
	const bool bSuction = SuctionStartServerTime > 0.0;
	if (!bSuction && Boat && Now >= NextSuctionServerTime)
	{
		SuctionStartServerTime = Now;
		ForceNetUpdate();
		UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s aspire %s (%.0f s, %.0f cm/s)"), *GetName(), *Boat->GetName(), SuctionDuration, Boat->GetBaseMaxForwardSpeed() * SuctionSpeedMultiplier);
	}
	if (SuctionStartServerTime > 0.0)
	{
		if (!Boat || Now - SuctionStartServerTime >= SuctionDuration)
		{
			StopSuction(TEXT("fin des 10 s sans toucher le bateau"));
		}
		else
		{
			const FVector Toward = (GetActorLocation() - Boat->GetHullCenter()).GetSafeNormal2D();
			Boat->SetExternalDrift(Toward * Boat->GetBaseMaxForwardSpeed() * SuctionSpeedMultiplier);
		}
	}

	// contact : coup pendant l'aspiration, simple poussée sinon
	if (Boat && IsTouching(Boat) && Now - LastContactServerTime > 1.0)
	{
		LastContactServerTime = Now;
		Smash(Boat, SuctionStartServerTime > 0.0);
	}
}

void AYSKrakenHead::StopSuction(const TCHAR* Reason)
{
	if (SuctionStartServerTime <= 0.0)
	{
		return;
	}
	SuctionStartServerTime = 0.0;
	NextSuctionServerTime = GetServerTime() + SuctionCooldown;
	ForceNetUpdate();
	if (AYSPirateShip* Boat = GetGameBoat())
	{
		Boat->SetExternalDrift(FVector::ZeroVector);
	}
	UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s : fin de l'aspiration (%s), prochaine dans %.0f s"), *GetName(), Reason, SuctionCooldown);
}

void AYSKrakenHead::Smash(AYSPirateShip* Boat, bool bWithDamage)
{
	if (!Boat)
	{
		return;
	}
	const FVector Away = (Boat->GetHullCenter() - GetActorLocation()).GetSafeNormal2D();
	if (!bWithDamage)
	{
		Boat->ApplyKnockback(Away * ContactKnockbackSpeed);
		UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s : %s touche la tête hors aspiration, repoussé"), *GetName(), *Boat->GetName());
		return;
	}
	// immense coup : bateau repoussé, Smash Damage à chaque poste qui a des PV et au core ; fin de l'aspiration
	SmashServerTime = GetServerTime();
	ForceNetUpdate();
	StopSuction(TEXT("le bateau a touché la tête"));
	Boat->ApplyKnockback(Away * SmashKnockbackSpeed);
	int32 Posts = 0;
	for (AYSShipStation* Station : Boat->GetStations())
	{
		if (Station && Station->GetMaxHealth() > 0.0f && Station->GetHealth() > 0.0f)
		{
			Station->TakeDamage(SmashDamage, FDamageEvent(), nullptr, this);
			++Posts;
		}
	}
	if (IsValid(Boat))
	{
		Boat->TakeCoreDamage(SmashDamage, nullptr, GetActorLocation());
	}
	UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s frappe %s : %.0f dégâts à %d poste(s) et au core"), *GetName(), *GetNameSafe(Boat), SmashDamage, Posts);
}

void AYSKrakenHead::UpdateSuctionVfx()
{
	const bool bActive = SuctionStartServerTime > 0.0 && SuctionVfx;
	if (!bActive)
	{
		if (bVfxVisible && SuctionVfx)
		{
			bVfxVisible = false;
			SuctionVfx->SetVisibility(false);
			SuctionVfx->ClearAllMeshSections();
		}
		return;
	}
	if (!bVfxVisible)
	{
		bVfxVisible = true;
		SuctionVfx->SetVisibility(true);
		SuctionVfx->SetMaterial(0, VfxMaterial);
	}

	const float Time = static_cast<float>(GetServerTime() - SuctionStartServerTime);
	const FVector Center(0.0f, 0.0f, VfxHeightAboveSea);
	const float Inner = ContactRadius;
	const float Outer = FMath::Max(VfxOuterRadius, Inner + 100.0f);
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FLinearColor> Colors;

	// anneaux qui se resserrent vers la tête, plus visibles au milieu de leur course
	for (int32 Ring = 0; Ring < VfxRingCount; ++Ring)
	{
		const float Phase = FMath::Frac(Time / VfxRingPeriod + static_cast<float>(Ring) / VfxRingCount);
		const float RingRadius = FMath::Lerp(Outer, Inner, Phase);
		FLinearColor Color = VfxColor;
		Color.A = VfxColor.A * FMath::Sin(PI * Phase);
		YSKraken::AddRing(Vertices, Triangles, Colors, Center, RingRadius, RingRadius + VfxRingWidth, Color, Color, 96);
	}

	// bras en spirale qui tournent et s'enroulent vers la tête
	constexpr int32 ArmSteps = 24;
	for (int32 Arm = 0; Arm < VfxSpiralArms; ++Arm)
	{
		const int32 Base = Vertices.Num();
		for (int32 Step = 0; Step <= ArmSteps; ++Step)
		{
			const float T = static_cast<float>(Step) / ArmSteps;
			const float R = FMath::Lerp(Outer, Inner, T);
			const float Angle = FMath::DegreesToRadians(360.0f * Arm / FMath::Max(VfxSpiralArms, 1) + VfxSpinSpeed * Time + 240.0f * T);
			const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
			const FVector Side(-Dir.Y, Dir.X, 0.0f);
			const float Width = FMath::Lerp(VfxRingWidth * 1.2f, VfxRingWidth * 0.4f, T);
			FLinearColor Color = VfxColor;
			Color.A = VfxColor.A * 0.7f * FMath::Sin(PI * T);
			Vertices.Add(Center + Dir * R - Side * Width * 0.5f);
			Vertices.Add(Center + Dir * R + Side * Width * 0.5f);
			Colors.Add(Color);
			Colors.Add(Color);
		}
		for (int32 Step = 0; Step < ArmSteps; ++Step)
		{
			const int32 A = Base + Step * 2;
			Triangles.Append({ A, A + 2, A + 1, A + 1, A + 2, A + 3 });
		}
	}
	SuctionVfx->CreateMeshSection_LinearColor(0, Vertices, Triangles, TArray<FVector>(), TArray<FVector2D>(), Colors, TArray<FProcMeshTangent>(), false);
	SuctionVfx->SetMaterial(0, VfxMaterial);
}

// ==================================================================== AYSKrakenHeadFinal

AYSKrakenHeadFinal::AYSKrakenHeadFinal()
{
	FoeName = NSLOCTEXT("YS", "KrakenFinalName", "Kraken");
	BaseHealth = 40000.0f;
}

void AYSKrakenHeadFinal::TickHeadServer(float DeltaSeconds)
{
	AYSPirateShip* Boat = GetGameBoat();
	const double Now = GetServerTime();

	// pas d'aspiration : un bateau qui touche la tête est seulement repoussé
	if (Boat && IsTouching(Boat) && Now - LastContactServerTime > 1.0)
	{
		LastContactServerTime = Now;
		Smash(Boat, false);
	}

	// alterne Pattern Interval d'attente et un motif au hasard
	if (NextPatternServerTime < 0.0)
	{
		NextPatternServerTime = Now + PatternInterval;
	}
	if (Now < NextPatternServerTime || Patterns.Num() == 0)
	{
		return;
	}
	int32 Pick = FMath::RandRange(0, Patterns.Num() - 1);
	if (Patterns.Num() > 1 && Pick == LastPattern)
	{
		Pick = (Pick + FMath::RandRange(1, Patterns.Num() - 1)) % Patterns.Num();
	}
	LastPattern = Pick;
	const float PatternLength = FirePattern(Patterns[Pick]);
	NextPatternServerTime = Now + PatternLength + PatternInterval;
}

float AYSKrakenHeadFinal::FirePattern(EYSKrakenInkPattern Pattern)
{
	const FVector Center = GetActorLocation();
	const float Sea = GetSeaLevel();
	const AYSKrakenArena* KrakenArena = Arena.Get();
	const float Outer = MaxPatternRadius > 0.0f ? MaxPatternRadius : (KrakenArena ? KrakenArena->GetRadius() : 20000.0f) - ShotRadius;
	const float Inner = FMath::Min(MinPatternRadius, Outer);
	const float Spacing = FMath::Max(ShotSpacing, 50.0f);
	float UsedGap = 0.0f;

	TArray<FYSKrakenInkShot> Shots;
	const auto AddShot = [&](float Radius, float AngleDeg, float Delay)
	{
		const FVector Dir = FRotator(0.0f, AngleDeg, 0.0f).Vector();
		FYSKrakenInkShot Shot;
		Shot.Target = FVector(Center.X + Dir.X * Radius, Center.Y + Dir.Y * Radius, Sea);
		Shot.Delay = Delay;
		Shots.Add(Shot);
	};

	const TCHAR* PatternName = TEXT("?");
	switch (Pattern)
	{
	case EYSKrakenInkPattern::Rings:
	{
		// cercles concentriques : d'un cercle au suivant, deux demi-impacts et l'écart libre où tient le bateau ; le premier à un rayon
		// au hasard (placement jamais le même) parmi ceux qui laissent tenir tous les cercles ; décalage angulaire au hasard
		PatternName = TEXT("cercles");
		const int32 Rings = FMath::Max(RingCount, 1);
		const float RingOuter = RingMaxRadius > 0.0f ? RingMaxRadius : (KrakenArena ? KrakenArena->GetRadius() : 20000.0f);
		const float RingInner = FMath::Min(RingMinRadius, RingOuter);
		float Pitch = 2.0f * ShotRadius + RingBoatGap;
		if (Rings > 1 && RingInner + (Rings - 1) * Pitch > RingOuter)
		{
			Pitch = (RingOuter - RingInner) / (Rings - 1);
			UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s : %d cercles ne tiennent pas avec un écart de %.0f, écart réduit à %.0f"), *GetName(), Rings, RingBoatGap, Pitch - 2.0f * ShotRadius);
		}
		UsedGap = Pitch - 2.0f * ShotRadius;
		const float FirstMax = FMath::Min(FMath::Max(FirstRingRadius.X, FirstRingRadius.Y), RingOuter - (Rings - 1) * Pitch);
		const float FirstMin = FMath::Min(FMath::Max(FirstRingRadius.X, RingInner), FirstMax);
		float Radius = FMath::FRandRange(FirstMin, FirstMax);
		const float Offset = FMath::FRandRange(0.0f, 360.0f);
		for (int32 Ring = 0; Ring < Rings && Radius <= RingOuter + 1.0f; ++Ring, Radius += Pitch)
		{
			const int32 Count = FMath::Max(FMath::CeilToInt(2.0f * PI * Radius / Spacing), 3);
			for (int32 K = 0; K < Count; ++K)
			{
				AddShot(Radius, Offset + 360.0f * K / Count, RingInterval * Ring);
			}
		}
		break;
	}
	case EYSKrakenInkPattern::Lines:
	{
		// lignes du centre jusqu'au bord, réparties autour (orientation au hasard), qui avancent vers l'extérieur
		PatternName = TEXT("lignes");
		const int32 Lines = FMath::Max(LineCount, 1);
		const float Offset = FMath::FRandRange(0.0f, 360.0f);
		for (int32 Line = 0; Line < Lines; ++Line)
		{
			int32 Step = 0;
			for (float Radius = Inner; Radius <= Outer + 1.0f; Radius += Spacing, ++Step)
			{
				AddShot(Radius, Offset + 360.0f * Line / Lines, LineStepDelay * Step);
			}
		}
		break;
	}
	case EYSKrakenInkPattern::Spiral:
	{
		// spirale d Archimède : le rayon grandit de deux demi-impacts + Spiral Boat Gap à chaque tour, un impact tous les Spacing le long de la courbe
		PatternName = TEXT("spirale");
		const float Sign = FMath::RandBool() ? 1.0f : -1.0f;
		const float StartAngle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Pitch = 2.0f * ShotRadius + SpiralBoatGap;
		UsedGap = SpiralBoatGap;
		const float B = Pitch / (2.0f * PI);
		float Radius = FMath::Clamp(FMath::FRandRange(SpiralStartRadius.X, FMath::Max(SpiralStartRadius.X, SpiralStartRadius.Y)), Inner, Outer);
		float Angle = 0.0f;
		TArray<FVector2D> Points;
		while (Radius <= Outer + 1.0f && Points.Num() < 2000)
		{
			Points.Add(FVector2D(Radius, Angle));
			// longueur d'arc ~ sqrt(r^2 + b^2) dθ
			const float DAngle = Spacing / FMath::Sqrt(Radius * Radius + B * B);
			Angle += DAngle;
			Radius += B * DAngle;
		}
		for (int32 K = 0; K < Points.Num(); ++K)
		{
			const float T = Points.Num() > 1 ? static_cast<float>(K) / (Points.Num() - 1) : 0.0f;
			AddShot(Points[K].X, FMath::RadiansToDegrees(StartAngle + Sign * Points[K].Y), SpiralDuration * T);
		}
		break;
	}
	}

	float MaxDelay = 0.0f;
	for (const FYSKrakenInkShot& Shot : Shots)
	{
		MaxDelay = FMath::Max(MaxDelay, Shot.Delay);
	}
	// toutes les boules partent du dessus de la tête ; affichées et touchées comme celles des tentacules
	const FVector Start(Center.X, Center.Y, Sea + ShotLaunchHeight);
	const FTransform Transform(FRotator::ZeroRotator, FVector(Center.X, Center.Y, Sea));
	AYSKrakenInkBarrage* Barrage = GetWorld()->SpawnActorDeferred<AYSKrakenInkBarrage>(AYSKrakenInkBarrage::StaticClass(), Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Barrage)
	{
		Barrage->Init(Start, ShotFlightDuration, ShotApexHeight, ShotRadius, ShotBallSize, MarkerLeadTime, ShotDamage, Shots);
		Barrage->FinishSpawning(Transform);
	}
	UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s : motif %s, %d tirs d'encre en cloche (%.0f dégâts chacun, écart %.0f, de %.0f à %.0f du centre, %.1f s)"),
		*GetName(), PatternName, Shots.Num(), ShotDamage, UsedGap, Inner, Outer, MaxDelay + ShotFlightDuration);
	return MarkerLeadTime + MaxDelay + ShotFlightDuration;
}

// ==================================================================== AYSKrakenTentacle

AYSKrakenTentacle::AYSKrakenTentacle()
{
	FoeName = NSLOCTEXT("YS", "KrakenTentacleName", "Tentacule");
	BaseHealth = 4000.0f;
	bShowOverheadHealthBar = true;

	// hurtbox : long cylindre penché vers l'avant, du pied au sommet de la courbe
	GetHurtbox()->SetRelativeLocation(FVector(450.0f, 0.0f, 1200.0f));
	GetHurtbox()->SetRelativeRotation(FRotator(-20.0f, 0.0f, 0.0f));
	GetHurtbox()->SetRelativeScale3D(FVector(9.0f, 9.0f, 26.0f));
}

void AYSKrakenTentacle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AYSKrakenTentacle, State);
}

void AYSKrakenTentacle::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		// sous l'eau là où l'arène l'a posée, sort après First Emerge Delay
		State.Phase = EYSKrakenTentaclePhase::Submerged;
		State.Location = FVector(GetActorLocation().X, GetActorLocation().Y, GetSeaLevel());
		State.PhaseStartServerTime = GetServerTime();
		NextEmergeServerTime = GetServerTime() + (bEmergeImmediately ? 0.0f : FirstEmergeDelay + EmergeStagger);
	}
	ApplyStateLocally();
}

void AYSKrakenTentacle::OnRep_State()
{
	ApplyStateLocally();
}

void AYSKrakenTentacle::Vanish(const TCHAR* Reason)
{
	Super::Vanish(Reason);
}

float AYSKrakenTentacle::GetPhaseElapsed() const
{
	return FMath::Max(static_cast<float>(GetServerTime() - State.PhaseStartServerTime), 0.0f);
}

void AYSKrakenTentacle::SetPhase(EYSKrakenTentaclePhase Phase)
{
	State.Phase = Phase;
	State.PhaseStartServerTime = GetServerTime();
	ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("[YSKraken] %s : phase %d en %s"), *GetName(), static_cast<int32>(Phase), *State.Location.ToCompactString());
	ApplyStateLocally();
}

void AYSKrakenTentacle::TickKraken(float DeltaSeconds)
{
	if (HasAuthority())
	{
		TickTentacleServer(DeltaSeconds);
		// retrait demandé (bouclier de Davy Jones brisé) : dès que sa prochaine attaque est lancée
		if (VanishAfterAttackRequestTime > 0.0 && State.Phase == EYSKrakenTentaclePhase::Recover && State.PhaseStartServerTime >= VanishAfterAttackRequestTime)
		{
			Vanish(TEXT("attaque lancée après la fin du bouclier"));
			return;
		}
	}
	ApplyStateLocally();
}

void AYSKrakenTentacle::VanishAfterNextAttack()
{
	if (HasAuthority() && VanishAfterAttackRequestTime <= 0.0)
	{
		VanishAfterAttackRequestTime = FMath::Max(GetServerTime(), 0.001);
		UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s disparaîtra après sa prochaine attaque"), *GetName());
	}
}

FVector AYSKrakenTentacle::ProjectOntoShips(const FVector& ImpactPoint) const
{
	// un bateau sous ce point (bateau des joueurs, Galions de Davy Jones) : l'impact est sur son pont, pas sur l'eau
	const float Sea = GetSeaLevel();
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSKrakenInkDeck), false, this);
	GetWorld()->LineTraceMultiByObjectType(Hits, FVector(ImpactPoint.X, ImpactPoint.Y, Sea + 2500.0f), FVector(ImpactPoint.X, ImpactPoint.Y, Sea - 100.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params);
	// le plus haut des ponts (gaillard compris), pas les voiles, vergues ni la plateforme de Davy Jones au-dessus
	constexpr float MaxDeckHeightAboveSea = 700.0f;
	float DeckZ = -TNumericLimits<float>::Max();
	for (const FHitResult& Hit : Hits)
	{
		if (AYSPirateShip::GetShipOf(Hit.GetActor()) && Hit.ImpactPoint.Z <= Sea + MaxDeckHeightAboveSea)
		{
			DeckZ = FMath::Max(DeckZ, static_cast<float>(Hit.ImpactPoint.Z));
		}
	}
	return DeckZ > Sea ? FVector(ImpactPoint.X, ImpactPoint.Y, DeckZ) : ImpactPoint;
}

void AYSKrakenTentacle::TickTentacleServer(float DeltaSeconds)
{
	const double Now = GetServerTime();
	const float Elapsed = GetPhaseElapsed();
	switch (State.Phase)
	{
	case EYSKrakenTentaclePhase::Submerged:
	{
		FVector Location;
		float Yaw = 0.0f;
		if (Now >= NextEmergeServerTime && PickEmergeLocation(Location, Yaw))
		{
			State.Location = Location;
			State.Yaw = Yaw;
			State.bGrabbed = false;
			SetPhase(EYSKrakenTentaclePhase::Rising);
			UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s sort de l'eau en %s"), *GetName(), *Location.ToCompactString());
		}
		break;
	}
	case EYSKrakenTentaclePhase::Rising:
		if (Elapsed >= RiseDuration)
		{
			CurrentAttackDelay = FMath::Max(static_cast<float>(FMath::FRandRange(AttackDelayRange.X, FMath::Max(AttackDelayRange.X, AttackDelayRange.Y))), 0.0f);
			SetPhase(EYSKrakenTentaclePhase::Waiting);
			UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s sortie, attaque dans %.1f s"), *GetName(), CurrentAttackDelay);
		}
		break;
	case EYSKrakenTentaclePhase::Waiting:
		if (Elapsed >= CurrentAttackDelay)
		{
			// amorce : tournée vers le bateau à ce moment
			if (const AYSPirateShip* Boat = GetGameBoat())
			{
				State.Yaw = (Boat->GetHullCenter() - State.Location).GetSafeNormal2D().Rotation().Yaw;
			}
			SetPhase(EYSKrakenTentaclePhase::Windup);
			OnWindupStarted();
		}
		break;
	case EYSKrakenTentaclePhase::Windup:
		if (Elapsed >= TentacleWindupDuration)
		{
			OnWindupFinished();
		}
		break;
	case EYSKrakenTentaclePhase::Recover:
		if (Elapsed >= RecoverDuration)
		{
			SetPhase(EYSKrakenTentaclePhase::Sinking);
		}
		break;
	case EYSKrakenTentaclePhase::Sinking:
		if (Elapsed >= SinkDuration)
		{
			NextEmergeServerTime = Now + FMath::FRandRange(SubmergedDelay.X, FMath::Max(SubmergedDelay.X, SubmergedDelay.Y));
			SetPhase(EYSKrakenTentaclePhase::Submerged);
			UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s replonge, ressort dans %.1f s"), *GetName(), NextEmergeServerTime - Now);
		}
		break;
	default:
		break;
	}
}

float AYSKrakenTentacle::GetVisualPitch() const
{
	if (State.Phase == EYSKrakenTentaclePhase::Windup)
	{
		return FMath::Lerp(0.0f, WindupPitch, YSKraken::EaseInOut(GetPhaseElapsed() / FMath::Max(TentacleWindupDuration, 0.01f)));
	}
	return 0.0f;
}

void AYSKrakenTentacle::ApplyStateLocally()
{
	const float Elapsed = GetPhaseElapsed();
	float Depth = 0.0f;
	switch (State.Phase)
	{
	case EYSKrakenTentaclePhase::Submerged:
		Depth = 1.0f;
		break;
	case EYSKrakenTentaclePhase::Rising:
		Depth = 1.0f - YSKraken::EaseInOut(Elapsed / FMath::Max(RiseDuration, 0.01f));
		break;
	case EYSKrakenTentaclePhase::Sinking:
		Depth = YSKraken::EaseInOut(Elapsed / FMath::Max(SinkDuration, 0.01f));
		break;
	default:
		break;
	}
	const FVector Wanted = State.Location - FVector(0.0f, 0.0f, SubmergedDepth * Depth);
	if (!GetActorLocation().Equals(Wanted, 0.5f))
	{
		SetActorLocation(Wanted, false, nullptr, ETeleportType::TeleportPhysics);
	}
	// sous l'eau (ou presque) : ni visible, ni touchable
	SetTargetable(State.Phase != EYSKrakenTentaclePhase::Submerged && Depth < 0.6f);
	if (AActor* VisualActor = Visual->GetChildActor())
	{
		VisualActor->SetActorHiddenInGame(State.Phase == EYSKrakenTentaclePhase::Submerged);
	}
	// sortie : légère oscillation ; tournée vers le bateau
	const float Sway = State.Phase == EYSKrakenTentaclePhase::Waiting || State.Phase == EYSKrakenTentaclePhase::Recover
		? FMath::Sin(GetWorld()->GetTimeSeconds() * SwaySpeed + static_cast<float>(GetUniqueID() % 97)) * SwayAngle : 0.0f;
	VisualPivot->SetRelativeRotation(FRotator(-GetVisualPitch(), State.Yaw + Sway, 0.0f));
}

FVector AYSKrakenTentacle::GetHullEdgePoint(const AYSPirateShip* Boat, const FVector& Direction)
{
	// rectangle de la coque dans le repère du bateau : bord dans la direction voulue (depuis le centre de la coque)
	const FBox2D Bounds = Boat->GetHullLocalBounds();
	const FVector2D Center = Bounds.GetCenter();
	const FVector2D Half = Bounds.GetExtent();
	const FVector Local = Boat->GetActorRotation().UnrotateVector(Direction.GetSafeNormal2D());
	const float TX = FMath::Abs(Local.X) > KINDA_SMALL_NUMBER ? Half.X / FMath::Abs(Local.X) : TNumericLimits<float>::Max();
	const float TY = FMath::Abs(Local.Y) > KINDA_SMALL_NUMBER ? Half.Y / FMath::Abs(Local.Y) : TNumericLimits<float>::Max();
	const float T = FMath::Min(TX, TY);
	const FVector Edge(Center.X + Local.X * T, Center.Y + Local.Y * T, 0.0f);
	return Boat->GetActorTransform().TransformPosition(Edge);
}

bool AYSKrakenTentacle::PickEmergeLocation(FVector& OutLocation, float& OutYaw) const
{
	const AYSPirateShip* Boat = GetGameBoat();
	if (!Boat)
	{
		return false;
	}
	const FVector BoatCenter = Boat->GetHullCenter();
	const AYSKrakenArena* KrakenArena = Arena.Get();
	const FVector ArenaCenter = KrakenArena ? KrakenArena->GetActorLocation() : BoatCenter;
	const float Sea = GetSeaLevel();
	FVector Best = FVector::ZeroVector;
	float BestScore = TNumericLimits<float>::Max();
	for (int32 Attempt = 0; Attempt < 24; ++Attempt)
	{
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		const FVector Candidate = GetHullEdgePoint(Boat, Dir) + Dir * EmergeDistance;
		// dans l'arène, pas dans la tête : sinon le moins mauvais
		const float FromCenter = FVector::Dist2D(Candidate, ArenaCenter);
		const float Score = !KrakenArena ? 0.0f
			: FMath::Max(FromCenter - MaxDistanceFromArenaCenter, 0.0f) + FMath::Max(MinDistanceFromArenaCenter - FromCenter, 0.0f);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Candidate;
			if (Score <= 0.0f)
			{
				break;
			}
		}
	}
	OutLocation = FVector(Best.X, Best.Y, Sea);
	OutYaw = (BoatCenter - OutLocation).GetSafeNormal2D().Rotation().Yaw;
	return true;
}

FVector AYSKrakenTentacle::GetTipLocation() const
{
	return State.Location + FRotator(0.0f, State.Yaw, 0.0f).RotateVector(YSKraken::TentacleTip);
}

// ==================================================================== AYSKrakenTentacleRange

AYSKrakenTentacleRange::AYSKrakenTentacleRange()
{
	FoeName = NSLOCTEXT("YS", "KrakenTentacleRangeName", "Tentacule");
	EmergeDistance = 3500.0f;
	TentacleWindupDuration = 1.0f;
	// attaque 1 à 3 s après la sortie (demande de Paul, 2026-11-11)
	AttackDelayRange = FVector2D(1.0, 3.0);
}

void AYSKrakenTentacleRange::OnWindupStarted()
{
	const AYSPirateShip* Boat = GetGameBoat();
	if (!Boat || Attacks.Num() == 0)
	{
		return;
	}
	// une attaque au hasard, pas deux fois la même de suite
	int32 Pick = FMath::RandRange(0, Attacks.Num() - 1);
	if (Attacks.Num() > 1 && Pick == LastAttack)
	{
		Pick = (Pick + FMath::RandRange(1, Attacks.Num() - 1)) % Attacks.Num();
	}
	LastAttack = Pick;
	if (Attacks[Pick] == EYSKrakenTentacleAttack::ScatterBalls)
	{
		FireScatterBalls(Boat);
	}
	else
	{
		FirePattern(Attacks[Pick], Boat);
	}
}

void AYSKrakenTentacleRange::FirePattern(EYSKrakenTentacleAttack Attack, const AYSPirateShip* Boat)
{
	// signes au sol tout de suite (pendant l'amorce), premiers départs à la fin de l'amorce, depuis le haut de la tentacule
	const float Sea = GetSeaLevel();
	const FVector Foot(State.Location.X, State.Location.Y, Sea);
	TArray<FYSKrakenInkShot> Shots;
	const auto AddShot = [&](float Radius, float AngleDeg, float Delay)
	{
		const FVector Dir = FRotator(0.0f, AngleDeg, 0.0f).Vector();
		FYSKrakenInkShot Shot;
		Shot.Target = ProjectOntoShips(FVector(Foot.X + Dir.X * Radius, Foot.Y + Dir.Y * Radius, Sea));
		Shot.Delay = Delay;
		Shots.Add(Shot);
	};

	const TCHAR* PatternName = TEXT("?");
	float PatternRadius = 0.0f;
	if (Attack == EYSKrakenTentacleAttack::Cross)
	{
		// branches réparties autour, la première vers le bateau ; impacts du pied vers le bout
		PatternName = TEXT("croix");
		const float Toward = (Boat->GetHullCenter() - Foot).GetSafeNormal2D().Rotation().Yaw;
		const int32 Branches = FMath::Max(CrossBranches, 1);
		const float Spacing = FMath::Max(CrossSpacing, 50.0f);
		for (int32 Branch = 0; Branch < Branches; ++Branch)
		{
			for (int32 Step = 0; Step < FMath::Max(CrossShotsPerBranch, 1); ++Step)
			{
				AddShot(Spacing * (Step + 1), Toward + 360.0f * Branch / Branches, CrossStepDelay * Step);
			}
		}
		PatternRadius = Spacing * FMath::Max(CrossShotsPerBranch, 1);
	}
	else
	{
		// cercle autour d'elle, rayon au hasard, tous les tirs en même temps
		PatternName = TEXT("cercle");
		PatternRadius = static_cast<float>(FMath::FRandRange(FMath::Max(CircleRadius.X, 0.0), FMath::Max(CircleRadius.X, CircleRadius.Y)));
		const int32 Count = FMath::Max(FMath::CeilToInt(2.0f * PI * PatternRadius / FMath::Max(CircleSpacing, 50.0f)), CircleMinShots);
		const float Offset = FMath::FRandRange(0.0f, 360.0f);
		for (int32 K = 0; K < Count; ++K)
		{
			AddShot(PatternRadius, Offset + 360.0f * K / Count, 0.0f);
		}
	}

	const FTransform Transform(FRotator::ZeroRotator, Foot);
	AYSKrakenInkBarrage* Barrage = GetWorld()->SpawnActorDeferred<AYSKrakenInkBarrage>(AYSKrakenInkBarrage::StaticClass(), Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Barrage)
	{
		Barrage->Init(GetTipLocation(), InkFlightDuration, InkApexHeight, InkRadius, InkBallSize, TentacleWindupDuration, InkDamage, Shots);
		Barrage->FinishSpawning(Transform);
	}
	UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s : motif %s, %d tirs d'encre en cloche (rayon %.0f, %.0f dégâts chacun, bateau à %.0f cm)"), *GetName(), PatternName, Shots.Num(),
		PatternRadius, InkDamage, FVector::Dist2D(Boat->GetHullCenter(), Foot));
}

void AYSKrakenTentacleRange::FireScatterBalls(const AYSPirateShip* Boat)
{
	// Ink Balls Per Attack boules, chacune vers un point au hasard autour du bateau (signe au sol tout de suite), la première part à la fin de l'amorce
	const FVector Center = Boat->GetHullCenter();
	for (int32 Index = 0; Index < FMath::Max(InkBallsPerAttack, 1); ++Index)
	{
		// uniforme dans le disque d'imprécision
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Distance = InkScatterRadius * FMath::Sqrt(FMath::FRand());
		FYSKrakenInkBallFlight Flight;
		Flight.Start = GetTipLocation();
		Flight.Target = ProjectOntoShips(FVector(Center.X + FMath::Cos(Angle) * Distance, Center.Y + FMath::Sin(Angle) * Distance, GetSeaLevel()));
		Flight.LaunchServerTime = GetServerTime() + TentacleWindupDuration + InkBallInterval * Index;
		Flight.FlightDuration = InkFlightDuration;
		Flight.ApexHeight = InkApexHeight;
		Flight.Radius = InkRadius;
		Flight.BallSize = InkBallSize;
		const FTransform Transform(FRotator::ZeroRotator, Flight.Target);
		AYSKrakenInkBall* Ball = GetWorld()->SpawnActorDeferred<AYSKrakenInkBall>(AYSKrakenInkBall::StaticClass(), Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Ball)
		{
			Ball->Init(Flight, InkDamage);
			Ball->FinishSpawning(Transform);
		}
		UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s vise %s à %.0f cm du bateau (boule d'encre %d / %d dans %.2f s, vol %.1f s, %.0f dégâts)"), *GetName(),
			*Flight.Target.ToCompactString(), Distance, Index + 1, InkBallsPerAttack, TentacleWindupDuration + InkBallInterval * Index, InkFlightDuration, InkDamage);
	}
}

void AYSKrakenTentacleRange::OnWindupFinished()
{
	SetPhase(EYSKrakenTentaclePhase::Recover);
}

// ==================================================================== AYSKrakenTentacleMelee

AYSKrakenTentacleMelee::AYSKrakenTentacleMelee()
{
	FoeName = NSLOCTEXT("YS", "KrakenTentacleMeleeName", "Tentacule");
	EmergeDistance = 500.0f;
	// amorce (bande rouge au sol) 1 s (2026-11-10) ; attaque 1 s après la sortie (demande de Paul, 2026-11-11)
	TentacleWindupDuration = 1.0f;
	AttackDelayRange = FVector2D(1.0, 1.0);

	// corps violet (la tentacule à distance reste rouge) ; ventouses roses inchangées
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RedMaterial(TEXT("/Game/Art/Materials/PH_materials/m_PH_monochrome_Red.m_PH_monochrome_Red"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PurpleMaterial(TEXT("/Game/Art/Materials/PH_materials/m_PH_monochrome_Purple.m_PH_monochrome_Purple"));
	if (RedMaterial.Succeeded() && PurpleMaterial.Succeeded())
	{
		BodyMaterialFrom = RedMaterial.Object;
		BodyMaterialTo = PurpleMaterial.Object;
	}

	SlamWarning = CreateDefaultSubobject<UYSGroundZoneComponent>(TEXT("SlamWarning"));
	SlamWarning->SetupAttachment(GetRootComponent());
	SlamWarning->SetUsingAbsoluteRotation(true);
	SlamWarning->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SlamWarning->SetTranslucentSortPriority(YSKraken::MarkerSortPriority);
	SlamWarning->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ZoneMaterial(YSKraken::ZoneMaterialPath);
	if (ZoneMaterial.Succeeded())
	{
		WarningMaterial = ZoneMaterial.Object;
	}
}

void AYSKrakenTentacleMelee::TickKraken(float DeltaSeconds)
{
	Super::TickKraken(DeltaSeconds);

	// visuel plus grand que la zone frappée (la hurtbox suit le pivot, pas le visuel)
	if (!Visual->GetRelativeScale3D().Equals(FVector(VisualScale), 0.001f))
	{
		Visual->SetRelativeScale3D(FVector(VisualScale));
	}

	// hurtbox à la taille du visuel ; posée sur le bateau : plus épaisse (toutes machines : même forme pour les coups du client)
	if (!bHurtboxBaseSaved)
	{
		bHurtboxBaseSaved = true;
		BaseHurtboxLocation = GetHurtbox()->GetRelativeLocation();
		BaseHurtboxScale = GetHurtbox()->GetRelativeScale3D();
	}
	const bool bOnBoat = State.Phase == EYSKrakenTentaclePhase::Grabbing || (State.bGrabbed && State.Phase == EYSKrakenTentaclePhase::Sinking);
	const float RadiusMultiplier = VisualScale * (bOnBoat ? GrabHurtboxRadiusMultiplier : 1.0f);
	const FVector WantedScale(BaseHurtboxScale.X * RadiusMultiplier, BaseHurtboxScale.Y * RadiusMultiplier, BaseHurtboxScale.Z * VisualScale);
	if (!GetHurtbox()->GetRelativeScale3D().Equals(WantedScale, 0.001f))
	{
		GetHurtbox()->SetRelativeLocation(BaseHurtboxLocation * VisualScale);
		GetHurtbox()->SetRelativeScale3D(WantedScale);
	}
	// contrôle (serveur, une fois par prise) : points du pont sous la tentacule, à 1 m au-dessus du pont, dans la hurtbox ?
	if (HasAuthority() && bOnBoat && !bGrabHurtboxLogged && GrabSpawnPoints.Num() > 0)
	{
		bGrabHurtboxLogged = true;
		int32 Inside = 0;
		float Farthest = 0.0f;
		for (const FVector& Point : GrabSpawnPoints)
		{
			FVector Closest;
			const float Distance = GetHurtbox()->GetClosestPointOnCollision(Point - FVector(0.0f, 0.0f, GrabSpawnHeight - 100.0f), Closest);
			Inside += Distance <= 0.0f ? 1 : 0;
			Farthest = FMath::Max(Farthest, Distance);
		}
		UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s posée sur le bateau : hurtbox x%.2f, %d / %d point(s) du pont (à 1 m) dedans, le plus loin à %.0f cm"), *GetName(),
			RadiusMultiplier, Inside, GrabSpawnPoints.Num(), Farthest);
	}
	else if (!bOnBoat)
	{
		bGrabHurtboxLogged = false;
	}

	// couleur du corps : remplace Body Material From par Body Material To sur les formes du prop (une fois qu'il existe)
	if (!bRecolored && BodyMaterialFrom && BodyMaterialTo)
	{
		if (AActor* VisualActor = Visual->GetChildActor())
		{
			bRecolored = true;
			int32 Recolored = 0;
			TInlineComponentArray<UMeshComponent*> Meshes(VisualActor);
			for (UMeshComponent* PropMesh : Meshes)
			{
				for (int32 Slot = 0; Slot < PropMesh->GetNumMaterials(); ++Slot)
				{
					if (PropMesh->GetMaterial(Slot) == BodyMaterialFrom)
					{
						PropMesh->SetMaterial(Slot, BodyMaterialTo);
						++Recolored;
					}
				}
			}
			UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s : %d forme(s) en %s"), *GetName(), Recolored, *BodyMaterialTo->GetName());
		}
	}

	// signe au sol pendant l'amorce : la bande qui va être frappée
	const bool bShow = State.Phase == EYSKrakenTentaclePhase::Windup;
	if (bShow && !bWarningShown)
	{
		bWarningShown = true;
		SlamWarning->SetWorldLocation(State.Location);
		SlamWarning->BuildStrip(FRotator(0.0f, State.Yaw, 0.0f).Vector(), SlamLength, SlamWidth, YSKraken::ZoneHeight);
		if (WarningMaterial)
		{
			UMaterialInstanceDynamic* Material = SlamWarning->CreateDynamicMaterialInstance(0, WarningMaterial);
			Material->SetVectorParameterValue(TEXT("Color"), WarningColor);
			Material->SetScalarParameterValue(TEXT("Glow"), 0.4f);
			Material->SetScalarParameterValue(TEXT("FillOpacity"), WarningOpacity);
			Material->SetScalarParameterValue(TEXT("EdgeOpacity"), FMath::Min(WarningOpacity * 2.0f, 1.0f));
			Material->SetScalarParameterValue(TEXT("Radius"), SlamLength);
			Material->SetScalarParameterValue(TEXT("EdgeWidth"), 40.0f);
		}
		SlamWarning->SetVisibility(true);
	}
	else if (!bShow && bWarningShown)
	{
		bWarningShown = false;
		SlamWarning->SetVisibility(false);
	}
}

void AYSKrakenTentacleMelee::OnWindupFinished()
{
	SetPhase(EYSKrakenTentaclePhase::Slam);
}

void AYSKrakenTentacleMelee::TickTentacleServer(float DeltaSeconds)
{
	Super::TickTentacleServer(DeltaSeconds);

	const float Elapsed = GetPhaseElapsed();
	if (State.Phase == EYSKrakenTentaclePhase::Slam && Elapsed >= SlamDuration)
	{
		FVector HitPoint;
		AYSPirateShip* Boat = GetGameBoat();
		if (Boat && FindSlamHit(HitPoint))
		{
			// touché : le bateau est immobilisé, la tentacule reste posée dessus, des NME sortent d'elle : points de son corps au-dessus du pont
			const FVector Forward = FRotator(0.0f, State.Yaw, 0.0f).Vector();
			FCollisionQueryParams Params(SCENE_QUERY_STAT(KrakenGrab), false, this);
			GrabSpawnPoints.Reset();
			for (float Distance = 0.0f; Distance <= SlamLength * VisualScale; Distance += 100.0f)
			{
				const FVector Point = State.Location + Forward * Distance;
				FHitResult Hit;
				if (Boat->IsInsideHull(Point) && GetWorld()->LineTraceSingleByChannel(Hit, FVector(Point.X, Point.Y, GetSeaLevel() + 1500.0f),
					FVector(Point.X, Point.Y, GetSeaLevel() - 200.0f), ECC_WorldStatic, Params) && AYSPirateShip::GetShipOf(Hit.GetActor()) == Boat)
				{
					GrabSpawnPoints.Add(Hit.ImpactPoint + FVector(0.0f, 0.0f, GrabSpawnHeight));
				}
			}
			if (GrabSpawnPoints.Num() == 0)
			{
				GrabSpawnPoints.Add(FVector(HitPoint.X, HitPoint.Y, GetSeaLevel() + 210.0f + GrabSpawnHeight));
			}
			GrabPoint = GrabSpawnPoints[0];
			GrabStartHealth = Health;
			GrabSpawnAccumulator = 0.0f;
			bHoldingBoat = true;
			State.bGrabbed = true;
			if (AYSKrakenArena* KrakenArena = Arena.Get())
			{
				KrakenArena->AddBoatGrab();
			}
			SetPhase(EYSKrakenTentaclePhase::Grabbing);
			UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s s'abat sur %s : bateau immobilisé, NME en %s"), *GetName(), *Boat->GetName(), *GrabPoint.ToCompactString());
		}
		else
		{
			SetPhase(EYSKrakenTentaclePhase::Recover);
			UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s s'abat et rate le bateau"), *GetName());
		}
	}
	else if (State.Phase == EYSKrakenTentaclePhase::Grabbing)
	{
		// lâche après avoir perdu Release Damage Ratio de ses PV max depuis la prise, ou sans bateau : replonge directement
		if (!GetGameBoat() || Health <= GrabStartHealth - ReleaseDamageRatio * MaxHealth)
		{
			ReleaseBoat(TEXT("PV perdus"));
			SetPhase(EYSKrakenTentaclePhase::Sinking);
			return;
		}
		GrabSpawnAccumulator += DeltaSeconds * GrabSpawnsPerSecond;
		while (GrabSpawnAccumulator >= 1.0f)
		{
			GrabSpawnAccumulator -= 1.0f;
			SpawnGrabFoe();
		}
	}
}

bool AYSKrakenTentacleMelee::FindSlamHit(FVector& OutPoint) const
{
	const AYSPirateShip* Boat = GetGameBoat();
	if (!Boat)
	{
		return false;
	}
	const FVector Forward = FRotator(0.0f, State.Yaw, 0.0f).Vector();
	const FVector Right(-Forward.Y, Forward.X, 0.0f);
	for (float Distance = 0.0f; Distance <= SlamLength; Distance += 100.0f)
	{
		for (const float Side : { 0.0f, -0.5f, 0.5f })
		{
			const FVector Point = State.Location + Forward * Distance + Right * (Side * SlamWidth);
			if (Boat->IsInsideHull(Point))
			{
				OutPoint = Point;
				return true;
			}
		}
	}
	return false;
}

void AYSKrakenTentacleMelee::SpawnGrabFoe()
{
	if (!GrabSpawnClass)
	{
		return;
	}
	// d'un point du corps de la tentacule posé sur le pont, au hasard ; ils tombent d'elle (pas de sortie du sol)
	const FVector Location = GrabSpawnPoints.Num() > 0 ? GrabSpawnPoints[FMath::RandRange(0, GrabSpawnPoints.Num() - 1)] : GrabPoint;
	const FTransform Transform(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), Location);
	AYSFoeCharacter* Foe = GetWorld()->SpawnActorDeferred<AYSFoeCharacter>(GrabSpawnClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (Foe)
	{
		Foe->SetSpawnRise(false);
		Foe->FinishSpawning(Transform);
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSKraken] %s : %s sort de la tentacule en %s"), *GetName(), *GetNameSafe(Foe), *Location.ToCompactString());
}

void AYSKrakenTentacleMelee::ReleaseBoat(const TCHAR* Reason)
{
	if (!bHoldingBoat)
	{
		return;
	}
	bHoldingBoat = false;
	if (AYSKrakenArena* KrakenArena = Arena.Get())
	{
		KrakenArena->RemoveBoatGrab();
	}
	UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s lâche le bateau (%s)"), *GetName(), Reason);
}

void AYSKrakenTentacleMelee::Die()
{
	ReleaseBoat(TEXT("morte"));
	Super::Die();
}

void AYSKrakenTentacleMelee::Vanish(const TCHAR* Reason)
{
	ReleaseBoat(Reason);
	Super::Vanish(Reason);
}

float AYSKrakenTentacleMelee::GetVisualPitch() const
{
	const float Elapsed = GetPhaseElapsed();
	switch (State.Phase)
	{
	case EYSKrakenTentaclePhase::Slam:
		return FMath::Lerp(WindupPitch, SlamPitch, FMath::InterpEaseIn(0.0f, 1.0f, FMath::Clamp(Elapsed / FMath::Max(SlamDuration, 0.01f), 0.0f, 1.0f), 2.0f));
	case EYSKrakenTentaclePhase::Grabbing:
		return SlamPitch;
	case EYSKrakenTentaclePhase::Recover:
		// ratée : se relève
		return FMath::Lerp(SlamPitch, 0.0f, YSKraken::EaseInOut(Elapsed / 0.8f));
	case EYSKrakenTentaclePhase::Sinking:
		return State.bGrabbed ? SlamPitch : 0.0f;
	default:
		return Super::GetVisualPitch();
	}
}

// ==================================================================== AYSKrakenInkBall

AYSKrakenInkBall::AYSKrakenInkBall()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);
	bAlwaysRelevant = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Ball = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ball"));
	Ball->SetupAttachment(Root);
	Ball->SetUsingAbsoluteLocation(true);
	Ball->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ball->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Ball->SetStaticMesh(Sphere.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> InkMaterial(YSKraken::InkMaterialPath);
	if (InkMaterial.Succeeded())
	{
		Ball->SetMaterial(0, InkMaterial.Object);
	}

	Marker = CreateDefaultSubobject<UYSGroundZoneComponent>(TEXT("Marker"));
	Marker->SetupAttachment(Root);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetTranslucentSortPriority(YSKraken::MarkerSortPriority);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ZoneMaterial(YSKraken::ZoneMaterialPath);
	if (ZoneMaterial.Succeeded())
	{
		MarkerMaterial = ZoneMaterial.Object;
	}
}

void AYSKrakenInkBall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AYSKrakenInkBall, Flight, COND_InitialOnly);
}

void AYSKrakenInkBall::BeginPlay()
{
	Super::BeginPlay();
	BuildMarker();
}

void AYSKrakenInkBall::OnRep_Flight()
{
	BuildMarker();
}

void AYSKrakenInkBall::BuildMarker()
{
	if (bMarkerBuilt || Flight.LaunchServerTime <= 0.0)
	{
		return;
	}
	bMarkerBuilt = true;
	SetActorLocation(Flight.Target);
	Marker->BuildZone(FMath::Max(Flight.Radius, 1.0f), YSKraken::ZoneHeight);
	if (MarkerMaterial)
	{
		UMaterialInstanceDynamic* Material = Marker->CreateDynamicMaterialInstance(0, MarkerMaterial);
		Material->SetVectorParameterValue(TEXT("Color"), MarkerColor);
		Material->SetScalarParameterValue(TEXT("Glow"), MarkerGlow);
		Material->SetScalarParameterValue(TEXT("FillOpacity"), 0.55f);
		Material->SetScalarParameterValue(TEXT("EdgeOpacity"), 0.9f);
		Material->SetScalarParameterValue(TEXT("Radius"), Flight.Radius);
		Material->SetScalarParameterValue(TEXT("EdgeWidth"), 40.0f);
	}
	Ball->SetWorldScale3D(FVector(Flight.BallSize / 100.0f));
}

void AYSKrakenInkBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Flight.LaunchServerTime <= 0.0 || bLanded)
	{
		return;
	}
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const float Time = static_cast<float>(Now - Flight.LaunchServerTime);
	if (Time < 0.0f)
	{
		return;
	}
	if (Time <= Flight.FlightDuration)
	{
		Ball->SetVisibility(true);
		Ball->SetWorldLocation(YSCombat::GetBellLocation(Flight.Start, Flight.Target, Flight.FlightDuration, Flight.ApexHeight, Time));
		return;
	}

	// impact : dégâts au bateau des joueurs s'il est dans le rayon (serveur)
	bLanded = true;
	Ball->SetVisibility(false);
	Marker->SetVisibility(false);
	if (HasAuthority())
	{
		const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
		AYSPirateShip* Boat = YSGameState ? YSGameState->GetGameBoat() : nullptr;
		const bool bHit = IsValid(Boat) && !Boat->IsSinking() && Boat->IsInsideHull(Flight.Target, Flight.Radius);
		if (bHit)
		{
			Boat->TakeCoreDamage(Damage, nullptr, Flight.Target);
		}
		UE_LOG(LogTemp, Log, TEXT("[YSKraken] Boule d'encre en %s : %s"), *Flight.Target.ToCompactString(), bHit ? *FString::Printf(TEXT("touche %s (%.0f)"), *Boat->GetName(), Damage) : TEXT("ratée"));
		SetLifeSpan(0.5f);
	}
}

// ==================================================================== AYSKrakenInkBarrage

AYSKrakenInkBarrage::AYSKrakenInkBarrage()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);
	bAlwaysRelevant = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// boules : mêmes sphère et matériau que celles des tentacules
	BallMeshes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BallMeshes"));
	BallMeshes->SetupAttachment(Root);
	BallMeshes->SetUsingAbsoluteLocation(true);
	BallMeshes->SetUsingAbsoluteRotation(true);
	BallMeshes->SetUsingAbsoluteScale(true);
	BallMeshes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BallMeshes->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		BallMeshes->SetStaticMesh(Sphere.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> InkMaterial(YSKraken::InkMaterialPath);
	if (InkMaterial.Succeeded())
	{
		BallMeshes->SetMaterial(0, InkMaterial.Object);
	}

	// signes au sol : disques sur la mer (couleur des sommets, M_ShipPreview)
	Markers = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Markers"));
	Markers->SetupAttachment(Root);
	Markers->SetUsingAbsoluteLocation(true);
	Markers->SetUsingAbsoluteRotation(true);
	Markers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Markers->SetCastShadow(false);
	Markers->SetTranslucentSortPriority(YSKraken::MarkerSortPriority);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PreviewMaterial(YSKraken::PreviewMaterialPath);
	if (PreviewMaterial.Succeeded())
	{
		MarkerMaterial = PreviewMaterial.Object;
	}
}

void AYSKrakenInkBarrage::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AYSKrakenInkBarrage, Start, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AYSKrakenInkBarrage, FlightDuration, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AYSKrakenInkBarrage, ApexHeight, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AYSKrakenInkBarrage, Radius, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AYSKrakenInkBarrage, BallSize, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AYSKrakenInkBarrage, MarkerLead, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AYSKrakenInkBarrage, LaunchServerTime, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AYSKrakenInkBarrage, Shots, COND_InitialOnly);
}

void AYSKrakenInkBarrage::Init(const FVector& InStart, float InFlightDuration, float InApexHeight, float InRadius, float InBallSize, float InMarkerLead, float InDamage,
	const TArray<FYSKrakenInkShot>& InShots)
{
	Start = InStart;
	FlightDuration = FMath::Max(InFlightDuration, 0.1f);
	ApexHeight = InApexHeight;
	Radius = InRadius;
	BallSize = InBallSize;
	MarkerLead = FMath::Max(InMarkerLead, 0.0f);
	Damage = InDamage;
	Shots = InShots;
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	LaunchServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void AYSKrakenInkBarrage::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		float MaxDelay = 0.0f;
		for (const FYSKrakenInkShot& Shot : Shots)
		{
			MaxDelay = FMath::Max(MaxDelay, Shot.Delay);
		}
		SetLifeSpan(MarkerLead + MaxDelay + FlightDuration + 1.0f);
	}
}

void AYSKrakenInkBarrage::RebuildMarkers()
{
	// un disque par signe affiché : remplissage + bord plus opaque (comme le signe des boules des tentacules)
	constexpr int32 Segments = 24;
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FLinearColor> Colors;
	const float EdgeWidth = FMath::Min(60.0f, Radius * 0.3f);
	for (int32 Index = 0; Index < Shots.Num(); ++Index)
	{
		if (!MarkerShown[Index])
		{
			continue;
		}
		const FVector Center = FVector(Shots[Index].Target) + FVector(0.0f, 0.0f, YSKraken::ZoneHeight);
		const int32 Base = Vertices.Num();
		Vertices.Add(Center);
		Colors.Add(MarkerColor);
		for (int32 K = 0; K <= Segments; ++K)
		{
			const float Angle = 2.0f * PI * K / Segments;
			Vertices.Add(Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * (Radius - EdgeWidth));
			Colors.Add(MarkerColor);
		}
		for (int32 K = 0; K < Segments; ++K)
		{
			Triangles.Append({ Base, Base + 2 + K, Base + 1 + K });
		}
		YSKraken::AddRing(Vertices, Triangles, Colors, Center, Radius - EdgeWidth, Radius, MarkerEdgeColor, MarkerEdgeColor, Segments);
	}
	if (Vertices.Num() == 0)
	{
		Markers->ClearAllMeshSections();
		return;
	}
	Markers->CreateMeshSection_LinearColor(0, Vertices, Triangles, TArray<FVector>(), TArray<FVector2D>(), Colors, TArray<FProcMeshTangent>(), false);
	Markers->SetMaterial(0, MarkerMaterial);
}

void AYSKrakenInkBarrage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Shots.Num() == 0 || LaunchServerTime <= 0.0)
	{
		return;
	}
	if (Landed.Num() != Shots.Num())
	{
		Landed.Init(false, Shots.Num());
		MarkerShown.Init(false, Shots.Num());
		BallMeshes->ClearInstances();
		TArray<FTransform> Initial;
		Initial.Init(FTransform(FQuat::Identity, Start, FVector::ZeroVector), Shots.Num());
		BallMeshes->AddInstances(Initial, false, true);
	}
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const float Elapsed = static_cast<float>(Now - LaunchServerTime);

	AYSPirateShip* Boat = nullptr;
	if (HasAuthority())
	{
		const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
		Boat = YSGameState ? YSGameState->GetGameBoat() : nullptr;
		if (!IsValid(Boat) || Boat->IsSinking())
		{
			Boat = nullptr;
		}
	}

	// tir Index : signe au sol à Delay, départ de la boule Marker Lead plus tard, impact Flight Duration après le départ
	const FVector Scale(BallSize / 100.0f);
	bool bMarkersChanged = false;
	int32 Hits = 0;
	for (int32 Index = 0; Index < Shots.Num(); ++Index)
	{
		const FYSKrakenInkShot& Shot = Shots[Index];
		const float Time = Elapsed - Shot.Delay - MarkerLead;
		const bool bMarker = Elapsed >= Shot.Delay && Time <= FlightDuration;
		if (bMarker != MarkerShown[Index])
		{
			MarkerShown[Index] = bMarker;
			bMarkersChanged = true;
		}
		const bool bFlying = Time >= 0.0f && Time <= FlightDuration;
		const FVector Location = bFlying ? YSCombat::GetBellLocation(Start, FVector(Shot.Target), FlightDuration, ApexHeight, Time) : Start;
		BallMeshes->UpdateInstanceTransform(Index, FTransform(FQuat::Identity, Location, bFlying ? Scale : FVector::ZeroVector), true, false, true);

		// impact : dégâts au bateau des joueurs s'il est dans le rayon (serveur)
		if (Time > FlightDuration && !Landed[Index])
		{
			Landed[Index] = true;
			if (Boat && Boat->IsInsideHull(FVector(Shot.Target), Radius))
			{
				Boat->TakeCoreDamage(Damage, nullptr, FVector(Shot.Target));
				++Hits;
			}
		}
	}
	BallMeshes->MarkRenderStateDirty();
	if (bMarkersChanged)
	{
		RebuildMarkers();
	}
	if (Hits > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSKraken] %d tir(s) d'encre en cloche touchent %s (%.0f chacun)"), Hits, *GetNameSafe(Boat), Damage);
	}
}
