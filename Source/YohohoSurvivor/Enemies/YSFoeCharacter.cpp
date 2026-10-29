#include "Enemies/YSFoeCharacter.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
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
#include "Summons/YSCaptainMinion.h"
#include "EngineUtils.h"
#include "UI/YSHealthBarWidget.h"
#include "Components/ChildActorComponent.h"
#include "Weapons/YSWeapon.h"
#include "Weapons/YSWeaponSet.h"
#include "Abilities/YSAutoSwingAbility.h"
#include "Abilities/YSBossBigSlashAbility.h"
#include "Map/YSGroundZoneComponent.h"
#include "Map/YSOcean.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Enemies/YSFoeRangedAction.h"
#include "Upgrades/YSUpgrade.h"

namespace YSFoe
{
	/** Au-delà de cet écart (téléportation), une main rejoint sa cible sans retard */
	constexpr float HandSnapDistance = 300.0f;

	/** Ligne de visée du Skull Gun : hauteur au-dessus du sol */
	constexpr float AimLineHeight = 5.0f;

	/** Attaque à distance sans arme (repli) : hauteur de départ au-dessus du centre du NME */
	constexpr float DefaultAttackHeight = 80.0f;
}

AYSFoeCharacter::AYSFoeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// un contrôleur IA est nécessaire pour que le CharacterMovementComponent consomme l'input
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();

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
		// transform monde entièrement absolu : seul UpdateHands déplace la main (sinon chaque mouvement du NME la mettait à
		// jour deux fois, elle et toute son arme)
		OutSocket = CreateDefaultSubobject<USceneComponent>(SocketName);
		OutSocket->SetupAttachment(RootComponent);
		OutSocket->SetUsingAbsoluteLocation(true);
		OutSocket->SetUsingAbsoluteRotation(true);
		OutSocket->SetUsingAbsoluteScale(true);

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

	// Skull Gun : ligne de visée au sol (coordonnées monde), cachée hors visée
	AimLine = CreateDefaultSubobject<UYSGroundZoneComponent>(TEXT("AimLine"));
	AimLine->SetupAttachment(RootComponent);
	AimLine->SetUsingAbsoluteLocation(true);
	AimLine->SetVisibility(false);

	// marque de la Captain AMMO : disque au sol, en coordonnées monde (reposé sur le relief quand le NME bouge)
	MarkZone = CreateDefaultSubobject<UYSGroundZoneComponent>(TEXT("MarkZone"));
	MarkZone->SetupAttachment(RootComponent);
	MarkZone->SetUsingAbsoluteLocation(true);
	MarkZone->SetUsingAbsoluteRotation(true);
	MarkZone->SetUsingAbsoluteScale(true);
	MarkZone->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMarkMaterial(TEXT("/Game/Art/Materials/M_BrookerZone.M_BrookerZone"));
	if (DefaultMarkMaterial.Succeeded())
	{
		MarkMaterial = DefaultMarkMaterial.Object;
	}
}

void AYSFoeCharacter::SetMarked(bool bMarked)
{
	if (bIsMarked == bMarked)
	{
		return;
	}
	bIsMarked = bMarked;
	MarkZone->SetVisibility(bMarked);
	if (!bMarked)
	{
		MarkZone->ClearAllMeshSections();
		return;
	}
	LastMarkLocation = FVector(TNumericLimits<float>::Max());
	UpdateMark();
}

void AYSFoeCharacter::UpdateMark()
{
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight());
	if (FVector::DistSquared(Feet, LastMarkLocation) < FMath::Square(MarkRebuildDistance))
	{
		return;
	}
	LastMarkLocation = Feet;

	const float Radius = Capsule->GetScaledCapsuleRadius() * MarkRadiusScale;
	MarkZone->SetWorldLocation(Feet);
	MarkZone->BuildZone(Radius, YSFoe::AimLineHeight);
	if (MarkMaterial)
	{
		UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(MarkZone->GetMaterial(0));
		if (!Material)
		{
			Material = MarkZone->CreateDynamicMaterialInstance(0, MarkMaterial);
		}
		Material->SetVectorParameterValue(TEXT("Color"), MarkColor);
		Material->SetScalarParameterValue(TEXT("Glow"), MarkGlow);
		Material->SetScalarParameterValue(TEXT("FillOpacity"), MarkFillOpacity);
		Material->SetScalarParameterValue(TEXT("EdgeOpacity"), MarkEdgeOpacity);
		Material->SetScalarParameterValue(TEXT("Radius"), Radius);
		Material->SetScalarParameterValue(TEXT("EdgeWidth"), MarkEdgeWidth);
	}
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
	// perf : pur visuel local (rien n'est répliqué) ; hors écran les mains restent figées, SAUF sur le serveur pendant une
	// attaque de mêlée : la hitbox de l'arme suit la main (un client peut se battre hors de l'écran de l'hôte)
	if (bHandsInitialized && GetWorld() && GetWorld()->IsGameWorld() && !WasRecentlyRendered(HandsOffscreenFreezeDelay) && !(HasAuthority() && (IsMeleeAttacking() || bLocalBossDashActive || (GetRangedAction() && GetRangedAction()->IsAnimating()))))
	{
		return;
	}

	HandSwayTime += DeltaSeconds;
	const FTransform& BodyTransform = GetRootComponent()->GetComponentTransform();

	auto UpdateHand = [this, DeltaSeconds, &BodyTransform](USceneComponent* Hand, FVector& WorldLocation, bool bLeft, float NoiseSeed)
	{
		// oscillation (option) : bruit de Perlin sur chaque axe, limité à HandSwayAmplitude autour de la cible
		FVector Sway = FVector::ZeroVector;
		if (bHandSway && HandSwayAmplitude > 0.0f)
		{
			const float NoiseTime = HandSwayTime * HandSwaySpeed + NoiseSeed;
			Sway = FVector(
				FMath::PerlinNoise1D(NoiseTime),
				FMath::PerlinNoise1D(NoiseTime + 31.7f),
				FMath::PerlinNoise1D(NoiseTime + 67.3f)) * HandSwayAmplitude;
		}
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

		FVector FinalLocation = WorldLocation;
		FRotator HandRotator = GetHandTargetRotation(bLeft);

		// recul après un tir (NME shot) : comme le pistolet du joueur, main reculée le long de l'axe de l'arme et cabrée,
		// ajouté après le lissage pour rester sec
		const AYSWeapon* Weapon = bLeft ? GetLeftHandWeapon() : GetRightHandWeapon();
		const UYSFoeRangedAction* Ranged = Weapon ? Weapon->FindComponentByClass<UYSFoeRangedAction>() : nullptr;
		float RecoilPitch = 0.0f;
		float RecoilKickBack = 0.0f;
		if (Ranged && Ranged->GetRecoil(RecoilPitch, RecoilKickBack))
		{
			FinalLocation += BodyTransform.TransformVectorNoScale(-HandRotator.Vector() * RecoilKickBack);
			HandRotator.Pitch += RecoilPitch;
		}
		FQuat FinalRotation = HandRotator.Quaternion();

		// attaque animée de l'arme (Auto swing : même animation que le joueur ; lancer de la NME bomb) : la pose animée
		// remplace la pose normale sans retard ; poses exprimées pour la main droite, symétrisées pour la gauche
		// (Boss Big Slash d'abord : plus prioritaire que l'attaque de base)
		const UYSAutoSwingAbility* BossSwing = Weapon ? Cast<UYSAutoSwingAbility>(Weapon->GetActiveAction()) : nullptr;
		const UYSAutoSwingAbility* Swing = Weapon ? Cast<UYSAutoSwingAbility>(Weapon->GetAutoAction()) : nullptr;
		float IdleWeight = 1.0f;
		FVector PoseLocation = FVector::ZeroVector;
		FQuat PoseRotation = FQuat::Identity;
		if ((BossSwing && BossSwing->EvaluateHandPose(IdleWeight, PoseLocation, PoseRotation)) || (Swing && Swing->EvaluateHandPose(IdleWeight, PoseLocation, PoseRotation))
			|| (Ranged && Ranged->EvaluateHandPose(IdleWeight, PoseLocation, PoseRotation)))
		{
			if (bLeft)
			{
				PoseLocation.Y = -PoseLocation.Y;
				FRotator Mirrored = PoseRotation.Rotator();
				Mirrored.Yaw = -Mirrored.Yaw;
				Mirrored.Roll = -Mirrored.Roll;
				PoseRotation = Mirrored.Quaternion();
			}
			FinalLocation = FMath::Lerp(BodyTransform.TransformPosition(PoseLocation), FinalLocation, IdleWeight);
			FinalRotation = FQuat::Slerp(PoseRotation, FinalRotation, IdleWeight);
		}

		// transform absolu (rotation exprimée par rapport au corps, échelle du NME) ; inchangé = rien à mettre à jour
		const FTransform NewTransform(BodyTransform.GetRotation() * FinalRotation, FinalLocation, BodyTransform.GetScale3D());
		if (!Hand->GetComponentTransform().Equals(NewTransform, 0.01f))
		{
			Hand->SetWorldTransform(NewTransform);
		}
	};

	UpdateHand(LeftHand, LeftHandWorldLocation, true, 0.0f);
	UpdateHand(RightHand, RightHandWorldLocation, false, 100.0f);

	// arme à deux mains : l'autre main toujours sur le socket de l'arme, sans retard, même pendant le swing
	// (après la main qui la tient : l'arme, attachée à cette main, l'a déjà suivie)
	for (const bool bWeaponInLeft : { false, true })
	{
		const AYSWeapon* Weapon = bWeaponInLeft ? GetLeftHandWeapon() : GetRightHandWeapon();
		const USceneComponent* Socket = Weapon && Weapon->IsTwoHanded() ? Weapon->GetOffHandSocket() : nullptr;
		if (Socket)
		{
			USceneComponent* OtherHand = bWeaponInLeft ? RightHand : LeftHand;
			const FTransform SocketTransform(Socket->GetComponentQuat(), Socket->GetComponentLocation(), BodyTransform.GetScale3D());
			if (!OtherHand->GetComponentTransform().Equals(SocketTransform, 0.01f))
			{
				OtherHand->SetWorldTransform(SocketTransform);
			}
			(bWeaponInLeft ? RightHandWorldLocation : LeftHandWorldLocation) = Socket->GetComponentLocation();
		}
	}
	bHandsInitialized = true;
}

void AYSFoeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSFoeCharacter, MaxHealth);
	DOREPLIFETIME(AYSFoeCharacter, Health);
	DOREPLIFETIME(AYSFoeCharacter, CriticalReadyShooters);
	DOREPLIFETIME(AYSFoeCharacter, Level);
	DOREPLIFETIME(AYSFoeCharacter, bLeftHanded);
	DOREPLIFETIME(AYSFoeCharacter, Bump);
	DOREPLIFETIME(AYSFoeCharacter, Rise);
	DOREPLIFETIME(AYSFoeCharacter, Death);
	DOREPLIFETIME(AYSFoeCharacter, Rage);
	DOREPLIFETIME(AYSFoeCharacter, Shield);
	DOREPLIFETIME(AYSFoeCharacter, MaxShield);
	DOREPLIFETIME(AYSFoeCharacter, BossDash);
}

void AYSFoeCharacter::OnRep_Health()
{
	UpdateHealthBar();
}

void AYSFoeCharacter::OnRep_CriticalReadyShooters()
{
	UpdateHealthBar();
}

void AYSFoeCharacter::UpdateHealthBar()
{
	if (UYSHealthBarWidget* Bar = Cast<UYSHealthBarWidget>(HealthBar->GetUserWidgetObject()))
	{
		// élite : barre de vie dans le HUD (AYSPlayerController) et, en option, sous lui (même PV pleins) ; le point critique reste
		if (bIsElite)
		{
			Bar->SetHealth(bEliteHealthBarUnder ? Health : 0.0f, MaxHealth, bEliteHealthBarUnder);
			// bouclier de boss par-dessus la barre de vie
			Bar->SetShield(bEliteHealthBarUnder ? Shield : 0.0f, MaxShield);
		}
		else
		{
			Bar->SetHealth(Health, MaxHealth);
		}
		const APlayerController* LocalController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		const APlayerState* LocalState = LocalController ? LocalController->PlayerState.Get() : nullptr;
		Bar->SetCriticalReady(LocalState && CriticalReadyShooters.Contains(LocalState));
	}
}

float AYSFoeCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Marquage Critique ++ : le NME marqué subit plus de dégâts (toutes les sources) ; dégâts toujours entiers (arrondis à l'entier le plus proche)
	const float MarkMultiplier = HasAuthority() ? 1.0f + AYSPlayerCharacter::GetMarkStat(this, UYSMarkCriticalUpgrade::MarkDamageBonusStat) : 1.0f;
	float Incoming = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (!HasAuthority() || Health <= 0.0f || Incoming <= 0.0f)
	{
		return 0.0f;
	}
	// élite : le bouclier de boss prend les dégâts bruts ; sans bouclier, Boss armor (réduction et rage)
	const bool bShieldHit = UsesBossArmor() && HasBossShield();
	if (UsesBossArmor() && !bShieldHit)
	{
		Incoming = ApplyBossArmor(Incoming, DamageEvent, EventInstigator);
	}
	const float Damage = FMath::RoundToFloat(Incoming * MarkMultiplier);
	if (Damage <= 0.0f)
	{
		return 0.0f;
	}

	// Golden Marquage : chaque coup d'un joueur (pas d'un minion : sans PlayerState) sur le NME marqué raccourcit la recharge de son Ability
	const float CooldownRefund = AYSPlayerCharacter::GetMarkStat(this, UYSMarkCriticalUpgrade::MarkCooldownRefundStat);
	AYSPlayerCharacter* Attacker = EventInstigator ? Cast<AYSPlayerCharacter>(EventInstigator->GetPawn()) : nullptr;
	if (CooldownRefund > 0.0f && Attacker && Attacker->GetPlayerState())
	{
		Attacker->ReduceAbilityCooldown(CooldownRefund);
	}
	if (MarkMultiplier > 1.0f)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[YSMark] %s marqué : dégâts x%.2f -> %.0f"), *GetName(), MarkMultiplier, Damage);
	}

	// chiffre de dégâts chez tous les joueurs, au point d'impact (position du projectile)
	AYSGameState::ReportDamage(this, Damage, DamageCauser ? DamageCauser->GetActorLocation() : GetActorLocation(), EventInstigator, UYSCriticalDamageType::IsCritical(DamageEvent));

	// bouclier de boss : il prend tout le coup (pas de débordement sur les PV), pas de rage, pas de bump (pushback normal)
	if (bShieldHit)
	{
		Shield = FMath::Max(0.0f, Shield - Damage);
		UE_LOG(LogTemp, Verbose, TEXT("[YSBoss] %s : bouclier -%.0f -> %.0f / %.0f%s"), *GetName(), Damage, Shield, MaxShield, Shield <= 0.0f ? TEXT(", BRISÉ") : TEXT(""));
		UpdateHealthBar();
		ApplyPushback(Damage, DamageEvent, EventInstigator);
		return Damage;
	}

	Health = FMath::Max(0.0f, Health - Damage);

	// OnRep n'est pas appelé sur le serveur : mise à jour directe pour le joueur hôte
	UpdateHealthBar();

	if (Health <= 0.0f)
	{
		PendingDeathImpulse = ComputeDeathImpulse(DamageEvent, EventInstigator);
		Die();
		return Damage;
	}

	// élite : rage pleine ou PV sous le seuil -> bouclier de boss
	TryTriggerBossShield();

	// coup qui bumpe (Bumping Slash) : NME lancé en cloche ; un élite seulement si l'Ability le permet (Bump Elites), sinon pushback normal
	// ; jamais sous bouclier de boss ni pendant le Boss Big Slash
	const FYSBumpDamageEvent* BumpEventPtr = DamageEvent.IsOfType(FYSBumpDamageEvent::ClassID) ? static_cast<const FYSBumpDamageEvent*>(&DamageEvent) : nullptr;
	if (BumpEventPtr && (!bIsElite || BumpEventPtr->bBumpElites) && !HasBossShield() && !BossDash.bActive)
	{
		const FYSBumpDamageEvent& BumpEvent = *BumpEventPtr;
		StartBump(BumpEvent.ShotDirection, BumpEvent.Distance, BumpEvent.ApexHeight, BumpEvent.Duration, BumpEvent.SpinTurns);
		return Damage;
	}

	ApplyPushback(Damage, DamageEvent, EventInstigator);
	return Damage;
}

void AYSFoeCharacter::StartBump(const FVector& Direction, float Distance, float ApexHeight, float Duration, float SpinTurns)
{
	const FVector FlatDirection = Direction.GetSafeNormal2D();
	if (!HasAuthority() || FlatDirection.IsNearlyZero())
	{
		return;
	}
	// encore en train de sortir du sol : sorti tout de suite, le vol part du sol
	FinishRiseNow();

	// arrivée : sol sous le point visé (même hauteur de capsule qu'au départ) ; rien dessous (mer) : même altitude, il tombera
	const FVector Start = GetActorLocation();
	FVector Landing = Start + FlatDirection * Distance;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FoeBump), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Landing + FVector(0.0f, 0.0f, 1000.0f), Landing - FVector(0.0f, 0.0f, 5000.0f), ECC_WorldStatic, Params))
	{
		Landing.Z = Hit.ImpactPoint.Z + GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}

	const AGameStateBase* GameState = GetWorld()->GetGameState();
	Bump.bActive = true;
	Bump.Start = Start;
	Bump.Target = Landing;
	Bump.Duration = FMath::Max(Duration, 0.05f);
	Bump.ApexHeight = ApexHeight;
	Bump.SpinTurns = SpinTurns;
	Bump.StartYaw = GetActorRotation().Yaw;
	Bump.StartServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	++Bump.Counter;

	// arrêt de la visée en cours (Skull Gun) : le comportement reprend à l'atterrissage
	if (bAiming)
	{
		bAiming = false;
		MulticastAimEnded();
	}

	BeginBumpLocal();
	ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("[YSBump] %s bumpé de (%.0f, %.0f, %.0f) vers (%.0f, %.0f, %.0f) : %.0f cm en %.2f s, %.1f tour(s)"), *GetName(),
		Start.X, Start.Y, Start.Z, Landing.X, Landing.Y, Landing.Z, FVector::Dist2D(Start, Landing), Bump.Duration, SpinTurns);
}

