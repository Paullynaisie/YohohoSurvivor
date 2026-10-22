#include "Enemies/YSFoeCharacter.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/WidgetComponent.h"
#include "Core/YSCombat.h"
#include "Core/YSDamageTypes.h"
#include "Core/YSGameplayTags.h"
#include "Core/YSGameMode.h"
#include "Core/YSGameState.h"
#include "Enemies/YSFoeBomb.h"
#include "Interaction/YSInteractiveObject.h"
#include "Player/YSFaceDecalComponent.h"
#include "Player/YSPlayerCharacter.h"
#include "UI/YSHealthBarWidget.h"
#include "Components/ChildActorComponent.h"
#include "Weapons/YSWeapon.h"
#include "Weapons/YSWeaponSet.h"

namespace YSFoe
{
	/** Au-delà de cet écart (téléportation), une main rejoint sa cible sans retard */
	constexpr float HandSnapDistance = 300.0f;
}

AYSFoeCharacter::AYSFoeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// un contrôleur IA est nécessaire pour que le CharacterMovementComponent consomme l'input
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();
	BombClass = AYSFoeBomb::StaticClass();

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	// la rotation est pilotée à la main vers la cible, pas par le mouvement ni le contrôleur
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->MaxWalkSpeed = MoveSpeed;
	Movement->bConstrainToPlane = true;
	Movement->bSnapToPlaneAtStart = true;

	// DA « primitives » comme le joueur : pas de mesh squelettique, le corps est un cylindre (BodyMesh), sans inclinaison
	GetMesh()->SetSkeletalMeshAsset(nullptr);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -96.0f), FRotator(0.0f, -90.0f, 0.0f));

	// mêmes proportions que le corps de PlayerCharacter_mouss (cylindre moteur 100 x 100 x 100)
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -20.0f));
	BodyMesh->SetRelativeScale3D(FVector(0.84f, 0.84f, 1.44f));
	YSCombat::ConfigureHurtbox(BodyMesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultBody(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (DefaultBody.Succeeded())
	{
		BodyMesh->SetStaticMesh(DefaultBody.Object);
	}

	// visage : decal projeté sur l'avant du cylindre (même placement que le joueur)
	FaceDecal = CreateDefaultSubobject<UYSFaceDecalComponent>(TEXT("FaceDecal"));
	FaceDecal->SetupAttachment(RootComponent);
	FaceDecal->SetRelativeLocationAndRotation(FVector(42.0f, 0.0f, 10.0f), FRotator(0.0f, 180.0f, 90.0f));
	FaceDecal->DecalSize = FVector(12.0f, 28.0f, 28.0f);

	// contact : un peu plus large que le corps (rayon 42), sinon les capsules empêchent tout contact
	Hitbox = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Hitbox"));
	Hitbox->SetupAttachment(RootComponent);
	Hitbox->SetRelativeLocation(FVector(0.0f, 0.0f, -20.0f));
	Hitbox->InitCapsuleSize(55.0f, 72.0f);
	YSCombat::ConfigureHitbox(Hitbox);

	GameplayTags.AddTag(YSTags::Team_Foe);
	ContactHitTags.AddTag(YSTags::Team_Player);

	// barre de vie en espace écran, juste au-dessus de la tête
	HealthBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(RootComponent);
	HealthBar->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	HealthBar->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBar->SetDrawAtDesiredSize(true);
	HealthBar->SetWidgetClass(UYSHealthBarWidget::StaticClass());
	HealthBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// mains comme le joueur : un point d'attache par côté (position monde calculée par UpdateHands) + une petite sphère
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultHand(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	auto CreateHand = [this](const TCHAR* SocketName, const TCHAR* MeshName, const TCHAR* WeaponName,
		TObjectPtr<USceneComponent>& OutSocket, TObjectPtr<UStaticMeshComponent>& OutMesh, TObjectPtr<UChildActorComponent>& OutWeapon)
	{
		OutSocket = CreateDefaultSubobject<USceneComponent>(SocketName);
		OutSocket->SetupAttachment(RootComponent);
		OutSocket->SetUsingAbsoluteLocation(true);

		OutMesh = CreateDefaultSubobject<UStaticMeshComponent>(MeshName);
		OutMesh->SetupAttachment(OutSocket);
		OutMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OutMesh->SetRelativeScale3D(FVector(0.25f));
		if (DefaultHand.Succeeded())
		{
			OutMesh->SetStaticMesh(DefaultHand.Object);
		}

		// arme : acteur enfant ; les armes de NME ne sont pas répliquées (Replicates décoché) : chaque machine crée la sienne
		OutWeapon = CreateDefaultSubobject<UChildActorComponent>(WeaponName);
		OutWeapon->SetupAttachment(OutSocket);
		OutWeapon->SetChildActorOwnerOnCreation(true);
	};
	CreateHand(TEXT("LeftHand"), TEXT("LeftHandMesh"), TEXT("LeftHandWeapon"), LeftHand, LeftHandMesh, LeftHandWeapon);
	CreateHand(TEXT("RightHand"), TEXT("RightHandMesh"), TEXT("RightHandWeapon"), RightHand, RightHandMesh, RightHandWeapon);
}

void AYSFoeCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// changer le set dans le Blueprint met à jour son visuel immédiatement
	ApplyWeaponClasses();

	// exécuté aussi dans l'éditeur : changer le matériau du corps change celui des mains
	if (bHandsUseBodyMaterial)
	{
		for (UStaticMeshComponent* HandMesh : { LeftHandMesh.Get(), RightHandMesh.Get() })
		{
			for (int32 Index = 0; Index < HandMesh->GetNumMaterials(); ++Index)
			{
				HandMesh->SetMaterial(Index, BodyMesh->GetMaterial(0));
			}
		}
	}

	// mains à leur position cible (sans retard) : visible dans l'éditeur et point de départ en jeu
	bHandsInitialized = false;
	UpdateHands(0.0f);
}

void AYSFoeCharacter::ApplyWeaponClasses()
{
	// gaucher : miroir du NME (réglé droitier) = armes des deux mains échangées
	UClass* SetRightClass = WeaponSet ? WeaponSet->RightHandWeapon.Get() : RightHandWeaponClass.Get();
	UClass* SetLeftClass = WeaponSet ? WeaponSet->LeftHandWeapon.Get() : LeftHandWeaponClass.Get();
	UClass* LeftClass = bLeftHanded ? SetRightClass : SetLeftClass;
	UClass* RightClass = bLeftHanded ? SetLeftClass : SetRightClass;
	for (const TPair<UChildActorComponent*, UClass*> Hand : { TPair<UChildActorComponent*, UClass*>(LeftHandWeapon, LeftClass), TPair<UChildActorComponent*, UClass*>(RightHandWeapon, RightClass) })
	{
		if (Hand.Key->GetChildActorClass() != Hand.Value)
		{
			Hand.Key->SetChildActorClass(Hand.Value);
		}
	}
}

void AYSFoeCharacter::OnRep_LeftHanded()
{
	ApplyWeaponClasses();
	bHandsInitialized = false;
}

AYSWeapon* AYSFoeCharacter::GetLeftHandWeapon() const
{
	return Cast<AYSWeapon>(LeftHandWeapon->GetChildActor());
}

AYSWeapon* AYSFoeCharacter::GetRightHandWeapon() const
{
	return Cast<AYSWeapon>(RightHandWeapon->GetChildActor());
}

FVector AYSFoeCharacter::GetHandTargetLocation(bool bLeft) const
{
	// l'arme tenue peut imposer sa position de main ; valeurs exprimées pour la main droite
	FVector Location = HandLocation;
	const AYSWeapon* Weapon = bLeft ? GetLeftHandWeapon() : GetRightHandWeapon();
	FVector WeaponLocation;
	if (Weapon && Weapon->GetHandLocationOverride(WeaponLocation))
	{
		Location = WeaponLocation;
	}
	if (bLeft)
	{
		Location.Y = -Location.Y;
	}
	return Location;
}

FRotator AYSFoeCharacter::GetHandTargetRotation(bool bLeft) const
{
	FRotator Rotation = HandRotation;
	const AYSWeapon* Weapon = bLeft ? GetLeftHandWeapon() : GetRightHandWeapon();
	FRotator WeaponRotation;
	if (Weapon && Weapon->GetHandRotationOverride(WeaponRotation))
	{
		Rotation = WeaponRotation;
	}
	// symétrie par rapport au plan avant/haut du corps : Yaw et Roll s'inversent
	if (bLeft)
	{
		Rotation.Yaw = -Rotation.Yaw;
		Rotation.Roll = -Rotation.Roll;
	}
	return Rotation;
}

void AYSFoeCharacter::UpdateHands(float DeltaSeconds)
{
	// perf : pur visuel local (rien n'est répliqué) ; hors écran les mains restent figées
	if (bHandsInitialized && GetWorld() && GetWorld()->IsGameWorld() && !WasRecentlyRendered(HandsOffscreenFreezeDelay))
	{
		return;
	}

	HandSwayTime += DeltaSeconds;
	const FTransform& BodyTransform = GetRootComponent()->GetComponentTransform();

	auto UpdateHand = [this, DeltaSeconds, &BodyTransform](USceneComponent* Hand, FVector& WorldLocation, bool bLeft, float NoiseSeed)
	{
		// oscillation : bruit de Perlin sur chaque axe, limité à HandSwayAmplitude autour de la cible
		const float NoiseTime = HandSwayTime * HandSwaySpeed + NoiseSeed;
		const FVector Sway = FVector(
			FMath::PerlinNoise1D(NoiseTime),
			FMath::PerlinNoise1D(NoiseTime + 31.7f),
			FMath::PerlinNoise1D(NoiseTime + 67.3f)) * HandSwayAmplitude;
		const FVector Target = BodyTransform.TransformPosition(GetHandTargetLocation(bLeft) + Sway.GetClampedToMaxSize(HandSwayAmplitude));

		// retard : la main rejoint sa cible progressivement (téléportation / première image : directement)
		if (!bHandsInitialized || HandFollowSpeed <= 0.0f || FVector::DistSquared(WorldLocation, Target) > FMath::Square(YSFoe::HandSnapDistance))
		{
			WorldLocation = Target;
		}
		else
		{
			WorldLocation = FMath::VInterpTo(WorldLocation, Target, DeltaSeconds, HandFollowSpeed);
		}

		Hand->SetWorldLocation(WorldLocation);
		Hand->SetRelativeRotation(GetHandTargetRotation(bLeft));
	};

	UpdateHand(LeftHand, LeftHandWorldLocation, true, 0.0f);
	UpdateHand(RightHand, RightHandWorldLocation, false, 100.0f);
	bHandsInitialized = true;
}

void AYSFoeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSFoeCharacter, MaxHealth);
	DOREPLIFETIME(AYSFoeCharacter, Health);
	DOREPLIFETIME(AYSFoeCharacter, Level);
	DOREPLIFETIME(AYSFoeCharacter, bLeftHanded);
}

