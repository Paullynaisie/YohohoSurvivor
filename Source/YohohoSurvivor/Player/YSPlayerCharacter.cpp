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
#include "NPC/YSBrookerNPC.h"

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
	CameraBoom->TargetArmLength = 1400.0f;
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
	DOREPLIFETIME(AYSPlayerCharacter, MaxHealthMultiplier);
	DOREPLIFETIME(AYSPlayerCharacter, CooldownMultipliers);
	DOREPLIFETIME(AYSPlayerCharacter, Luck);
	DOREPLIFETIME(AYSPlayerCharacter, bLeftHanded);
	DOREPLIFETIME(AYSPlayerCharacter, WeaponSet);
	DOREPLIFETIME(AYSPlayerCharacter, CarriedObject);
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

	// Armor : chaque coup est réduit de l'armure (jusqu'à MinDamageAfterArmor) ; BloodRage ++ : armure en plus par stack de rage
	const float TotalArmor = Armor + BloodRageStacks * BloodRageArmorPerStack;
	if (TotalArmor > 0.0f)
	{
		Damage = FMath::Max(FMath::Min(Damage, MinDamageAfterArmor), Damage - TotalArmor);
	}

	// BloodRage : chaque coup reçu donne une stack de rage (après l'armure du coup)
	AddBloodRageStack();

	// dégâts toujours entiers (arrondis à l'entier le plus proche, après l'armure)
	Damage = FMath::RoundToFloat(Damage);
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
		Die();
		return Damage;
	}

	ApplyPushback(Damage, DamageEvent, EventInstigator, DamageCauser);
	return Damage;
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
	// l'objet porté tombe au sol
	DropCarriedObject(TEXT("mort"));

	// le joueur reste mort (pas de réapparition) mais compte toujours pour la taille des vagues
	if (AYSPlayerState* YSPlayerState = GetPlayerState<AYSPlayerState>())
	{
		YSPlayerState->SetDead();
	}

	// la caméra du joueur reste là où il est mort
	if (AYSPlayerController* PlayerController = Cast<AYSPlayerController>(GetController()))
	{
		PlayerController->ClientPlayerDied(TopDownCamera->GetComponentLocation(), TopDownCamera->GetComponentRotation(), TopDownCamera->FieldOfView);
	}

	// les armes (acteurs enfants) disparaissent avec le personnage
	Destroy();
}

void AYSPlayerCharacter::FellOutOfWorld(const UDamageType& DamageType)
{
	// le serveur décide de la mort (le personnage répliqué ne doit pas être détruit par un client)
	if (!HasAuthority() || IsActorBeingDestroyed())
	{
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSDeadZone] %s tombé sous la map (Z %.0f) : mort"), *GetName(), GetActorLocation().Z);
	Health = 0.0f;
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
	return Total;
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
	default:						return 0.0f;
	}
}

float AYSPlayerCharacter::GetRarityChance(EYSUpgradeRarity Rarity) const
{
	const float Bronze = FMath::Max(0.0f, BronzeChance - Luck);
	switch (Rarity)
	{
	case EYSUpgradeRarity::Bronze:	return Bronze;
	case EYSUpgradeRarity::Silver:	return SilverChance;
	case EYSUpgradeRarity::Gold:	return GoldChance + (BronzeChance - Bronze);
	default:						return 0.0f;
	}
}

