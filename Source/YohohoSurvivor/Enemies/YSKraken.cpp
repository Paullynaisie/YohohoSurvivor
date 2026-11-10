#include "Enemies/YSKraken.h"
#include "Enemies/YSKrakenArena.h"
#include "Components/CapsuleComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
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
	UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s en %s : %.0f PV (%s)"), *GetName(), *GetActorLocation().ToCompactString(), MaxHealth, HasAuthority() ? TEXT("serveur") : TEXT("client"));
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
	BaseHealth = 50000.0f;
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
	BaseHealth = 65000.0f;
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
	const EYSKrakenInkPattern Pattern = Patterns[Pick];
	FirePattern(Pattern);
	const float PatternLength = Pattern == EYSKrakenInkPattern::Rings ? RingInterval * (RingCount - 1)
		: Pattern == EYSKrakenInkPattern::Spiral ? SpiralDuration
		: FanInterval * (FanCount - 1);
	NextPatternServerTime = Now + PatternLength + PatternInterval;
}

void AYSKrakenHeadFinal::FirePattern(EYSKrakenInkPattern Pattern)
{
	const AYSPirateShip* Boat = GetGameBoat();
	const FVector Center = GetActorLocation();
	float AimYaw = CurrentYaw;
	if (Boat)
	{
		FVector AimPoint = Boat->GetHullCenter();
		if (bLeadTarget)
		{
			const float Time = FMath::Max(FVector::Dist2D(AimPoint, Center) - BulletSpawnRadius, 0.0f) / FMath::Max(BulletSpeed, 1.0f);
			AimPoint += Boat->GetVelocity() * FVector(1.0f, 1.0f, 0.0f) * Time;
		}
		AimYaw = (AimPoint - Center).GetSafeNormal2D().Rotation().Yaw;
	}

	// répartit Bullets Per Pattern balles en Groups groupes (le reste sur les premiers)
	const auto GroupSize = [this](int32 Group, int32 Groups)
	{
		return BulletsPerPattern / Groups + (Group < BulletsPerPattern % Groups ? 1 : 0);
	};

	TArray<FYSKrakenInkBullet> Bullets;
	const TCHAR* PatternName = TEXT("?");
	switch (Pattern)
	{
	case EYSKrakenInkPattern::Rings:
	{
		// anneaux tout autour, chacun décalé d'un demi-écart : on passe entre les balles en tournant
		PatternName = TEXT("anneaux");
		const int32 Groups = FMath::Max(RingCount, 1);
		for (int32 Ring = 0; Ring < Groups; ++Ring)
		{
			const int32 Count = FMath::Max(GroupSize(Ring, Groups), 1);
			const float Step = 360.0f / Count;
			for (int32 K = 0; K < Count; ++K)
			{
				Bullets.Add({ AimYaw + Step * K + (Ring % 2) * Step * 0.5f, RingInterval * Ring });
			}
		}
		break;
	}
	case EYSKrakenInkPattern::Spiral:
	{
		// une balle à la fois, la direction tourne : spirale qui balaie toute l'arène
		PatternName = TEXT("spirale");
		const float Sign = FMath::RandBool() ? 1.0f : -1.0f;
		for (int32 K = 0; K < BulletsPerPattern; ++K)
		{
			const float T = BulletsPerPattern > 1 ? static_cast<float>(K) / (BulletsPerPattern - 1) : 0.0f;
			Bullets.Add({ AimYaw + Sign * 360.0f * SpiralTurns * T, SpiralDuration * T });
		}
		break;
	}
	case EYSKrakenInkPattern::AimedFans:
	{
		// éventails visés sur le bateau, un sur deux décalé d'un demi-écart
		PatternName = TEXT("éventails visés");
		const int32 Groups = FMath::Max(FanCount, 1);
		for (int32 Fan = 0; Fan < Groups; ++Fan)
		{
			const int32 Count = FMath::Max(GroupSize(Fan, Groups), 1);
			const float Step = Count > 1 ? 2.0f * FanHalfAngle / (Count - 1) : 0.0f;
			for (int32 K = 0; K < Count; ++K)
			{
				const float Offset = Count > 1 ? -FanHalfAngle + Step * K + (Fan % 2 ? Step * 0.5f : 0.0f) : 0.0f;
				Bullets.Add({ AimYaw + Offset, FanInterval * Fan });
			}
		}
		break;
	}
	}

	const FVector Origin(Center.X, Center.Y, GetSeaLevel() + BulletHeight);
	const FTransform Transform(FRotator::ZeroRotator, Origin);
	AYSKrakenInkVolley* Volley = GetWorld()->SpawnActorDeferred<AYSKrakenInkVolley>(AYSKrakenInkVolley::StaticClass(), Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Volley)
	{
		Volley->Init(Origin, BulletSpawnRadius, BulletSpeed, BulletRadius, BulletRange, BulletDamage, Bullets);
		Volley->FinishSpawning(Transform);
	}
	UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s : motif %s, %d balles d'encre (%.0f dégâts chacune)"), *GetName(), PatternName, Bullets.Num(), BulletDamage);
}

