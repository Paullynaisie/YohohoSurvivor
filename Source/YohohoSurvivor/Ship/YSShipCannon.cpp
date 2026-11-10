#include "Ship/YSShipCannon.h"
#include "Core/YSGameState.h"
#include "Core/YSPlayerState.h"
#include "ProceduralMeshComponent.h"
#include "Ship/YSCombatPostPreviewData.h"
#include "Components/StaticMeshComponent.h"
#include "Core/YSGameplayTags.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Enemies/YSFoeCharacter.h"
#include "Enemies/YSFoeGalion.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "Player/YSPlayerController.h"
#include "Interaction/YSInteractiveObject.h"
#include "Map/YSOcean.h"
#include "Ship/YSBoatCrew.h"
#include "Upgrades/YSPassive.h"
#include "Weapons/YSPlayerBomb.h"
#include "Ship/YSPirateShip.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/YSProjectile.h"


AYSShipCannon::AYSShipCannon()
{
	DisplayName = NSLOCTEXT("YS", "ShipCannonName", "Cannonerie");
	bTakesDamage = true;
	MaxHealth = 1000.0f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WoodFinder(TEXT("/Game/Art/Materials/PH_materials/m_PH_monochrome_Brown.m_PH_monochrome_Brown"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MetalFinder(TEXT("/Game/Art/Materials/PH_materials/m_PH_monochrome_Black.m_PH_monochrome_Black"));
	static ConstructorHelpers::FClassFinder<AYSProjectile> AmmoFinder(TEXT("/Game/Placeable/AMMO/PetoirAMMO"));

	// affût sur le pont (origine = pont), fût au-dessus du bastingage (100 cm)
	CannonBase = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CannonBase"));
	CannonBase->SetupAttachment(GetRootComponent());
	CannonBase->SetRelativeLocation(FVector(0.0f, 0.0f, 45.0f));
	CannonBase->SetRelativeScale3D(FVector(0.9f, 0.9f, 0.9f));
	CannonBase->SetCollisionProfileName(TEXT("BlockAll"));
	CannonBase->SetMobility(EComponentMobility::Movable);

	CannonPivot = CreateDefaultSubobject<USceneComponent>(TEXT("CannonPivot"));
	CannonPivot->SetupAttachment(GetRootComponent());
	CannonPivot->SetRelativeLocation(FVector(0.0f, 0.0f, 115.0f));

	// racine du fût (recule au tir) : porte le fût et les formes du Blueprint
	BarrelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BarrelRoot"));
	BarrelRoot->SetupAttachment(CannonPivot);

	CannonBarrel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CannonBarrel"));
	CannonBarrel->SetupAttachment(BarrelRoot);
	CannonBarrel->SetRelativeLocation(FVector(40.0f, 0.0f, 0.0f));
	CannonBarrel->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	CannonBarrel->SetRelativeScale3D(FVector(0.45f, 0.45f, 1.7f));
	CannonBarrel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CannonBarrel->SetMobility(EComponentMobility::Movable);

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(CannonPivot);
	Muzzle->SetRelativeLocation(FVector(135.0f, 0.0f, 0.0f));

	if (CubeFinder.Succeeded())
	{
		CannonBase->SetStaticMesh(CubeFinder.Object);
	}
	if (CylinderFinder.Succeeded())
	{
		CannonBarrel->SetStaticMesh(CylinderFinder.Object);
	}
	if (WoodFinder.Succeeded())
	{
		CannonBase->SetMaterial(0, WoodFinder.Object);
	}
	if (MetalFinder.Succeeded())
	{
		CannonBarrel->SetMaterial(0, MetalFinder.Object);
	}
	if (AmmoFinder.Succeeded())
	{
		ProjectileClass = AmmoFinder.Class;
	}

	// joueur derrière le canon, mains sur la culasse (suivent le canon)
	GetOccupantPoint()->SetRelativeLocation(FVector(-110.0f, 0.0f, 0.0f));
	GetHandPoint(true)->SetupAttachment(CannonPivot);
	GetHandPoint(false)->SetupAttachment(CannonPivot);
	GetHandPoint(true)->SetRelativeLocation(FVector(-55.0f, -22.0f, 5.0f));
	GetHandPoint(false)->SetRelativeLocation(FVector(-55.0f, 22.0f, 5.0f));

	// le joueur vise (regard = curseur) ; caméra dézoomée comme à la barre, décalée au maximum vers la visée du canon
	bLockAim = false;
	CameraArmLength = 3600.0f;
	bCameraOnShip = false;
	CameraOffsetMode = EYSStationCameraOffset::Aim;
	CameraOffsetDistance = 1000.0f;
}

void AYSShipCannon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSShipCannon, ReadyServerTime);
	DOREPLIFETIME(AYSShipCannon, FoeAimYaw);
}

