#include "Ship/YSPirateShip.h"
#include "Components/ChildActorComponent.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/YSGameState.h"
#include "Core/YSGameMode.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Interaction/YSInteractiveObject.h"
#include "Map/YSOcean.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "Ship/YSBoatCrew.h"
#include "Ship/YSCrowsNest.h"
#include "Ship/YSLongboatLauncher.h"
#include "Ship/YSShipBoarding.h"
#include "Ship/YSShipCannon.h"
#include "Ship/YSCombatPost.h"
#include "Ship/YSShipCore.h"
#include "Ship/YSShipHelm.h"
#include "Ship/YSShipPath.h"
#include "UI/YSHealthBarWidget.h"
#include "UObject/ConstructorHelpers.h"

namespace YSPirateShip
{
	/** Recherche du fond sous la coque : depuis un peu au-dessus de la mer jusqu'à loin dessous */
	constexpr float DepthTraceUp = 100.0f;
	constexpr float DepthTraceDown = 3000.0f;

	/** Pas de simulation max (une grosse image est découpée) */
	constexpr float MaxStep = 1.0f / 30.0f;
	/** Sous cette vitesse (cm/s), bloqué contre un bateau : pas un nouveau choc */
	constexpr float MinImpactSpeed = 50.0f;

	/** Autres bateaux traversés par la recherche du fond (chaloupe sous la coque) */
	constexpr int32 MaxIgnoredShips = 4;

	/** Apparition sur l'eau : recherche du fond depuis au-dessus des collines (une trace partie de plus bas ne voit pas le Landscape au-dessus d'elle) */
	constexpr float SpawnTraceUp = 2000.0f;

	/** Apparition sur l'eau : points vérifiés autour du centre (Clearance Radius) */
	constexpr int32 SpawnClearanceProbes = 8;

	/** Poste créé par un composant optionnel (absent sur une chaloupe) */
	UChildActorComponent* CreateStation(AYSPirateShip* Ship, const FObjectInitializer& ObjectInitializer, USceneComponent* Parent, const TCHAR* Name, UClass* StationClass, const FVector& Location, float Yaw)
	{
		UChildActorComponent* Component = ObjectInitializer.CreateOptionalDefaultSubobject<UChildActorComponent>(Ship, Name);
		if (Component)
		{
			Component->SetupAttachment(Parent);
			Component->SetChildActorClass(StationClass);
			Component->SetRelativeLocationAndRotation(Location, FRotator(0.0f, Yaw, 0.0f));
		}
		return Component;
	}
}

AYSPirateShip::AYSPirateShip(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// présence (cachée tant que l'objectif de doublons n'est pas atteint dans GameMap) et pilotage : à chaque image, sur toutes les machines
	PrimaryActorTick.bCanEverTick = true;

	// position répliquée pour le bateau objectif (un bateau pilotable réplique Motion à la place, voir BeginPlay)
	bReplicates = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(30.0f);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	// barre : centre de la roue du BP_PirateShip (gaillard arrière)
	HelmStation = CreateDefaultSubobject<UChildActorComponent>(TEXT("HelmStation"));
	HelmStation->SetupAttachment(Root);
	HelmStation->SetChildActorClass(AYSShipHelm::StaticClass());
	HelmStation->SetRelativeLocation(FVector(-2040.0f, 0.0f, 582.0f));

	// postes du grand bateau (pont à +210) ; placés sur les formes du BP par setup_ship_stations.py
	CoreStation = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("CoreStation"), AYSShipCore::StaticClass(), FVector(-600.0f, 0.0f, 210.0f), 0.0f);
	CrowsNestStation = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("CrowsNestStation"), AYSCrowsNest::StaticClass(), FVector(200.0f, 0.0f, 210.0f), 0.0f);
	LongboatStation = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("LongboatStation"), AYSLongboatLauncher::StaticClass(), FVector(-1355.0f, -360.0f, 210.0f), 180.0f);
	CannonFront = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("CannonFront"), AYSShipCannon::StaticClass(), FVector(3296.0f, 0.0f, 210.0f), 0.0f);
	CannonLeft = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("CannonLeft"), AYSShipCannon::StaticClass(), FVector(900.0f, -933.0f, 210.0f), -90.0f);
	CannonRight = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("CannonRight"), AYSShipCannon::StaticClass(), FVector(900.0f, 933.0f, 210.0f), 90.0f);
	CannonLeft2 = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("CannonLeft2"), AYSShipCannon::StaticClass(), FVector(-700.0f, -933.0f, 210.0f), -90.0f);
	CannonRight2 = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("CannonRight2"), AYSShipCannon::StaticClass(), FVector(-700.0f, 933.0f, 210.0f), 90.0f);
	CannonFront2 = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("CannonFront2"), nullptr, FVector(3000.0f, 300.0f, 210.0f), 15.0f);
	// accès : flanc bâbord (-Y), à l'ouverture du bastingage et de la planche d'embarquement, au niveau de la mer
	BoardingAccess = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("BoardingAccess"), AYSShipBoarding::StaticClass(), FVector(0.0f, -1000.0f, 0.0f), -90.0f);
	// second accès, côté tribord : on peut monter des deux côtés
	BoardingAccessStarboard = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("BoardingAccessStarboard"), AYSShipBoarding::StaticClass(), FVector(0.0f, 1000.0f, 0.0f), 90.0f);
	// accès supplémentaires (échelles de corde des bateaux des joueurs), vides par défaut
	BoardingAccess2 = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("BoardingAccess2"), nullptr, FVector(600.0f, -1000.0f, 0.0f), -90.0f);
	BoardingAccess3 = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("BoardingAccess3"), nullptr, FVector(-600.0f, -1000.0f, 0.0f), -90.0f);
	BoardingAccess4 = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("BoardingAccess4"), nullptr, FVector(1200.0f, -1000.0f, 0.0f), -90.0f);
	BoardingAccessStarboard2 = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("BoardingAccessStarboard2"), nullptr, FVector(600.0f, 1000.0f, 0.0f), 90.0f);
	BoardingAccessStarboard3 = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("BoardingAccessStarboard3"), nullptr, FVector(-600.0f, 1000.0f, 0.0f), 90.0f);
	BoardingAccessStarboard4 = YSPirateShip::CreateStation(this, ObjectInitializer, Root, TEXT("BoardingAccessStarboard4"), nullptr, FVector(1200.0f, 1000.0f, 0.0f), 90.0f);

	// barre de vie du bateau : à l'écran, au-dessus du pont ; rien tant qu'il n'est pas abîmé
	ShipHealthBar = ObjectInitializer.CreateOptionalDefaultSubobject<UWidgetComponent>(this, TEXT("ShipHealthBar"));
	if (ShipHealthBar)
	{
		ShipHealthBar->SetupAttachment(Root);
		ShipHealthBar->SetRelativeLocation(FVector(-600.0f, 0.0f, 800.0f));
		ShipHealthBar->SetWidgetSpace(EWidgetSpace::Screen);
		ShipHealthBar->SetDrawAtDesiredSize(true);
		ShipHealthBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		static ConstructorHelpers::FClassFinder<UUserWidget> HealthBarFinder(TEXT("/Game/Core/UI/WBP_FoeHealthBar"));
		ShipHealthBar->SetWidgetClass(HealthBarFinder.Succeeded() ? HealthBarFinder.Class : TSubclassOf<UUserWidget>(UYSHealthBarWidget::StaticClass()));
	}

	ShipName = NSLOCTEXT("YS", "PirateShipName", "Bateau pirate");

	// coque de BP_PirateShip : partie droite de -2400 à 2400, proue arrondie jusqu'à 3400, largeur 2000
	HullProbePoints = {
		FVector2D(3400.0f, 0.0f), FVector2D(3100.0f, -700.0f), FVector2D(3100.0f, 700.0f),
		FVector2D(2400.0f, -1000.0f), FVector2D(2400.0f, 1000.0f), FVector2D(0.0f, -1000.0f), FVector2D(0.0f, 1000.0f),
		FVector2D(-2400.0f, -1000.0f), FVector2D(-2400.0f, 1000.0f), FVector2D(-2400.0f, 0.0f)
	};
}

void AYSPirateShip::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSPirateShip, Motion);
	DOREPLIFETIME(AYSPirateShip, Health);
	DOREPLIFETIME(AYSPirateShip, bDisabled);
	DOREPLIFETIME(AYSPirateShip, bPlaytestMode);
	DOREPLIFETIME(AYSPirateShip, SinkStartServerTime);
	DOREPLIFETIME(AYSPirateShip, SinkStartLocation);
	DOREPLIFETIME(AYSPirateShip, SinkStartYaw);
	DOREPLIFETIME(AYSPirateShip, bSailsDeployed);
	DOREPLIFETIME(AYSPirateShip, ScaledMaxHealth);
}

void AYSPirateShip::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyHiddenComponents();
}

