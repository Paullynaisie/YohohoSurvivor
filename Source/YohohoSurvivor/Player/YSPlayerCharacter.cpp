#include "Player/YSPlayerCharacter.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "UObject/ConstructorHelpers.h"
#include "Abilities/YSCoupDPetoirAbility.h"
#include "Abilities/YSAutoSwingAbility.h"
#include "Abilities/YSSummonCrewAbility.h"
#include "Summons/YSCaptainMinion.h"
#include "TimerManager.h"
#include "Components/ChildActorComponent.h"
#include "Weapons/YSWeapon.h"
#include "Weapons/YSWeaponSet.h"
#include "Upgrades/YSPassive.h"
#include "Weapons/YSProjectile.h"
#include "Player/YSFaceDecalComponent.h"
#include "Player/YSDanglingChainComponent.h"
#include "Components/WidgetComponent.h"
#include "UI/YSNameplateWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Core/YSCombat.h"
#include "Enemies/YSFoeCharacter.h"
#include "Engine/DamageEvents.h"
#include "Core/YSDamageTypes.h"
#include "Core/YSGameplayTags.h"
#include "Core/YSGameState.h"
#include "Core/YSPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerController.h"
#include "Upgrades/YSUpgrade.h"
#include "Abilities/YSAbilityComponent.h"
#include "EngineUtils.h"
#include "Interaction/YSInteractiveObject.h"
#include "Interaction/YSPlayerCorpse.h"
#include "NPC/YSBrookerNPC.h"
#include "Map/YSOcean.h"
#include "Engine/StaticMesh.h"
#include "Ship/YSPirateShip.h"
#include "Ship/YSShipHelm.h"

namespace
{
	/** En dessous de cette vitesse, le personnage est considéré à l'arrêt (pas d'inclinaison ni de sautillement) */
	constexpr float MovingSpeedThreshold = 50.0f;

	/** Au-delà de cet écart (téléportation, respawn), une main est replacée directement sur sa cible */
	constexpr float HandSnapDistance = 300.0f;
}

namespace YSPlayerHands
{
	/** Poses animées exprimées pour la main droite : symétrie pour la gauche (Y, Yaw et Roll inversés) */
	void MirrorHandPose(bool bLeft, FVector& Location, FQuat& Rotation)
	{
		if (bLeft)
		{
			Location.Y = -Location.Y;
			FRotator Mirrored = Rotation.Rotator();
			Mirrored.Yaw = -Mirrored.Yaw;
			Mirrored.Roll = -Mirrored.Roll;
			Rotation = Mirrored.Quaternion();
		}
	}
}

AYSPlayerCharacter::AYSPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	CorpseClass = AYSPlayerCorpse::StaticClass();

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	// le personnage suit le yaw du contrôleur (= direction du curseur), pas sa direction de déplacement
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->MaxWalkSpeed = MoveSpeed;
	Movement->bConstrainToPlane = true;
	Movement->bSnapToPlaneAtStart = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f));
	CameraBoom->TargetArmLength = 1680.0f;
	CameraBoom->bDoCollisionTest = false;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;

	// nuit : image plus sombre et bleutée, vignette (réglable dans 3C > DayNight)
	NightPostProcess.bOverride_ColorGain = true;
	NightPostProcess.ColorGain = FVector4(0.55f, 0.65f, 1.0f, 1.0f);
	NightPostProcess.bOverride_ColorSaturation = true;
	NightPostProcess.ColorSaturation = FVector4(0.7f, 0.7f, 0.7f, 1.0f);
	NightPostProcess.bOverride_AutoExposureBias = true;
	NightPostProcess.AutoExposureBias = -1.5f;
	NightPostProcess.bOverride_VignetteIntensity = true;
	NightPostProcess.VignetteIntensity = 0.9f;

	// DA « primitives » : pas de mesh squelettique, le corps est un cylindre (BodyMesh)
	GetMesh()->SetSkeletalMeshAsset(nullptr);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -96.0f), FRotator(0.0f, -90.0f, 0.0f));

	// pivot à l'origine de la capsule : les positions relatives des visuels restent inchangées,
	// l'inclinaison est calculée autour des pieds dans UpdateLean
	BodyPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BodyPivot"));
	BodyPivot->SetupAttachment(RootComponent);

	// cylindre de la taille de la capsule (le cylindre moteur fait 100 x 100 x 100)
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(BodyPivot);
	YSCombat::ConfigureHurtbox(BodyMesh);
	BodyMesh->SetRelativeScale3D(FVector(0.84f, 0.84f, 1.92f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultBody(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (DefaultBody.Succeeded())
	{
		BodyMesh->SetStaticMesh(DefaultBody.Object);
	}

	// visage : decal projeté sur l'avant du cylindre, vers l'arrière (axe X du decal = sens de projection)
	FaceDecal = CreateDefaultSubobject<UYSFaceDecalComponent>(TEXT("FaceDecal"));
	FaceDecal->SetupAttachment(BodyPivot);
	FaceDecal->SetRelativeLocationAndRotation(FVector(42.0f, 0.0f, 50.0f), FRotator(0.0f, 180.0f, 0.0f));
	FaceDecal->DecalSize = FVector(12.0f, 28.0f, 28.0f);

	// pistolet par défaut : un cube allongé sur le flanc gauche (remplaçable dans le Blueprint)
	// mains : un point d'attache par côté (servira aux armes) + une petite sphère visible
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultHand(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	auto CreateHand = [this](const TCHAR* SocketName, const TCHAR* MeshName, float Side, TObjectPtr<USceneComponent>& OutSocket, TObjectPtr<UStaticMeshComponent>& OutMesh)
	{
		OutSocket = CreateDefaultSubobject<USceneComponent>(SocketName);
		OutSocket->SetupAttachment(BodyPivot);
		// position monde calculée à chaque image par UpdateHands (retard + oscillation) ; la rotation suit le corps
		OutSocket->SetUsingAbsoluteLocation(true);

		OutMesh = CreateDefaultSubobject<UStaticMeshComponent>(MeshName);
		OutMesh->SetupAttachment(OutSocket);
		OutMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OutMesh->SetRelativeScale3D(FVector(0.25f));
		if (DefaultHand.Succeeded())
		{
			OutMesh->SetStaticMesh(DefaultHand.Object);
		}
	};
	CreateHand(TEXT("LeftHand"), TEXT("LeftHandMesh"), -1.0f, LeftHand, LeftHandMesh);
	CreateHand(TEXT("RightHand"), TEXT("RightHandMesh"), 1.0f, RightHand, RightHandMesh);

	// armes : acteurs enfants des mains (spawnés par le serveur et répliqués, visibles aussi dans l'éditeur)
	LeftHandWeapon = CreateDefaultSubobject<UChildActorComponent>(TEXT("LeftHandWeapon"));
	LeftHandWeapon->SetupAttachment(LeftHand);
	LeftHandWeapon->SetChildActorOwnerOnCreation(true);

	RightHandWeapon = CreateDefaultSubobject<UChildActorComponent>(TEXT("RightHandWeapon"));
	RightHandWeapon->SetupAttachment(RightHand);
	RightHandWeapon->SetChildActorOwnerOnCreation(true);

	Nameplate = CreateDefaultSubobject<UWidgetComponent>(TEXT("Nameplate"));
	Nameplate->SetupAttachment(RootComponent);
	Nameplate->SetRelativeLocation(FVector(0.0f, 0.0f, 165.0f));
	Nameplate->SetWidgetSpace(EWidgetSpace::Screen);
	Nameplate->SetDrawAtDesiredSize(true);
	Nameplate->SetWidgetClass(UYSNameplateWidget::StaticClass());
	Nameplate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Nameplate->SetVisibility(false);

	for (int32 Index = 0; Index < 2; ++Index)
	{
		DamageMultipliers[Index] = 1.0f;
		ShootSystemBonus[Index] = 0.0f;
		CooldownMultipliers[Index] = 1.0f;
	}
	for (float& Offset : RarityChanceOffsets)
	{
		Offset = 0.0f;
	}

	GameplayTags.AddTag(YSTags::Team_Player);
}

void AYSPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSPlayerCharacter, MaxHealth);
	DOREPLIFETIME(AYSPlayerCharacter, Health);
	DOREPLIFETIME(AYSPlayerCharacter, UpgradeProgress);
	DOREPLIFETIME(AYSPlayerCharacter, MoveSpeedMultiplier);
	DOREPLIFETIME(AYSPlayerCharacter, SummonSystemBonus);
	DOREPLIFETIME(AYSPlayerCharacter, MarkedFoe);
	DOREPLIFETIME(AYSPlayerCharacter, Rise);
	DOREPLIFETIME(AYSPlayerCharacter, Death);
	DOREPLIFETIME(AYSPlayerCharacter, MaxHealthMultiplier);
	DOREPLIFETIME(AYSPlayerCharacter, CooldownMultipliers);
	DOREPLIFETIME(AYSPlayerCharacter, Luck);
	DOREPLIFETIME(AYSPlayerCharacter, ShardLevels);
	DOREPLIFETIME(AYSPlayerCharacter, RarityChanceOffsets);
	DOREPLIFETIME(AYSPlayerCharacter, bLeftHanded);
	DOREPLIFETIME(AYSPlayerCharacter, WeaponSet);
	DOREPLIFETIME(AYSPlayerCharacter, Passive);
	DOREPLIFETIME(AYSPlayerCharacter, CarriedObject);
	DOREPLIFETIME(AYSPlayerCharacter, PilotedHelm);
	DOREPLIFETIME(AYSPlayerCharacter, BloodlustStacks);
	DOREPLIFETIME(AYSPlayerCharacter, BloodlustMaxStacks);
	DOREPLIFETIME(AYSPlayerCharacter, BloodlustDuration);
	DOREPLIFETIME(AYSPlayerCharacter, BloodlustAtkSpeedPerStack);
	DOREPLIFETIME(AYSPlayerCharacter, bGoldenBloodlust);
	DOREPLIFETIME(AYSPlayerCharacter, CriticalDamageBonus);
	DOREPLIFETIME(AYSPlayerCharacter, CriticalDamageAdd);
	DOREPLIFETIME(AYSPlayerCharacter, BloodlustExpireServerTime);
	DOREPLIFETIME(AYSPlayerCharacter, BloodRageStacks);
	DOREPLIFETIME(AYSPlayerCharacter, BloodRageMaxStacks);
	DOREPLIFETIME(AYSPlayerCharacter, BloodRageDuration);
	DOREPLIFETIME(AYSPlayerCharacter, bGoldenBloodRage);
	DOREPLIFETIME(AYSPlayerCharacter, BloodRageExpireServerTime);
	DOREPLIFETIME(AYSPlayerCharacter, RagdollPose);
}

UPrimitiveComponent* AYSPlayerCharacter::GetHurtbox() const
{
	return BodyMesh;
}

float AYSPlayerCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// invulnérable (roulade du Rolling shot…)
	if (IsInvulnerable())
	{
		UE_LOG(LogTemp, Verbose, TEXT("[YSPlayerCharacter] %s invulnérable : %.0f dégâts ignorés"), *GetName(), DamageAmount);
		return 0.0f;
	}

	float Damage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (!HasAuthority() || !IsAlive() || Damage <= 0.0f)
	{
		return 0.0f;
	}

	// %Armor (passif Tough Guy) : part du coup ignorée, AVANT l'armure (100 avec 20 % et 10 d'armure -> 80 - 10 = 70)
	const float IncomingDamage = Damage;
	const float ArmorPercent = GetArmorPercent();
	if (ArmorPercent > 0.0f)
	{
		Damage *= FMath::Max(0.0f, 1.0f - ArmorPercent);
	}

	// Armor : chaque coup est réduit de l'armure, mais en garde au moins Min Damage Ratio After Armor (10 %) et Min Damage After Armor (1) ;
	// BloodRage ++ : armure en plus par stack de rage
	const float TotalArmor = Armor + BloodRageStacks * BloodRageArmorPerStack;
	const bool bArmorApplied = TotalArmor > 0.0f;
	if (bArmorApplied)
	{
		Damage = FMath::Max3(Damage - TotalArmor, Damage * MinDamageRatioAfterArmor, FMath::Min(Damage, MinDamageAfterArmor));
	}

	// dégâts toujours entiers (arrondis à l'entier le plus proche, après l'armure) ; l'armure ne ramène jamais un coup à 0
	Damage = FMath::RoundToFloat(Damage);
	if (bArmorApplied && IncomingDamage > 0.0f)
	{
		Damage = FMath::Max(Damage, MinDamageAfterArmor);
	}

	UE_LOG(LogTemp, Verbose, TEXT("[YSPlayerCharacter] %s subit %.1f -> %.0f (%%Armor %.0f %%, armure %.1f)"), *GetName(), IncomingDamage, Damage, ArmorPercent * 100.0f, TotalArmor);

	// BloodRage : chaque coup reçu donne une stack de rage (après l'armure du coup)
	AddBloodRageStack();
	LastDamageTime = GetWorld()->GetTimeSeconds();
	HealthRegenTimer = 0.0f;
	if (Damage <= 0.0f)
	{
		return 0.0f;
	}

	Health = FMath::Max(0.0f, Health - Damage);

	// chiffre chez tous les joueurs : rouge pour le joueur touché, orange pour les autres (couleurs sur AYSDamageNumber)
	AYSGameState::ReportDamage(this, Damage, DamageCauser ? DamageCauser->GetActorLocation() : GetActorLocation(), EventInstigator, UYSCriticalDamageType::IsCritical(DamageEvent));

	if (!IsAlive())
	{
		PendingDeathImpulse = ComputeDeathImpulse(DamageEvent, EventInstigator, DamageCauser);
		Die();
		return Damage;
	}

	ApplyPushback(Damage, DamageEvent, EventInstigator, DamageCauser);
	return Damage;
}

FVector AYSPlayerCharacter::ComputeDeathImpulse(const FDamageEvent& DamageEvent, const AController* EventInstigator, const AActor* DamageCauser) const
{
	// force MAXIMALE du pushback du NME tueur (comme la mort des NME), dans la direction du coup
	const AYSFoeCharacter* Attacker = EventInstigator ? Cast<AYSFoeCharacter>(EventInstigator->GetPawn()) : nullptr;
	if (!Attacker && DamageCauser)
	{
		Attacker = Cast<AYSFoeCharacter>(DamageCauser->GetInstigator());
	}
	const FVector Direction = DamageEvent.IsOfType(FPointDamageEvent::ClassID) ? static_cast<const FPointDamageEvent&>(DamageEvent).ShotDirection.GetSafeNormal2D() : FVector::ZeroVector;
	float Force = 0.0f;
	float Up = DeathExtraUpImpulse;
	if (Attacker && !Direction.IsNearlyZero())
	{
		Force = Attacker->ComputePushbackForce(GetMaxHealth(), GetMaxHealth());
		Up += Attacker->GetPushbackVerticalForce();
	}
	return (Direction * Force + FVector(0.0f, 0.0f, Up)) * DeathImpulseMultiplier;
}

void AYSPlayerCharacter::ApplyPushback(float Damage, const FDamageEvent& DamageEvent, const AController* EventInstigator, const AActor* DamageCauser)
{
	// NME qui frappe : celui du contrôleur (coup d'épée) ou l'instigateur du projectile
	const AYSFoeCharacter* Attacker = EventInstigator ? Cast<AYSFoeCharacter>(EventInstigator->GetPawn()) : nullptr;
	if (!Attacker && DamageCauser)
	{
		Attacker = Cast<AYSFoeCharacter>(DamageCauser->GetInstigator());
	}
	if (!Attacker || !DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		return;
	}
	// encore en train de sortir du sol (minion) : sorti tout de suite, puis poussé
	FinishRiseNow();
	const FVector Direction = static_cast<const FPointDamageEvent&>(DamageEvent).ShotDirection.GetSafeNormal2D();
	const float Force = Attacker->ComputePushbackForce(Damage, GetMaxHealth());
	UE_LOG(LogTemp, Verbose, TEXT("[YSPushback] %s frappé par %s : %.0f dégâts sur %.0f PV max (%.1f %%), pushback %.0f vers (%.2f, %.2f)"), *GetName(), *Attacker->GetName(),
		Damage, GetMaxHealth(), 100.0f * Damage / FMath::Max(GetMaxHealth(), 1.0f), Force, Direction.X, Direction.Y);
	if (Direction.IsNearlyZero() || Force <= 0.0f)
	{
		return;
	}

	// serveur + client qui contrôle le personnage (prédiction du mouvement) ; un peu de vertical : décolle du sol (moins de friction)
	const FVector LaunchVelocity = Direction * Force + FVector(0.0f, 0.0f, Attacker->GetPushbackVerticalForce());
	LaunchCharacter(LaunchVelocity, true, true);
	if (!IsLocallyControlled())
	{
		ClientLaunch(LaunchVelocity);
	}
}