void AYSFoeCharacter::OnRep_Health()
{
	UpdateHealthBar();
}

void AYSFoeCharacter::UpdateHealthBar()
{
	if (UYSHealthBarWidget* Bar = Cast<UYSHealthBarWidget>(HealthBar->GetUserWidgetObject()))
	{
		Bar->SetHealth(Health, MaxHealth);
	}
}

float AYSFoeCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// dégâts toujours entiers (arrondis à l'entier le plus proche)
	const float Damage = FMath::RoundToFloat(Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser));
	if (!HasAuthority() || Health <= 0.0f || Damage <= 0.0f)
	{
		return 0.0f;
	}

	Health = FMath::Max(0.0f, Health - Damage);

	// chiffre de dégâts chez tous les joueurs, au point d'impact (position du projectile)
	AYSGameState::ReportDamage(this, Damage, DamageCauser ? DamageCauser->GetActorLocation() : GetActorLocation(), EventInstigator, UYSCriticalDamageType::IsCritical(DamageEvent));

	// OnRep n'est pas appelé sur le serveur : mise à jour directe pour le joueur hôte
	UpdateHealthBar();

	if (Health <= 0.0f)
	{
		Die();
		return Damage;
	}

	ApplyPushback(Damage, DamageEvent, EventInstigator);
	return Damage;
}