float AYSShipCannon::ComputeAimYaw(const FVector& WorldTarget) const
{
	const FVector ToTarget = (WorldTarget - CannonPivot->GetComponentLocation()).GetSafeNormal2D();
	if (ToTarget.IsNearlyZero())
	{
		return CurrentAimYaw;
	}
	const float Relative = FRotator::NormalizeAxis(ToTarget.Rotation().Yaw - GetActorRotation().Yaw);
	return FMath::Clamp(Relative, -MaxAimAngle, MaxAimAngle);
}

float AYSShipCannon::GetTargetAimYaw() const
{
	if (GetFoeOccupant())
	{
		return FoeAimYaw;
	}
	// regard du joueur (tourné vers son curseur, répliqué avec ses mouvements) : même angle sur toutes les machines
	const AYSPlayerCharacter* Character = GetOccupant();
	if (!IsValid(Character))
	{
		return 0.0f;
	}
	const float Relative = FRotator::NormalizeAxis(Character->GetActorRotation().Yaw - GetActorRotation().Yaw);
	return FMath::Clamp(Relative, -MaxAimAngle, MaxAimAngle);
}

void AYSShipCannon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && GetFoeOccupant())
	{
		UpdateFoeFire(DeltaSeconds);
	}

	// visuel : le canon suit le regard du joueur (sans joueur : revient droit) ; à 0 PV il penche
	const float Target = GetTargetAimYaw();
	CurrentAimYaw = AimFollowSpeed > 0.0f ? FMath::FInterpTo(CurrentAimYaw, Target, DeltaSeconds, AimFollowSpeed) : Target;
	CannonPivot->SetRelativeRotation(FRotator(IsBroken() ? BrokenBarrelPitch : 0.0f, CurrentAimYaw, 0.0f));

	// recul du fût : en arrière d'un coup, puis retour
	if (!bBarrelRestCaptured)
	{
		BarrelRestLocation = BarrelRoot->GetRelativeLocation();
		bBarrelRestCaptured = true;
	}
	const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - LastFireTime);
	const float Kick = Elapsed < RecoilDuration ? FMath::InterpEaseOut(1.0f, 0.0f, Elapsed / RecoilDuration, 2.0f) : 0.0f;
	BarrelRoot->SetRelativeLocation(BarrelRestLocation - FVector(RecoilDistance * Kick, 0.0f, 0.0f));

	// prévisus du joueur au poste (local)
	UpdatePreview();
}

void AYSShipCannon::ServerHandleAction(AYSPlayerCharacter* Character, EYSStationAction Action, const FVector& AimLocation)
{
	if (!HasAuthority() || Action != EYSStationAction::Primary || !Character || Character != GetOccupant() || IsWorking())
	{
		return;
	}
	Fire(Character, AimLocation, false);
}

void AYSShipCannon::OnWorkStopped(EYSStationWork StoppedWork)
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	ReadyServerTime = FMath::Max(ReadyServerTime, Now + GetEffectiveCooldown());
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSCannon] %s : fin %s, recharge %.1f s"), *GetName(), StoppedWork == EYSStationWork::Repair ? TEXT("de la réparation") : TEXT("du démontage"), GetEffectiveCooldown());
}

void AYSShipCannon::OnBrokenChanged(bool bBroken)
{
	UE_LOG(LogTemp, Verbose, TEXT("[YSCannon] %s : %s"), *GetName(), bBroken ? TEXT("cassé") : TEXT("réparé"));
}

