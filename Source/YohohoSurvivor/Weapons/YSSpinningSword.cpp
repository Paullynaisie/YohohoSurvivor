#include "Weapons/YSSpinningSword.h"
#include "Weapons/YSWeapon.h"
#include "Abilities/YSAbilityComponent.h"
#include "Core/YSCustomization.h"
#include "Core/YSGameplayTags.h"
#include "Player/YSPlayerCharacter.h"
#include "Core/YSDamageTypes.h"
#include "Upgrades/YSUpgrade.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayTagAssetInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace YSSpinningSwordHelpers
{
	/** Blueprint de réglages (s'il existe), sinon la classe C++ */
	const TCHAR* const BlueprintClassPath = TEXT("/Game/Placeable/Weapons/Player/BP_SpinningSword.BP_SpinningSword_C");
}

AYSSpinningSword::AYSSpinningSword()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	// Self : suit la position du joueur, pas sa rotation (il regarde le curseur) ni son échelle
	Root->SetUsingAbsoluteRotation(true);
	Root->SetUsingAbsoluteScale(true);

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Root);

	SwordVisualClass = TSoftClassPtr<AYSWeapon>(FSoftObjectPath(TEXT("/Game/Placeable/Weapons/Player/Wp_Melee/BP_Epee.BP_Epee_C")));
}

void AYSSpinningSword::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSSpinningSword, Level);
	DOREPLIFETIME_CONDITION(AYSSpinningSword, bSelf, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AYSSpinningSword, SpawnServerTime, COND_InitialOnly);
	DOREPLIFETIME(AYSSpinningSword, bKinesis);
}

AYSSpinningSword* AYSSpinningSword::GiveLevel(AYSPlayerCharacter* Character, bool bInSelf, const FVector& Location, int32 Levels, bool bAllowMerge)
{
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!World || !Character->HasAuthority() || Levels <= 0)
	{
		return nullptr;
	}

	UClass* SpawnClass = LoadSpawnClass(YSSpinningSwordHelpers::BlueprintClassPath, AYSSpinningSword::StaticClass());

	// déjà là : des levels en plus (Self toujours ; posée : seulement si la fusion est permise ; jamais une Lesser Spinning Blade)
	for (TActorIterator<AYSSpinningSword> It(World); It; ++It)
	{
		AYSSpinningSword* Existing = *It;
		if (!IsValid(Existing) || !Existing->bLevelDecay || Existing->GetOwner() != Character || Existing->bSelf != bInSelf)
		{
			continue;
		}
		if (bInSelf || (bAllowMerge && FVector::Dist2D(Existing->GetActorLocation(), Location) <= Existing->MergeRadius))
		{
			Existing->AddLevel(Levels);
			return Existing;
		}
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.0f;
	FVector Ground = bInSelf ? Character->GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight) : Location;
	if (!bInSelf)
	{
		// au sol sous le point visé (rien dessous, ex. la mer : hauteur des pieds du joueur)
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SpinningSword), false, Character);
		FHitResult Hit;
		if (World->LineTraceSingleByObjectType(Hit, Location + FVector(0.0f, 0.0f, 500.0f), Location - FVector(0.0f, 0.0f, 3000.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			Ground = Hit.ImpactPoint;
		}
		else
		{
			Ground.Z = Character->GetActorLocation().Z - HalfHeight;
		}
	}

	AYSSpinningSword* Sword = World->SpawnActorDeferred<AYSSpinningSword>(SpawnClass, FTransform(Ground), Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Sword)
	{
		return nullptr;
	}
	Sword->bSelf = bInSelf;
	const AGameStateBase* GameState = World->GetGameState();
	Sword->SpawnServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
	Sword->FinishSpawning(FTransform(Ground));
	// Bladékinésis : la Self Spinning Sword n'est pas accrochée (elle suit le curseur)
	if (bInSelf && Character->GetCharacterWeaponStat(UYSBladeMasterUpgrade::BladekinesisStat) <= 0.0f)
	{
		Sword->AttachToComponent(Character->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		Sword->SetActorRelativeLocation(FVector(0.0f, 0.0f, -HalfHeight));
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSSpinningSword] %s : nouvelle %s (%s) en (%.0f, %.0f, %.0f)"), *Character->GetName(), *Sword->GetName(),
		bInSelf ? TEXT("Self") : TEXT("posée"), Ground.X, Ground.Y, Ground.Z);
	Sword->AddLevel(Levels);
	return Sword;
}

