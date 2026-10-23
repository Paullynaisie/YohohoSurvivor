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
#include "Map/YSGroundZoneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Enemies/YSFoeRangedAction.h"

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
	if (bHandsInitialized && GetWorld() && GetWorld()->IsGameWorld() && !WasRecentlyRendered(HandsOffscreenFreezeDelay) && !(HasAuthority() && (IsMeleeAttacking() || (GetRangedAction() && GetRangedAction()->IsAnimating()))))
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
		const UYSAutoSwingAbility* Swing = Weapon ? Cast<UYSAutoSwingAbility>(Weapon->GetAutoAction()) : nullptr;
		float IdleWeight = 1.0f;
		FVector PoseLocation = FVector::ZeroVector;
		FQuat PoseRotation = FQuat::Identity;
		if ((Swing && Swing->EvaluateHandPose(IdleWeight, PoseLocation, PoseRotation)) || (Ranged && Ranged->EvaluateHandPose(IdleWeight, PoseLocation, PoseRotation)))
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

		Hand->SetWorldLocation(FinalLocation);
		Hand->SetRelativeRotation(FinalRotation);
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
			OtherHand->SetWorldLocationAndRotation(Socket->GetComponentLocation(), Socket->GetComponentQuat());
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
		// élite : barre de vie dans le HUD (AYSPlayerController), pas au-dessus de sa tête ; le point critique reste
		Bar->SetHealth(bIsElite ? 0.0f : Health, MaxHealth);
		const APlayerController* LocalController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		const APlayerState* LocalState = LocalController ? LocalController->PlayerState.Get() : nullptr;
		Bar->SetCriticalReady(LocalState && CriticalReadyShooters.Contains(LocalState));
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

	// coup qui bumpe (Bumping Slash) : NME lancé en cloche ; un élite reçoit le pushback normal
	if (DamageEvent.IsOfType(FYSBumpDamageEvent::ClassID) && !bIsElite)
	{
		const FYSBumpDamageEvent& BumpEvent = static_cast<const FYSBumpDamageEvent&>(DamageEvent);
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
	const AYSPlayerCharacter* Attacker = EventInstigator ? Cast<AYSPlayerCharacter>(EventInstigator->GetPawn()) : nullptr;
	if (!Attacker || !DamageEvent.IsOfType(FPointDamageEvent::ClassID))
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
		Health = MaxHealth;

		if (bIsElite && YSGameState)
		{
			YSGameState->SetCurrentElite(this);
		}
	}

	HealthBar->InitWidget();
	UpdateHealthBar();
}

void AYSFoeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

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

	// bump (Bumping Slash) : vol en cloche calculé sur chaque machine, aucun comportement pendant le vol
	if (bLocalBumpActive)
	{
		UpdateBump();
		return;
	}

	// serveur autoritaire : les clients reçoivent position et rotation par réplication
	if (!HasAuthority())
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
		AddMovementInput(ToTarget.GetSafeNormal());
	}

	const FRotator Desired(0.0f, ToTarget.Rotation().Yaw, 0.0f);
	SetActorRotation(TurnSpeed > 0.0f ? FMath::RInterpTo(GetActorRotation(), Desired, DeltaSeconds, TurnSpeed) : Desired);
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

	// minions (Captain) : ciblés aussi, mais comptent Minion Target Distance Multiplier fois plus loin
	for (TActorIterator<AYSCaptainMinion> It(GetWorld()); It; ++It)
	{
		AYSCaptainMinion* Minion = *It;
		if (!Minion->IsAlive() || Minion->IsActorBeingDestroyed())
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
	TArray<UPrimitiveComponent*> Touching;
	Hitbox->GetOverlappingComponents(Touching);
	const bool bPlayerInReach = Touching.ContainsByPredicate([this](const UPrimitiveComponent* Component)
	{
		const AYSPlayerCharacter* Player = Component ? Cast<AYSPlayerCharacter>(Component->GetOwner()) : nullptr;
		return Player && Component == Player->GetHurtbox() && Player->IsAlive() && Player->HasAnyMatchingGameplayTags(ContactHitTags);
	});
	if (!bPlayerInReach)
	{
		return;
	}

	// l'arme frappe avec les dégâts du NME (niveau, élite) ; pas d'upgrade pour les NME
	TArray<UYSAutoSwingAbility*> MeleeActions;
	GetMeleeActions(MeleeActions);
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
		UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s attaque (%.0f dégâts)"), *GetName(), ContactDamage);
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
	AimLine->SetVisibility(bShow);
	UE_LOG(LogTemp, Verbose, TEXT("[YSFoe] %s ligne de visée %s (%s)"), *GetName(), bShow ? TEXT("affichée") : TEXT("cachée"), HasAuthority() ? TEXT("serveur") : TEXT("client"));
	if (!bShow)
	{
		AimLine->ClearAllMeshSections();
		return;
	}

	// bande posée sur le relief, du NME vers la cible ; matériau de la prévisualisation de la bombe (bord désactivé : rayon immense)
	AimLine->SetWorldLocation(Start);
	AimLine->BuildStrip(Direction, Shot->AimLineLength, Shot->AimLineWidth, YSFoe::AimLineHeight);
	if (Shot->PreviewMaterial)
	{
		UMaterialInstanceDynamic* Material = AimLine->CreateDynamicMaterialInstance(0, Shot->PreviewMaterial);
		Material->SetVectorParameterValue(TEXT("Color"), Shot->PreviewColor);
		Material->SetScalarParameterValue(TEXT("Glow"), 0.3f);
		Material->SetScalarParameterValue(TEXT("FillOpacity"), Shot->AimLineOpacity);
		Material->SetScalarParameterValue(TEXT("EdgeOpacity"), 0.0f);
		Material->SetScalarParameterValue(TEXT("Radius"), Shot->AimLineLength * 10.0f);
		Material->SetScalarParameterValue(TEXT("EdgeWidth"), 1.0f);
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