void AYSShipCannon::Fire(APawn* Shooter, const FVector& AimLocation, bool bFoeShooter)
{
	if (!HasAuthority() || !Shooter || !HasShotClass() || !IsUsable())
	{
		return;
	}
	if (GetCooldownRemaining() > 0.0f)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[YSCannon] %s : pas encore rechargé (%.1f s)"), *GetName(), GetCooldownRemaining());
		return;
	}

	// direction : vers le point visé, dans les limites du canon ; le fût est tourné tout de suite pour partir de sa bouche
	LastAimLocation = AimLocation;
	const float AimYaw = ComputeAimYaw(AimLocation);
	CurrentAimYaw = AimYaw;
	CannonPivot->SetRelativeRotation(FRotator(0.0f, AimYaw, 0.0f));
	SpawnVolley(Shooter, GetActorRotation().Yaw + AimYaw, bFoeShooter);

	// rafale (Gatling) : les tirs suivants partent vers la visée du moment
	BurstShotsLeft = ShotsPerBurst - 1;
	BurstShooter = Shooter;
	bBurstFoeShooter = bFoeShooter;
	if (BurstShotsLeft > 0)
	{
		GetWorldTimerManager().SetTimer(BurstTimer, this, &AYSShipCannon::FireNextBurstShot, BurstInterval, true);
	}

	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const float ShotCooldown = GetEffectiveCooldown() * (Shooter->IsPlayerControlled() ? 1.0f : NonPlayerCooldownMultiplier);
	ReadyServerTime = (GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds()) + ShotCooldown;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSCannon] %s : tir de %s, %d x %d AMMO x %.0f dégâts (x%.2f%s) (+/- %.0f°), angle %.0f°, recharge %.1f s"), *GetName(), *Shooter->GetName(),
		ShotsPerBurst, AmmoPerShot, Damage * GetShotDamageMultiplier(Shooter, bFoeShooter), GetShotDamageMultiplier(Shooter, bFoeShooter), ExplosionClass ? *FString::Printf(TEXT(", explosion %.0f sur %.0f cm"), ExplosionDamage * GetShotDamageMultiplier(Shooter, bFoeShooter), ExplosionRadius) : TEXT(""), Spread, AimYaw, ShotCooldown);
}

void AYSShipCannon::FireNextBurstShot()
{
	APawn* Shooter = BurstShooter.Get();
	if (!HasAuthority() || !Shooter || !IsUsable() || BurstShotsLeft <= 0)
	{
		BurstShotsLeft = 0;
		GetWorldTimerManager().ClearTimer(BurstTimer);
		return;
	}
	// vers la visée actuelle de celui qui est au poste (regard du joueur ou du minion, angle visé du NME)
	const float AimYaw = GetOccupant() || GetFoeOccupant() ? GetTargetAimYaw() : CurrentAimYaw;
	CurrentAimYaw = AimYaw;
	CannonPivot->SetRelativeRotation(FRotator(0.0f, AimYaw, 0.0f));
	SpawnVolley(Shooter, GetActorRotation().Yaw + AimYaw, bBurstFoeShooter);
	if (--BurstShotsLeft <= 0)
	{
		GetWorldTimerManager().ClearTimer(BurstTimer);
	}
}

void AYSShipCannon::SpawnVolley(APawn* Shooter, float BaseYaw, bool bFoeShooter)
{
	const FVector MuzzleLocation = Muzzle->GetComponentLocation();
	AYSPirateShip* Ship = GetShip();

	// dégâts selon le niveau de l'équipe (joueur, minion) ou les jours écoulés (NME)
	const float Multiplier = GetShotDamageMultiplier(Shooter, bFoeShooter);
	const FGameplayTagContainer TargetTags(bFoeShooter ? YSTags::Team_Player : YSTags::Team_Foe);
	FYSProjectileExplosion Blast;
	if (ExplosionClass)
	{
		const AYSPlayerBomb* BlastDefaults = ExplosionClass->GetDefaultObject<AYSPlayerBomb>();
		Blast.ExplosionClass = ExplosionClass;
		Blast.Damage = ExplosionDamage * Multiplier;
		Blast.RadiusMultiplier = BlastDefaults && BlastDefaults->GetExplosionRadius() > 0.0f ? ExplosionRadius / BlastDefaults->GetExplosionRadius() : 1.0f;
		Blast.bOnAnyHit = true;
		Blast.bAtEndOfLife = true;
		Blast.bOverrideBlastTags = true;
		Blast.BlastHitTags = TargetTags;
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSCannon] %s : dégâts x%.2f (%s)"), *GetName(), Multiplier, bFoeShooter ? TEXT("NME") : TEXT("joueur / minion"));

	// comme le Shotgun : toutes les AMMO d'un coup, en éventail régulier de -Spread à +Spread
	for (int32 Index = 0; Index < AmmoPerShot; ++Index)
	{
		const float Yaw = BaseYaw + (AmmoPerShot > 1 ? FMath::Lerp(-Spread, Spread, static_cast<float>(Index) / (AmmoPerShot - 1)) : 0.0f);
		const FTransform SpawnTransform(FRotator(0.0f, Yaw, 0.0f), MuzzleLocation, FVector(ProjectileScale));
		AYSProjectile* Projectile = GetWorld()->SpawnActorDeferred<AYSProjectile>(ProjectileClass, SpawnTransform, Shooter, Shooter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Projectile)
		{
			continue;
		}
		Projectile->SetDamage(Damage * Multiplier);
		Projectile->SetFlightOverrides(Ship ? static_cast<AActor*>(Ship) : this, ShotHeightAboveWater);
		Projectile->SetShipShot(TargetTags);
		if (Blast.ExplosionClass)
		{
			Projectile->SetExplosion(Blast);
		}
		Projectile->FinishSpawning(SpawnTransform);
	}
	MulticastFired();
}