void AYSPirateShip::ApplyHiddenComponents()
{
	if (HiddenComponentNames.Num() == 0)
	{
		return;
	}
	TInlineComponentArray<UPrimitiveComponent*> Components(this);
	for (UPrimitiveComponent* Component : Components)
	{
		// noms des variables du Blueprint (sans le suffixe des gabarits)
		const FName Name(*Component->GetName().Replace(TEXT("_GEN_VARIABLE"), TEXT("")));
		if (HiddenComponentNames.Contains(Name))
		{
			Component->SetVisibility(false, true);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

bool AYSPirateShip::HasBrokenCombatPostNear(const FVector& Point, float Radius) const
{
	for (const AYSShipStation* Station : GetStations())
	{
		if (Station->IsA<AYSShipCannon>() && Station->IsBroken() && FVector::DistSquared2D(Station->GetActorLocation(), Point) <= FMath::Square(Radius))
		{
			return true;
		}
	}
	return false;
}

FVector AYSPirateShip::GetRamImpactPoint(const AYSPirateShip* Target) const
{
	if (!Target)
	{
		return GetActorLocation();
	}
	// le point de la coque de ce bateau le plus proche de l'autre (la proue en fonçant), puis le point de la coque de l'autre en face
	TArray<FVector> Points;
	GetWorldHullPoints(Points);
	FVector Best = GetActorLocation();
	double BestDistSq = TNumericLimits<double>::Max();
	for (const FVector& Point : Points)
	{
		const FVector OnTarget = Target->GetClosestHullPoint(Point);
		const double DistSq = FVector::DistSquared2D(OnTarget, Point);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = OnTarget;
		}
	}
	return Best;
}

void AYSPirateShip::BeginPlay()
{
	Super::BeginPlay();
	ApplyHiddenComponents();

	// mode playtest : il se déplace comme un bateau pilotable
	bDrivable |= bPlaytestMode;

	// pilotable : les clients extrapolent Motion (mouvement fluide sous les pieds des persos) au lieu de recevoir la position brute
	if (bDrivable)
	{
		SetReplicatingMovement(false);
		if (HasAuthority())
		{
			Motion.Location = GetActorLocation();
			Motion.Yaw = GetActorRotation().Yaw;
		}
	}

	// PV du bateau : donnés par le core
	if (HasAuthority())
	{
		Health = GetMaxHealth();
	}
	if (ShipHealthBar)
	{
		ShipHealthBar->InitWidget();
		if (UYSHealthBarWidget* Bar = Cast<UYSHealthBarWidget>(ShipHealthBar->GetUserWidgetObject()))
		{
			Bar->SetSizeMultiplier(ShipHealthBarScale);
		}
		UpdateShipHealthBar();
	}

	FString StationNames;
	for (const AYSShipStation* Station : GetStations())
	{
		StationNames += FString::Printf(TEXT(" %s"), *Station->GetClass()->GetName());
	}
	UE_LOG(LogTemp, Log, TEXT("[YSShip] %s : PV %.0f, core %s, postes :%s"), *GetName(), GetMaxHealth(), GetCore() ? *GetCore()->GetClass()->GetName() : TEXT("aucun"), *StationNames);
}

AYSShipHelm* AYSPirateShip::GetHelm() const
{
	return HelmStation ? Cast<AYSShipHelm>(HelmStation->GetChildActor()) : nullptr;
}

AYSShipCore* AYSPirateShip::GetCore() const
{
	return CoreStation ? Cast<AYSShipCore>(CoreStation->GetChildActor()) : nullptr;
}

AYSShipBoarding* AYSPirateShip::GetBoardingAccess() const
{
	return BoardingAccess ? Cast<AYSShipBoarding>(BoardingAccess->GetChildActor()) : nullptr;
}

AYSShipBoarding* AYSPirateShip::GetBoardingAccess(const FVector& From) const
{
	// tous les accès du bateau (bâbord, tribord, échelles de corde supplémentaires) : le plus proche
	AYSShipBoarding* Nearest = nullptr;
	double NearestDistSq = TNumericLimits<double>::Max();
	TInlineComponentArray<UChildActorComponent*> Components(this);
	for (const UChildActorComponent* Component : Components)
	{
		AYSShipBoarding* Access = Cast<AYSShipBoarding>(Component->GetChildActor());
		if (!Access)
		{
			continue;
		}
		const double DistSq = FVector::DistSquared2D(Access->GetInteractLocation(From), From);
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Access;
		}
	}
	return Nearest;
}

void AYSPirateShip::SetDormant(bool bInDormant)
{
	if (HasAuthority() && bDormant != bInDormant)
	{
		bDormant = bInDormant;
		UE_LOG(LogTemp, Log, TEXT("[YSShip] %s %s"), *GetName(), bDormant ? TEXT("en sommeil (immobile)") : TEXT("se réveille"));
	}
}

FVector AYSPirateShip::GetHullCenter() const
{
	const FBox2D Bounds = GetHullBounds();
	if (!Bounds.bIsValid)
	{
		return GetActorLocation();
	}
	const FVector2D Center = Bounds.GetCenter();
	return GetActorTransform().TransformPositionNoScale(FVector(Center.X, Center.Y, 0.0f));
}

void AYSPirateShip::SetRamming(bool bInRamming, AController* InInstigator)
{
	// pas d'abordage d'un bateau immobilisé (sommeil, abordé, hors d'usage, qui coule)
	const bool bRamming = bInRamming && !bDormant && !bGrappled && !bDisabled && !IsSinking() && GetWorld()->GetTimeSeconds() >= RamRecoilEndTime;
	if (!HasAuthority() || Motion.bRamming == bRamming)
	{
		return;
	}
	Motion.bRamming = bRamming;
	RamInstigator = InInstigator;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSShip] %s : « À l'abordage » %s"), *GetName(), bRamming ? TEXT("(plein gaz, cap figé)") : TEXT("relâché"));
}

void AYSPirateShip::ReceiveRamDamage(float Damage, AController* EventInstigator, const FVector& ImpactLocation)
{
	TakeCoreDamage(Damage, EventInstigator, ImpactLocation);
}

void AYSPirateShip::HandleRamImpact(AYSPirateShip* Target)
{
	if (!HasAuthority() || !IsValid(Target) || Target->IsSinking())
	{
		return;
	}
	// point touché : là où la coque (la proue) rencontre celle de l'autre bateau
	const FVector Impact = GetRamImpactPoint(Target);
	UE_LOG(LogTemp, Log, TEXT("[YSShip] %s éperonne %s (« À l'abordage ») : %.0f dégâts"), *GetName(), *Target->GetName(), RamDamage);
	Target->ReceiveRamDamage(RamDamage, RamInstigator.Get(), Impact);
	if (IsValid(Target) && !Target->IsSinking() && Target->TryStartBoardingBy(this, Impact))
	{
		SetRamming(false);
		UE_LOG(LogTemp, Log, TEXT("[YSShip] %s : point abordable, phase d'abordage de %s"), *GetName(), *Target->GetName());
		return;
	}
	// pas de planche : le bateau est repoussé en arrière, « À l'abordage » indisponible un moment (demande de Paul du 2026-11-10)
	Motion.bRamming = false;
	Motion.Speed = -RamRecoilSpeed;
	Motion.YawRate = 0.0f;
	RamRecoilEndTime = GetWorld()->GetTimeSeconds() + RamRecoilDuration;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSShip] %s : pas d'abordage, repoussé (%.0f cm/s en arrière, abordage bloqué %.1f s)"), *GetName(), RamRecoilSpeed, RamRecoilDuration);
}

bool AYSPirateShip::TryPushShip(AYSPirateShip* Other, const FYSShipMotion& From, const FYSShipMotion& To, FYSShipMotion& OutMotion)
{
	if (!HasAuthority() || !IsValid(Other) || Other->IsSinking() || Other->bGrappled || Weight <= Other->Weight)
	{
		return false;
	}
	// le plus lourd garde Weight / (Weight + l'autre) de son avancée et pousse l'autre d'autant
	const float Fraction = Weight / FMath::Max(Weight + Other->Weight, KINDA_SMALL_NUMBER);
	const FVector Delta = (To.Location - From.Location) * FVector(1.0f, 1.0f, 0.0f) * Fraction;
	if (Delta.IsNearlyZero(0.01f))
	{
		return false;
	}
	FYSShipMotion OtherNext = Other->Motion;
	OtherNext.Location += Delta;
	if (Other->FindBlockingShip(Other->Motion, OtherNext) || (!Other->IsOnPlaytestAutoPilot() && !Other->CanMoveHull(Other->Motion, OtherNext)))
	{
		return false;
	}
	Other->Motion.Location = OtherNext.Location;
	Other->SetActorLocation(FVector(OtherNext.Location.X, OtherNext.Location.Y, Other->GetActorLocation().Z));
	Other->ForceNetUpdate();
	OutMotion = To;
	OutMotion.Location = From.Location + Delta;
	UE_LOG(LogTemp, VeryVerbose, TEXT("[YSShip] %s (poids %.1f) pousse %s (poids %.1f) de %.1f cm"), *GetName(), Weight, *Other->GetName(), Other->Weight, Delta.Size());
	if (!bPushLogged)
	{
		bPushLogged = true;
		UE_LOG(LogTemp, Log, TEXT("[YSShip] %s (poids %.1f) pousse %s (poids %.1f), avance x%.2f"), *GetName(), Weight, *Other->GetName(), Other->Weight, Fraction);
	}
	return true;
}