void AYSFoeCharacter::ApplyPushback(float Damage, const FDamageEvent& DamageEvent, const AController* EventInstigator)
{
	// direction du coup portée par l'événement (hitbox de l'arme, vol du projectile) ; force réglée sur le personnage qui frappe
	const AYSPlayerCharacter* Attacker = EventInstigator ? Cast<AYSPlayerCharacter>(EventInstigator->GetPawn()) : nullptr;
	if (!Attacker || !DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		return;
	}
	const FVector Direction = static_cast<const FPointDamageEvent&>(DamageEvent).ShotDirection.GetSafeNormal2D();
	const float Force = Attacker->ComputePushbackForce(Damage, MaxHealth);
	if (Direction.IsNearlyZero() || Force <= 0.0f)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[YSPushback] %s : %.0f dégâts sur %.0f PV max (%.1f %%), pas de pushback"), *GetName(), Damage, MaxHealth, 100.0f * Damage / FMath::Max(MaxHealth, 1.0f));
		return;
	}

	// serveur : le mouvement est répliqué aux clients
	LaunchCharacter(Direction * Force, true, false);

	// debug : distance réellement parcourue après 0,5 s (log Verbose)
	const FVector Start = GetActorLocation();
	FTimerHandle Timer;
	GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, Start]()
	{
		UE_LOG(LogTemp, Verbose, TEXT("[YSPushback] %s : %.0f cm parcourus en 0,5 s"), *GetName(), FVector::Dist2D(Start, GetActorLocation()));
	}), 0.5f, false);
	UE_LOG(LogTemp, Verbose, TEXT("[YSPushback] %s : %.0f dégâts sur %.0f PV max (%.1f %%), pushback %.0f vers (%.2f, %.2f)"), *GetName(), Damage, MaxHealth,
		100.0f * Damage / FMath::Max(MaxHealth, 1.0f), Force, Direction.X, Direction.Y);
}