void AYSFoeCharacter::OnRep_Bump()
{
	if (Bump.bActive && (!bLocalBumpActive || LocalBumpCounter != Bump.Counter))
	{
		BeginBumpLocal();
	}
	else if (!Bump.bActive && bLocalBumpActive)
	{
		EndBumpLocal();
	}
}

void AYSFoeCharacter::BeginBumpLocal()
{
	bLocalBumpActive = true;
	LocalBumpCounter = Bump.Counter;

	// attaque en cours arrêtée (élites compris) : coup d'épée, lancer de bombe ; armes locales, chaque machine arrête la sienne
	CancelWeaponAttacks();

	// position et rotation calculées par UpdateBump sur chaque machine : plus de mouvement du personnage ni de réplication
	// du mouvement pendant le vol (le serveur renvoie la position à l'atterrissage)
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->SetComponentTickEnabled(false);
	if (HasAuthority())
	{
		SetReplicateMovement(false);
	}
	UpdateBump();
}

void AYSFoeCharacter::CancelWeaponAttacks()
{
	TArray<UYSAutoSwingAbility*> MeleeActions;
	GetMeleeActions(MeleeActions);
	for (UYSAutoSwingAbility* Swing : MeleeActions)
	{
		Swing->StopActionLocally();
	}
	for (const AYSWeapon* Weapon : { GetLeftHandWeapon(), GetRightHandWeapon() })
	{
		if (UYSFoeRangedAction* Ranged = Weapon ? Weapon->FindComponentByClass<UYSFoeRangedAction>() : nullptr)
		{
			Ranged->CancelAttack();
		}
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSBump] %s (%s) : attaques arrêtées"), *GetName(), HasAuthority() ? TEXT("serveur") : TEXT("client"));
}

void AYSFoeCharacter::UpdateBump()
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const float Elapsed = static_cast<float>(Now - Bump.StartServerTime);
	if (Elapsed >= Bump.Duration)
	{
		EndBumpLocal();
		return;
	}

	SetActorLocation(YSCombat::GetBellLocation(Bump.Start, Bump.Target, Bump.Duration, Bump.ApexHeight, Elapsed));

	// rotation complète sur lui-même, basculé dans le sens du vol (le haut part vers l'avant du vol)
	const FVector Direction = (FVector(Bump.Target) - FVector(Bump.Start)).GetSafeNormal2D();
	const FVector Axis = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal();
	const float Angle = 2.0f * PI * Bump.SpinTurns * FMath::Clamp(Elapsed / Bump.Duration, 0.0f, 1.0f);
	const FQuat BaseRotation = FRotator(0.0f, Bump.StartYaw, 0.0f).Quaternion();
	SetActorRotation(Axis.IsNearlyZero() ? BaseRotation : FQuat(Axis, Angle) * BaseRotation);
}

void AYSFoeCharacter::EndBumpLocal()
{
	if (!bLocalBumpActive)
	{
		return;
	}
	bLocalBumpActive = false;

	SetActorLocationAndRotation(Bump.Target, FRotator(0.0f, Bump.StartYaw, 0.0f));
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->SetComponentTickEnabled(true);
	if (HasAuthority())
	{
		// pas de sol à l'arrivée (mer) : il tombe (KillZ) ; sinon il se pose et reprend son comportement
		Movement->Velocity = FVector::ZeroVector;
		Movement->SetMovementMode(MOVE_Falling);
		Bump.bActive = false;
		SetReplicateMovement(true);
		ForceNetUpdate();
		UE_LOG(LogTemp, Verbose, TEXT("[YSBump] %s atterrit en (%.0f, %.0f, %.0f)"), *GetName(), Bump.Target.X, Bump.Target.Y, Bump.Target.Z);
	}
}

void AYSFoeCharacter::ApplyPushback(float Damage, const FDamageEvent& DamageEvent, const AController* EventInstigator)
{
	// direction du coup portée par l'événement (hitbox de l'arme, vol du projectile) ; force réglée sur le personnage qui frappe
	// Boss Big Slash en cours : trajet calculé, pas de pushback
	const AYSPlayerCharacter* Attacker = EventInstigator ? Cast<AYSPlayerCharacter>(EventInstigator->GetPawn()) : nullptr;
	if (!Attacker || !DamageEvent.IsOfType(FPointDamageEvent::ClassID) || BossDash.bActive)
	{
		return;
	}
	const FVector Direction = static_cast<const FPointDamageEvent&>(DamageEvent).ShotDirection.GetSafeNormal2D();
	// pushback supplémentaire porté par le coup (ex. Mine repulsante), en plus du pushback naturel
	const float ExtraForce = DamageEvent.IsOfType(FYSPushbackDamageEvent::ClassID) ? static_cast<const FYSPushbackDamageEvent&>(DamageEvent).ExtraPushback : 0.0f;
	const float Force = Attacker->ComputePushbackForce(Damage, MaxHealth) + ExtraForce;
	if (Direction.IsNearlyZero() || Force <= 0.0f)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[YSPushback] %s : %.0f dégâts sur %.0f PV max (%.1f %%), pas de pushback"), *GetName(), Damage, MaxHealth, 100.0f * Damage / FMath::Max(MaxHealth, 1.0f));
		return;
	}

	// encore en train de sortir du sol : un coup qui pousse le sort tout de suite (comportement démarré)
	FinishRiseNow();

	// serveur : le mouvement est répliqué aux clients ; un peu de vertical : décolle du sol (moins de friction)
	LaunchCharacter(Direction * Force + FVector(0.0f, 0.0f, Attacker->GetPushbackVerticalForce()), true, true);

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
	// XP commune à toute l'équipe ; rien pour un NME mort dans l'eau (tombé dans la deadzone sous la mer, ou pieds sous la surface)
	if (AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>())
	{
		const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		const float WaterDepth = AYSOcean::GetWaterDepth(GetWorld(), Feet);
		// élite qui marche dans l'eau peu profonde (Elite Wade Depth) : XP gardée
		const float MaxWaterDepth = bIsElite ? FMath::Max(NoExperienceWaterDepth, EliteWadeDepth) : NoExperienceWaterDepth;
		if (bSkipDeathAnim || WaterDepth > MaxWaterDepth)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s meurt dans l'eau (%s, %.0f cm d'eau) : pas d'XP"), *GetName(), bSkipDeathAnim ? TEXT("deadzone") : TEXT("dans la mer"), WaterDepth);
		}
		else
		{
			GameMode->AddTeamExperience(GetExperienceReward());
		}

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

	// NME marqué (Cursed Captain Gun : un minion apparaît à sa place)
	for (TActorIterator<AYSPlayerCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetMarkedFoe() == this)
		{
			It->OnMarkedFoeKilled(this);
		}
	}

	// ragdoll : le corps vole puis rétrécit (UpdateDeath le détruit à la fin) ; deadzone : détruit tout de suite
	if (!bDeathRagdoll || bSkipDeathAnim)
	{
		Destroy();
		return;
	}
	Death.bDead = true;
	Death.Impulse = PendingDeathImpulse;
	ForceNetUpdate();
	BeginDeathLocal();
}

FVector AYSFoeCharacter::ComputeDeathImpulse(const FDamageEvent& DamageEvent, const AController* EventInstigator) const
{
	// force MAXIMALE du pushback du tueur (réglages de son personnage / set), dans la direction du coup
	const AYSPlayerCharacter* Attacker = EventInstigator ? Cast<AYSPlayerCharacter>(EventInstigator->GetPawn()) : nullptr;
	const FVector Direction = DamageEvent.IsOfType(FPointDamageEvent::ClassID) ? static_cast<const FPointDamageEvent&>(DamageEvent).ShotDirection.GetSafeNormal2D() : FVector::ZeroVector;
	float Force = 0.0f;
	float Up = DeathExtraUpImpulse;
	if (Attacker && !Direction.IsNearlyZero())
	{
		const float ExtraForce = DamageEvent.IsOfType(FYSPushbackDamageEvent::ClassID) ? static_cast<const FYSPushbackDamageEvent&>(DamageEvent).ExtraPushback : 0.0f;
		Force = Attacker->ComputePushbackForce(MaxHealth, MaxHealth) + ExtraForce;
		Up += Attacker->GetPushbackVerticalForce();
	}
	return (Direction * Force + FVector(0.0f, 0.0f, Up)) * DeathImpulseMultiplier;
}

void AYSFoeCharacter::OnRep_Death()
{
	if (Death.bDead)
	{
		BeginDeathLocal();
	}
}