bool AYSPirateShip::IsHullGrounded() const
{
	return GetWorld() && GetWorld()->GetTimeSeconds() - LastHullBlockedTime < 0.5;
}

bool AYSPirateShip::CanSailNow() const
{
	if ((!bDrivable && !IsFoeShip()) || bDisabled || bDormant || bGrappled || IsSinking())
	{
		return false;
	}
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	return !(YSGameState && YSGameState->IsHelmLockedFor(this));
}

void AYSPirateShip::UpdateSails()
{
	const int8 Wanted = bSailsDeployed ? 1 : 0;
	if (!bToggleSails || Wanted == AppliedSails)
	{
		return;
	}
	if (AppliedSails < 0)
	{
		TInlineComponentArray<USceneComponent*> Components(this);
		for (USceneComponent* Component : Components)
		{
			const FString Name = Component->GetName();
			if (!OpenSailTag.IsEmpty() && Name.Contains(OpenSailTag))
			{
				OpenSailComponents.Add(Component);
			}
			else if (!FurledSailTag.IsEmpty() && Name.Contains(FurledSailTag))
			{
				FurledSailComponents.Add(Component);
			}
		}
		// pas de voiles déployées dans ce Blueprint : on ne touche à rien (chaloupes, bateaux pas encore équipés)
		if (OpenSailComponents.Num() == 0)
		{
			FurledSailComponents.Reset();
		}
	}
	AppliedSails = Wanted;
	for (const TWeakObjectPtr<USceneComponent>& Component : OpenSailComponents)
	{
		if (Component.IsValid())
		{
			Component->SetVisibility(Wanted == 1);
		}
	}
	for (const TWeakObjectPtr<USceneComponent>& Component : FurledSailComponents)
	{
		if (Component.IsValid())
		{
			Component->SetVisibility(Wanted == 0);
		}
	}
}

void AYSPirateShip::UpdateBoardableOverlay()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextBoardableCheckTime)
	{
		return;
	}
	NextBoardableCheckTime = Now + 0.2;

	// état : core négatif, postes à 0 PV (positions dans le repère du bateau) ; rien = bande cachée
	const FBox2D Bounds = GetHullBounds();
	const bool bAll = Health < 0.0f;
	TArray<FVector2D> BrokenStations;
	FString Key;
	if (bShowBoardableZones && Bounds.bIsValid && GetMaxHealth() > 0.0f && !IsSinking() && IsPresent())
	{
		Key = bAll ? TEXT("all") : TEXT("");
		for (const AYSShipStation* Station : GetStations())
		{
			if (!bAll && Station && Station->IsBroken())
			{
				const FVector Local = GetActorTransform().InverseTransformPositionNoScale(Station->GetActorLocation());
				BrokenStations.Add(FVector2D(Local.X, Local.Y));
				Key += FString::Printf(TEXT("%s;"), *Station->GetName());
			}
		}
	}
	if (Key == BuiltBoardableKey)
	{
		return;
	}
	BuiltBoardableKey = Key;
	if (Key.IsEmpty())
	{
		if (BoardableOverlay)
		{
			BoardableOverlay->ClearAllMeshSections();
		}
		return;
	}
	if (!BoardableOverlay)
	{
		BoardableOverlay = NewObject<UProceduralMeshComponent>(this, TEXT("BoardableOverlay"));
		BoardableOverlay->SetupAttachment(GetRootComponent());
		BoardableOverlay->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BoardableOverlay->SetCastShadow(false);
		BoardableOverlay->bUseAsyncCooking = false;
		BoardableOverlay->RegisterComponent();
	}

	// tour du rectangle de la coque (comme CanBoardAt), par pas de 50 cm : points abordables, bande verticale entre deux points voisins
	const FVector2D Corners[4] = { Bounds.Min, FVector2D(Bounds.Max.X, Bounds.Min.Y), Bounds.Max, FVector2D(Bounds.Min.X, Bounds.Max.Y) };
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FLinearColor> Colors;
	const FLinearColor Color(BoardableColor.R * BoardableEmissive, BoardableColor.G * BoardableEmissive, BoardableColor.B * BoardableEmissive, BoardableColor.A);
	const FVector2D Center = Bounds.GetCenter();
	for (int32 Edge = 0; Edge < 4; ++Edge)
	{
		const FVector2D A = Corners[Edge];
		const FVector2D B = Corners[(Edge + 1) % 4];
		const int32 Steps = FMath::Max(1, FMath::CeilToInt32(FVector2D::Distance(A, B) / 50.0f));
		bool bPreviousIn = false;
		FVector2D Previous = A;
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			const FVector2D Point = FMath::Lerp(A, B, static_cast<float>(Step) / Steps);
			const bool bIn = bAll || BrokenStations.ContainsByPredicate([this, &Point](const FVector2D& Station)
			{
				return FVector2D::DistSquared(Station, Point) <= FMath::Square(BoardingStationRadius);
			});
			if (bIn && bPreviousIn)
			{
				// un peu hors de la coque, vers l'extérieur du rectangle
				const FVector2D Out = (FMath::Lerp(Previous, Point, 0.5f) - Center).GetSafeNormal();
				const FVector2D Normal = FMath::Abs(Out.X) * Bounds.GetExtent().Y > FMath::Abs(Out.Y) * Bounds.GetExtent().X ? FVector2D(FMath::Sign(Out.X), 0.0f) : FVector2D(0.0f, FMath::Sign(Out.Y));
				const FVector2D P0 = Previous + Normal * BoardableOutset;
				const FVector2D P1 = Point + Normal * BoardableOutset;
				const int32 Base = Vertices.Num();
				Vertices.Append({ FVector(P0.X, P0.Y, BoardableBottom), FVector(P1.X, P1.Y, BoardableBottom), FVector(P1.X, P1.Y, BoardableTop), FVector(P0.X, P0.Y, BoardableTop) });
				Colors.Append({ Color, Color, Color, Color });
				Triangles.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
			}
			bPreviousIn = bIn;
			Previous = Point;
		}
	}
	BoardableOverlay->CreateMeshSection_LinearColor(0, Vertices, Triangles, TArray<FVector>(), TArray<FVector2D>(), Colors, TArray<FProcMeshTangent>(), false);
	UMaterialInterface* Material = BoardableMaterial ? BoardableMaterial.Get() : LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Core/Ship/M_ShipPreview.M_ShipPreview"));
	BoardableOverlay->SetMaterial(0, Material);
	UE_LOG(LogTemp, Log, TEXT("[YSShip] %s : zones abordables affichées (%s, %d morceau(x))"), *GetName(), bAll ? TEXT("toute la coque, core négatif") : *FString::Printf(TEXT("%d poste(s) à 0 PV"), BrokenStations.Num()), Triangles.Num() / 6);
}

void AYSPirateShip::SetGrappled(bool bInGrappled)
{
	if (HasAuthority() && bGrappled != bInGrappled)
	{
		bGrappled = bInGrappled;
		Motion.bRamming = false;
		Motion.Speed = 0.0f;
		Motion.YawRate = 0.0f;
		SetPilotInput(0.0f, 0.0f);
		ForceNetUpdate();
		UE_LOG(LogTemp, Log, TEXT("[YSShip] %s %s"), *GetName(), bGrappled ? TEXT("abordé : immobilisé") : TEXT("libéré de l'abordage"));
	}
}

FVector AYSPirateShip::GetRouteAroundHull(const FVector& From, const FVector& To, float Margin) const
{
	const FBox2D Bounds = GetHullBounds();
	if (!Bounds.bIsValid)
	{
		return To;
	}
	const FTransform& Transform = GetActorTransform();
	const FVector LocalFrom = Transform.InverseTransformPositionNoScale(From);
	const FVector LocalTo = Transform.InverseTransformPositionNoScale(To);

	// déjà du côté de To (au-delà du flanc) : tout droit
	const float Side = LocalTo.Y >= Bounds.GetCenter().Y ? 1.0f : -1.0f;
	const float Flank = Side > 0.0f ? Bounds.Max.Y : Bounds.Min.Y;
	if ((LocalFrom.Y - Flank) * Side >= 0.0f)
	{
		return To;
	}

	// le tour le plus court : par la proue ou par la poupe ; d'abord le long du flanc, puis en travers une fois la coque dépassée
	const float Bow = Bounds.Max.X + Margin;
	const float Stern = Bounds.Min.X - Margin;
	const float EndX = FMath::Abs(LocalFrom.X - Bow) + FMath::Abs(LocalTo.X - Bow) < FMath::Abs(LocalFrom.X - Stern) + FMath::Abs(LocalTo.X - Stern) ? Bow : Stern;
	const bool bPastHull = LocalFrom.X >= Bow - 1.0f || LocalFrom.X <= Stern + 1.0f;
	return Transform.TransformPositionNoScale(FVector(EndX, bPastHull ? Flank + Side * Margin : LocalFrom.Y, LocalFrom.Z));
}