void AYSPlayerCharacter::ClientLaunch_Implementation(FVector_NetQuantize10 LaunchVelocity)
{
	LaunchCharacter(LaunchVelocity, true, true);
}

void AYSPlayerCharacter::Die()
{
	if (!HasAuthority() || IsDying() || IsActorBeingDestroyed())
	{
		return;
	}
	Health = 0.0f;

	// l'objet porté tombe au sol, la barre est lâchée
	DropCarriedObject(TEXT("mort"));
	StopPiloting(TEXT("mort"));

	// le joueur reste mort (pas de réapparition) mais compte toujours pour la taille des vagues
	if (AYSPlayerState* YSPlayerState = GetPlayerState<AYSPlayerState>())
	{
		YSPlayerState->SetDead();
	}

	// ragdoll : le corps vole puis rétrécit (UpdateDeath -> FinishDeath) ; deadzone : tout de suite
	if (!bDeathRagdoll || bSkipDeathAnim)
	{
		FinishDeath();
		return;
	}
	Death.bDead = true;
	// mort sans coup (minion qui meurt avec son Captain) : juste la poussée vers le haut
	Death.Impulse = PendingDeathImpulse.IsNearlyZero() ? FVector(0.0f, 0.0f, DeathExtraUpImpulse * DeathImpulseMultiplier) : PendingDeathImpulse;
	// cadavre gardé : le corps reste visible de tous les clients, même loin d'eux
	if (bKeepCorpse)
	{
		bAlwaysRelevant = true;
	}
	ForceNetUpdate();
	BeginDeathLocal();
}

void AYSPlayerCharacter::FinishDeath(bool bKeepActor)
{
	if (!HasAuthority() || IsActorBeingDestroyed())
	{
		return;
	}
	bCorpseHandled = true;
	// caméra de secours là où il est mort (s'il n'y a plus aucun joueur vivant à regarder)
	AYSPlayerController* PlayerController = Cast<AYSPlayerController>(GetController());
	if (PlayerController)
	{
		PlayerController->ClientPlayerDied(TopDownCamera->GetComponentLocation(), TopDownCamera->GetComponentRotation(), TopDownCamera->FieldOfView);
	}

	// cadavre : le personnage reste (son corps, ses armes) mais le joueur le quitte et passe en spectateur
	if (bKeepActor)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[YSPlayerCharacter] %s : reste en cadavre"), *GetName());
		if (PlayerController)
		{
			PlayerController->UnPossess();
		}
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[YSPlayerCharacter] %s : corps disparu, détruit"), *GetName());
	// les armes (acteurs enfants) disparaissent avec le personnage
	Destroy();
}

bool AYSPlayerCharacter::SpawnCorpse()
{
	UWorld* World = GetWorld();
	if (!World || !CorpseClass)
	{
		return false;
	}
	// au sol sous le corps (le corps ne bloque plus les traces), jamais sous la deadzone ; dans la mer : il flotte à la surface
	const FVector BodyLocation = BodyMesh->GetComponentLocation();
	FVector Ground = AYSInteractiveObject::FindGround(World, BodyLocation, this);
	const bool bHasGround = !Ground.Equals(BodyLocation);
	const AYSOcean* Ocean = AYSOcean::Find(World);
	if (bCorpseFloats && Ocean && (!bHasGround || AYSOcean::IsUnderwater(World, Ground)))
	{
		Ground = FVector(BodyLocation.X, BodyLocation.Y, Ocean->GetSeaLevel());
		UE_LOG(LogTemp, Log, TEXT("[YSCorpse] %s : corps dans la mer, cadavre à la surface"), *GetName());
	}
	else if (!bHasGround || !AYSInteractiveObject::IsSafeDropLocation(World, Ground))
	{
		UE_LOG(LogTemp, Log, TEXT("[YSCorpse] %s : pas de cadavre (corps dans l'eau ou sans sol)"), *GetName());
		return false;
	}

	const FTransform CorpseTransform(FRotator::ZeroRotator, Ground);
	AYSPlayerCorpse* Corpse = World->SpawnActorDeferred<AYSPlayerCorpse>(CorpseClass, CorpseTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Corpse)
	{
		return false;
	}
	// pose actuelle du corps par rapport au cadavre (identique sur toutes les machines), vue du spectateur = caméra du joueur au moment de sa mort
	const FTransform BodyRelative = BodyMesh->GetComponentTransform().GetRelativeTransform(CorpseTransform);
	Corpse->InitCorpse(this, BodyRelative.GetLocation(), BodyRelative.Rotator(), GetPlayerState(), TopDownCamera->GetComponentLocation() - GetActorLocation());
	Corpse->FinishSpawning(CorpseTransform);
	Corpse->AttachToShipBelow();
	UE_LOG(LogTemp, Log, TEXT("[YSCorpse] %s : cadavre %s posé à (%.0f, %.0f, %.0f)"), *GetName(), *Corpse->GetName(), Ground.X, Ground.Y, Ground.Z);
	return true;
}

void AYSPlayerCharacter::AttachBodyTo(USceneComponent* Parent, const FVector& RelativeLocation, const FRotator& RelativeRotation)
{
	if (!Parent)
	{
		return;
	}
	// corps répliqué avant sa mort sur cette machine : ragdoll mis en place d'abord (visuel qui suit le corps)
	BeginDeathLocal();
	bBodyOnCorpse = true;
	BodyMesh->SetSimulatePhysics(false);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (BodyMesh->GetAttachParent() != Parent)
	{
		BodyMesh->AttachToComponent(Parent, FAttachmentTransformRules::KeepWorldTransform);
	}
	BodyMesh->SetRelativeLocationAndRotation(RelativeLocation, RelativeRotation);
	BodyMesh->SetVisibility(true, true);
	HideCorpseWeapons();
	UE_LOG(LogTemp, Log, TEXT("[YSCorpse] %s (%s) : corps sur son cadavre en %s"), *GetName(), HasAuthority() ? TEXT("serveur") : TEXT("client"),
		*BodyMesh->GetComponentLocation().ToCompactString());
}

void AYSPlayerCharacter::HideCorpseWeapons()
{
	if (!bKeepCorpse)
	{
		return;
	}
	for (AYSWeapon* Weapon : GetHeldWeapons())
	{
		if (!Weapon->IsHidden())
		{
			Weapon->SetActorHiddenInGame(true);
		}
	}
}

void AYSPlayerCharacter::ApplyCorpseBuoyancy(float DeltaSeconds)
{
	const AYSOcean* Ocean = bCorpseFloats ? AYSOcean::Find(GetWorld()) : nullptr;
	if (!Ocean || !BodyMesh->IsSimulatingPhysics())
	{
		return;
	}
	// sous la surface : la gravité est compensée, plus une poussée qui grandit avec l'enfoncement (équilibre à Corpse Float Depth), et freinage
	const float Depth = Ocean->GetSeaLevel() - BodyMesh->GetComponentLocation().Z;
	const bool bInWater = Depth > 0.0f;
	const float Damping = bInWater ? CorpseWaterDamping : 0.01f;
	if (!FMath::IsNearlyEqual(BodyMesh->GetLinearDamping(), Damping))
	{
		BodyMesh->SetLinearDamping(Damping);
		BodyMesh->SetAngularDamping(bInWater ? CorpseWaterDamping : 0.0f);
		UE_LOG(LogTemp, Verbose, TEXT("[YSCorpse] %s : corps %s"), *GetName(), bInWater ? TEXT("dans la mer, flotte") : TEXT("hors de l'eau"));
	}
	if (bInWater)
	{
		const float Lift = FMath::Max(0.0f, -GetWorld()->GetGravityZ() + CorpseBuoyancy * (Depth - CorpseFloatDepth));
		BodyMesh->AddForce(FVector(0.0f, 0.0f, Lift), NAME_None, true);
	}
}

void AYSPlayerCharacter::FollowRagdollPose(float DeltaSeconds)
{
	if (!RagdollPose.bValid)
	{
		return;
	}
	// rattrape la pose du serveur (lissage entre deux mises à jour réseau) ; trop loin (première pose reçue) : placé directement
	const FVector Target = RagdollPose.Location;
	const FVector Current = BodyMesh->GetComponentLocation();
	const bool bSnap = FVector::DistSquared(Current, Target) > FMath::Square(500.0f);
	const FVector NewLocation = bSnap ? Target : FMath::VInterpTo(Current, Target, DeltaSeconds, RagdollFollowSpeed);
	const FRotator NewRotation = bSnap ? RagdollPose.Rotation : FMath::RInterpTo(BodyMesh->GetComponentRotation(), RagdollPose.Rotation, DeltaSeconds, RagdollFollowSpeed);
	BodyMesh->SetWorldLocationAndRotation(NewLocation, NewRotation);
}

void AYSPlayerCharacter::OnRep_Death()
{
	if (Death.bDead)
	{
		BeginDeathLocal();
	}
}

void AYSPlayerCharacter::BeginDeathLocal()
{
	if (bLocalDeathStarted)
	{
		return;
	}
	bLocalDeathStarted = true;
	bLocalRiseActive = false;
	DeathStartTime = GetWorld()->GetTimeSeconds();

	// plus de gameplay : sans tag il n'est plus ciblé ni touché, plus de mouvement (la caméra reste où il est tombé)
	GameplayTags.Reset();
	// le personnage peut rester (cadavre) : son invite « E » et l'affichage de sa marque disparaissent maintenant
	if (FocusedInteractable.IsValid())
	{
		FocusedInteractable->SetPromptVisible(false);
	}
	FocusedInteractable.Reset();
	if (DisplayedMarkedFoe.IsValid())
	{
		DisplayedMarkedFoe->SetMarked(false);
	}
	DisplayedMarkedFoe.Reset();
	if (HasAuthority())
	{
		SetMarkedFoe(nullptr);
	}
	Nameplate->SetVisibility(false);
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->DisableMovement();
	Movement->SetComponentTickEnabled(false);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (HasAuthority())
	{
		SetReplicateMovement(false);
	}

	// armes : actions arrêtées (pas de tir ni de coup après la mort) ; un joueur mort (cadavre) ne les montre plus
	for (AYSWeapon* Weapon : GetHeldWeapons())
	{
		for (UActorComponent* Component : Weapon->GetComponents())
		{
			GetWorldTimerManager().ClearAllTimersForObject(Component);
			Component->SetComponentTickEnabled(false);
		}
	}
	HideCorpseWeapons();

	// tout le visuel suit le corps (tête, visage, mains et armes), qui part en physique : ne bloque que le décor
	TArray<USceneComponent*> Followers;
	for (USceneComponent* Child : BodyPivot->GetAttachChildren())
	{
		if (Child && Child != BodyMesh)
		{
			Followers.Add(Child);
		}
	}
	Followers.AddUnique(LeftHand);
	Followers.AddUnique(RightHand);
	Followers.AddUnique(FaceDecal);
	for (USceneComponent* Component : Followers)
	{
		Component->SetUsingAbsoluteLocation(false);
		Component->SetUsingAbsoluteRotation(false);
		Component->AttachToComponent(BodyMesh, FAttachmentTransformRules::KeepWorldTransform);
	}
	BodyMesh->SetGenerateOverlapEvents(false);
	DeathBodyScale = BodyMesh->GetComponentScale();

	// joueur : seul le serveur simule le ragdoll, les clients suivent sa pose répliquée (même corps, même cadavre partout)
	if (UsesReplicatedRagdoll())
	{
		BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		UE_LOG(LogTemp, Verbose, TEXT("[YSPlayerCharacter] %s (client) meurt : ragdoll du serveur suivi"), *GetName());
		return;
	}

	BodyMesh->SetCollisionObjectType(ECC_PhysicsBody);
	BodyMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	BodyMesh->SetSimulatePhysics(true);
	// cadavre plus lourd : masse x Corpse Mass Multiplier, la même poussée le lance d'autant moins vite
	const float MassMultiplier = bKeepCorpse ? FMath::Max(CorpseMassMultiplier, 0.1f) : 1.0f;
	BodyMesh->SetMassScale(NAME_None, MassMultiplier);
	BodyMesh->SetPhysicsLinearVelocity(Death.Impulse / MassMultiplier);
	BodyMesh->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * DeathAngularImpulse);
	if (HasAuthority() && bKeepCorpse && bReplicateRagdoll)
	{
		RagdollPose.bValid = true;
		RagdollPose.Location = BodyMesh->GetComponentLocation();
		RagdollPose.Rotation = BodyMesh->GetComponentRotation();
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSPlayerCharacter] %s (%s) meurt : ragdoll à (%.0f, %.0f, %.0f) cm/s"), *GetName(), HasAuthority() ? TEXT("serveur") : TEXT("client"),
		Death.Impulse.X, Death.Impulse.Y, Death.Impulse.Z);
}

void AYSPlayerCharacter::UpdateDeath(float DeltaSeconds)
{
	// après Death Scale Delay, le corps (et tout ce qui le suit) rétrécit jusqu'à 0, puis le personnage disparaît
	const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - DeathStartTime);

	// joueur : le corps immobile devient un cadavre portable (serveur ; les clients le reçoivent avec le cadavre) ;
	// pas de cadavre possible (pas de sol, deadzone) : le personnage disparaît
	if (bKeepCorpse)
	{
		// armes répliquées arrivées après la mort chez un client : cachées elles aussi
		HideCorpseWeapons();
		if (bBodyOnCorpse)
		{
			return;
		}
		if (HasAuthority())
		{
			// ragdoll du serveur : flotte sur la mer, sa pose part aux clients
			ApplyCorpseBuoyancy(DeltaSeconds);
			if (bReplicateRagdoll)
			{
				RagdollPose.bValid = true;
				RagdollPose.Location = BodyMesh->GetComponentLocation();
				RagdollPose.Rotation = BodyMesh->GetComponentRotation();
			}
			if (!bCorpseHandled && Elapsed >= DeathScaleDelay)
			{
				FinishDeath(SpawnCorpse());
			}
		}
		else if (UsesReplicatedRagdoll())
		{
			FollowRagdollPose(DeltaSeconds);
		}
		return;
	}
	const float Alpha = FMath::Clamp((Elapsed - DeathScaleDelay) / FMath::Max(DeathScaleDuration, 0.01f), 0.0f, 1.0f);
	if (Alpha > 0.0f)
	{
		BodyMesh->SetWorldScale3D(DeathBodyScale * FMath::Max(1.0f - Alpha, 0.01f));
	}
	if (Alpha >= 1.0f)
	{
		BodyMesh->SetVisibility(false, true);
		FinishDeath();
	}
}

void AYSPlayerCharacter::StartRise()
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	Rise.bActive = true;
	Rise.Ground = GetActorLocation();
	Rise.Depth = GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0f + SpawnRiseExtraDepth * GetActorScale3D().Z;
	Rise.Duration = SpawnRiseDuration;
	Rise.StartServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	BeginRiseLocal();
	ForceNetUpdate();
}

void AYSPlayerCharacter::OnRep_Rise()
{
	if (Rise.bActive && !bLocalRiseActive && !bLocalDeathStarted)
	{
		BeginRiseLocal();
	}
	else if (!Rise.bActive && bLocalRiseActive)
	{
		EndRiseLocal();
	}
}

void AYSPlayerCharacter::BeginRiseLocal()
{
	bLocalRiseActive = true;

	// comme les NME : position calculée par UpdateRise sur chaque machine, mouvement du personnage et sa réplication coupés
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->SetComponentTickEnabled(false);
	if (HasAuthority())
	{
		SetReplicateMovement(false);
	}
	UpdateRise();
}

void AYSPlayerCharacter::UpdateRise()
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const float Alpha = FMath::Clamp(static_cast<float>(Now - Rise.StartServerTime) / FMath::Max(Rise.Duration, 0.05f), 0.0f, 1.0f);
	if (Alpha >= 1.0f)
	{
		EndRiseLocal();
		return;
	}
	const float Eased = 1.0f - FMath::Pow(1.0f - Alpha, SpawnRiseEaseExponent);
	SetActorLocation(FVector(Rise.Ground) - FVector(0.0f, 0.0f, Rise.Depth * (1.0f - Eased)));
}

void AYSPlayerCharacter::EndRiseLocal()
{
	if (!bLocalRiseActive)
	{
		return;
	}
	bLocalRiseActive = false;

	SetActorLocation(Rise.Ground);
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->SetComponentTickEnabled(true);
	if (HasAuthority())
	{
		Movement->Velocity = FVector::ZeroVector;
		Movement->SetMovementMode(MOVE_Falling);
		Rise.bActive = false;
		SetReplicateMovement(true);
		ForceNetUpdate();
	}
}