void AYSFoeCharacter::Die()
{
	// XP commune à toute l'équipe
	if (AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>())
	{
		GameMode->AddTeamExperience(GetExperienceReward());

		// butin (coffre de l'élite) posé au sol là où il meurt
		if (DeathLootClass)
		{
			const AYSInteractiveObject* Loot = AYSInteractiveObject::SpawnOnGround(GetWorld(), DeathLootClass, GetActorLocation(), GetActorRotation().Yaw);
			UE_LOG(LogTemp, Log, TEXT("[YSFoe] %s laisse %s"), *GetName(), Loot ? *Loot->GetName() : TEXT("(échec)"));
		}

		// la mort de l'élite termine la nuit
		if (bIsElite)
		{
			GameMode->OnEliteKilled(this);
		}
	}

	Destroy();
}

void AYSFoeCharacter::FellOutOfWorld(const UDamageType& DamageType)
{
	// mort normale (XP, fin de la nuit pour l'élite) décidée par le serveur
	if (!HasAuthority() || IsActorBeingDestroyed())
	{
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSDeadZone] %s tombé sous la map (Z %.0f) : mort"), *GetName(), GetActorLocation().Z);
	Health = 0.0f;
	Die();
}

UPrimitiveComponent* AYSFoeCharacter::GetHurtbox() const
{
	return BodyMesh;
}