void AYSShipCannon::MulticastFired_Implementation()
{
	LastFireTime = GetWorld()->GetTimeSeconds();
}

bool AYSShipCannon::GetCameraAimDirection(FVector& OutDirection) const
{
	// direction visée (limitée) : la caméra bute quand le canon est à son angle max
	OutDirection = FRotator(0.0f, GetActorRotation().Yaw + GetTargetAimYaw(), 0.0f).Vector();
	return true;
}

float AYSShipCannon::GetCooldownRemaining() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	return FMath::Max(0.0f, static_cast<float>(ReadyServerTime - Now));
}

AActor* AYSShipCannon::FindAutoFireTarget(bool bFoeShooter) const
{
	const float Range = GetAutoFireRange();
	const FVector Origin = CannonPivot->GetComponentLocation();
	const float BaseYaw = GetActorRotation().Yaw;

	AActor* Best = nullptr;
	float BestDistSq = FMath::Square(Range);
	const auto Consider = [&](AActor* Candidate, float Reach = 0.0f)
	{
		// (Reach : taille de la cible à plat, distance mesurée à son bord : énorme tête du Kraken)
		const FVector ToCandidate = (Candidate->GetActorLocation() - Origin) * FVector(1.0f, 1.0f, 0.0f);
		const float DistSq = FMath::Square(FMath::Max(ToCandidate.Size() - Reach, 0.0f));
		if (DistSq < BestDistSq && FMath::Abs(FRotator::NormalizeAxis(ToCandidate.Rotation().Yaw - BaseYaw)) <= MaxAimAngle)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	};

	const AYSPirateShip* OwnShip = GetShip();
	for (TActorIterator<AYSPirateShip> It(GetWorld()); It; ++It)
	{
		AYSPirateShip* Ship = *It;
		if (Ship == OwnShip || Ship->IsFoeShip() == bFoeShooter || !Ship->IsPresent() || Ship->IsActorBeingDestroyed())
		{
			// bateau du même camp (ou absent)
			continue;
		}
		for (AYSShipStation* Station : Ship->GetStations())
		{
			if (Station->TakesDamage())
			{
				Consider(Station);
			}
		}
	}

	if (bFoeShooter)
	{
		for (TActorIterator<AYSPlayerCharacter> It(GetWorld()); It; ++It)
		{
			AYSPlayerCharacter* Character = *It;
			if (Character->IsAlive() && !Character->IsActorBeingDestroyed() && !Character->IsDying())
			{
				const AYSPirateShip* Deck = Character->GetShipUnderFeet();
				if (!Deck || Deck->IsDisabled())
				{
					Consider(Character);
				}
			}
		}
		return Best;
	}

	for (TActorIterator<AYSFoeCharacter> It(GetWorld()); It; ++It)
	{
		AYSFoeCharacter* Foe = *It;
		if (Foe->GetHealth() <= 0.0f || Foe->IsDying() || Foe->IsRising() || Foe->IsActorBeingDestroyed())
		{
			continue;
		}
		// pas ciblable pour l'instant (tentacule du Kraken sous l'eau : sans tag d'équipe)
		FGameplayTagContainer FoeTags;
		Foe->GetOwnedGameplayTags(FoeTags);
		if (!FoeTags.HasTag(YSTags::Team_Foe))
		{
			continue;
		}
		// équipage d'un Galion : sur le pont, hors d'atteinte tant que son bateau tient
		const AYSFoeGalion* CrewShip = Foe->GetCrewShip();
		if (!CrewShip || CrewShip->IsDisabled())
		{
			const UPrimitiveComponent* Hurtbox = Foe->GetHurtbox();
			Consider(Foe, Hurtbox ? FMath::Max(Hurtbox->Bounds.BoxExtent.X, Hurtbox->Bounds.BoxExtent.Y) : 0.0f);
		}
	}
	return Best;
}