void AYSPlayerCharacter::FinishRiseNow()
{
	if (!HasAuthority() || !bLocalRiseActive)
	{
		return;
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSPlayerCharacter] %s : sortie du sol terminée tout de suite (coup ou Ability forcée)"), *GetName());
	EndRiseLocal();
}

void AYSPlayerCharacter::FellOutOfWorld(const UDamageType& DamageType)
{
	// le serveur décide de la mort (le personnage répliqué ne doit pas être détruit par un client) ; pas de ragdoll sous la map
	if (!HasAuthority() || IsActorBeingDestroyed() || IsDying())
	{
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSDeadZone] %s tombé sous la map (Z %.0f) : mort"), *GetName(), GetActorLocation().Z);
	Health = 0.0f;
	bSkipDeathAnim = true;
	Die();
}

AYSWeapon* AYSPlayerCharacter::GetLeftHandWeapon() const
{
	return Cast<AYSWeapon>(LeftHandWeapon->GetChildActor());
}

AYSWeapon* AYSPlayerCharacter::GetRightHandWeapon() const
{
	return Cast<AYSWeapon>(RightHandWeapon->GetChildActor());
}

FYSUpgradeProgress AYSPlayerCharacter::GetUpgradeProgress(int32 PoolIndex, int32 Variant) const
{
	const FYSUpgradeProgress* Found = UpgradeProgress.FindByPredicate([PoolIndex, Variant](const FYSUpgradeProgress& Progress)
	{
		return Progress.PoolIndex == PoolIndex && Progress.Variant == Variant;
	});
	if (Found)
	{
		return *Found;
	}

	FYSUpgradeProgress Empty;
	Empty.PoolIndex = PoolIndex;
	Empty.Variant = Variant;
	return Empty;
}

int32 AYSPlayerCharacter::GetTotalUpgradeStacks() const
{
	int32 Total = 0;
	for (const FYSUpgradeProgress& Progress : UpgradeProgress)
	{
		Total += Progress.Stacks;
	}
	for (const FYSShardLevel& Entry : ShardLevels)
	{
		Total += Entry.Level;
	}
	return Total;
}

int32 AYSPlayerCharacter::GetShardLevel(const UYSUpShard* Shard) const
{
	const FYSShardLevel* Found = Shard ? ShardLevels.FindByPredicate([Shard](const FYSShardLevel& Entry) { return Entry.Shard == Shard; }) : nullptr;
	return Found ? Found->Level : 0;
}

void AYSPlayerCharacter::AddShardLevels(UYSUpShard* Shard, int32 Levels)
{
	if (!HasAuthority() || !Shard || Shard->IsSpecial() || Levels <= 0)
	{
		return;
	}
	FYSShardLevel* Entry = ShardLevels.FindByPredicate([Shard](const FYSShardLevel& Other) { return Other.Shard == Shard; });
	if (!Entry)
	{
		Entry = &ShardLevels.AddDefaulted_GetRef();
		Entry->Shard = Shard;
	}
	Entry->Level = FMath::Min(Shard->MaxLevel, Entry->Level + Levels);
	// la partie est en pause pendant les choix : sans ça, les niveaux ne sont pas renvoyés aux clients
	ForceNetUpdate();
	Shard->Apply(this, Entry->Level);
}

void AYSPlayerCharacter::RestoreShards(const TArray<FYSShardLevel>& OldLevels)
{
	if (!HasAuthority())
	{
		return;
	}

	// shard qui ne concerne plus le nouveau set (ex. Atk speed sans épée) : ses niveaux passent à sa jumelle (Shoot system)
	ShardLevels.Reset();
	for (const FYSShardLevel& Old : OldLevels)
	{
		UYSUpShard* Shard = Old.Shard;
		UYSUpShard* Twin = Shard ? Shard->TwinShard.Get() : nullptr;
		if (Shard && Twin && !Shard->IsApplicable(this) && Twin->IsApplicable(this))
		{
			UE_LOG(LogTemp, Log, TEXT("[YSShard] %s : %s converti en %s (%d niveau(x))"), *GetName(), *Shard->GetName(), *Twin->GetName(), Old.Level);
			Shard = Twin;
		}
		if (!Shard)
		{
			continue;
		}
		FYSShardLevel* Entry = ShardLevels.FindByPredicate([Shard](const FYSShardLevel& Other) { return Other.Shard == Shard; });
		if (!Entry)
		{
			Entry = &ShardLevels.AddDefaulted_GetRef();
			Entry->Shard = Shard;
		}
		Entry->Level = FMath::Min(Shard->MaxLevel, Entry->Level + Old.Level);
	}
	for (const FYSShardLevel& Entry : ShardLevels)
	{
		Entry.Shard->Apply(this, Entry.Level);
	}
	ForceNetUpdate();
}

void AYSPlayerCharacter::SetRarityChanceOffsets(float Bronze, float Silver, float Gold, float Mythic)
{
	RarityChanceOffsets[0] = Bronze;
	RarityChanceOffsets[1] = Silver;
	RarityChanceOffsets[2] = Gold;
	RarityChanceOffsets[3] = Mythic;
	ForceNetUpdate();
}

TArray<FYSUpgradeOffer> AYSPlayerCharacter::PickUpgradeOffers(int32 Count) const
{
	// chaque variante encore proposable de chaque upgrade du pool
	TArray<FYSUpgradeOffer> Candidates;
	for (int32 PoolIndex = 0; PoolIndex < UpgradePool.Num(); ++PoolIndex)
	{
		const UYSUpgrade* Upgrade = UpgradePool[PoolIndex];
		for (int32 Variant = 0; Upgrade && Variant < Upgrade->GetVariantCount(); ++Variant)
		{
			// les upgrades de classe ne sont jamais tirées au hasard (PickClassOffers)
			if (Upgrade->GetUpgradeType() != EYSUpgradeType::Class && Upgrade->GetOfferWeight(Variant) > 0.0f && Upgrade->CanBeOffered(this, Variant, GetUpgradeProgress(PoolIndex, Variant)))
			{
				FYSUpgradeOffer& Candidate = Candidates.AddDefaulted_GetRef();
				Candidate.PoolIndex = PoolIndex;
				Candidate.Variant = Variant;
			}
		}
	}

	// pour chaque proposition : d'abord la rareté (chances du joueur, avec sa Luck), puis l'upgrade parmi celles
	// qui existent dans cette rareté, au poids ; sans remise (que des propositions différentes).
	// Rareté sans aucune upgrade possible : retirée du tirage, chances des autres renormalisées.
	static const EYSUpgradeRarity Rarities[] = { EYSUpgradeRarity::Bronze, EYSUpgradeRarity::Silver, EYSUpgradeRarity::Gold };
	TArray<FYSUpgradeOffer> Offers;
	while (Offers.Num() < Count && Candidates.Num() > 0)
	{
		float RarityTotal = 0.0f;
		float RarityChances[3] = { 0.0f, 0.0f, 0.0f };
		bool bAnyAvailable = false;
		bool bAvailable[3] = { false, false, false };
		for (int32 RarityIndex = 0; RarityIndex < 3; ++RarityIndex)
		{
			bAvailable[RarityIndex] = Candidates.ContainsByPredicate([this, RarityIndex](const FYSUpgradeOffer& Candidate)
			{
				return UpgradePool[Candidate.PoolIndex]->CanOfferRarity(this, Candidate.Variant, Rarities[RarityIndex]);
			});
			bAnyAvailable |= bAvailable[RarityIndex];
			RarityChances[RarityIndex] = bAvailable[RarityIndex] ? GetRarityChance(Rarities[RarityIndex]) : 0.0f;
			RarityTotal += RarityChances[RarityIndex];
		}
		if (!bAnyAvailable)
		{
			break;
		}

		int32 RarityPicked = INDEX_NONE;
		if (RarityTotal > 0.0f)
		{
			float RarityRoll = FMath::FRand() * RarityTotal;
			for (int32 RarityIndex = 0; RarityIndex < 3; ++RarityIndex)
			{
				if (RarityChances[RarityIndex] > 0.0f)
				{
					RarityPicked = RarityIndex;
					RarityRoll -= RarityChances[RarityIndex];
					if (RarityRoll < 0.0f)
					{
						break;
					}
				}
			}
		}
		else
		{
			// toutes les raretés possibles à 0 % : équiprobables
			TArray<int32> AvailableRarities;
			for (int32 RarityIndex = 0; RarityIndex < 3; ++RarityIndex)
			{
				if (bAvailable[RarityIndex])
				{
					AvailableRarities.Add(RarityIndex);
				}
			}
			RarityPicked = AvailableRarities[FMath::RandRange(0, AvailableRarities.Num() - 1)];
		}
		const EYSUpgradeRarity Rarity = Rarities[RarityPicked];

		float TotalWeight = 0.0f;
		for (const FYSUpgradeOffer& Candidate : Candidates)
		{
			const UYSUpgrade* Upgrade = UpgradePool[Candidate.PoolIndex];
			TotalWeight += Upgrade->CanOfferRarity(this, Candidate.Variant, Rarity) ? Upgrade->GetOfferWeight(Candidate.Variant) : 0.0f;
		}

		int32 Picked = INDEX_NONE;
		float Roll = FMath::FRand() * TotalWeight;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const UYSUpgrade* Upgrade = UpgradePool[Candidates[Index].PoolIndex];
			if (Upgrade->CanOfferRarity(this, Candidates[Index].Variant, Rarity))
			{
				Picked = Index;
				Roll -= Upgrade->GetOfferWeight(Candidates[Index].Variant);
				if (Roll < 0.0f)
				{
					break;
				}
			}
		}

		FYSUpgradeOffer& Offer = Offers.Add_GetRef(Candidates[Picked]);
		Offer.Rarity = Rarity;
		Candidates.RemoveAt(Picked);
	}
	return Offers;
}

float AYSPlayerCharacter::GetBaseRarityChance(EYSUpgradeRarity Rarity) const
{
	switch (Rarity)
	{
	case EYSUpgradeRarity::Bronze:	return BronzeChance;
	case EYSUpgradeRarity::Silver:	return SilverChance;
	case EYSUpgradeRarity::Gold:	return GoldChance;
	case EYSUpgradeRarity::Mythic:	return MythicChance;
	default:						return 0.0f;
	}
}

float AYSPlayerCharacter::GetRarityChance(EYSUpgradeRarity Rarity) const
{
	// ancienne upgrade Luck (points déplacés du Bronze vers le Gold), puis shard Luck (points ajoutés / retirés à chaque rareté)
	const float Bronze = FMath::Max(0.0f, BronzeChance - Luck);
	switch (Rarity)
	{
	case EYSUpgradeRarity::Bronze:	return FMath::Max(0.0f, Bronze + RarityChanceOffsets[0]);
	case EYSUpgradeRarity::Silver:	return FMath::Max(0.0f, SilverChance + RarityChanceOffsets[1]);
	case EYSUpgradeRarity::Gold:	return FMath::Max(0.0f, GoldChance + (BronzeChance - Bronze) + RarityChanceOffsets[2]);
	case EYSUpgradeRarity::Mythic:	return FMath::Max(0.0f, MythicChance + RarityChanceOffsets[3]);
	default:						return 0.0f;
	}
}

void AYSPlayerCharacter::ApplyUpgrade(const FYSUpgradeOffer& Offer)
{
	// système de shards : +1 niveau par shard donnée (déjà résolues au tirage)
	if (Offer.UpStats)
	{
		if (HasAuthority())
		{
			for (const TObjectPtr<UYSUpShard>& Shard : Offer.Shards)
			{
				AddShardLevels(Shard, 1);
			}
			UE_LOG(LogTemp, Log, TEXT("[YSShard] %s : %s (rareté %d, %d shard(s))"), *GetName(), *Offer.UpStats->GetName(), (int32)Offer.Rarity, Offer.Shards.Num());
		}
		return;
	}

	const UYSUpgrade* Upgrade = UpgradePool.IsValidIndex(Offer.PoolIndex) ? UpgradePool[Offer.PoolIndex].Get() : nullptr;
	if (!HasAuthority() || !Upgrade)
	{
		return;
	}

	FYSUpgradeProgress* Progress = UpgradeProgress.FindByPredicate([&Offer](const FYSUpgradeProgress& Entry)
	{
		return Entry.PoolIndex == Offer.PoolIndex && Entry.Variant == Offer.Variant;
	});
	if (!Progress)
	{
		Progress = &UpgradeProgress.AddDefaulted_GetRef();
		Progress->PoolIndex = Offer.PoolIndex;
		Progress->Variant = Offer.Variant;
	}

	++Progress->Stacks;
	const float BonusValue = Upgrade->GetBonusValue(Offer.Variant, Offer.Rarity);
	Progress->TotalValue += BonusValue;
	Progress->BonusValues.Add(BonusValue);
	Progress->BonusRarities.Add(Offer.Rarity);
	// la partie est en pause pendant les choix : sans ça, la progression n'est pas renvoyée aux clients
	ForceNetUpdate();
	Upgrade->Apply(this, Offer.Variant, *Progress);

	// upgrade d'arme du système de shards : shards en plus (Gold / Mythique)
	for (const TObjectPtr<UYSUpShard>& Shard : Offer.Shards)
	{
		AddShardLevels(Shard, 1);
	}
}

float AYSPlayerCharacter::GetWeaponDamageMultiplier(const AYSWeapon* Weapon, EYSActionSlot Slot) const
{
	// un bonus par type d'action pour tout le set d'arme
	return Weapon && GetHeldWeapons().Contains(Weapon) ? DamageMultipliers[GetSlotIndex(Slot)] : 1.0f;
}

float AYSPlayerCharacter::GetWeaponShootSystemBonus(const AYSWeapon* Weapon, EYSActionSlot Slot) const
{
	return Weapon && GetHeldWeapons().Contains(Weapon) ? ShootSystemBonus[GetSlotIndex(Slot)] : 0.0f;
}

TArray<AYSWeapon*> AYSPlayerCharacter::GetHeldWeapons() const
{
	TArray<AYSWeapon*> Weapons;
	for (AYSWeapon* Weapon : { GetRightHandWeapon(), GetLeftHandWeapon() })
	{
		if (Weapon)
		{
			Weapons.Add(Weapon);
		}
	}
	return Weapons;
}

void AYSPlayerCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyCharacterScale();

	// changer le set (ou l'arme d'une main sans set) dans le Blueprint met à jour son visuel immédiatement
	// gaucher : miroir du personnage (réglé en droitier) = armes des deux mains échangées
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

	UpdateWeaponColoration();

	// mains à leur position cible (sans retard) : visible dans l'éditeur et point de départ en jeu
	bHandsInitialized = false;
	UpdateHands(0.0f);
}

void AYSPlayerCharacter::ApplyCharacterScale()
{
	// taille du personnage : échelle ABSOLUE (classe x CharacterScale x set), identique sur toutes les machines et dans l'éditeur
	const USceneComponent* DefaultRoot = GetClass()->GetDefaultObject<AYSPlayerCharacter>()->GetRootComponent();
	const float SetMultiplier = WeaponSet ? WeaponSet->CharacterScaleMultiplier : 1.0f;
	const FVector ScaledSize = (DefaultRoot ? DefaultRoot->GetRelativeScale3D() : FVector::OneVector) * CharacterScale * SetMultiplier;
	if (!GetActorScale3D().Equals(ScaledSize))
	{
		SetActorScale3D(ScaledSize);
	}
}

float AYSPlayerCharacter::GetCharacterSize() const
{
	return CharacterScale * (WeaponSet ? WeaponSet->CharacterScaleMultiplier : 1.0f);
}

void AYSPlayerCharacter::OnRep_WeaponSet()
{
	ApplyCharacterScale();
}

void AYSPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// appliqué sur le serveur ET les clients pour que la prédiction de mouvement reste cohérente
	UpdateMaxWalkSpeed();

	// caméra à pied (réglée dans le Blueprint) : retrouvée en quittant la barre d'un bateau
	DefaultCameraArmLength = CameraBoom->TargetArmLength;

	// réappliqué en jeu : la hurtbox ne dépend pas de réglages de collision sauvegardés dans le Blueprint
	YSCombat::ConfigureHurtbox(BodyMesh);

	if (HasAuthority())
	{
		Health = GetMaxHealth();
		// regen de base ; les upgrades Hps regen (RestoreUpgrades au changement de classe) s'y ajoutent
		HealthRegen = BaseHealthRegen;

		// coups critiques dès le début, comme avec l'ancienne upgrade Atk critique
		if (bCriticalsFromStart)
		{
			SetAutoAttackCriticals(StartSwingCriticalInterval, StartBulletCriticalInterval, StartShotgunCriticalMaxDistance, StartSniperCriticalMinDistance);
		}

		// apparition en sortant du sol (minions du Captain), comme les NME
		if (bSpawnRise)
		{
			StartRise();
		}
	}

	// le strafe ne s'applique que si un mesh squelettique est renseigné
	if (StrafeBlendSpace && GetMesh()->GetSkeletalMeshAsset())
	{
		GetMesh()->PlayAnimation(StrafeBlendSpace, true);
	}
}

void AYSPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// mort : corps en ragdoll qui rétrécit, plus rien d'autre
	if (bLocalDeathStarted)
	{
		UpdateDeath(DeltaSeconds);
		return;
	}
	if (bLocalRiseActive)
	{
		UpdateRise();
	}

	UpdateSwimming(DeltaSeconds);
	UpdatePiloting();
	UpdateStrafeAnimation(DeltaSeconds);
	UpdateBodyPivot(DeltaSeconds);
	UpdateHands(DeltaSeconds);
	// les armes répliquées arrivent après le perso chez les clients : réappliqué tant qu'elles changent (sinon rien)
	UpdateWeaponColoration();
	UpdateDayNight(DeltaSeconds);
	UpdateCursorCamera(DeltaSeconds);
	UpdateAutoAttackInput();
	UpdateNameplate();
	UpdateInteractPrompt();
	// vitesse : peut dépendre d'une action en cours (visée du Rolling shot) ; serveur et client propriétaire
	UpdateMaxWalkSpeed();

	// objet porté : dernier sol sûr sous les pieds (il y revient s'il est lâché dans la mer)
	if (HasAuthority() && IsValid(CarriedObject) && GetCharacterMovement()->IsMovingOnGround())
	{
		const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		if (AYSInteractiveObject::IsSafeDropLocation(GetWorld(), Feet))
		{
			CarriedObject->SetLastSafeLocation(Feet);
		}
	}

	// déplacement forcé quand le joueur n'appuie sur aucune direction (roulade du Rolling shot) ou toujours si l'action
	// impose sa trajectoire (Dashing Slash) ; machine qui contrôle le perso
	const bool bMoveOverridden = IsMoveInputOverridden();
	if (bMoveOverridden)
	{
		ConsumeMovementInputVector();
	}
	if (IsLocallyControlled() && (bMoveOverridden || (GFrameCounter - LastMoveInputFrame > 1 && GetPendingMovementInputVector().IsNearlyZero())))
	{
		FVector ForcedDirection = FVector::ZeroVector;
		ForEachHeldAction([&ForcedDirection](const UYSAbilityComponent* Action)
		{
			FVector Direction;
			if (ForcedDirection.IsNearlyZero() && Action->GetForcedMoveDirection(Direction))
			{
				ForcedDirection = Direction;
			}
		});
		if (!ForcedDirection.IsNearlyZero())
		{
			AddMovementInput(ForcedDirection);
		}
	}

	// clients : PlayerState et options de personnalisation peuvent arriver dans n'importe quel ordre
	if (!bCustomizationApplied)
	{
		RefreshCustomization();
	}

	if (HasAuthority())
	{
		UpdateHealthRegen(DeltaSeconds);

		// Bloodlust : toutes les stacks perdues après BloodlustDuration sans toucher
		const AGameStateBase* GameState = GetWorld()->GetGameState();
		if (BloodlustStacks > 0 && GameState && GameState->GetServerWorldTimeSeconds() >= BloodlustExpireServerTime)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[YSBloodlust] %s : %d stack(s) perdue(s)"), *GetName(), BloodlustStacks);
			BloodlustStacks = 0;
			ForceNetUpdate();
		}

		// BloodRage : toutes les stacks perdues après BloodRageDuration sans subir de coup (jamais avec Golden BloodRage)
		if (BloodRageStacks > 0 && !bGoldenBloodRage && GameState && GameState->GetServerWorldTimeSeconds() >= BloodRageExpireServerTime)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[YSBloodRage] %s : %d stack(s) perdue(s)"), *GetName(), BloodRageStacks);
			BloodRageStacks = 0;
			ForceNetUpdate();
		}
	}
}

FVector AYSPlayerCharacter::GetHandTargetLocation(bool bLeft) const
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

FRotator AYSPlayerCharacter::GetHandTargetRotation(bool bLeft) const
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

const AActor* AYSPlayerCharacter::GetHandAssistTarget(bool bLeft) const
{
	// seulement en jeu : dans l'éditeur, les mains gardent leur rotation réglée
	if (!GetWorld() || !GetWorld()->IsGameWorld())
	{
		return nullptr;
	}

	// la direction de la souris est celle du regard du personnage (répliquée : identique sur toutes les machines)
	// action de tir du clic, sinon l'attaque auto (ex. Shotgun : son Ability, Roll and Bomb, ne tire pas)
	const AYSWeapon* Weapon = bLeft ? GetLeftHandWeapon() : GetRightHandWeapon();
	for (const UYSAbilityComponent* Action : { Weapon ? Weapon->GetActiveAction() : nullptr, Weapon ? Weapon->GetAutoAction() : nullptr })
	{
		const UYSCoupDPetoirAbility* Gun = Cast<UYSCoupDPetoirAbility>(Action);
		if (Gun && Gun->ProjectileClass)
		{
			return Gun->FindAssistTarget(GetActorLocation(), GetActorForwardVector());
		}
	}
	return nullptr;
}

void AYSPlayerCharacter::UpdateHands(float DeltaSeconds)
{
	HandSwayTime += DeltaSeconds;
	const FTransform& PivotTransform = BodyPivot->GetComponentTransform();

	auto UpdateHand = [this, DeltaSeconds, &PivotTransform](USceneComponent* Hand, FVector& WorldLocation, float& CurrentYaw, bool bLeft, float NoiseSeed)
	{
		// objet porté, barre : la main est sur son point de placement, sans retard (l'objet suit déjà le corps)
		const AYSInteractiveObject* HandObject = IsValid(CarriedObject) ? CarriedObject.Get() : IsValid(PilotedHelm) ? static_cast<const AYSInteractiveObject*>(PilotedHelm.Get()) : nullptr;
		if (const USceneComponent* HandPoint = HandObject ? HandObject->GetHandPoint(bLeft) : nullptr)
		{
			WorldLocation = HandPoint->GetComponentLocation();
			Hand->SetWorldLocation(WorldLocation);
			Hand->SetRelativeRotation(PivotTransform.GetRotation().Inverse() * HandPoint->GetComponentQuat());
			return;
		}

		// oscillation : bruit de Perlin sur chaque axe, limité à HandSwayAmplitude autour de la cible
		const float NoiseTime = HandSwayTime * HandSwaySpeed + NoiseSeed;
		const FVector Sway = FVector(
			FMath::PerlinNoise1D(NoiseTime),
			FMath::PerlinNoise1D(NoiseTime + 31.7f),
			FMath::PerlinNoise1D(NoiseTime + 67.3f)) * HandSwayAmplitude;

		FVector Target = PivotTransform.TransformPosition(GetHandTargetLocation(bLeft) + Sway.GetClampedToMaxSize(HandSwayAmplitude));
		// nage : les mains à la hauteur de l'eau
		Target.Z = FMath::Lerp(Target.Z, SwimSurfaceZ + SwimHandHeight, SwimBlend);

		// retard : la main rejoint sa cible progressivement (téléportation / première image : directement)
		if (!bHandsInitialized || HandFollowSpeed <= 0.0f || FVector::DistSquared(WorldLocation, Target) > FMath::Square(HandSnapDistance))
		{
			WorldLocation = Target;
		}
		else
		{
			WorldLocation = FMath::VInterpTo(WorldLocation, Target, DeltaSeconds, HandFollowSpeed);
		}

		// la main pivote (yaw) vers la cible de la visée assistée de son arme, pour montrer où partira le tir
		FRotator Rotation = GetHandTargetRotation(bLeft);
		float TargetYaw = Rotation.Yaw;
		if (const AActor* AssistTarget = GetHandAssistTarget(bLeft))
		{
			TargetYaw = FRotator::NormalizeAxis((AssistTarget->GetActorLocation() - WorldLocation).Rotation().Yaw - GetActorRotation().Yaw);
		}

		if (!bHandsInitialized || HandAimSpeed <= 0.0f)
		{
			CurrentYaw = TargetYaw;
		}
		else
		{
			// interpolation par le plus court chemin
			const float Alpha = FMath::Clamp(DeltaSeconds * HandAimSpeed, 0.0f, 1.0f);
			CurrentYaw = FRotator::NormalizeAxis(CurrentYaw + FRotator::NormalizeAxis(TargetYaw - CurrentYaw) * Alpha);
		}
		Rotation.Yaw = CurrentYaw;

		// recul après une activation : main reculée le long de l'axe de l'arme et cabrée (muzzle vers le haut)
		const AYSWeapon* Weapon = bLeft ? GetLeftHandWeapon() : GetRightHandWeapon();
		float RecoilPitch = 0.0f;
		float RecoilKickBack = 0.0f;
		FVector RecoilOffset = FVector::ZeroVector;
		if (Weapon && Weapon->GetRecoil(RecoilPitch, RecoilKickBack))
		{
			RecoilOffset = PivotTransform.TransformVectorNoScale(-Rotation.Vector() * RecoilKickBack);
			Rotation.Pitch += RecoilPitch;
		}

		// rechargement (fin de rafale) : l'arme fait un tour sur elle-même
		if (Weapon)
		{
			Rotation += Weapon->GetReloadSpin();
		}

		// le recul s'ajoute après le lissage : il reste sec malgré le retard des mains
		FVector FinalLocation = WorldLocation + RecoilOffset;
		FQuat FinalRotation = Rotation.Quaternion();

		// pose animée (coupe de l'Auto swing / Dashing Slash, lancer du BombThrower) : remplace la pose normale,
		// sans retard pour garder un geste brutal ; la plus récemment lancée l'emporte (une attaque auto peut partir pendant le
		// retour de l'Ability), à égalité l'Ability. Les animations de l'autre main (AnimatesOffHand) sont jouées avec l'Off Hand
		// Socket, plus bas.
		float IdleWeight = 1.0f;
		FVector PoseLocation = FVector::ZeroVector;
		FQuat PoseRotation = FQuat::Identity;
		bool bSwinging = false;
		float PoseActivationAge = 0.0f;
		for (const UYSAbilityComponent* Action : { Weapon ? Weapon->GetActiveAction() : nullptr, Weapon ? Weapon->GetAutoAction() : nullptr })
		{
			float ActionIdleWeight = 1.0f;
			FVector ActionLocation = FVector::ZeroVector;
			FQuat ActionRotation = FQuat::Identity;
			if (Action && !Action->AnimatesOffHand() && Action->EvaluateHandPose(ActionIdleWeight, ActionLocation, ActionRotation)
				&& (!bSwinging || Action->GetTimeSinceActivation() < PoseActivationAge))
			{
				bSwinging = true;
				PoseActivationAge = Action->GetTimeSinceActivation();
				IdleWeight = ActionIdleWeight;
				PoseLocation = ActionLocation;
				PoseRotation = ActionRotation;
			}
		}
		if (bSwinging)
		{
			YSPlayerHands::MirrorHandPose(bLeft, PoseLocation, PoseRotation);
			FinalLocation = FMath::Lerp(PivotTransform.TransformPosition(PoseLocation), FinalLocation, IdleWeight);
			FinalRotation = FQuat::Slerp(PoseRotation, FinalRotation, IdleWeight);
		}

		Hand->SetWorldLocation(FinalLocation);
		Hand->SetRelativeRotation(FinalRotation);
	};

	UpdateHand(LeftHand, LeftHandWorldLocation, LeftHandYaw, true, 0.0f);
	UpdateHand(RightHand, RightHandWorldLocation, RightHandYaw, false, 100.0f);

	// arme à deux mains (ex. Shotgun) : l'autre main toujours sur le socket de l'arme, sans retard, même pendant le recul
	// (après la main qui la tient : l'arme, attachée à cette main, l'a déjà suivie) ; pas pendant le port d'un objet, la nage ni à la barre
	if (!IsValid(CarriedObject) && !bSwimming && !IsPiloting())
	{
		for (const bool bWeaponInLeft : { false, true })
		{
			const AYSWeapon* Weapon = bWeaponInLeft ? GetLeftHandWeapon() : GetRightHandWeapon();
			const USceneComponent* Socket = Weapon && Weapon->IsTwoHanded() ? Weapon->GetOffHandSocket() : nullptr;
			if (Socket)
			{
				USceneComponent* OtherHand = bWeaponInLeft ? RightHand : LeftHand;
				FVector Location = Socket->GetComponentLocation();
				FQuat Rotation = Socket->GetComponentQuat();

				// action qui anime cette main (ex. lancer de bombe du Sniper) : elle quitte le socket le temps du geste
				const UYSAbilityComponent* Action = Weapon->GetActiveAction();
				float IdleWeight = 1.0f;
				FVector PoseLocation = FVector::ZeroVector;
				FQuat PoseRotation = FQuat::Identity;
				if (Action && Action->AnimatesOffHand() && Action->EvaluateHandPose(IdleWeight, PoseLocation, PoseRotation))
				{
					YSPlayerHands::MirrorHandPose(!bWeaponInLeft, PoseLocation, PoseRotation);
					Location = FMath::Lerp(PivotTransform.TransformPosition(PoseLocation), Location, IdleWeight);
					Rotation = FQuat::Slerp(PivotTransform.GetRotation() * PoseRotation, Rotation, IdleWeight);
				}

				OtherHand->SetWorldLocationAndRotation(Location, Rotation);
				(bWeaponInLeft ? RightHandWorldLocation : LeftHandWorldLocation) = Location;
			}
		}
	}
	bHandsInitialized = true;
}

void AYSPlayerCharacter::UpdateBodyPivot(float DeltaSeconds)
{
	const FVector Velocity = GetVelocity();
	const FQuat LeanQuat = UpdateLean(DeltaSeconds, Velocity);
	// pas de sautillement en nageant
	const float Hop = UpdateHop(DeltaSeconds, bSwimming ? 0.0f : Velocity.Size2D());

	// pivot autour des pieds (bas de la capsule) pour que le personnage reste posé au sol, puis sautillement vertical ;
	// nage : le corps est placé à Swim Immersion sous la surface
	const FVector ActorLocation = GetActorLocation();
	const FVector Feet = ActorLocation - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FVector PivotLocation = Feet + LeanQuat.RotateVector(ActorLocation - Feet) + FVector(0.0f, 0.0f, Hop + GetSwimBodyOffset());

	// roulade (Rolling shot) : le corps tourne sur lui-même vers la direction du mouvement et se met en boule
	FQuat RollQuat = FQuat::Identity;
	float RollAngle = 0.0f;
	float RollHeight = 0.0f;
	bool bRolling = false;
	ForEachHeldAction([&](const UYSAbilityComponent* Action) { bRolling = bRolling || Action->GetBodyRoll(RollAngle, RollHeight); });
	if (bRolling)
	{
		if (Velocity.SizeSquared2D() > FMath::Square(MovingSpeedThreshold))
		{
			RollAnimDirection = Velocity.GetSafeNormal2D();
		}
		RollQuat = FQuat(FVector::CrossProduct(FVector::UpVector, RollAnimDirection).GetSafeNormal(), FMath::DegreesToRadians(RollAngle));
		PivotLocation.Z += RollHeight;
	}
	BodyPivot->SetWorldLocationAndRotation(PivotLocation, RollQuat * LeanQuat * GetActorQuat());
}

float AYSPlayerCharacter::UpdateHop(float DeltaSeconds, float Speed)
{
	const bool bMoving = Speed > MovingSpeedThreshold;

	// à l'arrêt : on termine le saut en cours (à la cadence de référence) puis on reste au sol
	if (!bMoving && HopPhase <= 0.0f)
	{
		return 0.0f;
	}

	// cadence proportionnelle à la vitesse réelle, MoveSpeed servant de référence
	const float SpeedRatio = (bMoving && MoveSpeed > 0.0f) ? Speed / MoveSpeed : 1.0f;
	HopPhase += DeltaSeconds * SpeedRatio / HopDuration;
	if (HopPhase >= 1.0f)
	{
		HopPhase = bMoving ? FMath::Fmod(HopPhase, 1.0f) : 0.0f;
	}

	// montée sur la première moitié, retour sur la seconde
	const float Triangle = HopPhase < 0.5f ? HopPhase * 2.0f : (1.0f - HopPhase) * 2.0f;
	return HopHeight * UKismetMathLibrary::Ease(0.0f, 1.0f, Triangle, HopEasing, HopEaseExponent);
}