// ==================================================================== AYSKrakenTentacle

AYSKrakenTentacle::AYSKrakenTentacle()
{
	FoeName = NSLOCTEXT("YS", "KrakenTentacleName", "Tentacule");
	BaseHealth = 8000.0f;
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
		NextEmergeServerTime = GetServerTime() + FirstEmergeDelay + EmergeStagger;
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
	}
	ApplyStateLocally();
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
			SetPhase(EYSKrakenTentaclePhase::Waiting);
		}
		break;
	case EYSKrakenTentaclePhase::Waiting:
		if (Elapsed >= AttackDelay)
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
}

void AYSKrakenTentacleRange::OnWindupStarted()
{
	// signe au sol là où est le bateau, la boule part à la fin de l'amorce
	const AYSPirateShip* Boat = GetGameBoat();
	if (!Boat)
	{
		return;
	}
	FYSKrakenInkBallFlight Flight;
	Flight.Start = GetTipLocation();
	const FVector Center = Boat->GetHullCenter();
	Flight.Target = FVector(Center.X, Center.Y, GetSeaLevel());
	Flight.LaunchServerTime = GetServerTime() + TentacleWindupDuration;
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
	UE_LOG(LogTemp, Log, TEXT("[YSKraken] %s vise %s (boule d'encre dans %.1f s, %.0f dégâts)"), *GetName(), *Flight.Target.ToCompactString(), TentacleWindupDuration, InkDamage);
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
	TentacleWindupDuration = 2.0f;

	SlamWarning = CreateDefaultSubobject<UYSGroundZoneComponent>(TEXT("SlamWarning"));
	SlamWarning->SetupAttachment(GetRootComponent());
	SlamWarning->SetUsingAbsoluteRotation(true);
	SlamWarning->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
			// touché : le bateau est immobilisé, la tentacule reste posée dessus, des NME en sortent
			// (vers le milieu du pont : au bord, les NME tombaient à l'eau)
			const FVector HullCenter = Boat->GetHullCenter();
			const FVector DeckPoint = FMath::Lerp(FVector(HitPoint.X, HitPoint.Y, 0.0f), FVector(HullCenter.X, HullCenter.Y, 0.0f), GrabSpawnTowardCenter);
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(KrakenGrab), false, this);
			GrabPoint = GetWorld()->LineTraceSingleByChannel(Hit, DeckPoint + FVector(0.0f, 0.0f, GetSeaLevel() + 1500.0f), DeckPoint + FVector(0.0f, 0.0f, GetSeaLevel() - 500.0f), ECC_WorldStatic, Params)
				? Hit.ImpactPoint : DeckPoint + FVector(0.0f, 0.0f, GetSeaLevel() + 250.0f);
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
	const FVector2D Offset = FVector2D(FMath::FRandRange(-80.0f, 80.0f), FMath::FRandRange(-80.0f, 80.0f));
	const FVector Location = GrabPoint + FVector(Offset.X, Offset.Y, 150.0f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const AYSFoeCharacter* Foe = GetWorld()->SpawnActor<AYSFoeCharacter>(GrabSpawnClass, Location, FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), Params);
	UE_LOG(LogTemp, Verbose, TEXT("[YSKraken] %s : %s sort de la tentacule"), *GetName(), *GetNameSafe(Foe));
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
		Material->SetScalarParameterValue(TEXT("Glow"), 0.2f);
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

// ==================================================================== AYSKrakenInkVolley