void AYSSpinningSword::AddLevel(int32 Count)
{
	if (!HasAuthority() || Count <= 0)
	{
		return;
	}
	Level = FMath::Min(Level + Count, FMath::Max(MaxLevel, 1));
	// BladeMaster ++ : chaque level dure plus longtemps
	const AYSPlayerCharacter* Character = GetOwnerCharacter();
	const float Duration = LevelDuration + (Character ? Character->GetCharacterWeaponStat(UYSBladeMasterUpgrade::SpinningDurationStat) : 0.0f);
	if (bLevelDecay)
	{
		GetWorldTimerManager().SetTimer(LevelTimer, this, &AYSSpinningSword::LoseLevel, Duration, true);
	}
	ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("[YSSpinningSword] %s : level %d (délai relancé, %.1f s)"), *GetName(), Level, Duration);
	OnRep_Level();
}

UClass* AYSSpinningSword::LoadSpawnClass(const TCHAR* BlueprintPath, UClass* Fallback)
{
	UClass* Class = TSoftClassPtr<AYSSpinningSword>(FSoftObjectPath(BlueprintPath)).LoadSynchronous();
	return Class ? Class : Fallback;
}

float AYSSpinningSword::GetDefaultLevelDuration()
{
	return GetDefault<AYSSpinningSword>(LoadSpawnClass(YSSpinningSwordHelpers::BlueprintClassPath, AYSSpinningSword::StaticClass()))->LevelDuration;
}

AYSSpinningSword* AYSSpinningSword::FindSelf(const AYSPlayerCharacter* Character)
{
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	for (TActorIterator<AYSSpinningSword> It(World); World && It; ++It)
	{
		if (IsValid(*It) && It->bSelf && It->bLevelDecay && It->GetOwner() == Character)
		{
			return *It;
		}
	}
	return nullptr;
}

float AYSSpinningSword::FindGroundZ(const FVector& Location, float FallbackZ) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SpinningSwordGround), false, GetOwner());
	Params.AddIgnoredActor(this);
	FHitResult Hit;
	if (GetWorld() && GetWorld()->LineTraceSingleByObjectType(Hit, FVector(Location.X, Location.Y, Location.Z + 500.0f), FVector(Location.X, Location.Y, Location.Z - 3000.0f),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return Hit.ImpactPoint.Z;
	}
	return FallbackZ;
}

void AYSSpinningSword::ServerSetKinesisTarget_Implementation(FVector_NetQuantize Target)
{
	KinesisTarget = Target;
	bHasKinesisTarget = true;
}

void AYSSpinningSword::UpdateSelfMovement(float DeltaSeconds)
{
	AYSPlayerCharacter* Character = GetOwnerCharacter();
	if (!bSelf || !bLevelDecay || !IsValid(Character))
	{
		return;
	}
	if (HasAuthority())
	{
		bKinesis = Character->GetCharacterWeaponStat(UYSBladeMasterUpgrade::BladekinesisStat) > 0.0f;
	}
	// client qui contrôle le joueur : envoie le point visé (20 fois par seconde)
	if (bKinesis && !HasAuthority() && Character->IsLocallyControlled())
	{
		KinesisSendTimer -= DeltaSeconds;
		if (KinesisSendTimer <= 0.0f)
		{
			KinesisSendTimer = 0.05f;
			ServerSetKinesisTarget(Character->GetPublicAimLocation());
		}
	}
	if (!HasAuthority())
	{
		return;
	}

	if (!bKinesis)
	{
		// accrochée aux pieds du joueur (ex. Golden prise ou perdue)
		if (GetAttachParentActor() != Character)
		{
			const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
			AttachToComponent(Character->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
			SetActorRelativeLocation(FVector(0.0f, 0.0f, Capsule ? -Capsule->GetScaledCapsuleHalfHeight() : -90.0f));
		}
		return;
	}

	// Bladékinésis : vers le curseur, à la vitesse actuelle du joueur (ralentissements et accélérations compris)
	if (GetAttachParentActor())
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		UE_LOG(LogTemp, Verbose, TEXT("[YSSpinningSword] %s : Bladékinésis, suit le curseur"), *GetName());
	}
	if (Character->IsLocallyControlled())
	{
		KinesisTarget = Character->GetPublicAimLocation();
		bHasKinesisTarget = true;
	}
	if (!bHasKinesisTarget)
	{
		return;
	}
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const float Speed = Movement ? Movement->GetMaxSpeed() : 600.0f;
	const FVector Current = GetActorLocation();
	FVector Delta = KinesisTarget - Current;
	Delta.Z = 0.0f;
	const float Step = Speed * DeltaSeconds;
	FVector Next = Delta.SizeSquared() <= FMath::Square(Step) ? FVector(KinesisTarget.X, KinesisTarget.Y, Current.Z) : Current + Delta.GetSafeNormal() * Step;
	Next.Z = FindGroundZ(Next, Current.Z);
	SetActorLocation(Next);
	KinesisSendTimer -= DeltaSeconds;
	if (KinesisSendTimer <= 0.0f)
	{
		KinesisSendTimer = 1.0f;
		UE_LOG(LogTemp, Verbose, TEXT("[YSSpinningSword] %s : Bladékinésis en (%.0f, %.0f) vers (%.0f, %.0f) à %.0f cm/s"), *GetName(), Next.X, Next.Y, KinesisTarget.X, KinesisTarget.Y, Speed);
	}
}