void AYSPlayerCharacter::ApplyUpgrade(const FYSUpgradeOffer& Offer)
{
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

	// taille du personnage : échelle ABSOLUE (classe x CharacterScale), identique sur toutes les machines et dans l'éditeur
	const USceneComponent* DefaultRoot = GetClass()->GetDefaultObject<AYSPlayerCharacter>()->GetRootComponent();
	const FVector ScaledSize = (DefaultRoot ? DefaultRoot->GetRelativeScale3D() : FVector::OneVector) * CharacterScale;
	if (!GetActorScale3D().Equals(ScaledSize))
	{
		SetActorScale3D(ScaledSize);
	}

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

void AYSPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// appliqué sur le serveur ET les clients pour que la prédiction de mouvement reste cohérente
	UpdateMaxWalkSpeed();

	// réappliqué en jeu : la hurtbox ne dépend pas de réglages de collision sauvegardés dans le Blueprint
	YSCombat::ConfigureHurtbox(BodyMesh);

	if (HasAuthority())
	{
		Health = GetMaxHealth();
		// regen de base ; les upgrades Hps regen (RestoreUpgrades au changement de classe) s'y ajoutent
		HealthRegen = BaseHealthRegen;
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

	UpdateStrafeAnimation(DeltaSeconds);
	UpdateBodyPivot(DeltaSeconds);
	UpdateHands(DeltaSeconds);
	// les armes répliquées arrivent après le perso chez les clients : réappliqué tant qu'elles changent (sinon rien)
	UpdateWeaponColoration();
	UpdateDayNight(DeltaSeconds);
	UpdateNameplate();
	UpdateInteractPrompt();
	// vitesse : peut dépendre d'une action en cours (visée du Rolling shot) ; serveur et client propriétaire
	UpdateMaxWalkSpeed();

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
		// objet porté : la main est sur son point de placement, sans retard (l'objet suit déjà le corps)
		if (const USceneComponent* HandPoint = IsValid(CarriedObject) ? CarriedObject->GetHandPoint(bLeft) : nullptr)
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

		const FVector Target = PivotTransform.TransformPosition(GetHandTargetLocation(bLeft) + Sway.GetClampedToMaxSize(HandSwayAmplitude));

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
		// sans retard pour garder un geste brutal ; l'Ability d'abord (prioritaire). Les animations de l'autre main
		// (AnimatesOffHand) sont jouées avec l'Off Hand Socket, plus bas.
		float IdleWeight = 1.0f;
		FVector PoseLocation = FVector::ZeroVector;
		FQuat PoseRotation = FQuat::Identity;
		bool bSwinging = false;
		for (const UYSAbilityComponent* Action : { Weapon ? Weapon->GetActiveAction() : nullptr, Weapon ? Weapon->GetAutoAction() : nullptr })
		{
			if (!bSwinging && Action && !Action->AnimatesOffHand() && Action->EvaluateHandPose(IdleWeight, PoseLocation, PoseRotation))
			{
				bSwinging = true;
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
	// (après la main qui la tient : l'arme, attachée à cette main, l'a déjà suivie) ; pas pendant le port d'un objet
	if (!IsValid(CarriedObject))
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
	const float Hop = UpdateHop(DeltaSeconds, Velocity.Size2D());

	// pivot autour des pieds (bas de la capsule) pour que le personnage reste posé au sol, puis sautillement vertical
	const FVector ActorLocation = GetActorLocation();
	const FVector Feet = ActorLocation - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FVector PivotLocation = Feet + LeanQuat.RotateVector(ActorLocation - Feet) + FVector(0.0f, 0.0f, Hop);

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
	if (MoveAction && FireAction && InteractAction && DefaultMappingContext)
	{
		return;
	}

	MoveAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;

	FireAction = NewObject<UInputAction>(this, TEXT("IA_Fire"));
	FireAction->ValueType = EInputActionValueType::Boolean;

	InteractAction = NewObject<UInputAction>(this, TEXT("IA_Interact"));
	InteractAction->ValueType = EInputActionValueType::Boolean;

	DefaultMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Default"));
	DefaultMappingContext->MapKey(FireAction, EKeys::LeftMouseButton);
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
	// objet porté à deux mains : armes désactivées
	if (IsCarrying())
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
	const float HealthRatio = GetMaxHealth() > 0.0f ? Health / GetMaxHealth() : 1.0f;
	const FTransform SpawnTransform(FRotator(0.0f, GetActorRotation().Yaw, 0.0f), GetActorLocation());
	const FString OldName = GetName();
	const bool bWasLeftHanded = bLeftHanded;

	// l'objet porté reste au sol (le nouveau personnage a les mains libres)
	DropCarriedObject(TEXT("changement de classe"));

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
		NewCharacter->FinishSpawning(SpawnTransform);
	}
	if (!NewCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("[YSPlayerCharacter] Changement de classe impossible : %s n'a pas pu apparaître"), *NewClass->GetName());
		return;
	}

	OwningController->Possess(NewCharacter);
	NewCharacter->RestoreUpgrades(OldPool, OldProgress);
	NewCharacter->Health = FMath::Max(1.0f, HealthRatio * NewCharacter->GetMaxHealth());
	UE_LOG(LogTemp, Log, TEXT("[YSPlayerCharacter] Changement de classe : %s -> %s, set %s (%d upgrade(s) conservée(s), PV %.0f / %.0f)"),
		*OldName, *NewCharacter->GetName(), NewCharacter->WeaponSet ? *NewCharacter->WeaponSet->GetName() : TEXT("aucun"), NewCharacter->UpgradeProgress.Num(), NewCharacter->Health, NewCharacter->GetMaxHealth());
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
	if (!HasAuthority() || !Foe || !bMarkSpawnsMinion)
	{
		return;
	}
	// Cursed Captain Gun : un minion apparaît là où meurt le NME marqué (invocation de l'arme d'invocation tenue, sans dépasser le maximum)
	for (const AYSWeapon* Weapon : GetHeldWeapons())
	{
		TInlineComponentArray<UYSSummonCrewAbility*> Summons(Weapon);
		if (Summons.Num() > 0)
		{
			const AYSCaptainMinion* Minion = Summons[0]->SpawnMinionAt(Foe->GetActorLocation(), Foe->GetActorRotation().Yaw);
			UE_LOG(LogTemp, Verbose, TEXT("[YSMark] %s : NME marqué %s mort, minion %s"), *GetName(), *Foe->GetName(), Minion ? *Minion->GetName() : TEXT("non invoqué (maximum ou pas de sol)"));
			return;
		}
	}
}

void AYSPlayerCharacter::ForceAbility(const FYSForcedActivation& Forced)
{
	if (!HasAuthority() || !IsAlive() || IsCarrying())
	{
		return;
	}
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

float AYSPlayerCharacter::GetUnmodifiedWalkSpeed() const
{
	const float CarryMultiplier = IsValid(CarriedObject) ? CarriedObject->GetCarrierSpeedMultiplier() : 1.0f;
	return MoveSpeed * MoveSpeedMultiplier * CarryMultiplier;
}

void AYSPlayerCharacter::UpdateMaxWalkSpeed()
{
	// serveur et client propriétaire doivent avoir la même vitesse (prédiction de mouvement)
	float ActionMultiplier = 1.0f;
	ForEachHeldAction([&ActionMultiplier](const UYSAbilityComponent* Action) { ActionMultiplier *= Action->GetMoveSpeedMultiplier(); });
	GetCharacterMovement()->MaxWalkSpeed = GetUnmodifiedWalkSpeed() * ActionMultiplier;
}

bool AYSPlayerCharacter::IsMoveInputOverridden() const
{
	bool bOverridden = false;
	ForEachHeldAction([&bOverridden](const UYSAbilityComponent* Action) { bOverridden = bOverridden || Action->OverridesMoveInput(); });
	return bOverridden;
}

bool AYSPlayerCharacter::GetLockedAimDirection(FVector& OutDirection) const
{
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
	if (!World || !IsAlive() || IsCarrying())
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

	CarriedObject = Object;
	Object->SetCarrier(this);
	ApplyCarryVisuals();
	UpdateMaxWalkSpeed();
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSCarry] %s prend %s"), *GetName(), *Object->GetName());
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
	const FVector Ground = AYSInteractiveObject::FindGround(GetWorld(), GetActorLocation() + Forward * Object->GetDropDistance(), this);
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
			Weapon->SetActorHiddenInGame(IsCarrying());
		}
	}
}

void AYSPlayerCharacter::UpdateWeaponColoration()
{
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