TArray<AYSShipStation*> AYSPirateShip::GetStations() const
{
	TArray<AYSShipStation*> Stations;
	TInlineComponentArray<UChildActorComponent*> Components(this);
	for (const UChildActorComponent* Component : Components)
	{
		if (AYSShipStation* Station = Cast<AYSShipStation>(Component->GetChildActor()))
		{
			Stations.Add(Station);
		}
		// poste de combat posé sur un réceptacle
		else if (const AYSCombatPostReceptacle* Receptacle = Cast<AYSCombatPostReceptacle>(Component->GetChildActor()))
		{
			if (AYSShipStation* Post = Receptacle->GetInstalledPost())
			{
				Stations.Add(Post);
			}
		}
	}
	return Stations;
}

TArray<AYSCombatPostReceptacle*> AYSPirateShip::GetCombatPostReceptacles() const
{
	TArray<AYSCombatPostReceptacle*> Receptacles;
	TInlineComponentArray<UChildActorComponent*> Components(this);
	for (const UChildActorComponent* Component : Components)
	{
		if (AYSCombatPostReceptacle* Receptacle = Cast<AYSCombatPostReceptacle>(Component->GetChildActor()))
		{
			Receptacles.Add(Receptacle);
		}
	}
	return Receptacles;
}

TArray<UChildActorComponent*, TInlineAllocator<8>> AYSPirateShip::GetCannonComponents() const
{
	TArray<UChildActorComponent*, TInlineAllocator<8>> Cannons;
	for (UChildActorComponent* Component : { CannonFront.Get(), CannonLeft.Get(), CannonRight.Get(), CannonLeft2.Get(), CannonRight2.Get(), CannonFront2.Get() })
	{
		// emplacement vide (Cannon Front 2 par défaut) : pas un canon
		if (Component && Component->GetChildActorClass())
		{
			Cannons.Add(Component);
		}
	}
	return Cannons;
}

void AYSPirateShip::UpdateShipHealthBar()
{
	const float MaxHealth = GetMaxHealth();
	if (!ShipHealthBar || FMath::IsNearlyEqual(Health, DisplayedShipHealth) || MaxHealth <= 0.0f)
	{
		return;
	}
	DisplayedShipHealth = Health;
	if (UYSHealthBarWidget* Bar = Cast<UYSHealthBarWidget>(ShipHealthBar->GetUserWidgetObject()))
	{
		// PV reçus du serveur (clients : 0 avant le premier envoi) ; hors d'usage : affichée même à 0 et en négatif
		Bar->SetHealth(Health, MaxHealth, bDisabled);
	}
}

void AYSPirateShip::TakeCoreDamage(float Damage, AController* EventInstigator, const FVector& ImpactLocation)
{
	const float MaxHealth = GetMaxHealth();
	if (!HasAuthority() || Damage <= 0.0f || MaxHealth <= 0.0f || IsActorBeingDestroyed() || IsSinking())
	{
		return;
	}

	Health -= Damage;
	UE_LOG(LogTemp, Log, TEXT("[YSShip] %s : core -%.0f -> %.0f / %.0f"), *GetName(), Damage, Health, MaxHealth);
	ForceNetUpdate();
	AYSGameState::ReportDamage(this, Damage, ImpactLocation, EventInstigator);

	if (!bDisabled && Health <= 0.0f)
	{
		bDisabled = true;
		UE_LOG(LogTemp, Log, TEXT("[YSShip] %s : core à 0 PV, bateau hors d'usage"), *GetName());
		OnShipDisabled();
	}
	if (Health <= -MaxHealth)
	{
		SinkAtSea(*FString::Printf(TEXT("core %.0f / -%.0f"), Health, MaxHealth));
	}
}

void AYSPirateShip::SinkAtSea(const TCHAR* Reason)
{
	if (!HasAuthority() || IsActorBeingDestroyed() || IsSinking())
	{
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSShip] %s coule (%s) en %.0f s"), *GetName(), Reason, SinkDuration);
	// bateau des joueurs coulé : game over (demande de Paul, 2026-11-08)
	const AYSGameState* SinkGameState = GetWorld()->GetGameState<AYSGameState>();
	if (SinkGameState && SinkGameState->GetGameBoat() == this)
	{
		if (AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>())
		{
			GameMode->GameOver(TEXT("le bateau des joueurs a coulé"));
		}
	}
	OnShipDestroyedByDamage();

	// hors d'usage : postes vidés, plus de commandes
	if (!bDisabled)
	{
		bDisabled = true;
		OnShipDisabled();
	}
	SetPilotInput(0.0f, 0.0f);
	Motion.Speed = 0.0f;
	Motion.YawRate = 0.0f;

	// objets posés sur le pont : un cadavre flotte, le reste coule avec le bateau
	TArray<AActor*> Attached;
	GetAttachedActors(Attached, true, true);
	for (AActor* Actor : Attached)
	{
		AYSInteractiveObject* Object = Cast<AYSInteractiveObject>(Actor);
		if (!Object || Object->GetParentActor())
		{
			continue;
		}
		if (Object->FloatsOnWater())
		{
			Object->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			const FVector Location = Object->GetActorLocation();
			Object->SetActorLocation(FVector(Location.X, Location.Y, AYSOcean::ClampAboveSea(GetWorld(), -1.0e6f)));
		}
		else
		{
			Object->Destroy();
		}
	}

	if (SinkDuration <= 0.0f)
	{
		Destroy();
		return;
	}
	// coulaison progressive, calculée sur chaque machine depuis l'heure serveur
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	SinkStartServerTime = FMath::Max(GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds(), UE_KINDA_SMALL_NUMBER);
	SinkStartLocation = GetActorLocation();
	SinkStartYaw = GetActorRotation().Yaw;
	ForceNetUpdate();
}

void AYSPirateShip::UpdateSink()
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const float Alpha = SinkDuration > 0.0f ? FMath::Clamp(static_cast<float>(Now - SinkStartServerTime) / SinkDuration, 0.0f, 1.0f) : 1.0f;
	const float Eased = FMath::Pow(Alpha, SinkEaseExponent);
	const float Depth = SinkDepth * Eased;
	const FVector Location = SinkStartLocation - FVector(0.0f, 0.0f, Depth);
	// la proue pique en même temps qu'il descend (la poupe ne remonte jamais au-dessus de sa hauteur de départ pour un petit angle)
	const FRotator Rotation(-SinkPitch * Eased, SinkStartYaw, 0.0f);
	if (!GetActorLocation().Equals(Location, 0.1f) || !GetActorRotation().Equals(Rotation, 0.01f))
	{
		SetActorLocationAndRotation(Location, Rotation);
	}
	// le pont et le gaillard sont sous les nageurs : plus rien ne les touche
	if (Depth >= SinkCollisionOffDepth && GetActorEnableCollision())
	{
		SetActorEnableCollision(false);
		TArray<AActor*> Attached;
		GetAttachedActors(Attached, true, true);
		for (AActor* Actor : Attached)
		{
			if (!Actor->IsA<APawn>())
			{
				Actor->SetActorEnableCollision(false);
			}
		}
		UE_LOG(LogTemp, Verbose, TEXT("[YSShip] %s : coulé de %.0f cm, plus de collision"), *GetName(), Depth);
	}
	if (HasAuthority() && Alpha >= 1.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSShip] %s a fini de couler : détruit"), *GetName());
		Destroy();
	}
}

void AYSPirateShip::OnShipDisabled()
{
	for (AYSShipStation* Station : GetStations())
	{
		if (AYSPlayerCharacter* Occupant = Station->GetOccupant())
		{
			Occupant->LeaveStation(TEXT("bateau hors d'usage"), true);
		}
	}
}

float AYSPirateShip::GetMaxHealth() const
{
	// calculé en jeu (Galion de NME selon le jour), sinon réglé sur le bateau, sinon par le core
	if (ScaledMaxHealth > 0.0f)
	{
		return ScaledMaxHealth;
	}
	if (ShipMaxHealth > 0.0f)
	{
		return ShipMaxHealth;
	}
	const AYSShipCore* Core = GetCore();
	return Core ? Core->GetShipMaxHealth() : 0.0f;
}

void AYSPirateShip::SetScaledMaxHealth(float NewMaxHealth)
{
	if (!HasAuthority() || NewMaxHealth <= 0.0f)
	{
		return;
	}
	const float OldMax = GetMaxHealth();
	ScaledMaxHealth = NewMaxHealth;
	// même part de PV (au départ : plein)
	Health = OldMax > 0.0f && HasActorBegunPlay() ? Health / OldMax * NewMaxHealth : NewMaxHealth;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSShip] %s : PV max %.0f -> %.0f (%.0f PV)"), *GetName(), OldMax, NewMaxHealth, Health);
}

void AYSPirateShip::SyncMotionToActor()
{
	if (!HasAuthority())
	{
		return;
	}
	Motion.Location = GetActorLocation();
	Motion.Yaw = GetActorRotation().Yaw;
	Motion.Speed = 0.0f;
	Motion.YawRate = 0.0f;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSShip] %s : pilotage recalé sur sa position (%s, yaw %.0f)"), *GetName(), *Motion.Location.ToCompactString(), Motion.Yaw);
}

bool AYSPirateShip::IsOnPlaytestAutoPilot() const
{
	const AYSShipHelm* Helm = GetHelm();
	return bPlaytestMode && !(Helm && Helm->GetOccupant());
}