void AYSSpinningSword::LoseLevel()
{
	--Level;
	UE_LOG(LogTemp, Verbose, TEXT("[YSSpinningSword] %s : level perdu -> %d"), *GetName(), Level);
	if (Level <= 0)
	{
		Destroy();
		return;
	}
	ForceNetUpdate();
	OnRep_Level();
}

void AYSSpinningSword::OnRep_Level()
{
	RebuildSwords();
}

void AYSSpinningSword::BeginPlay()
{
	Super::BeginPlay();

	// même angle sur toutes les machines au départ (heure serveur depuis la création)
	Angle = FMath::Fmod(static_cast<float>(FMath::Max(GetServerTime() - SpawnServerTime, 0.0)) * GetCurrentRotationSpeed(), 360.0f);
	Pivot->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, Height), FRotator(0.0f, Angle, 0.0f));
	if (Level > 0)
	{
		RebuildSwords();
	}
}

void AYSSpinningSword::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroySwords();
	GetWorldTimerManager().ClearTimer(LevelTimer);
	Super::EndPlay(EndPlayReason);
}

void AYSSpinningSword::DestroySwords()
{
	for (FSword& Sword : Swords)
	{
		if (AYSWeapon* Visual = Sword.Visual.Get())
		{
			Visual->Destroy();
		}
	}
	Swords.Reset();
}

void AYSSpinningSword::RebuildSwords()
{
	UWorld* World = GetWorld();
	if (!World || !HasActorBegunPlay())
	{
		return;
	}
	const int32 Count = FMath::Max(Level, 0);
	if (Swords.Num() == Count)
	{
		return;
	}
	DestroySwords();
	bColorsApplied = false;

	UClass* VisualClass = SwordVisualClass.LoadSynchronous();
	if (!VisualClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSSpinningSword] %s : pas de Sword Visual Class"), *GetName());
		return;
	}

	const float Scale = SwordScale * (GetOwnerCharacter() ? GetOwnerCharacter()->GetMeleeRangeMultiplier() : 1.0f);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// visuel local (pas répliqué), sans ses actions (pas d'attaque toute seule), sans collision
		AYSWeapon* Visual = World->SpawnActorDeferred<AYSWeapon>(VisualClass, Pivot->GetComponentTransform(), this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Visual)
		{
			continue;
		}
		Visual->SetReplicates(false);
		Visual->FinishSpawning(Pivot->GetComponentTransform());
		TInlineComponentArray<UYSAbilityComponent*> Actions(Visual);
		for (UYSAbilityComponent* Action : Actions)
		{
			Action->DestroyComponent();
		}
		Visual->SetActorEnableCollision(false);
		Visual->AttachToComponent(Pivot, FAttachmentTransformRules::KeepRelativeTransform);
		// épée à l'angle 0, tournée de Index / Count tour
		const FTransform Local(SwordRotation.Quaternion(), SwordLocation, FVector(Scale));
		Visual->SetActorRelativeTransform(Local * FTransform(FRotator(0.0f, 360.0f * Index / Count, 0.0f)));

		FSword& Sword = Swords.AddDefaulted_GetRef();
		Sword.Visual = Visual;
		Sword.Hitbox = Visual->FindComponentByClass<UBoxComponent>();
	}
	ApplyColors();
	// orientation (axes de l'arme) : lame (Z) vers l'extérieur = 1 ; axe X dans le sens de la rotation = 1
	float BladeDot = 0.0f;
	float EdgeDot = 0.0f;
	if (const AYSWeapon* First = Swords.Num() > 0 ? Swords[0].Visual.Get() : nullptr)
	{
		const FVector Radial = (First->GetActorLocation() - Pivot->GetComponentLocation()).GetSafeNormal2D();
		BladeDot = FVector::DotProduct(First->GetActorUpVector(), Radial);
		EdgeDot = FVector::DotProduct(First->GetActorForwardVector(), FVector::UpVector ^ Radial) * FMath::Sign(RotationSpeed);
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSSpinningSword] %s : %d épée(s), échelle %.2f, %.0f°/s, lame vers l'extérieur %.2f, axe X / sens de rotation %.2f"), *GetName(), Swords.Num(), Scale, GetCurrentRotationSpeed(), BladeDot, EdgeDot);
}