void AYSFoeCharacter::BeginDeathLocal()
{
	if (bLocalDeathStarted)
	{
		return;
	}
	bLocalDeathStarted = true;
	DeathStartTime = GetWorld()->GetTimeSeconds();

	// plus de gameplay : sans tag il n'est plus ciblé (visée assistée, ricochet, minions...), sans collision de requête il
	// n'est plus touché (hurtbox, hitbox), plus de mouvement ni de comportement
	GameplayTags.Reset();
	SetMarked(false);
	AimLine->SetVisibility(false);
	bAiming = false;
	bLocalBumpActive = false;
	bLocalRiseActive = false;
	bLocalBossDashActive = false;
	HealthBar->SetVisibility(false);
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->DisableMovement();
	Movement->SetComponentTickEnabled(false);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (HasAuthority())
	{
		SetReplicateMovement(false);
		DetachFromControllerPendingDestroy();
	}

	// armes : actions arrêtées (pas de bombe lâchée ni de coup après la mort)
	for (AYSWeapon* Weapon : { GetLeftHandWeapon(), GetRightHandWeapon() })
	{
		if (!Weapon)
		{
			continue;
		}
		for (UActorComponent* Component : Weapon->GetComponents())
		{
			GetWorldTimerManager().ClearAllTimersForObject(Component);
			Component->SetComponentTickEnabled(false);
		}
	}

	// tout le visuel suit le corps (mains, armes, visage), qui part en physique : ne bloque que le décor (pas les persos)
	for (USceneComponent* Component : { static_cast<USceneComponent*>(LeftHand), static_cast<USceneComponent*>(RightHand), static_cast<USceneComponent*>(FaceDecal) })
	{
		Component->SetUsingAbsoluteLocation(false);
		Component->SetUsingAbsoluteRotation(false);
		Component->SetUsingAbsoluteScale(false);
		Component->AttachToComponent(BodyMesh, FAttachmentTransformRules::KeepWorldTransform);
	}
	BodyMesh->SetGenerateOverlapEvents(false);
	BodyMesh->SetCollisionObjectType(ECC_PhysicsBody);
	BodyMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	BodyMesh->SetSimulatePhysics(true);
	BodyMesh->SetPhysicsLinearVelocity(Death.Impulse);
	BodyMesh->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * DeathAngularImpulse);
	DeathBodyScale = BodyMesh->GetComponentScale();
	UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s (%s) meurt : ragdoll à (%.0f, %.0f, %.0f) cm/s"), *GetName(), HasAuthority() ? TEXT("serveur") : TEXT("client"),
		Death.Impulse.X, Death.Impulse.Y, Death.Impulse.Z);
}

void AYSFoeCharacter::UpdateDeath()
{
	// après Death Scale Delay, le corps (et tout ce qui le suit) rétrécit jusqu'à 0, puis le NME est détruit
	const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - DeathStartTime);
	const float Alpha = FMath::Clamp((Elapsed - DeathScaleDelay) / FMath::Max(DeathScaleDuration, 0.01f), 0.0f, 1.0f);
	if (Alpha > 0.0f)
	{
		BodyMesh->SetWorldScale3D(DeathBodyScale * FMath::Max(1.0f - Alpha, 0.01f));
	}
	if (Alpha >= 1.0f)
	{
		BodyMesh->SetVisibility(false, true);
		if (HasAuthority() && !IsActorBeingDestroyed())
		{
			UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s : corps disparu, détruit"), *GetName());
			Destroy();
		}
	}
}

void AYSFoeCharacter::StartRise()
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

void AYSFoeCharacter::OnRep_Rise()
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

void AYSFoeCharacter::BeginRiseLocal()
{
	bLocalRiseActive = true;

	// comme le bump : position calculée par UpdateRise sur chaque machine, mouvement du personnage et sa réplication coupés
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->SetComponentTickEnabled(false);
	if (HasAuthority())
	{
		SetReplicateMovement(false);
	}
	UpdateRise();
}

void AYSFoeCharacter::UpdateRise()
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const float Alpha = FMath::Clamp(static_cast<float>(Now - Rise.StartServerTime) / FMath::Max(Rise.Duration, 0.05f), 0.0f, 1.0f);
	if (Alpha >= 1.0f)
	{
		EndRiseLocal();
		return;
	}
	// rapide au début, ralentit en arrivant au sol (Spawn Rise Ease Exponent)
	const float Eased = 1.0f - FMath::Pow(1.0f - Alpha, SpawnRiseEaseExponent);
	SetActorLocation(FVector(Rise.Ground) - FVector(0.0f, 0.0f, Rise.Depth * (1.0f - Eased)));
}

void AYSFoeCharacter::EndRiseLocal()
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
		// se pose (il était peut-être un peu au-dessus du sol) et démarre son comportement
		Movement->Velocity = FVector::ZeroVector;
		Movement->SetMovementMode(MOVE_Falling);
		Rise.bActive = false;
		SetReplicateMovement(true);
		ForceNetUpdate();
	}
}

void AYSFoeCharacter::FinishRiseNow()
{
	if (!HasAuthority() || !bLocalRiseActive)
	{
		return;
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s : coup pendant la sortie du sol, sorti tout de suite"), *GetName());
	EndRiseLocal();
}

void AYSFoeCharacter::FellOutOfWorld(const UDamageType& DamageType)
{
	// mort normale (XP, fin de la nuit pour l'élite) décidée par le serveur ; pas de ragdoll (sous la map)
	if (!HasAuthority() || IsActorBeingDestroyed() || IsDying())
	{
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSDeadZone] %s tombé sous la map (Z %.0f) : mort"), *GetName(), GetActorLocation().Z);
	Health = 0.0f;
	bSkipDeathAnim = true;
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

	// perf (foules) : aucun overlap entretenu par les NME. La hurtbox reste une cible de requêtes (coups, explosions, balles :
	// AYSProjectile::SweepPawnHits) ; la Hitbox de contact est testée à la main contre les joueurs (GetPlayersInHitbox)
	BodyMesh->SetGenerateOverlapEvents(false);
	Hitbox->SetGenerateOverlapEvents(false);
	Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCapsuleComponent()->SetGenerateOverlapEvents(false);
	// un NME ne tente pas de monter sur un autre (marches sur les capsules voisines : balayages en plus dans la foule)
	GetCapsuleComponent()->CanCharacterStepUpOn = ECB_No;
	// capsule sur le canal Foe : bloque tout comme une capsule Pawn, sauf les autres NME (séparation douce à la place)
	GetCapsuleComponent()->SetCollisionObjectType(ECC_YSFoe);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_YSFoe, bFoesBlockEachOther ? ECR_Block : ECR_Ignore);
	// mesh squelettique vide : ni tick, ni lissage réseau (le visuel est attaché à la capsule, le lissage ne bougeait que lui)
	GetMesh()->SetComponentTickEnabled(false);
	GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
	// ... et plus mis à jour quand le NME bouge
	GetMesh()->SetUsingAbsoluteLocation(true);
	GetMesh()->SetUsingAbsoluteRotation(true);
	GetMesh()->SetUsingAbsoluteScale(true);
	// contrôleur IA : ne sert qu'à faire consommer l'input au CharacterMovement (rotation pilotée par le NME)
	if (AController* FoeController = GetController())
	{
		FoeController->SetActorTickEnabled(false);
	}
	// ombres : seul le corps en projette (option)
	for (UStaticMeshComponent* HandMesh : { LeftHandMesh.Get(), RightHandMesh.Get() })
	{
		HandMesh->SetCastShadow(bHandsCastShadow);
		HandMesh->SetGenerateOverlapEvents(false);
	}
	// phase des vérifications « loin » répartie entre les NME
	FarTickCheckTimer = FMath::FRandRange(0.0f, FarTickCheckInterval);

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
		if (ForcedLevel != INDEX_NONE)
		{
			Level = FMath::Max(0, ForcedLevel);
			UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s : niveau imposé %d (%.0f PV)"), *GetName(), Level, (BaseHealth + HealthPerLevel * Level) * HealthMultiplier);
		}
		else if (bLevelFromDay && YSGameState)
		{
			Level = FMath::Max(0, YSGameState->GetDayNumber() + LevelFromDayOffset);
		}
		if (LevelBonus != 0)
		{
			Level = FMath::Max(0, Level + LevelBonus);
			UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s : niveau +%d -> %d"), *GetName(), LevelBonus, Level);
		}

		// statistiques du niveau (MaxHealth est répliqué pour les barres de vie) ; PV selon le nombre de joueurs de la partie
		const AYSGameMode* YSGameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
		const FYSPlayerCountScaling PlayerScaling = YSGameMode ? YSGameMode->GetPlayerCountScaling() : FYSPlayerCountScaling();
		const float PlayerHealthMultiplier = bIsElite ? PlayerScaling.EliteHealthMultiplier : PlayerScaling.HealthMultiplier;
		PlayerCountHealthMultiplier = PlayerHealthMultiplier;
		MaxHealth = FMath::RoundToFloat((BaseHealth + HealthPerLevel * Level) * HealthMultiplier * PlayerHealthMultiplier);
		UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s : niveau %d, %.0f PV (x%.2f pour %d joueur(s)), %.1f XP"), *GetName(), Level, MaxHealth, PlayerHealthMultiplier,
			YSGameMode ? YSGameMode->GetPlayersInMatchCount() : 0, GetExperienceReward());
		ContactDamage = (BaseContactDamage + ContactDamagePerLevel * Level) * DamageMultiplier;
		Health = MaxHealth;

		if (bIsElite && YSGameState)
		{
			YSGameState->SetCurrentElite(this);
		}

		// apparition : sort du sol avant de démarrer son comportement
		if (bSpawnRise)
		{
			StartRise();
		}
	}

	// élite : barre de vie sous lui (position relative, suit sa taille)
	if (bIsElite && bEliteHealthBarUnder)
	{
		HealthBar->SetRelativeLocation(EliteHealthBarLocation);
	}
	HealthBar->InitWidget();
	if (bIsElite && bEliteHealthBarUnder)
	{
		if (UYSHealthBarWidget* Bar = Cast<UYSHealthBarWidget>(HealthBar->GetUserWidgetObject()))
		{
			Bar->SetSizeMultiplier(EliteHealthBarScale);
		}
		UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s : barre de vie sous l'élite (x%.2f, %s)"), *GetName(), EliteHealthBarScale,
			HealthBar->GetUserWidgetObject() ? *HealthBar->GetUserWidgetObject()->GetClass()->GetName() : TEXT("AUCUN WIDGET"));
	}
	UpdateHealthBar();
}

void AYSFoeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// mort : corps en ragdoll qui rétrécit, plus rien d'autre
	if (bLocalDeathStarted)
	{
		UpdateDeath();
		return;
	}

	// perf : armes (re)créées optimisées, ticks ralentis loin des joueurs
	OptimizeWeapons();
	FarTickCheckTimer -= DeltaSeconds;
	if (FarTickCheckTimer <= 0.0f)
	{
		FarTickCheckTimer = FarTickCheckInterval;
		UpdateFarTicking();
	}

	// mains : visuel local sur toutes les machines
	UpdateHands(DeltaSeconds);

	// arme cachée un moment (bombe partie de la main, pas encore revenue) : visuel local
	for (AYSWeapon* Weapon : { GetLeftHandWeapon(), GetRightHandWeapon() })
	{
		const UYSFoeRangedAction* Ranged = Weapon ? Weapon->FindComponentByClass<UYSFoeRangedAction>() : nullptr;
		const bool bWeaponHidden = Ranged && Ranged->IsWeaponHidden();
		if (Weapon && Weapon->IsHidden() != bWeaponHidden)
		{
			Weapon->SetActorHiddenInGame(bWeaponHidden);
		}
	}

	// marque de la Captain AMMO : suit le NME (visuel local)
	if (bIsMarked)
	{
		UpdateMark();
	}

	// apparition : sort du sol (calculé sur chaque machine), ni comportement ni attaque
	if (bLocalRiseActive)
	{
		UpdateRise();
		return;
	}

	// bump (Bumping Slash) : vol en cloche calculé sur chaque machine, aucun comportement pendant le vol
	if (bLocalBumpActive)
	{
		UpdateBump();
		return;
	}

	// Boss Big Slash : préparation puis dash calculés sur chaque machine, aucun autre comportement
	if (bLocalBossDashActive)
	{
		UpdateBossDash();
		return;
	}

	// serveur autoritaire : les clients reçoivent position et rotation par réplication
	if (!HasAuthority())
	{
		return;
	}

	// bouclier de boss : son attaque en plus passe avant l'attaque de base
	if (HasBossShield() && TryStartBossAttack())
	{
		return;
	}

	// corps à corps : la hitbox déclenche l'attaque de l'arme de mêlée, sinon blesse directement au contact
	// (NME armé d'une attaque à distance : pas de corps à corps)
	UYSFoeRangedAction* Ranged = GetRangedAction();
	if (!Ranged)
	{
		TArray<UYSAutoSwingAbility*> MeleeActions;
		GetMeleeActions(MeleeActions);
		if (MeleeActions.Num() > 0)
		{
			TriggerMeleeAttack();
		}
		else
		{
			ApplyContactDamage();
		}
	}

	// Skull Gun en visée : immobile, tourné vers la direction figée au début de la visée, tire à la fin
	if (bAiming)
	{
		SetActorRotation(FRotator(0.0f, AimDirection.Rotation().Yaw, 0.0f));
		AimTimeLeft -= DeltaSeconds;
		if (AimTimeLeft <= 0.0f)
		{
			FireShot();
		}
		return;
	}

	// NME qui se traversent : écartés de leurs voisins (s'ajoute à la poursuite, l'input total est plafonné à 1)
	if (!bFoesBlockEachOther && SeparationStrength > 0.0f)
	{
		const FVector Separation = ComputeSeparation();
		if (!Separation.IsNearlyZero())
		{
			AddMovementInput(Separation);
		}
	}

	// NME à distance : les joueurs qui nagent restent des cibles ; élite : aussi (il entre dans l'eau peu profonde)
	const bool bWades = bIsElite && EliteWadeDepth > 0.0f;
	Target = FindNearestPlayer(Ranged != nullptr || bWades);
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

	// à distance (action de l'arme) : l'attaque se recharge en permanence (même hors de portée) ; rechargée, elle part dès
	// que la cible est à portée, et le NME s'arrête à portée
	if (Ranged)
	{
		ThrowTimer = FMath::Min(ThrowTimer + DeltaSeconds, Ranged->AttackInterval);
	}
	const bool bInRange = Ranged && ToTarget.SizeSquared() <= FMath::Square(Ranged->AttackRange);
	if (bInRange)
	{
		if (ThrowTimer >= Ranged->AttackInterval && Ranged->StartAttack(this, Target, GetAttackOrigin(Ranged)))
		{
			ThrowTimer = 0.0f;
			if (bAiming)
			{
				return;
			}
		}
	}
	else
	{
		// cible qui nage : le NME s'arrête au bord de l'eau (il attend qu'elle revienne à portée)
		// élite : il entre dans la mer jusqu'à Elite Wade Depth (quelle que soit la cible)
		const AYSPlayerCharacter* TargetCharacter = Cast<AYSPlayerCharacter>(Target);
		const bool bBlockedByWater = bWades ? IsWaterAhead(ToTarget, WaterStopDistance * GetActorScale3D().X, EliteWadeDepth)
			: TargetCharacter && TargetCharacter->IsSwimming() && IsWaterAhead(ToTarget, WaterStopDistance);
		if (!bBlockedByWater)
		{
			AddMovementInput(ToTarget.GetSafeNormal());
		}
	}

	// attaque de mêlée en cours : reste tourné vers le joueur qui l'a déclenchée
	const APawn* Victim = MeleeVictim.Get();
	if (Victim && IsMeleeAttacking())
	{
		const FVector ToVictim = Victim->GetActorLocation() - GetActorLocation();
		if (!ToVictim.IsNearlyZero())
		{
			ToTarget = FVector(ToVictim.X, ToVictim.Y, 0.0f);
		}
	}

	const FRotator Desired(0.0f, ToTarget.Rotation().Yaw, 0.0f);
	SetActorRotation(TurnSpeed > 0.0f ? FMath::RInterpTo(GetActorRotation(), Desired, DeltaSeconds, TurnSpeed) : Desired);
}