void AYSShipCannon::UpdateFoeFire(float DeltaSeconds)
{
	if (!IsUsable())
	{
		return;
	}
	FoeTargetTimer -= DeltaSeconds;
	if (FoeTargetTimer <= 0.0f)
	{
		FoeTargetTimer = FoeTargetInterval;
		FoeTarget = FindAutoFireTarget(true);
	}
	if (!IsValid(FoeTarget))
	{
		FoeTarget = nullptr;
		return;
	}
	const float Aim = ComputeAimYaw(FoeTarget->GetActorLocation());
	if (!FMath::IsNearlyEqual(Aim, FoeAimYaw, 1.0f))
	{
		FoeAimYaw = Aim;
	}
	if (GetCooldownRemaining() <= 0.0f && IsAimedAt(FoeTarget))
	{
		Fire(GetFoeOccupant(), FoeTarget->GetActorLocation(), true);
	}
}

bool AYSShipCannon::IsAimedAt(const AActor* Target) const
{
	return Target && FMath::Abs(FRotator::NormalizeAxis(CurrentAimYaw - ComputeAimYaw(Target->GetActorLocation()))) <= AutoFireAimTolerance;
}

bool AYSShipCannon::GetActionIcon(UTexture2D*& OutIcon, FLinearColor& OutTint, float& OutRemaining, float& OutDuration) const
{
	OutIcon = Icon;
	OutTint = IconTint;
	OutRemaining = GetCooldownRemaining();
	OutDuration = GetEffectiveCooldown();
	return true;
}

void AYSShipCannon::GetStationActionIcons(FYSStationActionIcon& OutLeft, FYSStationActionIcon& OutRight, FYSStationActionIcon& OutSpace) const
{
	// clic gauche : tirer ; clic droit : démonter (poste de combat posé) ; Espace : réparer
	Super::GetStationActionIcons(OutLeft, OutRight, OutSpace);
	if (OutRight.Name.IsEmpty() && OutRight.Icon == Icon)
	{
		OutRight = FYSStationActionIcon();
	}
	OutLeft.bShow = true;
	OutLeft.Icon = Icon;
	OutLeft.Tint = IconTint;
	OutLeft.Name = FireActionName;
	OutLeft.Remaining = GetCooldownRemaining();
	OutLeft.Duration = GetEffectiveCooldown();
	OutLeft.bBlocked = !IsUsable() || IsWorking();
}

const UYSCombatPostPreviewData* AYSShipCannon::GetPreviewData() const
{
	if (PreviewData)
	{
		return PreviewData;
	}
	static TWeakObjectPtr<UYSCombatPostPreviewData> DefaultData;
	if (!DefaultData.IsValid())
	{
		DefaultData = LoadObject<UYSCombatPostPreviewData>(nullptr, TEXT("/Game/Core/Ship/DA_CombatPostPreview.DA_CombatPostPreview"));
	}
	return DefaultData.Get();
}

namespace YSCannonPreview
{
	/** Trait plat (deux triangles) de Start à End, de la largeur donnée, à la hauteur Z */
	void AddLine(TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FLinearColor>& Colors, const FVector2D& Start, const FVector2D& End, float Width, float Z, const FLinearColor& Color)
	{
		const FVector2D Direction = (End - Start).GetSafeNormal();
		const FVector2D Side(-Direction.Y * Width * 0.5f, Direction.X * Width * 0.5f);
		const int32 Base = Vertices.Num();
		for (const FVector2D& Point : { Start - Side, Start + Side, End + Side, End - Side })
		{
			Vertices.Add(FVector(Point.X, Point.Y, Z));
			Colors.Add(Color);
		}
		Triangles.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
	}
}