void AYSSpinningSword::ApplyColors()
{
	if (bColorsApplied || Swords.Num() == 0)
	{
		return;
	}
	const FYSCustomizationColor* Color = YSPlayerColor::Find(GetOwner());
	if (!Color || !Color->Material)
	{
		return;
	}
	bColorsApplied = true;
	for (FSword& Sword : Swords)
	{
		AYSWeapon* Visual = Sword.Visual.Get();
		if (!Visual)
		{
			continue;
		}
		// tout à la couleur du joueur, puis le composant Coloration à son émissif
		TInlineComponentArray<UMeshComponent*> Meshes(Visual);
		for (UMeshComponent* Mesh : Meshes)
		{
			for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
			{
				Mesh->SetMaterial(Slot, Color->Material);
			}
		}
		Visual->SetColoration(Color->EmissiveMaterial);
	}
}

void AYSSpinningSword::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// propriétaire détruit (mort, changement de classe ou de set = nouveau personnage) : la Spinning Sword disparaît
	if (HasAuthority() && !IsValid(GetOwnerCharacter()))
	{
		Destroy();
		return;
	}

	UpdateSelfMovement(DeltaSeconds);
	ApplyColors();

	// taille : Melee Range du propriétaire (répliqué)
	const float Scale = SwordScale * (GetOwnerCharacter() ? GetOwnerCharacter()->GetMeleeRangeMultiplier() : 1.0f);
	const int32 Count = Swords.Num();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		AYSWeapon* Visual = Swords[Index].Visual.Get();
		if (Visual && !FMath::IsNearlyEqual(Visual->GetRootComponent()->GetRelativeScale3D().X, Scale, 0.001f))
		{
			const FTransform Local(SwordRotation.Quaternion(), SwordLocation, FVector(Scale));
			Visual->SetActorRelativeTransform(Local * FTransform(FRotator(0.0f, 360.0f * Index / Count, 0.0f)));
		}
	}

	const float PreviousAngle = Angle;
	Angle += GetCurrentRotationSpeed() * DeltaSeconds;
	Pivot->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, Height), FRotator(0.0f, Angle, 0.0f));
	if (HasAuthority())
	{
		ApplyHits(PreviousAngle, Angle);
	}
	if (Angle >= 360.0f || Angle < 0.0f)
	{
		Angle = FMath::Fmod(Angle, 360.0f);
	}
}