bool AYSFoeCharacter::IsWaterAhead(const FVector& Direction, float Distance, float MaxDepth) const
{
	const UWorld* World = GetWorld();
	const FVector Ahead = GetActorLocation() + Direction.GetSafeNormal2D() * Distance;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSFoeWaterAhead), false, this);
	if (!World->LineTraceSingleByObjectType(Hit, Ahead + FVector(0.0f, 0.0f, 200.0f), Ahead - FVector(0.0f, 0.0f, 3000.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return true;
	}
	// profondeur tolérée (élite) : eau peu profonde praticable
	return MaxDepth > 0.0f ? AYSOcean::GetWaterDepth(World, Hit.ImpactPoint) > MaxDepth : AYSOcean::IsUnderwater(World, Hit.ImpactPoint);
}

APawn* AYSFoeCharacter::FindNearestPlayer(bool bIncludeSwimmers) const
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
		const AYSPlayerCharacter* PlayerCharacter = Cast<AYSPlayerCharacter>(Pawn);
		// corps en ragdoll d'un joueur mort, joueur qui nage (sauf pour un NME à distance) : pas une cible
		if (!Pawn || (PlayerCharacter && (!PlayerCharacter->IsAlive() || (PlayerCharacter->IsSwimming() && !bIncludeSwimmers))))
		{
			continue;
		}

		// toujours le plus proche (distance au bord de sa capsule : un gros personnage collé au NME est bien le plus proche),
		// mais un joueur qui porte un objet (coffre) compte comme N fois plus proche
		const float Attraction = PlayerCharacter ? PlayerCharacter->GetFoeAttractionMultiplier() : 1.0f;
		const float EdgeDistance = FMath::Max(0.0f, FVector::Dist2D(Pawn->GetActorLocation(), GetActorLocation()) - Pawn->GetSimpleCollisionRadius());
		const float DistSq = FMath::Square(EdgeDistance) / FMath::Square(FMath::Max(Attraction, KINDA_SMALL_NUMBER));
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Pawn;
		}
	}

	// minions (Captain) : ciblés aussi, mais comptent Minion Target Distance Multiplier fois plus loin
	for (TActorIterator<AYSCaptainMinion> It(GetWorld()); It; ++It)
	{
		AYSCaptainMinion* Minion = *It;
		if (!Minion->IsAlive() || Minion->IsActorBeingDestroyed() || (Minion->IsSwimming() && !bIncludeSwimmers))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(Minion->GetActorLocation(), GetActorLocation()) * FMath::Square(MinionTargetDistanceMultiplier);
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Minion;
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
	TArray<AYSPlayerCharacter*> Touching;
	GetPlayersInHitbox(Touching);

	for (AYSPlayerCharacter* Player : Touching)
	{
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

void AYSFoeCharacter::GetPlayersInHitbox(TArray<AYSPlayerCharacter*>& OutPlayers) const
{
	const FCollisionShape Shape = Hitbox->GetCollisionShape();
	const FVector Center = Hitbox->GetComponentLocation();
	const FQuat Rotation = Hitbox->GetComponentQuat();
	const float Reach = Shape.GetExtent().Size();
	for (TActorIterator<AYSPlayerCharacter> It(GetWorld()); It; ++It)
	{
		// seule la hurtbox (le corps) d'un joueur compte, pas sa capsule de mouvement
		AYSPlayerCharacter* Player = *It;
		UPrimitiveComponent* Hurtbox = Player->GetHurtbox();
		if (!Hurtbox || !Player->IsAlive() || !Player->HasAnyMatchingGameplayTags(ContactHitTags)
			|| FVector::DistSquared(Center, Hurtbox->Bounds.Origin) > FMath::Square(Reach + Hurtbox->Bounds.SphereRadius))
		{
			continue;
		}
		if (Hurtbox->OverlapComponent(Center, Rotation, Shape))
		{
			OutPlayers.Add(Player);
		}
	}
}

void AYSFoeCharacter::UpdateFarTicking()
{
	// loin = hors écran (ici) et à plus de Far Tick Distance de tout personnage joueur (minions compris : ils se battent aussi)
	bool bFar = FarTickInterval > 0.0f && !bLocalDeathStarted && !WasRecentlyRendered(FarTickCheckInterval);
	if (bFar)
	{
		const float FarDistanceSq = FMath::Square(FarTickDistance);
		for (TActorIterator<AYSPlayerCharacter> It(GetWorld()); It && bFar; ++It)
		{
			bFar = FVector::DistSquared2D(It->GetActorLocation(), GetActorLocation()) > FarDistanceSq;
		}
	}
	if (bFar == bFarTicking)
	{
		return;
	}
	bFarTicking = bFar;

	// le tick de l'acteur reste à chaque image : l'IA ajoute son input, le déplacement le consomme à son rythme (somme plafonnée)
	const float Interval = bFar ? FarTickInterval : 0.0f;
	GetCharacterMovement()->SetComponentTickInterval(Interval);
	HealthBar->SetComponentTickInterval(Interval);
	for (const AYSWeapon* Weapon : { GetLeftHandWeapon(), GetRightHandWeapon() })
	{
		if (Weapon)
		{
			for (UActorComponent* Component : Weapon->GetComponents())
			{
				Component->SetComponentTickInterval(Interval);
			}
		}
	}
	UE_LOG(LogTemp, VeryVerbose, TEXT("[YSFoe] %s : ticks %s"), *GetName(), bFar ? TEXT("ralentis (loin)") : TEXT("normaux"));
}

namespace YSFoeCrowd
{
	/** Grille de voisinage des NME (serveur), reconstruite une fois par image par le premier NME qui en a besoin */
	constexpr float CellSize = 200.0f;

	struct FGrid
	{
		const UWorld* World = nullptr;
		uint64 Frame = MAX_uint64;
		float MaxRadius = 0.0f;
		TMap<FIntPoint, TArray<const AYSFoeCharacter*, TInlineAllocator<8>>> Cells;
	};
	FGrid Grid;

	FIntPoint GetCell(const FVector& Location)
	{
		return FIntPoint(FMath::FloorToInt(Location.X / CellSize), FMath::FloorToInt(Location.Y / CellSize));
	}

	const FGrid& GetGrid(const UWorld* World)
	{
		if (Grid.World != World || Grid.Frame != GFrameCounter)
		{
			Grid.World = World;
			Grid.Frame = GFrameCounter;
			Grid.MaxRadius = 0.0f;
			Grid.Cells.Reset();
			for (TActorIterator<AYSFoeCharacter> It(const_cast<UWorld*>(World)); It; ++It)
			{
				if (!It->IsDying() && !It->IsActorBeingDestroyed())
				{
					Grid.Cells.FindOrAdd(GetCell(It->GetActorLocation())).Add(*It);
					Grid.MaxRadius = FMath::Max(Grid.MaxRadius, It->GetSimpleCollisionRadius());
				}
			}
		}
		return Grid;
	}
}

FVector AYSFoeCharacter::ComputeSeparation() const
{
	const YSFoeCrowd::FGrid& Grid = YSFoeCrowd::GetGrid(GetWorld());
	const FVector Location = GetActorLocation();
	const float Radius = GetSimpleCollisionRadius();
	const FIntPoint Cell = YSFoeCrowd::GetCell(Location);
	const int32 Ring = FMath::Clamp(FMath::CeilToInt((Radius + Grid.MaxRadius) * SeparationRadiusScale / YSFoeCrowd::CellSize), 1, 4);

	FVector Push = FVector::ZeroVector;
	for (int32 X = Cell.X - Ring; X <= Cell.X + Ring; ++X)
	{
		for (int32 Y = Cell.Y - Ring; Y <= Cell.Y + Ring; ++Y)
		{
			const auto* Foes = Grid.Cells.Find(FIntPoint(X, Y));
			if (!Foes)
			{
				continue;
			}
			for (const AYSFoeCharacter* Other : *Foes)
			{
				if (Other == this)
				{
					continue;
				}
				FVector Away = Location - Other->GetActorLocation();
				Away.Z = 0.0f;
				const float Range = (Radius + Other->GetSimpleCollisionRadius()) * SeparationRadiusScale;
				const float DistSq = Away.SizeSquared();
				if (DistSq >= FMath::Square(Range))
				{
					continue;
				}
				// superposés exactement : direction fixe propre à la paire (pas de hasard d'une image à l'autre)
				const float Dist = FMath::Sqrt(DistSq);
				if (Dist < 1.0f)
				{
					const float Angle = static_cast<float>((reinterpret_cast<UPTRINT>(this) >> 4) % 360);
					Away = FVector(FMath::Cos(FMath::DegreesToRadians(Angle)), FMath::Sin(FMath::DegreesToRadians(Angle)), 0.0f);
				}
				else
				{
					Away /= Dist;
				}
				Push += Away * (1.0f - Dist / Range);
			}
		}
	}
	return Push * SeparationStrength;
}

void AYSFoeCharacter::OptimizeWeapons()
{
	AYSWeapon* Weapons[2] = { GetLeftHandWeapon(), GetRightHandWeapon() };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		if (Weapons[Index] && OptimizedWeapons[Index].Get() != Weapons[Index])
		{
			OptimizedWeapons[Index] = Weapons[Index];
			Weapons[Index]->OptimizeForCrowd(bWeaponsCastShadow);
			// arme recréée (gaucher) pendant un tick ralenti : même rythme
			if (bFarTicking)
			{
				for (UActorComponent* Component : Weapons[Index]->GetComponents())
				{
					Component->SetComponentTickInterval(FarTickInterval);
				}
			}
		}
	}
}

float AYSFoeCharacter::ComputePushbackForce(float Damage, float TargetMaxHealth) const
{
	return YSCombat::ComputePushbackForce(Damage, TargetMaxHealth, PushbackMinDamageRatio, PushbackMaxDamageRatio, PushbackMinForce, PushbackMaxForce);
}

void AYSFoeCharacter::GetMeleeActions(TArray<UYSAutoSwingAbility*>& OutActions) const
{
	for (const AYSWeapon* Weapon : { GetLeftHandWeapon(), GetRightHandWeapon() })
	{
		if (UYSAutoSwingAbility* Swing = Weapon ? Cast<UYSAutoSwingAbility>(Weapon->GetAutoAction()) : nullptr)
		{
			OutActions.Add(Swing);
		}
	}
}

bool AYSFoeCharacter::IsMeleeAttacking() const
{
	TArray<UYSAutoSwingAbility*> MeleeActions;
	GetMeleeActions(MeleeActions);
	return MeleeActions.ContainsByPredicate([](const UYSAutoSwingAbility* Swing) { return Swing->IsActionInProgress(); });
}

void AYSFoeCharacter::TriggerMeleeAttack()
{
	if (Health <= 0.0f)
	{
		return;
	}

	// un joueur vivant (sa hurtbox, pas sa capsule) touche la hitbox de contact : déclencheur de l'attaque
	// (le plus proche s'il y en a plusieurs : c'est vers lui que part le coup)
	TArray<AYSPlayerCharacter*> Touching;
	GetPlayersInHitbox(Touching);
	AYSPlayerCharacter* Victim = nullptr;
	for (AYSPlayerCharacter* Player : Touching)
	{
		if (!Victim || FVector::DistSquared2D(Player->GetActorLocation(), GetActorLocation()) < FVector::DistSquared2D(Victim->GetActorLocation(), GetActorLocation()))
		{
			Victim = Player;
		}
	}
	if (!Victim)
	{
		return;
	}
	if (IsMeleeAttacking())
	{
		return;
	}

	// l'arme frappe avec les dégâts du NME (niveau, élite) ; pas d'upgrade pour les NME
	TArray<UYSAutoSwingAbility*> MeleeActions;
	GetMeleeActions(MeleeActions);
	if (!MeleeActions.ContainsByPredicate([](const UYSAutoSwingAbility* Swing) { return Swing->IsReady(); }))
	{
		return;
	}

	// tourné tout de suite vers le joueur touché : le coup part vers lui (pas vers la cible de déplacement)
	const FVector ToVictim = (Victim->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (!ToVictim.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.0f, ToVictim.Rotation().Yaw, 0.0f));
	}

	bool bStarted = false;
	EYSSwingAnim Anim = EYSSwingAnim::Main;
	for (UYSAutoSwingAbility* Swing : MeleeActions)
	{
		Swing->Damage = ContactDamage;
		if (Swing->StartAttack() && !bStarted)
		{
			bStarted = true;
			Anim = Swing->GetCurrentAnim();
		}
	}
	if (bStarted)
	{
		MeleeVictim = Victim;
		UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s attaque %s (%.0f dégâts)"), *GetName(), *Victim->GetName(), ContactDamage);
		MulticastMeleeAttack(Anim);
	}
}