AYSKrakenInkVolley::AYSKrakenInkVolley()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);
	bAlwaysRelevant = true;

	BulletMeshes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BulletMeshes"));
	SetRootComponent(BulletMeshes);
	BulletMeshes->SetUsingAbsoluteLocation(true);
	BulletMeshes->SetUsingAbsoluteRotation(true);
	BulletMeshes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BulletMeshes->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		BulletMeshes->SetStaticMesh(Sphere.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> InkMaterial(YSKraken::InkMaterialPath);
	if (InkMaterial.Succeeded())
	{
		BulletMeshes->SetMaterial(0, InkMaterial.Object);
	}
}

void AYSKrakenInkVolley::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AYSKrakenInkVolley, Origin, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AYSKrakenInkVolley, SpawnRadius, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AYSKrakenInkVolley, Speed, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AYSKrakenInkVolley, Radius, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AYSKrakenInkVolley, Range, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AYSKrakenInkVolley, LaunchServerTime, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AYSKrakenInkVolley, Bullets, COND_InitialOnly);
	DOREPLIFETIME(AYSKrakenInkVolley, HitBullets);
}

void AYSKrakenInkVolley::Init(const FVector& InOrigin, float InSpawnRadius, float InSpeed, float InRadius, float InRange, float InDamage, const TArray<FYSKrakenInkBullet>& InBullets)
{
	Origin = InOrigin;
	SpawnRadius = InSpawnRadius;
	Speed = InSpeed;
	Radius = InRadius;
	Range = InRange;
	Damage = InDamage;
	Bullets = InBullets;
	HitBullets.Init(0, Bullets.Num());
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	LaunchServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void AYSKrakenInkVolley::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		float MaxDelay = 0.0f;
		for (const FYSKrakenInkBullet& Bullet : Bullets)
		{
			MaxDelay = FMath::Max(MaxDelay, Bullet.Delay);
		}
		SetLifeSpan(MaxDelay + Range / FMath::Max(Speed, 1.0f) + 1.0f);
	}
}

bool AYSKrakenInkVolley::GetBulletLocation(int32 Index, float Elapsed, FVector& OutLocation) const
{
	const FYSKrakenInkBullet& Bullet = Bullets[Index];
	const float Travel = (Elapsed - Bullet.Delay) * Speed;
	if (Travel < 0.0f || Travel > Range)
	{
		return false;
	}
	OutLocation = Origin + FRotator(0.0f, Bullet.Yaw, 0.0f).Vector() * (SpawnRadius + Travel);
	return true;
}

void AYSKrakenInkVolley::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Bullets.Num() == 0 || LaunchServerTime <= 0.0)
	{
		return;
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

	// visuel (toutes machines) : une instance par balle, cachée (échelle 0) avant son départ, après sa portée ou si elle a touché
	if (BulletMeshes->GetInstanceCount() != Bullets.Num())
	{
		BulletMeshes->ClearInstances();
		TArray<FTransform> Initial;
		Initial.Init(FTransform(FQuat::Identity, Origin, FVector::ZeroVector), Bullets.Num());
		BulletMeshes->AddInstances(Initial, false, true);
	}
	const FVector Scale(Radius * 2.0f / 100.0f);
	bool bHitChanged = false;
	for (int32 Index = 0; Index < Bullets.Num(); ++Index)
	{
		FVector Location;
		const bool bHit = HitBullets.IsValidIndex(Index) && HitBullets[Index] != 0;
		const bool bFlying = !bHit && GetBulletLocation(Index, Elapsed, Location);
		// touche le bateau des joueurs (serveur) : dégâts au core, balle retirée partout
		if (bFlying && Boat && Boat->IsInsideHull(Location, Radius))
		{
			HitBullets[Index] = 1;
			bHitChanged = true;
			Boat->TakeCoreDamage(Damage, nullptr, Location);
			UE_LOG(LogTemp, Verbose, TEXT("[YSKraken] Balle d'encre %d touche %s (%.0f)"), Index, *Boat->GetName(), Damage);
		}
		const bool bVisible = bFlying && !(HitBullets.IsValidIndex(Index) && HitBullets[Index] != 0);
		BulletMeshes->UpdateInstanceTransform(Index, FTransform(FQuat::Identity, bVisible ? Location : Origin, bVisible ? Scale : FVector::ZeroVector), true, false, true);
	}
	BulletMeshes->MarkRenderStateDirty();
	if (bHitChanged)
	{
		ForceNetUpdate();
	}
}