void AYSShipCannon::UpdatePreview()
{
	// seulement chez le joueur installé au poste
	const AYSPlayerCharacter* User = GetOccupant();
	const UYSCombatPostPreviewData* Data = GetPreviewData();
	const bool bShow = Data && User && User->IsLocallyControlled() && User->IsPlayerControlled();
	if (!bShow)
	{
		if (PreviewMesh && PreviewMesh->IsVisible())
		{
			PreviewMesh->SetVisibility(false);
			BuiltPreviewColor = FLinearColor::Transparent;
			BuiltPreviewAimYaw = 1.0e6f;
		}
		return;
	}
	if (!PreviewMesh)
	{
		PreviewMesh = NewObject<UProceduralMeshComponent>(this, TEXT("CombatPostPreview"));
		PreviewMesh->SetupAttachment(GetRootComponent());
		PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PreviewMesh->SetCastShadow(false);
		PreviewMesh->bUseAsyncCooking = false;
		PreviewMesh->SetTranslucentSortPriority(100);
		PreviewMesh->RegisterComponent();
	}
	PreviewMesh->SetVisibility(true);

	// couleur du joueur (personnalisation)
	FLinearColor PlayerColor = FLinearColor::White;
	const AYSPlayerState* State = User->GetPlayerState<AYSPlayerState>();
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	if (State && YSGameState && YSGameState->GetCustomizationColors().IsValidIndex(State->GetColorIndex()))
	{
		PlayerColor = YSGameState->GetCustomizationColors()[State->GetColorIndex()].ButtonColor;
	}

	// repère du poste (+X = alignement d'origine), depuis le pivot du canon
	const FVector Pivot = CannonPivot->GetRelativeLocation();
	const FVector2D Origin(Pivot.X, Pivot.Y);
	const float Z = Data->HeightOffset;
	const auto Dir = [](float YawDegrees) { return FVector2D(FMath::Cos(FMath::DegreesToRadians(YawDegrees)), FMath::Sin(FMath::DegreesToRadians(YawDegrees))); };

	// section 0 : remplissage (couleur du joueur, estompé avec la distance) et traits des limites d'angle
	if (!PlayerColor.Equals(BuiltPreviewColor))
	{
		BuiltPreviewColor = PlayerColor;
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FLinearColor> Colors;
		const int32 Segments = FMath::Max(Data->FillAngleSegments, 2);
		const int32 Rings = FMath::Max(Data->FillRings, 1);
		for (int32 Ring = 0; Ring <= Rings; ++Ring)
		{
			const float Ratio = static_cast<float>(Ring) / Rings;
			const float Alpha = Data->FillStartAlpha * FMath::Pow(1.0f - Ratio, Data->FillFadeExponent);
			for (int32 Segment = 0; Segment <= Segments; ++Segment)
			{
				const FVector2D Point = Origin + Dir(FMath::Lerp(-MaxAimAngle, MaxAimAngle, static_cast<float>(Segment) / Segments)) * Data->FillFadeDistance * Ratio;
				Vertices.Add(FVector(Point.X, Point.Y, Z - 1.0f));
				Colors.Add(FLinearColor(PlayerColor.R, PlayerColor.G, PlayerColor.B, Alpha));
			}
		}
		for (int32 Ring = 0; Ring < Rings; ++Ring)
		{
			for (int32 Segment = 0; Segment < Segments; ++Segment)
			{
				const int32 A = Ring * (Segments + 1) + Segment;
				const int32 B = A + Segments + 1;
				Triangles.Append({ A, B, B + 1, A, B + 1, A + 1 });
			}
		}
		for (const float Side : { -MaxAimAngle, MaxAimAngle })
		{
			YSCannonPreview::AddLine(Vertices, Triangles, Colors, Origin, Origin + Dir(Side) * Data->AngleLineLength, Data->AngleLineWidth, Z, Data->AngleLineColor);
		}
		PreviewMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, TArray<FVector>(), TArray<FVector2D>(), Colors, TArray<FProcMeshTangent>(), false);
		PreviewMesh->SetMaterial(0, Data->Material);
	}

	// section 1 : trait de visée (là où le prochain tir partira)
	const float AimYaw = GetTargetAimYaw();
	const float PreviewKey = GetAimPreviewKey();
	if (!FMath::IsNearlyEqual(PreviewKey, BuiltPreviewAimYaw, 0.05f))
	{
		BuiltPreviewAimYaw = PreviewKey;
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FLinearColor> Colors;
		BuildAimPreview(Vertices, Triangles, Colors, Origin, Z + 1.0f, Data, AimYaw);
		PreviewMesh->CreateMeshSection_LinearColor(1, Vertices, Triangles, TArray<FVector>(), TArray<FVector2D>(), Colors, TArray<FProcMeshTangent>(), false);
		PreviewMesh->SetMaterial(1, Data->Material);
	}
}