void AYSPirateShip::SetPlaytestMode(bool bEnabled)
{
	if (HasAuthority() && bDrivable)
	{
		bPlaytestMode = bEnabled;
		ForceNetUpdate();
		UE_LOG(LogTemp, Log, TEXT("[YSShip] %s : mode playtest %s"), *GetName(), bEnabled ? TEXT("activé") : TEXT("désactivé"));
	}
}

bool AYSPirateShip::ComputeAutoPilot(float& OutThrottle, float& OutSteer)
{
	if (!IsOnPlaytestAutoPilot())
	{
		return false;
	}

	const AYSShipPath* Path = PlaytestPath;
	if (!Path)
	{
		TActorIterator<AYSShipPath> It(GetWorld());
		Path = It ? *It : nullptr;
	}
	FVector Ahead;
	if (!Path || !Path->GetPointAhead(GetActorLocation(), PlaytestLookAhead, Ahead))
	{
		if (!bPlaytestPathMissingLogged)
		{
			bPlaytestPathMissingLogged = true;
			UE_LOG(LogTemp, Warning, TEXT("[YSShip] %s : mode playtest sans trajet (poser un AYSShipPath dans la map)"), *GetName());
		}
		return false;
	}

	OutThrottle = 1.0f;
	OutSteer = ComputeSteerTowards(Ahead - GetActorLocation(), PlaytestSteerFullAngle);
	return true;
}

float AYSPirateShip::ComputeSteerTowards(const FVector& Direction, float FullSteerAngle) const
{
	const float Delta = FRotator::NormalizeAxis(Direction.Rotation().Yaw - Motion.Yaw);
	return FMath::RoundToFloat(FMath::Clamp(Delta / FMath::Max(FullSteerAngle, 1.0f), -1.0f, 1.0f) * 10.0f) / 10.0f;
}

void AYSPirateShip::SetPilotInput(float Throttle, float Steer)
{
	if (!HasAuthority())
	{
		return;
	}
	// en sommeil ou abordé : plus de commandes
	if (bDormant || bGrappled)
	{
		Throttle = 0.0f;
		Steer = 0.0f;
	}
	Motion.Throttle = FMath::Clamp(Throttle, -1.0f, 1.0f);
	Motion.Steer = FMath::Clamp(Steer, -1.0f, 1.0f);
	if (Motion.Throttle != LastSentThrottle || Motion.Steer != LastSentSteer)
	{
		LastSentThrottle = Motion.Throttle;
		LastSentSteer = Motion.Steer;
		UE_LOG(LogTemp, Verbose, TEXT("[YSShip] %s : barre Z/S %.0f, Q/D %.0f (vitesse %.0f)"), *GetName(), Motion.Throttle, Motion.Steer, Motion.Speed);
		ForceNetUpdate();
	}
}

void AYSPirateShip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// bateau objectif : présent seulement une fois l'objectif de doublons atteint (GameMap)
	if (bObjectiveShip)
	{
		const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
		const int8 Presence = !YSGameState || YSGameState->IsShipPresent() ? 1 : 0;
		if (Presence != AppliedPresence)
		{
			AppliedPresence = Presence;
			ApplyPresence(Presence == 1);
			// caché, il ne simulait pas son pilotage : il apparaît là où il a été posé (port), pas à sa position du BeginPlay
			if (Presence == 1 && bDrivable)
			{
				SyncMotionToActor();
			}
			UE_LOG(LogTemp, Log, TEXT("[YSShip] %s %s"), *GetName(), Presence ? TEXT("présent") : TEXT("absent (caché, sans collision)"));
		}
	}

	UpdateShipHealthBar();

	// voiles déployées = il peut naviguer, carguées = bloqué
	if (HasAuthority())
	{
		const bool bCanSail = CanSailNow();
		if (bCanSail != bSailsDeployed)
		{
			bSailsDeployed = bCanSail;
			ForceNetUpdate();
			UE_LOG(LogTemp, Log, TEXT("[YSShip] %s : voiles %s"), *GetName(), bSailsDeployed ? TEXT("déployées (peut naviguer)") : TEXT("carguées (bloqué)"));
		}
	}
	UpdateSails();
	UpdateBoardableOverlay();

	// coulé : il descend, plus de pilotage
	if (IsSinking())
	{
		UpdateSink();
		return;
	}

	if (bDrivable && !IsHidden())
	{
		if (HasAuthority())
		{
			TickServerMotion(DeltaSeconds);
		}
		else
		{
			TickClientMotion(DeltaSeconds);
		}
	}
}

void AYSPirateShip::ApplyPresence(bool bPresent)
{
	SetActorHiddenInGame(!bPresent);
	SetActorEnableCollision(bPresent);
	// postes (acteurs enfants) et ce qui est accroché au bateau : cachés avec lui
	TArray<AActor*> Attached;
	GetAttachedActors(Attached, true, true);
	for (AActor* Actor : Attached)
	{
		if (Actor->IsA<AYSShipStation>() || Actor->IsA<AYSShipCore>() || Actor->IsA<AYSShipBoarding>())
		{
			Actor->SetActorHiddenInGame(!bPresent);
			Actor->SetActorEnableCollision(bPresent);
		}
	}
}

void AYSPirateShip::StepMotion(FYSShipMotion& State, float DeltaSeconds) const
{
	// stats du core (multiplicateurs, mêmes valeurs sur toutes les machines : acteur local créé depuis le Blueprint)
	const AYSShipCore* Core = GetCore();
	// (Navigateur dans l'équipage : vitesses max du bateau des joueurs augmentées)
	const float SpeedMultiplier = (Core ? Core->GetSpeedMultiplier() : 1.0f) * AYSBoatCrew::GetShipSpeedMultiplier(this);
	const float AccelerationMultiplier = Core ? Core->GetAccelerationMultiplier() : 1.0f;
	const float TurnMultiplier = Core ? Core->GetTurnRateMultiplier() : 1.0f;
	// « À l'abordage » : plein gaz, vitesse max (et accélération) x Ram Speed Multiplier, plus de rotation
	const float RamMultiplier = State.bRamming ? RamSpeedMultiplier : 1.0f;
	const float MaxForward = MaxForwardSpeed * SpeedMultiplier * RamMultiplier * (IsOnPlaytestAutoPilot() ? PlaytestSpeedMultiplier : 1.0f);
	const float MaxReverse = MaxReverseSpeed * SpeedMultiplier;
	const float ForwardRate = ForwardAcceleration * AccelerationMultiplier * RamMultiplier;
	const float ReverseRate = ReverseAcceleration * AccelerationMultiplier;
	const float Throttle = State.bRamming ? 1.0f : State.Throttle;

	// vitesse : Z accélère vers l'avant, S freine puis recule, sans commande le bateau glisse et ralentit (inertie)
	if (Throttle > 0.0f)
	{
		const float Rate = State.Speed < 0.0f ? FMath::Max(ForwardRate, Deceleration) : ForwardRate;
		// (au-delà du max, abordage relâché : redescend comme sans commande)
		State.Speed = State.Speed > MaxForward ? FMath::FInterpConstantTo(State.Speed, MaxForward, DeltaSeconds, Deceleration)
			: FMath::Min(State.Speed + Rate * Throttle * DeltaSeconds, MaxForward);
	}
	else if (Throttle < 0.0f)
	{
		const float Rate = State.Speed > 0.0f ? FMath::Max(ReverseRate, Deceleration) : ReverseRate;
		State.Speed = FMath::Max(State.Speed + Rate * Throttle * DeltaSeconds, -MaxReverse);
	}
	else
	{
		State.Speed = FMath::FInterpConstantTo(State.Speed, 0.0f, DeltaSeconds, Deceleration);
	}

	// rotation : la barre donne une vitesse de rotation visée (plus faible à basse vitesse, inversée en marche arrière),
	// suivie avec inertie
	const float SpeedRatio = FMath::Clamp(FMath::Abs(State.Speed) / FullTurnSpeed, 0.0f, 1.0f);
	const float TurnRatio = FMath::Lerp(StandstillTurnRatio, 1.0f, SpeedRatio);
	const float TargetYawRate = State.Steer * MaxTurnRate * TurnMultiplier * TurnRatio * (State.Speed < -1.0f ? -1.0f : 1.0f);
	State.YawRate = State.bRamming ? 0.0f : TurnResponsiveness > 0.0f ? FMath::FInterpTo(State.YawRate, TargetYawRate, DeltaSeconds, TurnResponsiveness) : TargetYawRate;

	State.Yaw = FRotator::NormalizeAxis(State.Yaw + State.YawRate * DeltaSeconds);
	State.Location += FRotator(0.0f, State.Yaw, 0.0f).Vector() * State.Speed * DeltaSeconds;

	// forces extérieures (Kraken) : dérive tant qu'elle est posée, poussée amortie
	State.Location += (State.Drift + State.Knockback) * FVector(1.0f, 1.0f, 0.0f) * DeltaSeconds;
	if (!State.Knockback.IsNearlyZero(1.0f))
	{
		State.Knockback *= FMath::Exp(-KnockbackDamping * DeltaSeconds);
	}
	else
	{
		State.Knockback = FVector::ZeroVector;
	}
}