void AYSSpinningSword::ApplyHits(float PreviousAngle, float CurrentAngle)
{
	AYSPlayerCharacter* Character = GetOwnerCharacter();
	UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	const float Delta = CurrentAngle - PreviousAngle;
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(FMath::Abs(Delta) / MaxHitStepAngle), 1, 36);
	const FTransform PivotTransform = Pivot->GetComponentTransform();
	const float FinalDamage = Damage * Character->GetSlotDamageMultiplier(EYSActionSlot::AutoAtk) * Character->GetSlotDamageMultiplier(EYSActionSlot::Ability);
	const FGameplayTagContainer FoeTags(YSTags::Team_Foe);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SpinningSword), false, Character);

	for (FSword& Sword : Swords)
	{
		const UBoxComponent* Hitbox = Sword.Hitbox.Get();
		if (!Hitbox)
		{
			continue;
		}
		// hitbox balayée entre l'angle précédent et l'angle actuel (rotation autour du centre)
		const FTransform Relative = Hitbox->GetComponentTransform().GetRelativeTransform(PivotTransform);
		TSet<TWeakObjectPtr<AActor>> NowOverlapping;
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			const float StepAngle = PreviousAngle + Delta * Step / Steps;
			const FTransform StepPivot(FRotator(0.0f, StepAngle, 0.0f), PivotTransform.GetLocation(), PivotTransform.GetScale3D());
			const FTransform HitTransform = Relative * StepPivot;
			TArray<FOverlapResult> Overlaps;
			World->OverlapMultiByObjectType(Overlaps, HitTransform.GetLocation(), HitTransform.GetRotation(), FCollisionObjectQueryParams(ECC_Pawn),
				FCollisionShape::MakeBox(Hitbox->GetUnscaledBoxExtent() * HitTransform.GetScale3D().GetAbs()), Params);
			for (const FOverlapResult& Overlap : Overlaps)
			{
				AActor* Target = Overlap.GetActor();
				const IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Target);
				if (Target && TagInterface && TagInterface->HasAnyMatchingGameplayTags(FoeTags))
				{
					NowOverlapping.Add(Target);
				}
			}
		}

		for (const TWeakObjectPtr<AActor>& WeakTarget : NowOverlapping)
		{
			AActor* Target = WeakTarget.Get();
			const float* LastHit = Sword.LastHitTimes.Find(WeakTarget);
			// entrée dans la hitbox (passage de l'épée), ou resté dedans depuis Rehit Interval
			if (!Target || (Sword.Overlapping.Contains(WeakTarget) && LastHit && Now - *LastHit < RehitInterval))
			{
				continue;
			}
			Sword.LastHitTimes.Add(WeakTarget, Now);
			// pushback : du centre de rotation vers le NME
			const FVector ShotDirection = (Target->GetActorLocation() - PivotTransform.GetLocation()).GetSafeNormal2D();
			// Lesser Spinning Blade de Blade Throw : critique dans la zone de croisement
			const bool bCritical = IsCriticalAt(Target->GetActorLocation());
			const float HitDamage = bCritical ? FinalDamage * Character->GetCriticalDamageMultiplier() : FinalDamage;
			const FPointDamageEvent DamageEvent(HitDamage, FHitResult(), ShotDirection, bCritical ? UYSCriticalDamageType::StaticClass() : nullptr);
			UE_LOG(LogTemp, Verbose, TEXT("[YSSpinningSword] %s touche %s : %.1f dégâts (%.0f x Auto atk %.2f x Ability %.2f%s)"), *GetName(), *Target->GetName(), HitDamage,
				Damage, Character->GetSlotDamageMultiplier(EYSActionSlot::AutoAtk), Character->GetSlotDamageMultiplier(EYSActionSlot::Ability), bCritical ? TEXT(", CRITIQUE") : TEXT(""));
			Target->TakeDamage(HitDamage, DamageEvent, Character->GetController(), nullptr);
		}
		Sword.Overlapping = MoveTemp(NowOverlapping);

		if (Sword.LastHitTimes.Num() > 64)
		{
			for (auto It = Sword.LastHitTimes.CreateIterator(); It; ++It)
			{
				if (!It->Key.IsValid() || Now - It->Value >= RehitInterval)
				{
					It.RemoveCurrent();
				}
			}
		}
	}
}

AYSPlayerCharacter* AYSSpinningSword::GetOwnerCharacter() const
{
	return Cast<AYSPlayerCharacter>(GetOwner());
}

float AYSSpinningSword::GetCurrentRotationSpeed() const
{
	// Atk speed : recharge de l'Auto atk x0.5 = 2x plus vite
	const AYSPlayerCharacter* Character = GetOwnerCharacter();
	const float CooldownMultiplier = Character ? Character->GetSlotCooldownMultiplier(EYSActionSlot::AutoAtk) : 1.0f;
	return RotationSpeed / FMath::Max(CooldownMultiplier, 0.05f);
}

double AYSSpinningSword::GetServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}