FQuat AYSPlayerCharacter::UpdateLean(float DeltaSeconds, const FVector& Velocity)
{
	// cible : MaxAngle dans la direction du déplacement (monde), indépendamment du regard
	FVector2D Target = FVector2D::ZeroVector;
	if (Velocity.SizeSquared2D() > FMath::Square(MovingSpeedThreshold))
	{
		Target = FVector2D(Velocity.X, Velocity.Y).GetSafeNormal() * LeanMaxAngle;
	}

	// nouvelle transition seulement si la cible change vraiment (sinon on la suit sans relancer la courbe)
	if (FVector2D::Distance(Target, LeanTarget) > 1.0f)
	{
		LeanStart = CurrentLean;
		LeanAlpha = 0.0f;
	}
	LeanTarget = Target;

	LeanAlpha = LeanDuration > 0.0f ? FMath::Min(1.0f, LeanAlpha + DeltaSeconds / LeanDuration) : 1.0f;
	const float EasedAlpha = UKismetMathLibrary::Ease(0.0f, 1.0f, LeanAlpha, LeanEasing, LeanEaseExponent);
	CurrentLean = FMath::Lerp(LeanStart, LeanTarget, EasedAlpha);

	// rotation monde : on bascule le haut du corps vers CurrentLean, autour d'un axe horizontal
	const float Angle = CurrentLean.Size();
	FQuat LeanQuat = FQuat::Identity;
	if (Angle > KINDA_SMALL_NUMBER)
	{
		const FVector LeanDirection(CurrentLean.X / Angle, CurrentLean.Y / Angle, 0.0f);
		LeanQuat = FQuat(FVector::CrossProduct(FVector::UpVector, LeanDirection), FMath::DegreesToRadians(Angle));
	}
	return LeanQuat;
}

void AYSPlayerCharacter::UpdateStrafeAnimation(float DeltaSeconds)
{
	UAnimSingleNodeInstance* AnimInstance = StrafeBlendSpace ? GetMesh()->GetSingleNodeInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	// la vélocité est disponible sur toutes les machines (répliquée pour les autres joueurs)
	const FVector Velocity = GetVelocity();
	const float TargetSpeed = Velocity.Size2D();

	// angle du mouvement par rapport au regard : 0 = avant, 90 = droite, +-180 = arrière
	float TargetDirection = StrafeBlendPosition.X;
	if (TargetSpeed > 10.0f)
	{
		TargetDirection = FRotator::NormalizeAxis(Velocity.Rotation().Yaw - GetActorRotation().Yaw);
	}

	if (StrafeBlendSmoothing > 0.0f)
	{
		// interpolation de l'angle par le plus court chemin pour éviter un tour complet entre -180 et 180
		const float DeltaAngle = FRotator::NormalizeAxis(TargetDirection - StrafeBlendPosition.X);
		const float Alpha = FMath::Clamp(DeltaSeconds * StrafeBlendSmoothing, 0.0f, 1.0f);
		StrafeBlendPosition.X = FRotator::NormalizeAxis(StrafeBlendPosition.X + DeltaAngle * Alpha);
		StrafeBlendPosition.Y = FMath::FInterpTo(StrafeBlendPosition.Y, TargetSpeed, DeltaSeconds, StrafeBlendSmoothing);
	}
	else
	{
		StrafeBlendPosition = FVector2D(TargetDirection, TargetSpeed);
	}

	AnimInstance->SetBlendSpacePosition(FVector(StrafeBlendPosition.X, StrafeBlendPosition.Y, 0.0f));
}

void AYSPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// appelé uniquement pour le pawn contrôlé localement
	CreateDefaultInput();

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AYSPlayerCharacter::Move);
		EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &AYSPlayerCharacter::Fire);
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AYSPlayerCharacter::Interact);
	}
}

void AYSPlayerCharacter::CreateDefaultInput()
{
	if (MoveAction && FireAction && AutoAttackAction && InteractAction && DefaultMappingContext)
	{
		return;
	}

	MoveAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;

	FireAction = NewObject<UInputAction>(this, TEXT("IA_Fire"));
	FireAction->ValueType = EInputActionValueType::Boolean;

	AutoAttackAction = NewObject<UInputAction>(this, TEXT("IA_AutoAttack"));
	AutoAttackAction->ValueType = EInputActionValueType::Boolean;

	InteractAction = NewObject<UInputAction>(this, TEXT("IA_Interact"));
	InteractAction->ValueType = EInputActionValueType::Boolean;

	// clic droit = Ability ; clic gauche maintenu = attaque auto (lu à chaque image : UpdateAutoAttackInput)
	DefaultMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Default"));
	DefaultMappingContext->MapKey(FireAction, EKeys::RightMouseButton);
	DefaultMappingContext->MapKey(AutoAttackAction, EKeys::LeftMouseButton);
	DefaultMappingContext->MapKey(InteractAction, EKeys::E);

	// une touche renvoie (1,0) : Swizzle la passe sur Y, Negate l'inverse
	auto MapKey = [this](const FKey& Key, bool bSwizzle, bool bNegate)
	{
		FEnhancedActionKeyMapping& Mapping = DefaultMappingContext->MapKey(MoveAction, Key);
		if (bSwizzle)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(DefaultMappingContext));
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(DefaultMappingContext));
		}
	};

	// les flèches sont réservées aux codes de triche (AYSPlayerController::SetupInputComponent)
	// avant : Z (AZERTY), W (QWERTY)
	MapKey(EKeys::Z, true, false);
	MapKey(EKeys::W, true, false);

	// arrière
	MapKey(EKeys::S, true, true);

	// gauche : Q (AZERTY), A (QWERTY)
	MapKey(EKeys::Q, false, true);
	MapKey(EKeys::A, false, true);

	// droite
	MapKey(EKeys::D, false, false);
}

void AYSPlayerCharacter::Move(const FInputActionValue& Value)
{
	// trajectoire imposée par une action (Dashing Slash) : ZQSD ignoré jusqu'à la fin
	if (IsMoveInputOverridden())
	{
		return;
	}

	const FVector2D Input = Value.Get<FVector2D>();

	// à la barre : ZQSD pilote le bateau (Z / S = avancer / reculer, Q / D = tourner), le personnage ne bouge pas
	if (IsPiloting())
	{
		HelmInput = FVector2D(FMath::Clamp(Input.X, -1.0f, 1.0f), FMath::Clamp(Input.Y, -1.0f, 1.0f));
		LastHelmInputFrame = GFrameCounter;
		return;
	}

	// directions relatives à la caméra (seulement le yaw) : "haut" = vers le haut de l'écran
	const FRotator CameraYaw(0.0f, CameraBoom->GetComponentRotation().Yaw, 0.0f);
	const FRotationMatrix CameraMatrix(CameraYaw);

	AddMovementInput(CameraMatrix.GetUnitAxis(EAxis::X), Input.Y);
	AddMovementInput(CameraMatrix.GetUnitAxis(EAxis::Y), Input.X);

	if (!Input.IsNearlyZero())
	{
		LastMoveInputFrame = GFrameCounter;
	}
}

void AYSPlayerCharacter::Fire()
{
	// objet porté à deux mains, barre : armes désactivées
	if (IsCarrying() || IsPiloting())
	{
		return;
	}

	const AYSWeapon* MainWeapon = GetMainHandWeapon();
	const AYSWeapon* OffWeapon = GetOffHandWeapon();
	UYSAbilityComponent* MainAction = MainWeapon ? MainWeapon->GetActiveAction() : nullptr;
	UYSAbilityComponent* OffAction = OffWeapon ? OffWeapon->GetActiveAction() : nullptr;

	// action qui occupe tout le personnage (Rolling shot) : seule la main principale la lance (elle gère l'autre arme)
	if (MainAction && OffAction && IsAkimbo() && MainAction->BlocksAllWeapons())
	{
		MainAction->TryActivate(GetAimLocation());
		return;
	}

	// akimbo : la main principale (droite, gauche pour un gaucher) tire, puis l'autre après le délai de l'arme
	const bool bAkimbo = MainAction && OffAction && IsAkimbo();
	if (bAkimbo && MainAction->IsReady())
	{
		MainAction->TryActivate(GetAimLocation());
		if (OffAction->IsReady())
		{
			TWeakObjectPtr<UYSAbilityComponent> WeakOffAction = OffAction;
			GetWorldTimerManager().SetTimer(AkimboTimer, FTimerDelegate::CreateWeakLambda(this, [this, WeakOffAction]()
			{
				if (WeakOffAction.IsValid())
				{
					WeakOffAction->TryActivate(GetAimLocation());
				}
			}), FMath::Max(MainAction->AkimboFireDelay, KINDA_SMALL_NUMBER), false);
		}
		return;
	}

	// sinon : chaque main déclenche son action si elle est prête
	for (UYSAbilityComponent* Action : { MainAction, OffAction })
	{
		if (Action && Action->IsReady())
		{
			Action->TryActivate(GetAimLocation());
		}
	}
}

void AYSPlayerCharacter::UpdateAutoAttackInput()
{
	if (!IsLocallyControlled() || !UsesHeldAutoAttack())
	{
		return;
	}

	// état lu à chaque image (pas d'événement perdu pendant une pause, un menu ou un changement de classe)
	bool bHeld = false;
	const APlayerController* PC = Cast<APlayerController>(GetController());
	const UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (const UEnhancedPlayerInput* PlayerInput = Subsystem ? Subsystem->GetPlayerInput() : nullptr)
	{
		bHeld = AutoAttackAction && PlayerInput->GetActionValue(AutoAttackAction).Get<bool>();
	}
	if (bHeld == bAutoAttackHeld)
	{
		return;
	}

	bAutoAttackHeld = bHeld;
	UE_LOG(LogTemp, Verbose, TEXT("[YSAutoAttack] %s : clic gauche %s"), *GetName(), bHeld ? TEXT("maintenu") : TEXT("relâché"));
	if (!HasAuthority())
	{
		ServerSetAutoAttackHeld(bHeld);
	}
}

void AYSPlayerCharacter::ServerSetAutoAttackHeld_Implementation(bool bHeld)
{
	bAutoAttackHeld = bHeld;
}

void AYSPlayerCharacter::SetDebugAutoAttack(bool bHeld)
{
	bDebugAutoAttack = bHeld;
	UE_LOG(LogTemp, Log, TEXT("[YSAutoAttack] %s : clic gauche (debug) %s"), *GetName(), bHeld ? TEXT("maintenu") : TEXT("relâché"));
	if (!HasAuthority())
	{
		ServerSetDebugAutoAttack(bHeld);
	}
}

void AYSPlayerCharacter::ServerSetDebugAutoAttack_Implementation(bool bHeld)
{
	bDebugAutoAttack = bHeld;
}

FVector AYSPlayerCharacter::GetAimLocation() const
{
	const AYSPlayerController* PC = Cast<AYSPlayerController>(GetController());
	return PC ? PC->GetAimLocation() : GetActorLocation() + GetActorForwardVector() * 1000.0f;
}

void AYSPlayerCharacter::SetMoveSpeedMultiplier(float Multiplier)
{
	MoveSpeedMultiplier = Multiplier;
	OnRep_MoveSpeedMultiplier();
}

void AYSPlayerCharacter::OnRep_MoveSpeedMultiplier()
{
	UpdateMaxWalkSpeed();
}

void AYSPlayerCharacter::SetMaxHealthMultiplier(float Multiplier)
{
	const float OldMaxHealth = GetMaxHealth();
	MaxHealthMultiplier = Multiplier;
	Health = FMath::Clamp(Health + GetMaxHealth() - OldMaxHealth, 0.0f, GetMaxHealth());
}

void AYSPlayerCharacter::UpdateHealthRegen(float DeltaSeconds)
{
	// regen seulement avec l'upgrade, vivant, pas au max, et sans dégât subi depuis HealthRegenDelay
	if (HealthRegen <= 0.0f || !IsAlive() || Health >= GetMaxHealth() || GetWorld()->GetTimeSeconds() - LastDamageTime < HealthRegenDelay)
	{
		HealthRegenTimer = 0.0f;
		return;
	}

	HealthRegenTimer += DeltaSeconds;
	while (HealthRegenTimer >= HealthRegenInterval)
	{
		HealthRegenTimer -= HealthRegenInterval;
		Health = FMath::Min(Health + HealthRegen, GetMaxHealth());
		UE_LOG(LogTemp, Verbose, TEXT("[YSHealthRegen] %s +%.1f -> %.1f / %.1f"), *GetName(), HealthRegen, Health, GetMaxHealth());
	}
}

float AYSPlayerCharacter::GetWeaponCooldownMultiplier(const AYSWeapon* Weapon, EYSActionSlot Slot) const
{
	return Weapon && GetHeldWeapons().Contains(Weapon) ? CooldownMultipliers[GetSlotIndex(Slot)] : 1.0f;
}

float AYSPlayerCharacter::GetWeaponStat(const AYSWeapon* Weapon, FName Stat, float Default) const
{
	const float* Value = Weapon && GetHeldWeapons().Contains(Weapon) ? WeaponStats.Find(Stat) : nullptr;
	return Value ? *Value : Default;
}

float AYSPlayerCharacter::GetMarkStat(const AActor* Target, FName Stat)
{
	float Best = 0.0f;
	UWorld* World = Target ? Target->GetWorld() : nullptr;
	for (TActorIterator<AYSPlayerCharacter> It(World); It && World; ++It)
	{
		if (It->MarkedFoe && It->MarkedFoe == Target)
		{
			Best = FMath::Max(Best, It->GetCharacterWeaponStat(Stat, 0.0f));
		}
	}
	return Best;
}

bool AYSPlayerCharacter::HasForcedCriticalAgainst(const AActor* Target) const
{
	// Marquage Critique : tous les joueurs font des critiques sur le NME marqué par un Captain qui l'a débloqué
	return GetMarkStat(Target, UYSMarkCriticalUpgrade::MarkCriticalStat) > 0.0f;
}

void AYSPlayerCharacter::ReduceAbilityCooldown(float Seconds)
{
	if (!HasAuthority() || Seconds <= 0.0f)
	{
		return;
	}
	for (const AYSWeapon* Weapon : GetHeldWeapons())
	{
		UYSAbilityComponent* Action = Weapon->GetActiveAction();
		if (Action && Action->GetActionSlot() == EYSActionSlot::Ability)
		{
			Action->ReduceCooldown(Seconds);
		}
	}
}

void AYSPlayerCharacter::UpdateDayNight(float DeltaSeconds)
{
	if (!IsLocallyControlled() || !IsPlayerControlled())
	{
		return;
	}

	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	const float Target = YSGameState && YSGameState->IsNight() ? 1.0f : 0.0f;
	NightBlend = DayNightBlendDuration > 0.0f ? FMath::FInterpConstantTo(NightBlend, Target, DeltaSeconds, 1.0f / DayNightBlendDuration) : Target;

	TopDownCamera->PostProcessSettings = NightPostProcess;
	TopDownCamera->PostProcessBlendWeight = NightBlend;
}