void AYSShipCannon::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (HiddenComponentNames.Num() == 0)
	{
		return;
	}
	TInlineComponentArray<USceneComponent*> Components(this);
	for (USceneComponent* Component : Components)
	{
		// noms des variables du Blueprint (sans le suffixe des gabarits)
		const FName Name(*Component->GetName().Replace(TEXT("_GEN_VARIABLE"), TEXT("")));
		if (!HiddenComponentNames.Contains(Name))
		{
			continue;
		}
		Component->SetVisibility(false, true);
		TArray<USceneComponent*> HiddenTree;
		Component->GetChildrenComponents(true, HiddenTree);
		HiddenTree.Add(Component);
		for (USceneComponent* Child : HiddenTree)
		{
			if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Child))
			{
				Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}
}

float AYSShipCannon::GetEffectiveCooldown() const
{
	// Flibustier dans l'équipage : recharge des postes de canon du bateau des joueurs divisée
	return Cooldown * AYSBoatCrew::GetCannonCooldownMultiplier(GetShip());
}

float AYSShipCannon::GetShotDamageMultiplier(const APawn* Shooter, bool bFoeShooter) const
{
	// NME au poste : selon les jours écoulés (réglage du Galion)
	if (bFoeShooter)
	{
		const AYSFoeGalion* Galion = Cast<AYSFoeGalion>(GetShip());
		return Galion ? Galion->GetPostDamageMultiplier() : 1.0f;
	}
	// joueur ou minion : + Damage Per Player Level par niveau de l'équipe (passif : le sien, ex. Maître canonnier 10 %)
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	const int32 Level = YSGameState ? YSGameState->GetTeamLevel() : 0;
	float PerLevel = DamagePerPlayerLevel;
	const AYSPlayerCharacter* Character = Cast<AYSPlayerCharacter>(Shooter);
	const UYSPassive* Passive = Character ? Character->GetPassive() : nullptr;
	if (Passive && Passive->CombatPostDamagePerLevel >= 0.0f)
	{
		PerLevel = Passive->CombatPostDamagePerLevel;
	}
	return 1.0f + PerLevel * Level;
}

float AYSShipCannon::GetAutoFireRange() const
{
	const AYSProjectile* Ammo = ProjectileClass ? ProjectileClass->GetDefaultObject<AYSProjectile>() : nullptr;
	return AutoFireRange > 0.0f ? AutoFireRange : (Ammo ? Ammo->GetMaxRange() : 0.0f);
}

void AYSShipCannon::BuildAimPreview(TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FLinearColor>& Colors, const FVector2D& Origin, float Z,
	const UYSCombatPostPreviewData* Data, float AimYaw) const
{
	// un trait par AMMO du tir (grappeshot : l'éventail)
	for (int32 Index = 0; Index < AmmoPerShot; ++Index)
	{
		const float Yaw = AimYaw + (AmmoPerShot > 1 ? FMath::Lerp(-Spread, Spread, static_cast<float>(Index) / (AmmoPerShot - 1)) : 0.0f);
		const FVector2D Direction(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)));
		YSCannonPreview::AddLine(Vertices, Triangles, Colors, Origin, Origin + Direction * Data->AimLineLength, Data->AimLineWidth, Z, Data->AimLineColor);
	}
}

float AYSShipCannon::GetAimPreviewKey() const
{
	return GetTargetAimYaw();
}

FVector AYSShipMortar::ComputeShellTarget(const FVector& Desired) const
{
	// dans l'angle du poste, entre Min Range et Max Range du pivot, posé sur le sol (ou à la surface de la mer)
	const FVector Pivot = GetCannonPivot()->GetComponentLocation();
	const float Yaw = GetActorRotation().Yaw + ComputeAimYaw(Desired);
	const float Distance = FMath::Clamp(static_cast<float>(FVector::Dist2D(Desired, Pivot)), MinRange, FMath::Max(MinRange, MaxRange));
	FVector Target = Pivot + FRotator(0.0f, Yaw, 0.0f).Vector() * Distance;
	const FVector Ground = AYSInteractiveObject::FindGround(GetWorld(), Target, GetShip());
	Target.Z = AYSOcean::ClampAboveSea(GetWorld(), Ground.Equals(Target) ? -1.0e6f : Ground.Z);
	return Target;
}

float AYSShipMortar::GetShellRadius() const
{
	const AYSPlayerBomb* Shell = ShellClass ? ShellClass->GetDefaultObject<AYSPlayerBomb>() : nullptr;
	return Shell ? Shell->GetExplosionRadius() : 0.0f;
}