void AYSPirateShip::SetExternalDrift(const FVector& Velocity)
{
	if (!HasAuthority() || Motion.Drift.Equals(Velocity, 1.0f))
	{
		return;
	}
	Motion.Drift = Velocity * FVector(1.0f, 1.0f, 0.0f);
	ForceNetUpdate();
}

void AYSPirateShip::ApplyKnockback(const FVector& Velocity)
{
	if (!HasAuthority())
	{
		return;
	}
	Motion.Knockback += Velocity * FVector(1.0f, 1.0f, 0.0f);
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSShip] %s : poussé à %.0f cm/s (%s)"), *GetName(), Motion.Knockback.Size2D(), *Velocity.GetSafeNormal2D().ToCompactString());
}

bool AYSPirateShip::FindWaterPlacement(const FVector& From, const FVector& Direction, float Yaw, float MaxDistance, float Step, FVector& OutLocation) const
{
	for (float Distance = 0.0f; Distance <= MaxDistance; Distance += FMath::Max(Step, 1.0f))
	{
		const FVector Location = From + Direction * Distance;
		if (GetHullMinDepth(Location, Yaw) >= MinWaterDepth)
		{
			OutLocation = Location;
			return true;
		}
	}
	return false;
}

float AYSPirateShip::GetHullMinDepth(const FVector& Location, float Yaw) const
{
	const UWorld* World = GetWorld();
	const AYSOcean* Ocean = AYSOcean::Find(World);
	float MinDepth = YSPirateShip::DepthTraceDown;
	if (!Ocean)
	{
		return MinDepth;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSShipDepth), false, this);
	TArray<AActor*> OwnActors;
	GetAttachedActors(OwnActors, true, true);
	Params.AddIgnoredActors(OwnActors);
	const float Sea = Ocean->GetSeaLevel();
	const FRotator Rotation(0.0f, Yaw, 0.0f);
	for (const FVector2D& Probe : HullProbePoints)
	{
		const FVector Point = Location + Rotation.RotateVector(FVector(Probe.X, Probe.Y, 0.0f));
		// les autres bateaux (chaloupe le long de la coque, leurs postes) ne sont pas le fond : traversés
		FCollisionQueryParams ProbeParams = Params;
		for (int32 Attempt = 0; Attempt <= YSPirateShip::MaxIgnoredShips; ++Attempt)
		{
			FHitResult Hit;
			if (!World->LineTraceSingleByObjectType(Hit, FVector(Point.X, Point.Y, Sea + YSPirateShip::DepthTraceUp),
				FVector(Point.X, Point.Y, Sea - YSPirateShip::DepthTraceDown), FCollisionObjectQueryParams(ECC_WorldStatic), ProbeParams))
			{
				break;
			}
			if (AYSPirateShip* OtherShip = GetShipOf(Hit.GetActor()))
			{
				TArray<AActor*> OtherActors;
				OtherShip->GetAttachedActors(OtherActors, true, true);
				ProbeParams.AddIgnoredActor(OtherShip);
				ProbeParams.AddIgnoredActors(OtherActors);
				continue;
			}
			MinDepth = FMath::Min(MinDepth, Sea - Hit.ImpactPoint.Z);
			break;
		}
	}
	return MinDepth;
}

float AYSPirateShip::GetHullPenetration(const FVector& Location, float Yaw, const AYSPirateShip* Other) const
{
	const FBox2D BoundsA = GetHullBounds();
	const FBox2D BoundsB = Other->GetHullBounds();
	if (!BoundsA.bIsValid || !BoundsB.bIsValid)
	{
		return 0.0f;
	}
	const FVector LocationB = Other->GetActorLocation();
	const float Reach = BoundsA.GetExtent().Size() + BoundsB.GetExtent().Size() + BoundsA.GetCenter().Size() + BoundsB.GetCenter().Size();
	if (FVector::DistSquared2D(Location, LocationB) > FMath::Square(Reach))
	{
		return 0.0f;
	}

	// deux rectangles orientés : séparés si un axe de l'un ou de l'autre les sépare ; enfoncement = le plus petit recouvrement
	const auto GetCorners = [](const FBox2D& Bounds, const FVector& Origin, float CornersYaw, FVector2D (&OutCorners)[4])
	{
		const FRotator Rotation(0.0f, CornersYaw, 0.0f);
		const FVector2D Local[4] = { Bounds.Min, FVector2D(Bounds.Min.X, Bounds.Max.Y), Bounds.Max, FVector2D(Bounds.Max.X, Bounds.Min.Y) };
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const FVector World = Origin + Rotation.RotateVector(FVector(Local[Index].X, Local[Index].Y, 0.0f));
			OutCorners[Index] = FVector2D(World.X, World.Y);
		}
	};
	FVector2D CornersA[4];
	FVector2D CornersB[4];
	GetCorners(BoundsA, Location, Yaw, CornersA);
	GetCorners(BoundsB, LocationB, Other->GetActorRotation().Yaw, CornersB);

	float Penetration = TNumericLimits<float>::Max();
	for (const FVector2D* Corners : { &CornersA[0], &CornersB[0] })
	{
		for (int32 Edge = 0; Edge < 2; ++Edge)
		{
			const FVector2D Axis = (Corners[Edge + 1] - Corners[Edge]).GetSafeNormal();
			float MinA = TNumericLimits<float>::Max();
			float MaxA = -MinA;
			float MinB = MinA;
			float MaxB = -MinA;
			for (int32 Index = 0; Index < 4; ++Index)
			{
				const float ProjectionA = FVector2D::DotProduct(CornersA[Index], Axis);
				const float ProjectionB = FVector2D::DotProduct(CornersB[Index], Axis);
				MinA = FMath::Min(MinA, ProjectionA);
				MaxA = FMath::Max(MaxA, ProjectionA);
				MinB = FMath::Min(MinB, ProjectionB);
				MaxB = FMath::Max(MaxB, ProjectionB);
			}
			const float Overlap = FMath::Min(MaxA, MaxB) - FMath::Max(MinA, MinB);
			if (Overlap <= 0.0f)
			{
				return 0.0f;
			}
			Penetration = FMath::Min(Penetration, Overlap);
		}
	}
	return Penetration;
}

AYSPirateShip* AYSPirateShip::FindBlockingShip(const FYSShipMotion& From, const FYSShipMotion& To) const
{
	for (TActorIterator<AYSPirateShip> It(GetWorld()); It; ++It)
	{
		AYSPirateShip* Other = *It;
		if (Other == this || !Other->IsPresent() || Other->IsActorBeingDestroyed()
			|| (IsFoeShip() == Other->IsFoeShip() && (IgnoresAlliedShips() || Other->IgnoresAlliedShips())))
		{
			continue;
		}
		const float ToPenetration = GetHullPenetration(To.Location, To.Yaw, Other);
		if (ToPenetration > 0.0f && ToPenetration >= GetHullPenetration(From.Location, From.Yaw, Other) - KINDA_SMALL_NUMBER)
		{
			return Other;
		}
	}
	return nullptr;
}

bool AYSPirateShip::FindBoardingPoint(const FVector& From, FVector& OutPoint) const
{
	if (Health < 0.0f)
	{
		OutPoint = GetClosestHullPoint(From);
		return true;
	}
	bool bFound = false;
	float BestDistSq = TNumericLimits<float>::Max();
	for (const AYSShipStation* Station : GetStations())
	{
		if (Station->IsBroken())
		{
			const FVector Point = GetClosestHullPoint(Station->GetActorLocation());
			const float DistSq = FVector::DistSquared2D(Point, From);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				OutPoint = Point;
				bFound = true;
			}
		}
	}
	return bFound;
}

bool AYSPirateShip::CanBoardAt(const FVector& ContactPoint, bool bFoeBoarder) const
{
	if (bFoeBoarder == IsFoeShip() || Health < 0.0f)
	{
		return true;
	}
	for (const AYSShipStation* Station : GetStations())
	{
		if (Station->IsBroken() && FVector::DistSquared2D(GetClosestHullPoint(Station->GetActorLocation()), ContactPoint) <= FMath::Square(BoardingStationRadius))
		{
			return true;
		}
	}
	return false;
}

bool AYSPirateShip::CanMoveHull(const FYSShipMotion& From, const FYSShipMotion& To) const
{
	// eau assez profonde partout, ou au moins pas moins profonde qu'avant (un bateau posé trop près du bord peut s'en éloigner)
	const float ToDepth = GetHullMinDepth(To.Location, To.Yaw);
	return ToDepth >= MinWaterDepth || ToDepth >= GetHullMinDepth(From.Location, From.Yaw) - KINDA_SMALL_NUMBER;
}