void AYSPlayerCharacter::UpdateCursorCamera(float DeltaSeconds)
{
	if (!IsLocallyControlled() || !IsPlayerControlled())
	{
		return;
	}

	// position du curseur par rapport au centre de l'écran : 0 au centre, 1 au bord (chaque axe), plafonnée à 1 dans les coins
	FVector Target = FVector::ZeroVector;
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	float MouseX = 0.0f, MouseY = 0.0f;
	int32 ViewportX = 0, ViewportY = 0;
	// Camera Lock (réglage du joueur, TAB) : caméra centrée sur le PJ
	const AYSPlayerController* YSController = Cast<AYSPlayerController>(PlayerController);
	const bool bCameraLocked = YSController && YSController->IsCameraLocked();
	if (bCursorCameraOffset && !bCameraLocked && PlayerController && PlayerController->GetMousePosition(MouseX, MouseY))
	{
		PlayerController->GetViewportSize(ViewportX, ViewportY);
		if (ViewportX > 0 && ViewportY > 0)
		{
			const FVector2D Ratio((MouseX - 0.5f * ViewportX) / (0.5f * ViewportX), (MouseY - 0.5f * ViewportY) / (0.5f * ViewportY));
			const FVector2D Clamped = Ratio.GetSafeNormal() * FMath::Min(Ratio.Size(), 1.0f);

			// haut de l'écran = devant la caméra (à plat), droite de l'écran = sa droite
			const FRotationMatrix CameraYaw(FRotator(0.0f, CameraBoom->GetComponentRotation().Yaw, 0.0f));
			Target = (CameraYaw.GetUnitAxis(EAxis::X) * -Clamped.Y + CameraYaw.GetUnitAxis(EAxis::Y) * Clamped.X) * CursorCameraMaxOffset;
		}
	}

	// à la barre : caméra centrée (sur le bateau ou le pilote), plus de décalage vers le curseur, dézoomée
	const AYSPirateShip* Ship = IsValid(PilotedHelm) ? PilotedHelm->GetShip() : nullptr;
	if (Ship)
	{
		Target = FVector::ZeroVector;
		HelmCameraOffset = Ship->IsHelmCameraOnShip() ? Ship->GetActorLocation() - GetActorLocation() : FVector::ZeroVector;
		HelmCameraOffset.Z = 0.0f;
	}
	const float HelmInterpSpeed = Ship ? Ship->GetHelmCameraInterpSpeed() : 2.0f;
	HelmCameraBlend = HelmInterpSpeed > 0.0f ? FMath::FInterpTo(HelmCameraBlend, Ship ? 1.0f : 0.0f, DeltaSeconds, HelmInterpSpeed) : (Ship ? 1.0f : 0.0f);
	if (Ship)
	{
		HelmCameraArmLength = Ship->GetHelmCameraArmLength();
	}
	CameraBoom->TargetArmLength = FMath::Lerp(DefaultCameraArmLength, HelmCameraArmLength, HelmCameraBlend);

	// lissé : les tours de curseur ne secouent pas la caméra
	CursorCameraOffset = CursorCameraInterpSpeed > 0.0f ? FMath::VInterpTo(CursorCameraOffset, Target, DeltaSeconds, CursorCameraInterpSpeed) : Target;
	CameraBoom->TargetOffset = CursorCameraOffset + HelmCameraOffset * HelmCameraBlend;
}

TArray<FYSUpgradeOffer> AYSPlayerCharacter::PickClassOffers() const
{
	TArray<FYSUpgradeOffer> Offers;
	for (int32 PoolIndex = 0; PoolIndex < UpgradePool.Num(); ++PoolIndex)
	{
		const UYSUpgrade* Upgrade = UpgradePool[PoolIndex];
		for (int32 Variant = 0; Upgrade && Upgrade->GetUpgradeType() == EYSUpgradeType::Class && Variant < Upgrade->GetVariantCount(); ++Variant)
		{
			if (Upgrade->CanBeOffered(this, Variant, GetUpgradeProgress(PoolIndex, Variant)))
			{
				FYSUpgradeOffer& Offer = Offers.AddDefaulted_GetRef();
				Offer.PoolIndex = PoolIndex;
				Offer.Variant = Variant;
			}
		}
	}
	return Offers;
}

void AYSPlayerCharacter::ChangeClass(TSubclassOf<AYSPlayerCharacter> NewClass, UYSWeaponSet* NewWeaponSet)
{
	AController* OwningController = GetController();
	UWorld* World = GetWorld();
	if (!HasAuthority() || !NewClass || !OwningController || !World)
	{
		return;
	}

	// tout ce qui passe au nouveau personnage
	const TArray<TObjectPtr<UYSUpgrade>> OldPool = UpgradePool;
	const TArray<FYSUpgradeProgress> OldProgress = UpgradeProgress;
	const TArray<FYSShardLevel> OldShards = ShardLevels;
	const float HealthRatio = GetMaxHealth() > 0.0f ? Health / GetMaxHealth() : 1.0f;
	const FTransform SpawnTransform(FRotator(0.0f, GetActorRotation().Yaw, 0.0f), GetActorLocation());
	const FString OldName = GetName();
	const bool bWasLeftHanded = bLeftHanded;

	// l'objet porté reste au sol (le nouveau personnage a les mains libres)
	DropCarriedObject(TEXT("changement de classe"));

	// minions (Captain) : ils suivent le nouveau personnage (sinon ils mourraient avec celui-ci)
	TArray<AYSCaptainMinion*> Minions;
	for (TActorIterator<AYSCaptainMinion> It(World); It; ++It)
	{
		if (It->GetCaptain() == this && It->IsAlive() && !It->IsActorBeingDestroyed())
		{
			Minions.Add(*It);
		}
	}

	OwningController->UnPossess();
	Destroy();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	// droitier / gaucher conservé : réglé avant la construction (armes des mains)
	Params.bDeferConstruction = true;
	AYSPlayerCharacter* NewCharacter = World->SpawnActor<AYSPlayerCharacter>(NewClass, SpawnTransform, Params);
	if (NewCharacter)
	{
		NewCharacter->SetLeftHanded(bWasLeftHanded);
		// set d'arme : celui demandé (upgrade de classe), sinon celui du nouveau personnage
		if (NewWeaponSet)
		{
			NewCharacter->WeaponSet = NewWeaponSet;
		}
		// même personnage (changement de set) : le passif suit, même s'il a été remplacé
		if (NewClass == GetClass())
		{
			NewCharacter->Passive = Passive;
		}
		NewCharacter->FinishSpawning(SpawnTransform);
	}
	if (!NewCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("[YSPlayerCharacter] Changement de classe impossible : %s n'a pas pu apparaître"), *NewClass->GetName());
		return;
	}

	OwningController->Possess(NewCharacter);
	for (AYSCaptainMinion* Minion : Minions)
	{
		Minion->SetCaptain(NewCharacter);
	}
	NewCharacter->RestoreUpgrades(OldPool, OldProgress);
	NewCharacter->RestoreShards(OldShards);
	NewCharacter->Health = FMath::Max(1.0f, HealthRatio * NewCharacter->GetMaxHealth());
	UE_LOG(LogTemp, Log, TEXT("[YSPlayerCharacter] Changement de classe : %s -> %s, set %s (%d upgrade(s) conservée(s), PV %.0f / %.0f)"),
		*OldName, *NewCharacter->GetName(), NewCharacter->WeaponSet ? *NewCharacter->WeaponSet->GetName() : TEXT("aucun"), NewCharacter->UpgradeProgress.Num(), NewCharacter->Health, NewCharacter->GetMaxHealth());
}

AYSPlayerCharacter* AYSPlayerCharacter::Revive(AController* NewController, const FVector& Ground, float HealthRatio)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !NewController || IsAlive())
	{
		return nullptr;
	}

	// debout sur le sol (capsule de ce corps : même classe, même taille), tourné comme le corps à sa mort
	const FTransform SpawnTransform(FRotator(0.0f, GetActorRotation().Yaw, 0.0f), Ground + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0f));
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	// main, set d'arme et passif réglés avant la construction (armes des mains)
	Params.bDeferConstruction = true;
	AYSPlayerCharacter* NewCharacter = World->SpawnActor<AYSPlayerCharacter>(GetClass(), SpawnTransform, Params);
	if (!NewCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("[YSMedic] Résurrection impossible : %s n'a pas pu apparaître"), *GetClass()->GetName());
		return nullptr;
	}
	NewCharacter->SetLeftHanded(bLeftHanded);
	NewCharacter->WeaponSet = WeaponSet;
	NewCharacter->Passive = Passive;
	NewCharacter->FinishSpawning(SpawnTransform);

	NewController->Possess(NewCharacter);
	NewCharacter->RestoreUpgrades(UpgradePool, UpgradeProgress);
	NewCharacter->RestoreShards(ShardLevels);
	NewCharacter->Health = FMath::Max(1.0f, FMath::RoundToFloat(HealthRatio * NewCharacter->GetMaxHealth()));
	UE_LOG(LogTemp, Log, TEXT("[YSMedic] %s ressuscité en %s : %s, set %s (%d upgrade(s), PV %.0f / %.0f)"), *GetName(), *Ground.ToCompactString(), *NewCharacter->GetName(),
		NewCharacter->WeaponSet ? *NewCharacter->WeaponSet->GetName() : TEXT("aucun"), NewCharacter->UpgradeProgress.Num(), NewCharacter->Health, NewCharacter->GetMaxHealth());

	// l'ancien corps (et ses armes) disparaît
	Destroy();
	return NewCharacter;
}

void AYSPlayerCharacter::RestoreUpgrades(const TArray<TObjectPtr<UYSUpgrade>>& OldPool, const TArray<FYSUpgradeProgress>& OldProgress)
{
	if (!HasAuthority())
	{
		return;
	}

	// même upgrade (même asset) dans le pool du nouveau personnage : la progression est reprise telle quelle
	UpgradeProgress.Reset();
	for (const FYSUpgradeProgress& Old : OldProgress)
	{
		const UYSUpgrade* Upgrade = OldPool.IsValidIndex(Old.PoolIndex) ? OldPool[Old.PoolIndex].Get() : nullptr;
		const int32 NewIndex = Upgrade ? UpgradePool.IndexOfByKey(Upgrade) : INDEX_NONE;
		if (NewIndex == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("[YSPlayerCharacter] %s : upgrade %s absente du pool de %s, progression perdue"),
				*GetName(), Upgrade ? *Upgrade->GetName() : TEXT("?"), *GetClass()->GetName());
			continue;
		}

		FYSUpgradeProgress& Entry = UpgradeProgress.Add_GetRef(Old);
		Entry.PoolIndex = NewIndex;
	}

	// upgrade incompatible avec le nouveau set (ex. Ricochet sans pistolet, Atk speed sans épée) : convertie vers sa jumelle
	// (Bloodlust, Shoot system), même variante (palier, type d'action), chaque bonus gardant sa rareté
	for (FYSUpgradeProgress& Entry : UpgradeProgress)
	{
		const UYSUpgrade* Upgrade = UpgradePool[Entry.PoolIndex];
		const UYSUpgrade* Twin = Upgrade ? Upgrade->TwinUpgrade.Get() : nullptr;
		const int32 TwinIndex = Twin ? UpgradePool.IndexOfByKey(Twin) : INDEX_NONE;
		if (!Twin || TwinIndex == INDEX_NONE || Upgrade->IsCompatibleWith(this, Entry.Variant) || !Twin->IsCompatibleWith(this, Entry.Variant))
		{
			continue;
		}

		// rareté de chaque bonus (anciennes parties sans rareté notée : celle dont la valeur est la plus proche)
		TArray<EYSUpgradeRarity> Rarities = Entry.BonusRarities;
		for (int32 Index = Rarities.Num(); Index < Entry.Stacks; ++Index)
		{
			const float Value = Entry.BonusValues.IsValidIndex(Index) ? Entry.BonusValues[Index] : 0.0f;
			EYSUpgradeRarity Closest = EYSUpgradeRarity::Silver;
			float ClosestGap = TNumericLimits<float>::Max();
			for (const EYSUpgradeRarity Rarity : { EYSUpgradeRarity::Bronze, EYSUpgradeRarity::Silver, EYSUpgradeRarity::Gold })
			{
				const float Gap = FMath::Abs(Upgrade->GetBonusValue(Entry.Variant, Rarity) - Value);
				if (Upgrade->SupportsRarity(Entry.Variant, Rarity) && Gap < ClosestGap)
				{
					Closest = Rarity;
					ClosestGap = Gap;
				}
			}
			Rarities.Add(Closest);
		}

		Entry.PoolIndex = TwinIndex;
		Entry.BonusRarities = Rarities;
		Entry.BonusValues.Reset();
		Entry.TotalValue = 0.0f;
		for (const EYSUpgradeRarity Rarity : Rarities)
		{
			const float Value = Twin->GetBonusValue(Entry.Variant, Rarity);
			Entry.BonusValues.Add(Value);
			Entry.TotalValue += Value;
		}
		UE_LOG(LogTemp, Log, TEXT("[YSPlayerCharacter] %s : %s converti en %s (palier %d, %d bonus, somme %.3f)"), *GetName(), *Upgrade->GetName(), *Twin->GetName(),
			Entry.Variant, Entry.Stacks, Entry.TotalValue);
	}

	// effets réappliqués une fois toute la progression en place (les upgrades d'arme lisent leurs 3 paliers) ;
	// pas les upgrades de classe (elles relanceraient un changement de classe)
	for (const FYSUpgradeProgress& Entry : UpgradeProgress)
	{
		const UYSUpgrade* Upgrade = UpgradePool[Entry.PoolIndex];
		if (Upgrade->GetUpgradeType() != EYSUpgradeType::Class)
		{
			Upgrade->Apply(this, Entry.Variant, Entry);
		}
	}
}

void AYSPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// serveur : le PlayerState est connu dès la possession (y compris après un changement de classe)
	RefreshCustomization();
}

void AYSPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// clients : le PlayerState arrive par réplication
	RefreshCustomization();
}

void AYSPlayerCharacter::RefreshCustomization()
{
	const AYSPlayerState* YSPlayerState = GetPlayerState<AYSPlayerState>();
	const AYSGameState* YSGameState = GetWorld() ? GetWorld()->GetGameState<AYSGameState>() : nullptr;
	if (YSPlayerState && YSGameState)
	{
		UMaterialInterface* ColorMaterial = YSGameState->GetCustomizationMaterial(YSPlayerState->GetColorIndex());
		UTexture2D* FaceTexture = YSGameState->GetCustomizationFace(YSPlayerState->GetFaceIndex());
		ApplyCustomization(ColorMaterial, FaceTexture);
		bCustomizationApplied = ColorMaterial && FaceTexture;
	}
}

void AYSPlayerCharacter::ApplyCustomization(UMaterialInterface* ColorMaterial, UTexture2D* FaceTexture)
{
	if (FaceTexture && FaceDecal)
	{
		FaceDecal->SetFaceTexture(FaceTexture);
	}

	if (!ColorMaterial)
	{
		return;
	}

	// composant du Blueprint (nom de variable, espaces et casse ignorés) puis tous ses sous-composants
	const FString WantedName = CustomizationColorComponentName.ToString().Replace(TEXT(" "), TEXT(""));
	TInlineComponentArray<USceneComponent*> Components(this);
	for (USceneComponent* Component : Components)
	{
		if (!Component->GetName().Replace(TEXT(" "), TEXT("")).Equals(WantedName, ESearchCase::IgnoreCase))
		{
			continue;
		}

		TArray<USceneComponent*> Targets;
		Component->GetChildrenComponents(true, Targets);
		Targets.Add(Component);
		for (USceneComponent* Target : Targets)
		{
			if (UYSDanglingChainComponent* Chain = Cast<UYSDanglingChainComponent>(Target))
			{
				Chain->SetSegmentMaterial(ColorMaterial);
				UE_LOG(LogTemp, Verbose, TEXT("[YSCustomization] %s : %s (chaîne)"), *GetName(), *Target->GetName());
			}
			else if (UMeshComponent* TargetMesh = Cast<UMeshComponent>(Target))
			{
				for (int32 Slot = 0; Slot < TargetMesh->GetNumMaterials(); ++Slot)
				{
					TargetMesh->SetMaterial(Slot, ColorMaterial);
					UE_LOG(LogTemp, Verbose, TEXT("[YSCustomization] %s : %s"), *GetName(), *Target->GetName());
				}
			}
		}
	}
}

void AYSPlayerCharacter::UpdateNameplate()
{
	// alliés uniquement : pas le joueur local (HUD), ni un personnage sans joueur (aperçu)
	const APlayerState* OwningPlayerState = GetPlayerState();
	const bool bShow = OwningPlayerState && !IsLocallyControlled() && IsAlive();
	Nameplate->SetVisibility(bShow);
	if (!bShow)
	{
		return;
	}

	if (UYSNameplateWidget* Widget = Cast<UYSNameplateWidget>(Nameplate->GetUserWidgetObject()))
	{
		const AYSPlayerState* YSOwningState = Cast<AYSPlayerState>(OwningPlayerState);
		Widget->SetPlayer(FText::FromString(YSOwningState ? YSOwningState->GetDisplayName() : OwningPlayerState->GetPlayerName()), Health, GetMaxHealth());
	}
}

bool AYSPlayerCharacter::IsAkimbo() const
{
	const AYSWeapon* LeftWeapon = GetLeftHandWeapon();
	const AYSWeapon* RightWeapon = GetRightHandWeapon();
	return LeftWeapon && RightWeapon && LeftWeapon->GetClass() == RightWeapon->GetClass();
}

void AYSPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// l'invite « E » affichée pour ce joueur disparaît avec lui
	if (FocusedInteractable.IsValid())
	{
		FocusedInteractable->SetPromptVisible(false);
	}
	FocusedInteractable.Reset();

	// serveur : un personnage qui disparaît (déconnexion…) ne garde pas l'objet
	DropCarriedObject(TEXT("fin de jeu du personnage"));
	StopPiloting(TEXT("fin de jeu du personnage"));

	// sa marque (Captain AMMO) disparaît avec lui
	if (DisplayedMarkedFoe.IsValid())
	{
		DisplayedMarkedFoe->SetMarked(false);
	}
	DisplayedMarkedFoe.Reset();

	Super::EndPlay(EndPlayReason);
}

void AYSPlayerCharacter::SetMarkedFoe(AYSFoeCharacter* Foe, bool bMinionsCritical, bool bSpawnsMinion)
{
	if (!HasAuthority())
	{
		return;
	}
	// effets de la dernière arme qui a marqué
	bMarkMinionsCritical = Foe && bMinionsCritical;
	bMarkSpawnsMinion = Foe && bSpawnsMinion;
	if (MarkedFoe == Foe)
	{
		return;
	}
	MarkedFoe = Foe;
	ForceNetUpdate();
	UpdateMarkDisplay();
	UE_LOG(LogTemp, Verbose, TEXT("[YSMark] %s marque %s"), *GetName(), Foe ? *Foe->GetName() : TEXT("(personne)"));
}

void AYSPlayerCharacter::OnMarkedFoeKilled(const AYSFoeCharacter* Foe)
{
	if (bMarkSpawnsMinion)
	{
		SpawnMinionAtFoe(Foe, TEXT("NME marqué mort"));
	}
}

void AYSPlayerCharacter::SpawnMinionAtFoe(const AYSFoeCharacter* Foe, const TCHAR* Reason)
{
	if (!HasAuthority() || !Foe)
	{
		return;
	}
	// un minion apparaît là où est le NME (invocation de l'arme d'invocation tenue, sans dépasser le maximum)
	for (const AYSWeapon* Weapon : GetHeldWeapons())
	{
		TInlineComponentArray<UYSSummonCrewAbility*> Summons(Weapon);
		if (Summons.Num() > 0)
		{
			const AYSCaptainMinion* Minion = Summons[0]->SpawnMinionAt(Foe->GetActorLocation(), Foe->GetActorRotation().Yaw);
			UE_LOG(LogTemp, Verbose, TEXT("[YSMark] %s : %s (%s), minion %s"), *GetName(), Reason, *Foe->GetName(), Minion ? *Minion->GetName() : TEXT("non invoqué (maximum ou pas de sol)"));
			return;
		}
	}
}

void AYSPlayerCharacter::RestoreOnLevelUp()
{
	if (!HasAuthority() || !IsAlive())
	{
		return;
	}
	Health = GetMaxHealth();
	for (const AYSWeapon* Weapon : { GetMainHandWeapon(), GetOffHandWeapon() })
	{
		UYSAbilityComponent* Action = Weapon ? Weapon->GetActiveAction() : nullptr;
		if (Action && Action->GetActionSlot() == EYSActionSlot::Ability)
		{
			Action->ResetCooldown();
		}
	}
	// pendant la pause du level up : réplication forcée
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSGameMode] Level up : %s soigné (%.0f PV), recharge de l'Ability rendue"), *GetName(), Health);
}

void AYSPlayerCharacter::ForceAbility(const FYSForcedActivation& Forced)
{
	if (!HasAuthority() || !IsAlive() || AreWeaponsStowed())
	{
		return;
	}
	// encore en train de sortir du sol (minion) : sortie terminée tout de suite pour lancer l'Ability
	FinishRiseNow();
	// accord du serveur sur les actions de clic des deux mains
	for (const AYSWeapon* Weapon : { GetMainHandWeapon(), GetOffHandWeapon() })
	{
		if (UYSAbilityComponent* Action = Weapon ? Weapon->GetActiveAction() : nullptr)
		{
			Action->GrantForcedActivation(Forced);
		}
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSGoMinions] Ability de %s forcée"), *GetName());

	// lancée par la machine qui contrôle le personnage, vers son curseur (hôte, minion : tout de suite)
	if (IsLocallyControlled())
	{
		ForceFire();
	}
	else
	{
		ClientForceAbility(Forced);
	}
}

void AYSPlayerCharacter::ClientForceAbility_Implementation(FYSForcedActivation Forced)
{
	for (const AYSWeapon* Weapon : { GetMainHandWeapon(), GetOffHandWeapon() })
	{
		if (UYSAbilityComponent* Action = Weapon ? Weapon->GetActiveAction() : nullptr)
		{
			Action->GrantForcedActivation(Forced);
		}
	}
	ForceFire();
}

void AYSPlayerCharacter::ForceFire()
{
	const AYSWeapon* MainWeapon = GetMainHandWeapon();
	const AYSWeapon* OffWeapon = GetOffHandWeapon();
	UYSAbilityComponent* MainAction = MainWeapon ? MainWeapon->GetActiveAction() : nullptr;
	UYSAbilityComponent* OffAction = OffWeapon ? OffWeapon->GetActiveAction() : nullptr;
	const bool bAkimbo = MainAction && OffAction && IsAkimbo();

	// comme Fire : une action qui occupe tout le personnage (Rolling shot) n'est lancée que par la main principale
	if (bAkimbo && MainAction->BlocksAllWeapons())
	{
		MainAction->TryForcedActivate(GetAimLocation());
		return;
	}
	// akimbo : la main principale, puis l'autre après le délai de l'arme
	if (bAkimbo)
	{
		MainAction->TryForcedActivate(GetAimLocation());
		TWeakObjectPtr<UYSAbilityComponent> WeakOffAction = OffAction;
		GetWorldTimerManager().SetTimer(AkimboTimer, FTimerDelegate::CreateWeakLambda(this, [this, WeakOffAction]()
		{
			if (WeakOffAction.IsValid())
			{
				WeakOffAction->TryForcedActivate(GetAimLocation());
			}
		}), FMath::Max(MainAction->AkimboFireDelay, KINDA_SMALL_NUMBER), false);
		return;
	}
	for (UYSAbilityComponent* Action : { MainAction, OffAction })
	{
		if (Action)
		{
			Action->TryForcedActivate(GetAimLocation());
		}
	}
}

void AYSPlayerCharacter::OnRep_MarkedFoe()
{
	UpdateMarkDisplay();
}

void AYSPlayerCharacter::UpdateMarkDisplay()
{
	if (DisplayedMarkedFoe.Get() == MarkedFoe)
	{
		return;
	}
	if (DisplayedMarkedFoe.IsValid())
	{
		DisplayedMarkedFoe->SetMarked(false);
	}
	DisplayedMarkedFoe = MarkedFoe;
	if (MarkedFoe)
	{
		MarkedFoe->SetMarked(true);
	}
}

void AYSPlayerCharacter::SetBloodlust(int32 MaxStacks, float Duration, float AtkSpeedPerStack, bool bGolden)
{
	BloodlustMaxStacks = FMath::Max(0, MaxStacks);
	BloodlustDuration = Duration;
	BloodlustAtkSpeedPerStack = AtkSpeedPerStack;
	bGoldenBloodlust = bGolden;
	BloodlustStacks = FMath::Min(BloodlustStacks, BloodlustMaxStacks);
	ForceNetUpdate();
}

void AYSPlayerCharacter::AddBloodlustStack()
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!HasAuthority() || BloodlustMaxStacks <= 0 || !GameState)
	{
		return;
	}
	// une stack de plus ; au maximum, le coup relance quand même le délai
	BloodlustStacks = FMath::Min(BloodlustStacks + 1, BloodlustMaxStacks);
	BloodlustExpireServerTime = GameState->GetServerWorldTimeSeconds() + BloodlustDuration;
	ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("[YSBloodlust] %s : %d / %d stack(s), facteur de recharge %.3f%s"), *GetName(), BloodlustStacks, BloodlustMaxStacks,
		GetBloodlustCooldownFactor(), IsBloodlustCritical() ? TEXT(", critiques") : TEXT(""));
}

void AYSPlayerCharacter::DebugSetBloodlustStacks(int32 Stacks)
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!HasAuthority() || !GameState)
	{
		return;
	}
	BloodlustStacks = FMath::Clamp(Stacks, 0, BloodlustMaxStacks);
	BloodlustExpireServerTime = GameState->GetServerWorldTimeSeconds() + BloodlustDuration;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSBloodlust] %s : stacks fixées à %d / %d"), *GetName(), BloodlustStacks, BloodlustMaxStacks);
}

void AYSPlayerCharacter::SetBloodRage(int32 MaxStacks, float Duration, float ArmorPerStack, bool bGolden)
{
	BloodRageMaxStacks = FMath::Max(0, MaxStacks);
	BloodRageDuration = Duration;
	BloodRageArmorPerStack = ArmorPerStack;
	bGoldenBloodRage = bGolden;
	BloodRageStacks = FMath::Min(BloodRageStacks, BloodRageMaxStacks);
	ForceNetUpdate();
}

void AYSPlayerCharacter::AddBloodRageStack()
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!HasAuthority() || BloodRageMaxStacks <= 0 || !GameState)
	{
		return;
	}
	BloodRageStacks = FMath::Min(BloodRageStacks + 1, BloodRageMaxStacks);
	BloodRageExpireServerTime = GameState->GetServerWorldTimeSeconds() + BloodRageDuration;
	ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("[YSBloodRage] %s : %d / %d stack(s), armure %.0f%s"), *GetName(), BloodRageStacks, BloodRageMaxStacks,
		Armor + BloodRageStacks * BloodRageArmorPerStack, IsBloodRageReady() ? TEXT(", prochaine attaque critique") : TEXT(""));
}

bool AYSPlayerCharacter::ConsumeBloodRageCritical()
{
	if (!HasAuthority() || !IsBloodRageReady())
	{
		return false;
	}
	BloodRageStacks = 0;
	ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("[YSBloodRage] %s : rage au maximum, attaque critique (stacks consommées)"), *GetName());
	return true;
}

float AYSPlayerCharacter::GetBloodRageTimeRatio() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (BloodRageStacks <= 0 || !GameState)
	{
		return 0.0f;
	}
	if (bGoldenBloodRage || BloodRageDuration <= 0.0f)
	{
		return 1.0f;
	}
	return FMath::Clamp(static_cast<float>((BloodRageExpireServerTime - GameState->GetServerWorldTimeSeconds()) / BloodRageDuration), 0.0f, 1.0f);
}

void AYSPlayerCharacter::DebugSetBloodRageStacks(int32 Stacks)
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!HasAuthority() || !GameState)
	{
		return;
	}
	BloodRageStacks = FMath::Clamp(Stacks, 0, BloodRageMaxStacks);
	BloodRageExpireServerTime = GameState->GetServerWorldTimeSeconds() + BloodRageDuration;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSBloodRage] %s : stacks fixées à %d / %d"), *GetName(), BloodRageStacks, BloodRageMaxStacks);
}

float AYSPlayerCharacter::GetBloodlustTimeRatio() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (BloodlustStacks <= 0 || BloodlustDuration <= 0.0f || !GameState)
	{
		return 0.0f;
	}
	return FMath::Clamp(static_cast<float>((BloodlustExpireServerTime - GameState->GetServerWorldTimeSeconds()) / BloodlustDuration), 0.0f, 1.0f);
}

float AYSPlayerCharacter::GetBloodlustCooldownFactor() const
{
	// garde-fou : jamais une recharge nulle
	return FMath::Max(0.1f, 1.0f - BloodlustStacks * BloodlustAtkSpeedPerStack);
}

bool AYSPlayerCharacter::HasUpgrade(const UYSUpgrade* Upgrade) const
{
	return Upgrade && UpgradeProgress.ContainsByPredicate([this, Upgrade](const FYSUpgradeProgress& Entry)
	{
		return Entry.Stacks > 0 && UpgradePool.IsValidIndex(Entry.PoolIndex) && UpgradePool[Entry.PoolIndex] == Upgrade;
	});
}

float AYSPlayerCharacter::ComputePushbackForce(float Damage, float TargetMaxHealth) const
{
	// réglages du set d'arme s'il en fixe, sinon ceux du personnage
	if (WeaponSet && WeaponSet->bOverridePushback)
	{
		return YSCombat::ComputePushbackForce(Damage, TargetMaxHealth, WeaponSet->PushbackMinDamageRatio, WeaponSet->PushbackMaxDamageRatio,
			WeaponSet->PushbackMinForce, WeaponSet->PushbackMaxForce);
	}
	return YSCombat::ComputePushbackForce(Damage, TargetMaxHealth, PushbackMinDamageRatio, PushbackMaxDamageRatio, PushbackMinForce, PushbackMaxForce);
}

float AYSPlayerCharacter::GetPushbackVerticalForce() const
{
	return WeaponSet && WeaponSet->bOverridePushback ? WeaponSet->PushbackVerticalForce : PushbackVerticalForce;
}

float AYSPlayerCharacter::GetBaseMaxHealth() const
{
	return WeaponSet && WeaponSet->bOverrideMaxHealth ? WeaponSet->MaxHealth : MaxHealth;
}

float AYSPlayerCharacter::GetBaseCriticalDamageMultiplier() const
{
	const float Override = Passive ? Passive->GetCriticalDamageMultiplierOverride() : 0.0f;
	return Override > 0.0f ? Override : CriticalDamageMultiplier;
}

float AYSPlayerCharacter::GetArmorPercent() const
{
	return Passive ? Passive->GetArmorPercent() : 0.0f;
}

void AYSPlayerCharacter::SetPassive(UYSPassive* NewPassive)
{
	if (HasAuthority())
	{
		Passive = NewPassive;
		UpdateMaxWalkSpeed();
		ForceNetUpdate();
	}
}

float AYSPlayerCharacter::GetUnmodifiedWalkSpeed() const
{
	// passif Larbin : pas de ralentissement en portant un objet
	const bool bCarrySlowdown = IsValid(CarriedObject) && !(Passive && Passive->IgnoresCarrySlowdown());
	const float CarryMultiplier = bCarrySlowdown ? CarriedObject->GetCarrierSpeedMultiplier() : 1.0f;
	return MoveSpeed * MoveSpeedMultiplier * CarryMultiplier;
}

void AYSPlayerCharacter::UpdateMaxWalkSpeed()
{
	// serveur et client propriétaire doivent avoir la même vitesse (prédiction de mouvement)
	float ActionMultiplier = 1.0f;
	ForEachHeldAction([&ActionMultiplier](const UYSAbilityComponent* Action) { ActionMultiplier *= Action->GetMoveSpeedMultiplier(); });
	// attaque auto en train de frapper (joueurs) : ralentissement le plus fort des armes, appliqué une seule fois (akimbo)
	if (UsesHeldAutoAttack())
	{
		float AutoAttackMultiplier = 1.0f;
		ForEachHeldAction([&AutoAttackMultiplier](const UYSAbilityComponent* Action)
		{
			float Multiplier = 1.0f;
			if (Action->GetAutoAttackSlowdown(Multiplier))
			{
				AutoAttackMultiplier = FMath::Min(AutoAttackMultiplier, Multiplier);
			}
		});
		ActionMultiplier *= AutoAttackMultiplier;
	}
	// nage : Swim Speed Multiplier, ou celui du passif (Larbin : nage moins ralentie)
	const float PassiveSwim = Passive ? Passive->GetSwimSpeedMultiplierOverride() : 0.0f;
	const float SwimMultiplier = bSwimming ? (PassiveSwim > 0.0f ? PassiveSwim : SwimSpeedMultiplier) : 1.0f;
	GetCharacterMovement()->MaxWalkSpeed = GetUnmodifiedWalkSpeed() * ActionMultiplier * SwimMultiplier;
}

bool AYSPlayerCharacter::IsMoveInputOverridden() const
{
	bool bOverridden = false;
	ForEachHeldAction([&bOverridden](const UYSAbilityComponent* Action) { bOverridden = bOverridden || Action->OverridesMoveInput(); });
	return bOverridden;
}

bool AYSPlayerCharacter::GetLockedAimDirection(FVector& OutDirection) const
{
	// à la barre : tourné vers la roue
	if (IsValid(PilotedHelm))
	{
		OutDirection = PilotedHelm->GetPilotForward();
		return true;
	}

	bool bLocked = false;
	ForEachHeldAction([&bLocked, &OutDirection](const UYSAbilityComponent* Action) { bLocked = bLocked || Action->GetLockedAimDirection(OutDirection); });
	return bLocked;
}

float AYSPlayerCharacter::GetFoeAttractionMultiplier() const
{
	return IsValid(CarriedObject) ? CarriedObject->GetFoeAttractionMultiplier() : 1.0f;
}