void AYSFoeCharacter::MulticastMeleeAttack_Implementation(EYSSwingAnim Anim)
{
	// serveur : déjà lancée par StartAttack ; clients : leur arme locale joue l'animation
	if (HasAuthority())
	{
		return;
	}
	TArray<UYSAutoSwingAbility*> MeleeActions;
	GetMeleeActions(MeleeActions);
	for (UYSAutoSwingAbility* Swing : MeleeActions)
	{
		Swing->PlayAttackLocally(Anim);
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s attaque (client, %d arme(s))"), *GetName(), MeleeActions.Num());
}

UYSFoeRangedAction* AYSFoeCharacter::GetRangedAction() const
{
	for (const AYSWeapon* Weapon : { GetRightHandWeapon(), GetLeftHandWeapon() })
	{
		if (UYSFoeRangedAction* Action = Weapon ? Weapon->FindComponentByClass<UYSFoeRangedAction>() : nullptr)
		{
			return Action;
		}
	}
	return nullptr;
}

FVector AYSFoeCharacter::GetAttackOrigin(const UYSFoeRangedAction* Action) const
{
	// Muzzle de l'arme qui porte l'attaque (canon du pistolet, bombe dans la main)
	if (const AYSWeapon* Weapon = Action ? Cast<AYSWeapon>(Action->GetOwner()) : nullptr)
	{
		return Weapon->GetMuzzleLocation();
	}
	return GetActorLocation() + FVector(0.0f, 0.0f, YSFoe::DefaultAttackHeight);
}

bool AYSFoeCharacter::StartAim(const APawn* AimTarget, float AimDuration, float YawOffset)
{
	if (!AimTarget)
	{
		return false;
	}

	// direction FIGÉE : celle de la cible au début de la visée, le NME ne réajuste pas
	FVector Direction = AimTarget->GetActorLocation() - GetActorLocation();
	Direction.Z = 0.0f;
	if (Direction.IsNearlyZero())
	{
		Direction = GetActorForwardVector();
	}
	// imprécision de l'arme : décalage de YawOffset degrés
	AimDirection = Direction.GetSafeNormal2D().RotateAngleAxis(YawOffset, FVector::UpVector);
	AimTimeLeft = AimDuration;
	bAiming = true;
	SetActorRotation(FRotator(0.0f, AimDirection.Rotation().Yaw, 0.0f));

	const FVector Start = AYSInteractiveObject::FindGround(GetWorld(), GetActorLocation(), this);
	UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s vise %s pendant %.2f s, direction (%.2f, %.2f), décalage %.1f°"), *GetName(), *AimTarget->GetName(), AimDuration, AimDirection.X, AimDirection.Y, YawOffset);
	MulticastAimStarted(Start, AimDirection);
	return true;
}

void AYSFoeCharacter::FireShot()
{
	bAiming = false;
	MulticastAimEnded();
	if (UYSFoeShotAction* Shot = Cast<UYSFoeShotAction>(GetRangedAction()))
	{
		Shot->Fire(this, GetAttackOrigin(Shot), AimDirection);
		// recul du pistolet sur toutes les machines
		NotifyRangedAttack(Shot);
	}
}

void AYSFoeCharacter::MulticastAimStarted_Implementation(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction)
{
	ShowAimLine(true, Start, Direction);
}

void AYSFoeCharacter::MulticastAimEnded_Implementation()
{
	ShowAimLine(false, FVector::ZeroVector, FVector::ForwardVector);
}

void AYSFoeCharacter::ShowAimLine(bool bShow, const FVector& Start, const FVector& Direction)
{
	// réglages de la ligne : action de tir de l'arme (arme locale sur chaque machine)
	const UYSFoeShotAction* Shot = Cast<UYSFoeShotAction>(GetRangedAction());
	bShow = bShow && Shot;
	UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s ligne de visée %s (%s)"), *GetName(), bShow ? TEXT("affichée") : TEXT("cachée"), HasAuthority() ? TEXT("serveur") : TEXT("client"));
	if (!bShow)
	{
		ShowGroundStrip(false, Start, Direction, 0.0f, 0.0f, nullptr, FLinearColor::White, 0.0f);
		return;
	}
	// matériau de la prévisualisation de la bombe
	ShowGroundStrip(true, Start, Direction, Shot->AimLineLength, Shot->AimLineWidth, Shot->PreviewMaterial, Shot->PreviewColor, Shot->AimLineOpacity);
}

void AYSFoeCharacter::ShowGroundStrip(bool bShow, const FVector& Start, const FVector& Direction, float Length, float Width, UMaterialInterface* Material, const FLinearColor& Color, float Opacity)
{
	AimLine->SetVisibility(bShow);
	if (!bShow)
	{
		AimLine->ClearAllMeshSections();
		return;
	}

	// bande posée sur le relief depuis Start (bord désactivé : rayon immense)
	AimLine->SetWorldLocation(Start);
	AimLine->BuildStrip(Direction, Length, Width, YSFoe::AimLineHeight);
	if (Material)
	{
		UMaterialInstanceDynamic* Instance = AimLine->CreateDynamicMaterialInstance(0, Material);
		Instance->SetVectorParameterValue(TEXT("Color"), Color);
		Instance->SetScalarParameterValue(TEXT("Glow"), 0.3f);
		Instance->SetScalarParameterValue(TEXT("FillOpacity"), Opacity);
		Instance->SetScalarParameterValue(TEXT("EdgeOpacity"), 0.0f);
		Instance->SetScalarParameterValue(TEXT("Radius"), Length * 10.0f);
		Instance->SetScalarParameterValue(TEXT("EdgeWidth"), 1.0f);
	}
}

float AYSFoeCharacter::ApplyBossArmor(float Damage, const FDamageEvent& DamageEvent, const AController* EventInstigator)
{
	// dégâts hors critique : le critique de l'attaquant est retiré puis réappliqué après la réduction
	const AYSPlayerCharacter* Attacker = EventInstigator ? Cast<AYSPlayerCharacter>(EventInstigator->GetPawn()) : nullptr;
	const float CriticalMultiplier = UYSCriticalDamageType::IsCritical(DamageEvent) && Attacker ? FMath::Max(Attacker->GetCriticalDamageMultiplier(), 0.01f) : 1.0f;
	const float Base = Damage / CriticalMultiplier;

	// sous Rage Cap : dégâts normaux, rage = dégâts ; au-delà : l'excès est réduit, rage = Rage Cap
	float Reduced = Base;
	float RageGain = Base;
	if (Base > RageCap)
	{
		const float Excess = Base - RageCap;
		Reduced = RageCap + Excess * DmgReduceFactor / (DmgReduceFactor + Excess);
		RageGain = RageCap;
	}
	Rage = FMath::Min(Rage + RageGain, MaxRage);
	UE_LOG(LogTemp, Verbose, TEXT("[YSBoss] %s : coup %.1f (hors critique %.1f, x%.2f) -> %.1f, rage +%.0f = %.0f / %.0f"), *GetName(), Damage, Base, CriticalMultiplier,
		Reduced * CriticalMultiplier, RageGain, Rage, MaxRage);
	return Reduced * CriticalMultiplier;
}

void AYSFoeCharacter::TryTriggerBossShield()
{
	if (!UsesBossArmor() || HasBossShield() || BossShield <= 0.0f || Health <= 0.0f)
	{
		return;
	}
	const bool bRageFull = Rage >= MaxRage;
	const bool bLowHealth = !bHealthShieldTriggered && Health <= MaxHealth * ShieldHealthThreshold;
	if (!bRageFull && !bLowHealth)
	{
		return;
	}
	if (bLowHealth)
	{
		bHealthShieldTriggered = true;
	}
	MaxShield = FMath::RoundToFloat(BossShield * (bScaleShieldWithPlayerCount ? PlayerCountHealthMultiplier : 1.0f));
	Shield = MaxShield;
	Rage = 0.0f;
	ForceNetUpdate();
	UpdateHealthBar();
	UE_LOG(LogTemp, Log, TEXT("[YSBoss] %s : bouclier de boss %.0f (%s), rage vidée"), *GetName(), Shield, bRageFull ? TEXT("rage pleine") : TEXT("PV sous le seuil"));
}

void AYSFoeCharacter::DebugAddRage(float Amount)
{
	if (!HasAuthority() || !UsesBossArmor() || HasBossShield() || IsDying())
	{
		return;
	}
	Rage = FMath::Clamp(Rage + Amount, 0.0f, MaxRage);
	UE_LOG(LogTemp, Log, TEXT("[YSBoss] %s : rage %.0f / %.0f (debug)"), *GetName(), Rage, MaxRage);
	TryTriggerBossShield();
	ForceNetUpdate();
}

UYSBossBigSlashAbility* AYSFoeCharacter::GetBossAttack() const
{
	for (const AYSWeapon* Weapon : { GetRightHandWeapon(), GetLeftHandWeapon() })
	{
		if (UYSBossBigSlashAbility* Slash = Weapon ? Weapon->FindComponentByClass<UYSBossBigSlashAbility>() : nullptr)
		{
			return Slash;
		}
	}
	return nullptr;
}

bool AYSFoeCharacter::TryStartBossAttack()
{
	UYSBossBigSlashAbility* Slash = GetBossAttack();
	if (!Slash || !Slash->IsReady() || bAiming)
	{
		return false;
	}
	const APawn* Victim = FindNearestPlayer();
	if (!Victim)
	{
		return false;
	}
	const FVector ToVictim = (Victim->GetActorLocation() - GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
	if (ToVictim.SizeSquared() > FMath::Square(Slash->TriggerRange))
	{
		return false;
	}

	// plus prioritaire que l'attaque de base (Ability > Auto atk) : StartAttack l'interrompt
	Slash->Damage = ContactDamage * Slash->ContactDamageMultiplier;
	if (!Slash->StartAttack())
	{
		return false;
	}

	const FVector Direction = ToVictim.IsNearlyZero() ? GetActorForwardVector().GetSafeNormal2D() : ToVictim.GetSafeNormal();
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	BossDash.bActive = true;
	BossDash.Start = GetActorLocation();
	BossDash.End = Slash->FindDashEnd(GetActorLocation(), Direction, GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), this);
	BossDash.Direction = Direction;
	BossDash.WindupDuration = Slash->GetWindupTime();
	BossDash.DashDuration = Slash->GetSwingTime();
	BossDash.StartServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	++BossDash.Counter;
	MeleeVictim = nullptr;

	MulticastBossAttack();
	BeginBossDashLocal();
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSBoss] %s : Boss Big Slash vers %s (%.0f dégâts), %.0f cm, préparation %.1f s, dash %.2f s"), *GetName(), *Victim->GetName(), Slash->Damage,
		FVector::Dist2D(BossDash.Start, BossDash.End), BossDash.WindupDuration, BossDash.DashDuration);
	return true;
}