void AYSPirateShip::TickServerMotion(float DeltaSeconds)
{
	// en sommeil (bataille navale pas activée) ou abordé : immobile
	if (bDormant || bGrappled)
	{
		SetPilotInput(0.0f, 0.0f);
		if (!FMath::IsNearlyZero(Motion.Speed) || !FMath::IsNearlyZero(Motion.YawRate))
		{
			Motion.Speed = 0.0f;
			Motion.YawRate = 0.0f;
			ForceNetUpdate();
		}
		const AGameStateBase* FrozenGameState = GetWorld()->GetGameState();
		Motion.ServerTime = FrozenGameState ? FrozenGameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
		return;
	}

	// pilote automatique (mode playtest, NME Jolly Ship) ; sinon, le pilote parti, la barre ne commande plus
	const AYSShipHelm* Helm = GetHelm();
	float AutoThrottle = 0.0f;
	float AutoSteer = 0.0f;
	if (!bDisabled && ComputeAutoPilot(AutoThrottle, AutoSteer))
	{
		SetPilotInput(AutoThrottle, AutoSteer);
	}
	else if (bDisabled || !Helm || !Helm->GetOccupant())
	{
		SetPilotInput(0.0f, 0.0f);
	}
	// le trajet du mode playtest est réglé à la main : pas d'arrêt contre le fond (ponton, hauts-fonds)
	const bool bCheckHull = !IsOnPlaytestAutoPilot();

	const bool bWasMoving = !FMath::IsNearlyZero(Motion.Speed) || !FMath::IsNearlyZero(Motion.YawRate);
	float Remaining = DeltaSeconds;
	while (Remaining > KINDA_SMALL_NUMBER)
	{
		const float Step = FMath::Min(Remaining, YSPirateShip::MaxStep);
		Remaining -= Step;

		FYSShipMotion Next = Motion;
		StepMotion(Next, Step);
		// fond trop haut (plage, ponton) ou autre bateau : le bateau s'arrête net ; il peut toujours repartir vers l'eau profonde / se dégager
		AYSPirateShip* BlockingShip = FindBlockingShip(Motion, Next);

		// « À l'abordage » : premier contact avec un bateau (même un plus léger qu'il pousse) = éperonnage (dégâts, abordage ou recul)
		if (!BlockingShip)
		{
			LastRamTarget.Reset();
		}
		else if (Motion.bRamming && LastRamTarget.Get() != BlockingShip)
		{
			LastRamTarget = BlockingShip;
			HandleRamImpact(BlockingShip);
			if (!IsValid(this) || IsActorBeingDestroyed() || bGrappled)
			{
				break;
			}
			// recul : l'état a changé, ce pas est refait avec la nouvelle vitesse
			Remaining += Step;
			continue;
		}

		// poids : un bateau plus lourd pousse le plus léger (Galion > Brigantin > Sloop > Jolly Ship), en ralentissant
		FYSShipMotion Pushed;
		if (BlockingShip && TryPushShip(BlockingShip, Motion, Next, Pushed) && (!bCheckHull || CanMoveHull(Motion, Pushed)))
		{
			Motion = Pushed;
			bGrounded = false;
			continue;
		}

		const bool bBlocked = BlockingShip || (bCheckHull && !CanMoveHull(Motion, Next));
		if (bBlocked)
		{
			// la rotation seule passe peut-être encore
			FYSShipMotion Turned = Motion;
			Turned.Yaw = Next.Yaw;
			Turned.YawRate = Next.YawRate;
			Turned.Speed = 0.0f;
			const bool bTurnOk = !FindBlockingShip(Motion, Turned) && (!bCheckHull || CanMoveHull(Motion, Turned));
			// gaz maintenu contre un obstacle : il repart à peine et se rebloque, un seul choc
			if (FMath::Abs(Motion.Speed) < YSPirateShip::MinImpactSpeed)
			{
				bGrounded = bGrounded || BlockingShip != nullptr;
			}
			if (!BlockingShip)
			{
				LastHullBlockedTime = GetWorld()->GetTimeSeconds();
			}
			Motion.Speed = 0.0f;
			Motion.YawRate = bTurnOk ? Next.YawRate : 0.0f;
			Motion.Yaw = bTurnOk ? Next.Yaw : Motion.Yaw;
			if (!bGrounded)
			{
				bGrounded = true;
				UE_LOG(LogTemp, Log, TEXT("[YSShip] %s s'arrête : %s (%s)"), *GetName(), BlockingShip ? *FString::Printf(TEXT("percute %s"), *BlockingShip->GetName()) : TEXT("eau trop peu profonde devant"), *Motion.Location.ToCompactString());
				ForceNetUpdate();
				if (BlockingShip)
				{
					OnShipCollision(BlockingShip);
				}
				else
				{
					OnHullBlocked();
				}
				if (!IsValid(this) || IsActorBeingDestroyed())
				{
					return;
				}
			}
		}
		else
		{
			Motion = Next;
			bGrounded = false;
		}
	}

	const AGameStateBase* GameState = GetWorld()->GetGameState();
	Motion.ServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	Motion.Location.Z = GetActorLocation().Z;
	if (bWasMoving || !GetActorLocation().Equals(Motion.Location, 0.1f))
	{
		SetActorLocationAndRotation(Motion.Location, FRotator(0.0f, Motion.Yaw, 0.0f));
		// une ligne par seconde en mouvement
		if (Motion.ServerTime - LastMotionLogTime >= 1.0)
		{
			LastMotionLogTime = Motion.ServerTime;
			UE_LOG(LogTemp, Verbose, TEXT("[YSShip] %s en (%.0f, %.0f) yaw %.1f, vitesse %.0f, rotation %.1f°/s"), *GetName(), Motion.Location.X, Motion.Location.Y, Motion.Yaw, Motion.Speed, Motion.YawRate);
		}
	}
}

void AYSPirateShip::OnRep_Motion()
{
	if (!bDrivable || HasAuthority())
	{
		return;
	}

	// nouvel état : l'écart avec ce qui était affiché est rattrapé en douceur (pas de saut sous les pieds des persos)
	const FVector Displayed = GetActorLocation();
	const float DisplayedYaw = GetActorRotation().Yaw;
	ClientPredicted = Motion;
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const float Elapsed = GameState ? FMath::Clamp(static_cast<float>(GameState->GetServerWorldTimeSeconds() - Motion.ServerTime), 0.0f, 0.5f) : 0.0f;
	for (float Remaining = Elapsed; Remaining > KINDA_SMALL_NUMBER; Remaining -= YSPirateShip::MaxStep)
	{
		StepMotion(ClientPredicted, FMath::Min(Remaining, YSPirateShip::MaxStep));
	}
	if (bHasClientMotion)
	{
		ClientLocationError = Displayed - ClientPredicted.Location;
		ClientLocationError.Z = 0.0f;
		ClientYawError = FRotator::NormalizeAxis(DisplayedYaw - ClientPredicted.Yaw);
		// trop loin (téléport, premier état) : pas de rattrapage
		if (ClientLocationError.SizeSquared() > FMath::Square(500.0f))
		{
			ClientLocationError = FVector::ZeroVector;
			ClientYawError = 0.0f;
		}
	}
	bHasClientMotion = true;
}

void AYSPirateShip::TickClientMotion(float DeltaSeconds)
{
	if (!bHasClientMotion)
	{
		return;
	}

	// extrapolation avec les commandes reçues, puis l'écart avec l'affichage précédent disparaît peu à peu
	for (float Remaining = DeltaSeconds; Remaining > KINDA_SMALL_NUMBER; Remaining -= YSPirateShip::MaxStep)
	{
		StepMotion(ClientPredicted, FMath::Min(Remaining, YSPirateShip::MaxStep));
	}
	// le serveur l'a arrêté contre le fond : pas d'extrapolation au-delà
	if (FMath::IsNearlyZero(Motion.Speed) && FMath::IsNearlyZero(Motion.Throttle))
	{
		ClientPredicted.Location = FMath::VInterpTo(ClientPredicted.Location, Motion.Location, DeltaSeconds, ClientCorrectionSpeed);
	}
	const float Decay = ClientCorrectionSpeed > 0.0f ? FMath::Exp(-ClientCorrectionSpeed * DeltaSeconds) : 0.0f;
	ClientLocationError *= Decay;
	ClientYawError *= Decay;

	const FVector Location(ClientPredicted.Location.X + ClientLocationError.X, ClientPredicted.Location.Y + ClientLocationError.Y, GetActorLocation().Z);
	const FRotator Rotation(0.0f, ClientPredicted.Yaw + ClientYawError, 0.0f);
	if (!GetActorLocation().Equals(Location, 0.05f) || !FMath::IsNearlyEqual(GetActorRotation().Yaw, Rotation.Yaw, 0.01f))
	{
		SetActorLocationAndRotation(Location, Rotation);
	}
}