AYSInteractiveObject* AYSPlayerCharacter::FindInteractTarget() const
{
	const UWorld* World = GetWorld();
	if (!World || !IsAlive() || IsCarrying() || IsPiloting())
	{
		return nullptr;
	}

	// le plus proche devant le personnage (regard = direction du curseur), à portée
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const float MinCos = FMath::Cos(FMath::DegreesToRadians(InteractionAngle));
	AYSInteractiveObject* Best = nullptr;
	float BestDistSq = FMath::Square(InteractionDistance);
	for (TActorIterator<AYSInteractiveObject> It(World); It; ++It)
	{
		AYSInteractiveObject* Object = *It;
		if (!Object->CanInteract(this))
		{
			continue;
		}

		const FVector ToObject = (Object->GetActorLocation() - GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
		const float DistSq = ToObject.SizeSquared();
		if (DistSq > BestDistSq)
		{
			continue;
		}

		// collé au personnage : toujours « devant »
		if (DistSq > FMath::Square(GetCapsuleComponent()->GetScaledCapsuleRadius()) && FVector::DotProduct(ToObject.GetSafeNormal(), Forward) < MinCos)
		{
			continue;
		}

		Best = Object;
		BestDistSq = DistSq;
	}
	return Best;
}

void AYSPlayerCharacter::Interact()
{
	if (HasAuthority())
	{
		InteractOnServer();
	}
	else
	{
		ServerInteract();
	}
}

void AYSPlayerCharacter::ServerInteract_Implementation()
{
	InteractOnServer();
}

void AYSPlayerCharacter::InteractOnServer()
{
	if (IsCarrying())
	{
		DropCarriedObject(TEXT("touche E"));
		return;
	}
	if (IsPiloting())
	{
		StopPiloting(TEXT("touche E"));
		return;
	}

	if (AYSInteractiveObject* Target = FindInteractTarget())
	{
		Target->Interact(this);
	}
}

void AYSPlayerCharacter::StartCarrying(AYSInteractiveObject* Object)
{
	if (!HasAuthority() || IsCarrying() || !IsValid(Object))
	{
		return;
	}

	// les armes sont désactivées : actions en cours interrompues (anim du sabre, main suivante de l'akimbo)
	CancelAllActions();

	CarriedObject = Object;
	Object->SetCarrier(this);
	ApplyCarryVisuals();
	UpdateMaxWalkSpeed();
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSCarry] %s prend %s (vitesse %.0f%s)"), *GetName(), *Object->GetName(), GetCharacterMovement()->MaxWalkSpeed,
		Passive && Passive->IgnoresCarrySlowdown() ? TEXT(", passif : pas de ralentissement") : TEXT(""));
}

void AYSPlayerCharacter::DropCarriedObject(const TCHAR* Reason)
{
	AYSInteractiveObject* Object = CarriedObject;
	if (!HasAuthority() || !IsValid(Object))
	{
		return;
	}

	// reposé au sol devant le personnage
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	FVector Ground = AYSInteractiveObject::FindGround(GetWorld(), GetActorLocation() + Forward * Object->GetDropDistance(), this);
	// lâché dans la mer (porteur tombé à l'eau, mort au bord) : sinon perdu pour de bon -> dernier sol sûr du porteur ;
	// un objet qui flotte (cadavre) reste à la surface
	const AYSOcean* Ocean = AYSOcean::Find(GetWorld());
	const FVector DropPoint = GetActorLocation() + Forward * Object->GetDropDistance();
	if (Object->FloatsOnWater() && Ocean && (Ground.Equals(DropPoint) || AYSOcean::IsUnderwater(GetWorld(), Ground)))
	{
		Ground = FVector(DropPoint.X, DropPoint.Y, Ocean->GetSeaLevel());
		UE_LOG(LogTemp, Log, TEXT("[YSCarry] %s : %s lâché dans la mer, flotte en %s"), *GetName(), *Object->GetName(), *Ground.ToCompactString());
	}
	else if (!AYSInteractiveObject::IsSafeDropLocation(GetWorld(), Ground))
	{
		UE_LOG(LogTemp, Log, TEXT("[YSCarry] %s : %s lâché hors du sol (%s), reposé au dernier sol sûr %s"), *GetName(), *Object->GetName(),
			*Ground.ToCompactString(), *Object->GetLastSafeLocation().ToCompactString());
		Ground = Object->GetLastSafeLocation();
	}
	CarriedObject = nullptr;
	Object->SetCarrier(nullptr, Ground, GetActorRotation().Yaw);
	ApplyCarryVisuals();
	UpdateMaxWalkSpeed();
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSCarry] %s repose %s (%s)"), *GetName(), *Object->GetName(), Reason);

	// près d'un Brooker : racheté (doublons pour l'équipe)
	AYSBrookerNPC::TrySell(Object);
}

void AYSPlayerCharacter::OnRep_CarriedObject()
{
	ApplyCarryVisuals();
	UpdateMaxWalkSpeed();
}

void AYSPlayerCharacter::ApplyCarryVisuals()
{
	for (AYSWeapon* Weapon : { GetLeftHandWeapon(), GetRightHandWeapon() })
	{
		if (Weapon)
		{
			Weapon->SetActorHiddenInGame(AreWeaponsStowed());
		}
	}
}

void AYSPlayerCharacter::CancelAllActions()
{
	GetWorldTimerManager().ClearTimer(AkimboTimer);
	for (const AYSWeapon* Weapon : { GetLeftHandWeapon(), GetRightHandWeapon() })
	{
		if (Weapon)
		{
			TInlineComponentArray<UYSAbilityComponent*> Actions(Weapon);
			for (UYSAbilityComponent* Action : Actions)
			{
				Action->CancelAction();
			}
		}
	}
}

void AYSPlayerCharacter::StartPiloting(AYSShipHelm* Helm)
{
	if (!HasAuthority() || IsPiloting() || IsCarrying() || !IsValid(Helm))
	{
		return;
	}

	// armes rangées : actions en cours interrompues
	CancelAllActions();
	PilotedHelm = Helm;
	Helm->SetPilot(this);
	ApplyCarryVisuals();
	UpdatePiloting();
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSHelm] %s prend la barre de %s"), *GetName(), Helm->GetShip() ? *Helm->GetShip()->GetName() : TEXT("?"));
}

void AYSPlayerCharacter::StopPiloting(const TCHAR* Reason)
{
	AYSShipHelm* Helm = PilotedHelm;
	if (!HasAuthority() || !Helm)
	{
		return;
	}

	PilotedHelm = nullptr;
	if (IsValid(Helm) && Helm->GetPilot() == this)
	{
		Helm->SetPilot(nullptr);
	}
	HelmInput = LastSentHelmInput = FVector2D::ZeroVector;
	ApplyCarryVisuals();
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSHelm] %s quitte la barre (%s)"), *GetName(), Reason);
}

void AYSPlayerCharacter::DebugTakeHelm(int32 Skip)
{
	if (!HasAuthority())
	{
		ServerDebugTakeHelm(Skip);
		return;
	}
	ServerDebugTakeHelm_Implementation(Skip);
}

void AYSPlayerCharacter::ServerDebugTakeHelm_Implementation(int32 Skip)
{
	TArray<AYSShipHelm*> Helms;
	for (TActorIterator<AYSShipHelm> It(GetWorld()); It; ++It)
	{
		if (It->CanInteract(this))
		{
			Helms.Add(*It);
		}
	}
	const FVector Location = GetActorLocation();
	Helms.Sort([&Location](const AYSShipHelm& A, const AYSShipHelm& B) { return FVector::DistSquared(A.GetActorLocation(), Location) < FVector::DistSquared(B.GetActorLocation(), Location); });
	AYSShipHelm* Best = Helms.IsValidIndex(Skip) ? Helms[Skip] : nullptr;
	if (!Best)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSHelm] %s : aucune barre libre de bateau pilotable"), *GetName());
		return;
	}
	StartPiloting(Best);
}

void AYSPlayerCharacter::OnRep_PilotedHelm()
{
	HelmInput = LastSentHelmInput = FVector2D::ZeroVector;
	ApplyCarryVisuals();
	UpdateMaxWalkSpeed();
}

void AYSPlayerCharacter::ServerSetHelmInput_Implementation(FVector2D Input)
{
	AYSPirateShip* Ship = IsValid(PilotedHelm) ? PilotedHelm->GetShip() : nullptr;
	if (Ship && Ship->GetHelm() == PilotedHelm)
	{
		// Z / S = avancer / reculer, D / Q = tourner à droite / à gauche
		Ship->SetPilotInput(Input.Y, Input.X);
	}
}

void AYSPlayerCharacter::UpdatePiloting()
{
	AYSShipHelm* Helm = PilotedHelm;
	if (!IsValid(Helm))
	{
		return;
	}

	// tombé à l'eau, mort : la barre est lâchée
	if (HasAuthority() && (bSwimming || !IsAlive()))
	{
		StopPiloting(bSwimming ? TEXT("dans l'eau") : TEXT("mort"));
		return;
	}

	// gardé derrière la roue (le pont l'emporte déjà : seulement pour le placer et corriger une dérive) ; serveur et machine qui
	// le contrôle, les autres reçoivent sa position
	if (HasAuthority() || IsLocallyControlled())
	{
		const FVector Target = Helm->GetPilotFeetLocation() + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		if (!GetActorLocation().Equals(Target, 5.0f))
		{
			SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
			GetCharacterMovement()->Velocity = FVector::ZeroVector;
		}
	}

	// commandes (Move ne reçoit rien quand aucune touche n'est enfoncée) envoyées au changement
	if (IsLocallyControlled() && IsPlayerControlled())
	{
		const FVector2D Input = GFrameCounter - LastHelmInputFrame <= 1 ? HelmInput : DebugHelmInput;
		if (!Input.Equals(LastSentHelmInput, 0.01f))
		{
			LastSentHelmInput = Input;
			if (HasAuthority())
			{
				ServerSetHelmInput_Implementation(Input);
			}
			else
			{
				ServerSetHelmInput(Input);
			}
		}
	}
}

void AYSPlayerCharacter::UpdateSwimming(float DeltaSeconds)
{
	// pendant que l'île coule, plus de sol de nage : tout le monde coule avec elle (seuls ceux du bateau survivent)
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	const bool bFloorActive = !(YSGameState && YSGameState->IsIslandSinking());
	if (bFloorActive != bSwimFloorBlocked)
	{
		bSwimFloorBlocked = bFloorActive;
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_YSWater, bFloorActive ? ECR_Block : ECR_Ignore);
	}

	// nage : pieds presque à la profondeur du sol de nage (même calcul sur toutes les machines, depuis la position)
	const AYSOcean* Ocean = AYSOcean::Find(GetWorld());
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const bool bNowSwimming = Ocean && bFloorActive && IsAlive() && AYSOcean::GetWaterDepth(GetWorld(), Feet) >= Ocean->GetSwimFloorDepth() - SwimEnterTolerance;
	if (Ocean)
	{
		SwimSurfaceZ = Ocean->GetSeaLevel();
	}
	if (bNowSwimming != bSwimming)
	{
		bSwimming = bNowSwimming;
		ApplyCarryVisuals();
		UpdateMaxWalkSpeed();
		if (bSwimming && HasAuthority())
		{
			// armes rangées : actions en cours interrompues
			CancelAllActions();
		}
		UE_LOG(LogTemp, Verbose, TEXT("[YSSwim] %s (%s) %s à (%.0f, %.0f, %.0f), vitesse %.0f"), *GetName(), HasAuthority() ? TEXT("serveur") : TEXT("client"),
			bSwimming ? TEXT("nage") : TEXT("sort de l'eau"), Feet.X, Feet.Y, Feet.Z, GetCharacterMovement()->MaxWalkSpeed);
	}
	SwimBlend = SwimBlendSpeed > 0.0f ? FMath::FInterpTo(SwimBlend, bSwimming ? 1.0f : 0.0f, DeltaSeconds, SwimBlendSpeed) : (bSwimming ? 1.0f : 0.0f);
}

float AYSPlayerCharacter::GetSwimBodyOffset() const
{
	const UStaticMesh* BodyStaticMesh = BodyMesh->GetStaticMesh();
	if (SwimBlend <= KINDA_SMALL_NUMBER || !BodyStaticMesh)
	{
		return 0.0f;
	}
	// bas et hauteur du corps sans inclinaison (relatifs au pivot, à l'échelle du personnage)
	const FBox Box = BodyStaticMesh->GetBoundingBox();
	const FTransform Relative = BodyMesh->GetRelativeTransform();
	const float PivotScale = BodyPivot->GetComponentScale().Z;
	const float Bottom = GetActorLocation().Z + (Relative.GetLocation().Z + Box.Min.Z * Relative.GetScale3D().Z) * PivotScale;
	const float Height = Box.GetSize().Z * Relative.GetScale3D().Z * PivotScale;
	return (SwimSurfaceZ - SwimImmersion * Height - Bottom) * SwimBlend;
}

void AYSPlayerCharacter::UpdateWeaponColoration()
{
	if (!bWeaponColoration)
	{
		return;
	}

	// set différent de celui du Blueprint du personnage = obtenu par une upgrade de classe (joueurs seulement : le set d'un
	// minion est tiré au hasard)
	const AYSPlayerCharacter* Defaults = GetClass()->GetDefaultObject<AYSPlayerCharacter>();
	const bool bClassUpgraded = GetPlayerState() && WeaponSet && Defaults && Defaults->WeaponSet && WeaponSet != Defaults->WeaponSet;

	if (AYSWeapon* MainWeapon = GetMainHandWeapon())
	{
		MainWeapon->SetColoration(bClassUpgraded ? EYSWeaponColoration::ClassUpgrade : EYSWeaponColoration::MainHand);
	}
	if (AYSWeapon* OffWeapon = GetOffHandWeapon())
	{
		OffWeapon->SetColoration(bClassUpgraded ? EYSWeaponColoration::ClassUpgrade : EYSWeaponColoration::OffHand);
	}
}

void AYSPlayerCharacter::UpdateInteractPrompt()
{
	// joueur local seulement (un minion, piloté par l'IA, est « contrôlé localement » sur le serveur)
	if (!IsLocallyControlled() || !IsPlayerControlled())
	{
		return;
	}

	AYSInteractiveObject* Target = FindInteractTarget();
	if (Target != FocusedInteractable.Get())
	{
		if (FocusedInteractable.IsValid())
		{
			FocusedInteractable->SetPromptVisible(false);
		}
		if (Target)
		{
			Target->SetPromptVisible(true);
		}
		FocusedInteractable = Target;
	}
}

void AYSPlayerCharacter::ForEachHeldAction(TFunctionRef<void(const UYSAbilityComponent*)> Function) const
{
	for (const AYSWeapon* Weapon : GetHeldWeapons())
	{
		TInlineComponentArray<UYSAbilityComponent*> Actions(Weapon);
		for (const UYSAbilityComponent* Action : Actions)
		{
			Function(Action);
		}
	}
}

bool AYSPlayerCharacter::IsInvulnerable() const
{
	bool bInvulnerable = false;
	ForEachHeldAction([&bInvulnerable](const UYSAbilityComponent* Action) { bInvulnerable = bInvulnerable || Action->IsGrantingInvulnerability(); });
	return bInvulnerable;
}

bool AYSPlayerCharacter::IsPassingThroughPawns() const
{
	return GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore;
}

void AYSPlayerCharacter::SetCriticalDamageAdd(float Value)
{
	CriticalDamageAdd = Value;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSCritical] %s : Dégât critique +%.3f -> dégâts critiques x%.3f"), *GetName(), CriticalDamageAdd, GetCriticalDamageMultiplier());
}

void AYSPlayerCharacter::RecomputeCriticalDamageBonus()
{
	// chaque bonus des upgrades de stat qui ont un effet critique (Atk speed, Shoot system), selon sa rareté
	float Bonus = 0.0f;
	for (const FYSUpgradeProgress& Entry : UpgradeProgress)
	{
		const UYSStatUpgrade* Stat = UpgradePool.IsValidIndex(Entry.PoolIndex) ? Cast<UYSStatUpgrade>(UpgradePool[Entry.PoolIndex].Get()) : nullptr;
		if (!Stat)
		{
			continue;
		}
		for (const EYSUpgradeRarity Rarity : Entry.BonusRarities)
		{
			Bonus += Stat->GetCriticalDamageBonus(Rarity);
		}
	}
	if (!FMath::IsNearlyEqual(Bonus, CriticalDamageBonus))
	{
		CriticalDamageBonus = Bonus;
		UE_LOG(LogTemp, Log, TEXT("[YSCritical] %s : bonus critique %.2f -> dégâts critiques x%.3f"), *GetName(), CriticalDamageBonus, GetCriticalDamageMultiplier());
	}
}