void AYSFoeCharacter::BeginPlay()
{
	Super::BeginPlay();

	// réappliqué en jeu : les collisions de combat ne dépendent pas de réglages sauvegardés dans le Blueprint
	YSCombat::ConfigureHurtbox(BodyMesh);
	YSCombat::ConfigureHitbox(Hitbox);
	// NME à distance : pas de corps à corps
	if (bRanged)
	{
		Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;

	// taille (élites) : réglage du Blueprint, appliqué à l'identique sur toutes les machines.
	// Échelle ABSOLUE (celle de la classe x SizeMultiplier) : chez un client, l'acteur arrive déjà à l'échelle du serveur
	// (transform répliqué) ; multiplier l'échelle actuelle la doublait (élite x9 au lieu de x3 chez les clients)
	if (!FMath::IsNearlyEqual(SizeMultiplier, 1.0f))
	{
		const USceneComponent* DefaultRoot = GetClass()->GetDefaultObject<AYSFoeCharacter>()->GetRootComponent();
		const FVector BaseScale = DefaultRoot ? DefaultRoot->GetRelativeScale3D() : FVector::OneVector;
		SetActorScale3D(BaseScale * SizeMultiplier);
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s (%s) échelle %.2f"), *GetName(), HasAuthority() ? TEXT("serveur") : TEXT("client"), GetActorScale3D().X);

	if (HasAuthority())
	{
		// gaucher ou droitier (répliqué : chaque machine échange alors ses armes locales)
		bLeftHanded = LeftHandedChance > 0.0f && FMath::FRand() < LeftHandedChance;
		ApplyWeaponClasses();

		AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
		if (bLevelFromDay && YSGameState)
		{
			Level = FMath::Max(0, YSGameState->GetDayNumber() + LevelFromDayOffset);
		}

		// statistiques du niveau (MaxHealth est répliqué pour les barres de vie)
		MaxHealth = (BaseHealth + HealthPerLevel * Level) * HealthMultiplier;
		ContactDamage = (BaseContactDamage + ContactDamagePerLevel * Level) * DamageMultiplier;
		BombDamage = (BaseBombDamage + BombDamagePerLevel * Level) * DamageMultiplier;
		Health = MaxHealth;

		if (bIsElite && YSGameState)
		{
			YSGameState->SetCurrentElite(this);
		}
	}

	HealthBar->InitWidget();
	// élite : barre de vie dans le HUD (AYSPlayerController), pas au-dessus de sa tête
	HealthBar->SetVisibility(!bIsElite);
	UpdateHealthBar();
}

void AYSFoeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// mains : visuel local sur toutes les machines
	UpdateHands(DeltaSeconds);

	// serveur autoritaire : les clients reçoivent position et rotation par réplication
	if (!HasAuthority())
	{
		return;
	}

	if (!bRanged)
	{
		ApplyContactDamage();
	}

	Target = FindNearestPlayer();
	if (!Target)
	{
		return;
	}

	FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	// à distance : la bombe se recharge en permanence (même hors de portée) ; rechargée, elle part dès que la cible
	// est à portée, et le NME s'arrête à portée
	if (bRanged)
	{
		ThrowTimer = FMath::Min(ThrowTimer + DeltaSeconds, ThrowInterval);
	}
	const bool bInRange = bRanged && ToTarget.SizeSquared() <= FMath::Square(AttackRange);
	if (bInRange)
	{
		if (ThrowTimer >= ThrowInterval)
		{
			ThrowTimer = 0.0f;
			ThrowBomb(Target);
		}
	}
	else
	{
		AddMovementInput(ToTarget.GetSafeNormal());
	}

	const FRotator Desired(0.0f, ToTarget.Rotation().Yaw, 0.0f);
	SetActorRotation(TurnSpeed > 0.0f ? FMath::RInterpTo(GetActorRotation(), Desired, DeltaSeconds, TurnSpeed) : Desired);
}

void AYSFoeCharacter::ThrowBomb(const APawn* BombTarget)
{
	if (!BombClass || !BombTarget)
	{
		return;
	}

	// vise le sol sous la cible au moment du lancer
	FYSBombThrow Throw;
	Throw.Start = GetActorLocation() + FVector(0.0f, 0.0f, ThrowHeight);
	Throw.Target = AYSInteractiveObject::FindGround(GetWorld(), BombTarget->GetActorLocation(), BombTarget);
	Throw.ApexHeight = BombApexHeight;
	Throw.FlightDuration = BombFlightDuration;
	Throw.FuseDuration = BombFuseDuration;
	Throw.BombColor = BombColor;
	Throw.FuseColor = FuseColor;
	Throw.FuseGlow = FuseGlow;
	Throw.BombMaterial = BombMaterial;
	Throw.Damage = BombDamage;
	Throw.ExplosionRadius = ExplosionRadius;
	Throw.ExplosionGrowDuration = ExplosionGrowDuration;
	Throw.ExplosionFadeDuration = ExplosionFadeDuration;
	Throw.ExplosionColor = ExplosionColor;
	Throw.LandingZoneColor = LandingZoneColor;
	Throw.LandingZoneFillOpacity = LandingZoneFillOpacity;
	Throw.LandingZoneEdgeOpacity = LandingZoneEdgeOpacity;
	Throw.LandingZoneMaterial = LandingZoneMaterial;
	Throw.ExplosionMaterial = ExplosionMaterial;

	const FTransform SpawnTransform(Throw.Start);
	AYSFoeBomb* Bomb = GetWorld()->SpawnActorDeferred<AYSFoeBomb>(BombClass, SpawnTransform, this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Bomb)
	{
		Bomb->SetThrow(Throw);
		Bomb->FinishSpawning(SpawnTransform);
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s lance une bombe sur %s (%.0f cm)"), *GetName(), *BombTarget->GetName(), FVector::Dist2D(Throw.Start, Throw.Target));
}

APawn* AYSFoeCharacter::FindNearestPlayer() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!GameState)
	{
		return nullptr;
	}

	APawn* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();

	for (const APlayerState* Player : GameState->PlayerArray)
	{
		APawn* Pawn = Player ? Player->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}

		// toujours le plus proche, mais un joueur qui porte un objet (coffre) compte comme N fois plus proche
		const AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(Pawn);
		const float Attraction = PlayerCharacter ? PlayerCharacter->GetFoeAttractionMultiplier() : 1.0f;
		const float DistSq = FVector::DistSquared2D(Pawn->GetActorLocation(), GetActorLocation()) / FMath::Square(FMath::Max(Attraction, KINDA_SMALL_NUMBER));
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Pawn;
		}
	}

	return Nearest;
}

void AYSFoeCharacter::ApplyContactDamage()
{
	if (ContactDamage <= 0.0f || Health <= 0.0f)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	TArray<UPrimitiveComponent*> Touching;
	Hitbox->GetOverlappingComponents(Touching);

	for (UPrimitiveComponent* Component : Touching)
	{
		// seule la hurtbox (le corps) d'un joueur compte, pas sa capsule de mouvement
		AYSPlayerCharacter* Player = Component ? Cast<AYSPlayerCharacter>(Component->GetOwner()) : nullptr;
		if (!Player || Component != Player->GetHurtbox() || !Player->IsAlive() || !Player->HasAnyMatchingGameplayTags(ContactHitTags))
		{
			continue;
		}

		double& NextTime = NextContactDamageTime.FindOrAdd(Player, 0.0);
		if (Now < NextTime)
		{
			continue;
		}

		NextTime = Now + ContactDamageInterval;
		// pas de DamageCauser : le chiffre de dégâts s'affiche sur le joueur
		Player->TakeDamage(ContactDamage, FDamageEvent(), GetController(), nullptr);
	}
}