bool AYSPirateShip::FindSeaSpawnPoint(UWorld* World, float RequiredDepth, float MinDistance, float MaxDistance, float Step, int32 Attempts, float ShipMargin, float ClearanceRadius, FVector& OutPoint, float& OutYaw)
{
	const AYSOcean* Ocean = AYSOcean::Find(World);
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!Ocean || !GameState)
	{
		return false;
	}

	TArray<const APawn*> Players;
	for (const APlayerState* Player : GameState->PlayerArray)
	{
		const AYSPlayerCharacter* Character = Player ? Cast<AYSPlayerCharacter>(Player->GetPawn()) : nullptr;
		if (Character && Character->IsAlive())
		{
			Players.Add(Character);
		}
	}
	if (Players.Num() == 0)
	{
		return false;
	}

	const float Sea = Ocean->GetSeaLevel();
	const float Stride = FMath::Max(Step, 10.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSShipSeaSpawn), false);
	const auto IsDeepEnough = [World, Sea, RequiredDepth, &Params](const FVector& Point)
	{
		// assez d'eau (ni l'île, ni un pont : un bateau au-dessus compte pour de l'eau, son emprise est vérifiée à part)
		FHitResult Hit;
		return !World->LineTraceSingleByObjectType(Hit, FVector(Point.X, Point.Y, Sea + YSPirateShip::SpawnTraceUp), FVector(Point.X, Point.Y, Sea - YSPirateShip::DepthTraceDown),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params) || GetShipOf(Hit.GetActor()) || Sea - Hit.ImpactPoint.Z >= RequiredDepth;
	};

	bool bFound = false;
	FVector BestDirection = FVector::ForwardVector;
	for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
	{
		const APawn* Reference = Players[FMath::RandHelper(Players.Num())];
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		for (float Distance = MinDistance; Distance <= MaxDistance; Distance += Stride)
		{
			const FVector Point = Reference->GetActorLocation() + Direction * Distance;
			bool bDeep = IsDeepEnough(Point);
			for (int32 Probe = 0; bDeep && ClearanceRadius > 0.0f && Probe < YSPirateShip::SpawnClearanceProbes; ++Probe)
			{
				const float ProbeAngle = 2.0f * PI * Probe / YSPirateShip::SpawnClearanceProbes;
				bDeep = IsDeepEnough(Point + FVector(FMath::Cos(ProbeAngle), FMath::Sin(ProbeAngle), 0.0f) * ClearanceRadius);
			}
			if (!bDeep)
			{
				continue;
			}
			// loin de tous les joueurs (autre joueur plus près que le joueur de référence)
			if (Players.ContainsByPredicate([&Point, MinDistance](const APawn* Player) { return FVector::DistSquared2D(Player->GetActorLocation(), Point) < FMath::Square(MinDistance); }))
			{
				continue;
			}
			// pas dans un autre bateau à pont (Jolly Ships : sans collision, ils peuvent se croiser)
			bool bInsideShip = false;
			for (TActorIterator<AYSPirateShip> It(World); It && !bInsideShip; ++It)
			{
				bInsideShip = It->IsBoardable() && It->IsPresent() && It->IsInsideHull(Point, ShipMargin);
			}
			if (bInsideShip)
			{
				continue;
			}

			// de préférence avec de l'eau jusqu'au joueur (sinon il s'échoue sur l'île sans jamais atteindre le bateau où il se trouve) ;
			// aucune direction comme ça (joueur à terre) : le premier emplacement trouvé
			bool bClearWater = true;
			for (float Along = Stride; Along < Distance && bClearWater; Along += Stride)
			{
				bClearWater = IsDeepEnough(Reference->GetActorLocation() + Direction * Along);
			}
			if (!bFound || bClearWater)
			{
				bFound = true;
				OutPoint = FVector(Point.X, Point.Y, Sea);
				BestDirection = Direction;
			}
			if (bClearWater)
			{
				Attempt = Attempts;
			}
			break;
		}
	}
	OutYaw = (-BestDirection).Rotation().Yaw;
	return bFound;
}

bool AYSPirateShip::FindSeaPointNear(UWorld* World, const FVector& Center, float Radius, float RequiredDepth, float ShipMargin, float ClearanceRadius, int32 Attempts, FVector& OutPoint)
{
	const AYSOcean* Ocean = AYSOcean::Find(World);
	if (!Ocean)
	{
		return false;
	}
	const float Sea = Ocean->GetSeaLevel();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSShipSeaPointNear), false);
	const auto IsDeepEnough = [World, Sea, RequiredDepth, &Params](const FVector& Point)
	{
		FHitResult Hit;
		return !World->LineTraceSingleByObjectType(Hit, FVector(Point.X, Point.Y, Sea + YSPirateShip::SpawnTraceUp), FVector(Point.X, Point.Y, Sea - YSPirateShip::DepthTraceDown),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params) || Sea - Hit.ImpactPoint.Z >= RequiredDepth;
	};
	for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
	{
		// au centre d'abord, puis au hasard dans le disque
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Distance = Attempt == 0 ? 0.0f : Radius * FMath::Sqrt(FMath::FRand());
		const FVector Point = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance;
		bool bDeep = IsDeepEnough(Point);
		for (int32 Probe = 0; bDeep && ClearanceRadius > 0.0f && Probe < YSPirateShip::SpawnClearanceProbes; ++Probe)
		{
			const float ProbeAngle = 2.0f * PI * Probe / YSPirateShip::SpawnClearanceProbes;
			bDeep = IsDeepEnough(Point + FVector(FMath::Cos(ProbeAngle), FMath::Sin(ProbeAngle), 0.0f) * ClearanceRadius);
		}
		bool bInsideShip = false;
		for (TActorIterator<AYSPirateShip> It(World); It && bDeep && !bInsideShip; ++It)
		{
			bInsideShip = It->IsBoardable() && It->IsPresent() && It->IsInsideHull(Point, ShipMargin);
		}
		if (bDeep && !bInsideShip)
		{
			OutPoint = FVector(Point.X, Point.Y, Sea);
			return true;
		}
	}
	return false;
}

AYSPirateShip* AYSPirateShip::FindShipBelow(const UWorld* World, const FVector& Location, const AActor* IgnoredActor)
{
	if (!World)
	{
		return nullptr;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YSShipBelow), false, IgnoredActor);
	if (!World->LineTraceSingleByObjectType(Hit, Location + FVector(0.0f, 0.0f, 50.0f), Location - FVector(0.0f, 0.0f, 150.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return nullptr;
	}
	return GetShipOf(Hit.GetActor());
}

AYSPirateShip* AYSPirateShip::GetShipOf(const AActor* Actor)
{
	// postes = acteurs enfants (parent actor), objets posés = accrochés au bateau
	for (int32 Depth = 0; Actor && Depth < 8; ++Depth)
	{
		if (const AYSPirateShip* Ship = Cast<AYSPirateShip>(Actor))
		{
			return const_cast<AYSPirateShip*>(Ship);
		}
		Actor = Actor->GetParentActor() ? Actor->GetParentActor() : Actor->GetAttachParentActor();
	}
	return nullptr;
}

FBox2D AYSPirateShip::GetHullBounds() const
{
	FBox2D Bounds(ForceInit);
	for (const FVector2D& Probe : HullProbePoints)
	{
		Bounds += Probe;
	}
	return Bounds;
}

bool AYSPirateShip::IsInsideHull(const FVector& WorldLocation, float Margin) const
{
	const FBox2D Bounds = GetHullBounds();
	if (!Bounds.bIsValid)
	{
		return false;
	}
	const FVector Local = GetActorTransform().InverseTransformPositionNoScale(WorldLocation);
	return Local.X >= Bounds.Min.X - Margin && Local.X <= Bounds.Max.X + Margin && Local.Y >= Bounds.Min.Y - Margin && Local.Y <= Bounds.Max.Y + Margin;
}

FVector AYSPirateShip::GetClosestHullPoint(const FVector& WorldLocation, float Inset) const
{
	const FBox2D Bounds = GetHullBounds();
	if (!Bounds.bIsValid)
	{
		return GetActorLocation();
	}
	const FVector Local = GetActorTransform().InverseTransformPositionNoScale(WorldLocation);
	const float InsetX = FMath::Min(Inset, Bounds.GetExtent().X);
	const float InsetY = FMath::Min(Inset, Bounds.GetExtent().Y);
	const FVector Clamped(FMath::Clamp(Local.X, Bounds.Min.X + InsetX, Bounds.Max.X - InsetX), FMath::Clamp(Local.Y, Bounds.Min.Y + InsetY, Bounds.Max.Y - InsetY), 0.0f);
	return GetActorTransform().TransformPositionNoScale(Clamped);
}

void AYSPirateShip::GetWorldHullPoints(TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	const FTransform& Transform = GetActorTransform();
	for (const FVector2D& Probe : HullProbePoints)
	{
		OutPoints.Add(Transform.TransformPositionNoScale(FVector(Probe.X, Probe.Y, 0.0f)));
	}
}

void AYSPirateShip::RepairCore(float DeltaSeconds)
{
	RepairCoreBy(CoreRepairPerSecond * DeltaSeconds);
}

void AYSPirateShip::RepairCoreBy(float Amount)
{
	if (!HasAuthority() || !CanRepairCore() || Amount <= 0.0f)
	{
		return;
	}
	Health = FMath::Min(Health + Amount, GetMaxHealth());
	if (bDisabled && Health >= CoreRepairMaxHealth)
	{
		// de retour dans le positif : le bateau est de nouveau utilisable (ses postes à 0 PV se réparent comme avant)
		bDisabled = false;
		UE_LOG(LogTemp, Log, TEXT("[YSShipRepair] %s : core réparé (%.0f PV), bateau de nouveau utilisable"), *GetName(), Health);
	}
	ForceNetUpdate();
}