void AYSFoeCharacter::MulticastBossAttack_Implementation()
{
	// serveur : déjà lancée par StartAttack ; clients : attaque de base arrêtée, l'arme locale joue l'animation
	if (HasAuthority())
	{
		return;
	}
	TArray<UYSAutoSwingAbility*> MeleeActions;
	GetMeleeActions(MeleeActions);
	for (UYSAutoSwingAbility* Swing : MeleeActions)
	{
		Swing->StopActionLocally();
	}
	if (UYSBossBigSlashAbility* Slash = GetBossAttack())
	{
		Slash->PlayAttackLocally(EYSSwingAnim::Main);
	}
}

void AYSFoeCharacter::OnRep_BossDash()
{
	if (BossDash.bActive && (!bLocalBossDashActive || LocalBossDashCounter != BossDash.Counter) && !bLocalDeathStarted)
	{
		BeginBossDashLocal();
	}
	else if (!BossDash.bActive && bLocalBossDashActive)
	{
		EndBossDashLocal();
	}
}

void AYSFoeCharacter::BeginBossDashLocal()
{
	bLocalBossDashActive = true;
	LocalBossDashCounter = BossDash.Counter;

	// comme le bump : position calculée par UpdateBossDash sur chaque machine, mouvement du personnage et sa réplication coupés
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->SetComponentTickEnabled(false);
	if (HasAuthority())
	{
		SetReplicateMovement(false);
	}

	// zone rouge au sol sur tout le trajet pendant la préparation (réglages de l'arme locale)
	const UYSBossBigSlashAbility* Slash = GetBossAttack();
	const FVector Direction = BossDash.Direction;
	bBossWarningShown = Slash != nullptr;
	if (Slash)
	{
		const FVector Ground = AYSInteractiveObject::FindGround(GetWorld(), BossDash.Start, this);
		ShowGroundStrip(true, Ground, Direction, FVector::Dist2D(BossDash.Start, BossDash.End) + Slash->WarningExtraLength, Slash->WarningWidth,
			Slash->WarningMaterial, Slash->WarningColor, Slash->WarningOpacity);
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSBoss] %s (%s) : Boss Big Slash commencé"), *GetName(), HasAuthority() ? TEXT("serveur") : TEXT("client"));
	UpdateBossDash();
}

void AYSFoeCharacter::UpdateBossDash()
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const float Elapsed = static_cast<float>(Now - BossDash.StartServerTime);
	const FRotator Facing(0.0f, FVector(BossDash.Direction).Rotation().Yaw, 0.0f);

	// préparation : immobile, tourné vers la direction figée
	if (Elapsed < BossDash.WindupDuration)
	{
		SetActorLocationAndRotation(BossDash.Start, Facing);
		return;
	}
	if (bBossWarningShown)
	{
		bBossWarningShown = false;
		ShowGroundStrip(false, FVector::ZeroVector, FVector::ForwardVector, 0.0f, 0.0f, nullptr, FLinearColor::White, 0.0f);
	}
	const float Alpha = (Elapsed - BossDash.WindupDuration) / FMath::Max(BossDash.DashDuration, 0.01f);
	if (Alpha >= 1.0f)
	{
		EndBossDashLocal();
		return;
	}

	// dash : même profil de vitesse que le Dashing Slash ; hauteur = sol sous le NME (au-dessus de la mer)
	const UYSBossBigSlashAbility* Slash = GetBossAttack();
	FVector Location = FMath::Lerp(FVector(BossDash.Start), FVector(BossDash.End), Slash ? Slash->GetDashDistanceFraction(Alpha) : Alpha);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSBossDash), false, this);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Location + FVector(0.0f, 0.0f, 1000.0f), Location - FVector(0.0f, 0.0f, 5000.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		Location.Z = AYSOcean::ClampAboveSea(GetWorld(), Hit.ImpactPoint.Z) + GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	SetActorLocationAndRotation(Location, Facing);
}

void AYSFoeCharacter::EndBossDashLocal()
{
	if (!bLocalBossDashActive)
	{
		return;
	}
	bLocalBossDashActive = false;
	if (bBossWarningShown)
	{
		bBossWarningShown = false;
		ShowGroundStrip(false, FVector::ZeroVector, FVector::ForwardVector, 0.0f, 0.0f, nullptr, FLinearColor::White, 0.0f);
	}

	SetActorLocation(BossDash.End);
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->SetComponentTickEnabled(true);
	if (HasAuthority())
	{
		Movement->Velocity = FVector::ZeroVector;
		Movement->SetMovementMode(MOVE_Falling);
		BossDash.bActive = false;
		SetReplicateMovement(true);
		ForceNetUpdate();
		UE_LOG(LogTemp, Verbose, TEXT("[YSBoss] %s : Boss Big Slash fini en (%.0f, %.0f, %.0f)"), *GetName(), BossDash.End.X, BossDash.End.Y, BossDash.End.Z);
	}
}

void AYSFoeCharacter::NotifyRangedAttack(UYSFoeRangedAction* Action)
{
	if (Action)
	{
		Action->PlayAttackLocally();
	}
	MulticastRangedAttack();
}

void AYSFoeCharacter::MulticastRangedAttack_Implementation()
{
	// serveur : déjà joué par NotifyRangedAttack ; clients : l'arme locale joue l'animation (lancer, recul)
	if (HasAuthority())
	{
		return;
	}
	if (UYSFoeRangedAction* Action = GetRangedAction())
	{
		Action->PlayAttackLocally();
	}
}

bool AYSFoeCharacter::RegisterCriticalBullet(const AActor* Shooter, int32 Interval)
{
	if (!Shooter || Interval <= 0)
	{
		return false;
	}
	int32& Count = CriticalBulletCounts.FindOrAdd(Shooter, 0);
	++Count;
	const bool bCritical = Count % Interval == 0;

	// point critique : la prochaine balle de ce joueur sur ce NME sera critique
	if (APlayerState* ShooterState = Cast<APawn>(Shooter) ? Cast<APawn>(Shooter)->GetPlayerState() : nullptr)
	{
		const bool bNextCritical = (Count + 1) % Interval == 0;
		if (bNextCritical != CriticalReadyShooters.Contains(ShooterState))
		{
			if (bNextCritical)
			{
				CriticalReadyShooters.Add(ShooterState);
			}
			else
			{
				CriticalReadyShooters.Remove(ShooterState);
			}
			ForceNetUpdate();
			UpdateHealthBar();
		}
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSCritical] %s : balle %d de %s%s"), *GetName(), Count, *Shooter->GetName(), bCritical ? TEXT(", critique") : TEXT(""));
	return bCritical;
}