void AYSShipMortar::SpawnVolley(APawn* Shooter, float BaseYaw, bool bFoeShooter)
{
	if (!ShellClass)
	{
		return;
	}
	const FVector Start = GetMuzzle()->GetComponentLocation();
	const FVector Target = ComputeShellTarget(LastAimLocation);
	const float Multiplier = GetShotDamageMultiplier(Shooter, bFoeShooter);
	const FTransform SpawnTransform(FRotator::ZeroRotator, Start);
	AYSPlayerBomb* Shell = GetWorld()->SpawnActorDeferred<AYSPlayerBomb>(ShellClass, SpawnTransform, Shooter, Shooter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Shell)
	{
		return;
	}
	Shell->SetThrow(Start, Target, FlightDuration, ApexHeight);
	Shell->SetDamage(Damage * Multiplier);
	Shell->SetBlastHitTags(FGameplayTagContainer(bFoeShooter ? YSTags::Team_Player : YSTags::Team_Foe), true);
	Shell->FinishSpawning(SpawnTransform);
	MulticastFired();
	UE_LOG(LogTemp, Log, TEXT("[YSMortar] %s : obus de %s vers %s (%.0f cm), %.0f dégâts (x%.2f), rayon %.0f, impact dans %.1f s"), *GetName(), *GetNameSafe(Shooter),
		*Target.ToCompactString(), FVector::Dist2D(Start, Target), Damage * Multiplier, Multiplier, GetShellRadius(), FlightDuration);
}

FVector AYSShipMortar::GetLocalAimTarget() const
{
	const AYSPlayerCharacter* User = GetOccupant();
	const AYSPlayerController* Controller = User ? Cast<AYSPlayerController>(User->GetController()) : nullptr;
	return ComputeShellTarget(Controller ? Controller->GetAimLocation() : GetActorLocation() + GetActorForwardVector() * MaxRange);
}

float AYSShipMortar::GetAimPreviewKey() const
{
	// le cercle suit le curseur (repère du poste, au cm près)
	const FVector Local = GetActorTransform().InverseTransformPosition(GetLocalAimTarget());
	return FMath::RoundToFloat(Local.X) * 1.0f + FMath::RoundToFloat(Local.Y) * 7919.0f;
}

void AYSShipMortar::BuildAimPreview(TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FLinearColor>& Colors, const FVector2D& Origin, float Z,
	const UYSCombatPostPreviewData* Data, float AimYaw) const
{
	// cercle de l'explosion sous le point visé (repère du poste), intérieur translucide, et trait fin jusqu'à lui
	const FVector Local = GetActorTransform().InverseTransformPosition(GetLocalAimTarget());
	const FVector2D Center(Local.X, Local.Y);
	// à la hauteur du point d'impact (sol ou mer), un peu au-dessus
	const float CircleZ = Local.Z + Data->HeightOffset;
	const float Radius = GetShellRadius();
	const int32 Segments = FMath::Max(Data->MortarCircleSegments, 8);
	const int32 CenterIndex = Vertices.Num();
	Vertices.Add(FVector(Center.X, Center.Y, CircleZ));
	Colors.Add(Data->MortarFillColor);
	for (int32 Segment = 0; Segment <= Segments; ++Segment)
	{
		const float Angle = 2.0f * PI * Segment / Segments;
		const FVector2D Point = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
		Vertices.Add(FVector(Point.X, Point.Y, CircleZ));
		Colors.Add(FLinearColor(Data->MortarFillColor.R, Data->MortarFillColor.G, Data->MortarFillColor.B, Data->MortarFillColor.A * 0.5f));
		if (Segment > 0)
		{
			Triangles.Append({ CenterIndex, CenterIndex + Segment, CenterIndex + Segment + 1 });
		}
	}
	for (int32 Segment = 0; Segment < Segments; ++Segment)
	{
		const float A0 = 2.0f * PI * Segment / Segments;
		const float A1 = 2.0f * PI * (Segment + 1) / Segments;
		YSCannonPreview::AddLine(Vertices, Triangles, Colors, Center + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * Radius,
			Center + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * Radius, Data->MortarCircleWidth, CircleZ + 1.0f, Data->MortarCircleColor);
	}
	YSCannonPreview::AddLine(Vertices, Triangles, Colors, Origin, Center, Data->AngleLineWidth, Z, Data->AimLineColor);
}

bool AYSShipCannon::HasShotClass() const
{
	return ProjectileClass != nullptr;
}
