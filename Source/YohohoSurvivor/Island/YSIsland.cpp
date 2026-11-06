#include "Island/YSIsland.h"
#include "Components/CapsuleComponent.h"
#include "Core/YSFlowMode.h"
#include "Core/YSGameMode.h"
#include "Core/YSGameState.h"
#include "Enemies/YSEnemySpawnRule.h"
#include "Enemies/YSFoeCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/YSInteractiveObject.h"
#include "Interaction/YSTreasureMap.h"
#include "Island/YSIslandData.h"
#include "Island/YSIslandNPC.h"
#include "Island/YSIslandParts.h"
#include "Map/YSOcean.h"
#include "Net/UnrealNetwork.h"
#include "NPC/YSBrookerNPC.h"
#include "Player/YSPlayerCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Async/Async.h"

namespace YSIslandActor
{
	/** Marge autour d'un massif pour poser un objet ou un NME */
	constexpr float SpotMargin = 150.0f;
	/** Circle spawn : distance au bord de la falaise, puis recherche de place vers l'intérieur */
	constexpr float CircleShoreInset = 150.0f;
	constexpr float CircleSearchStep = 50.0f;
	constexpr float CircleSearchMax = 3000.0f;
	constexpr int32 SpotAttempts = 400;
	constexpr int32 OffScreenAttempts = 30;
	/** Hauteur d'apparition d'un joueur au-dessus du sol */
	constexpr float PlayerSpawnHeight = 120.0f;
	/** Apparition au centre : anneaux de places essayées autour du point, et attente avant de réessayer */
	constexpr float CenterRingStep = 400.0f;
	constexpr int32 CenterRingCount = 4;
	constexpr int32 CenterRingSpots = 8;
	constexpr float CenterRetryInterval = 0.5f;
	/** Props par part répliquée (une propriété répliquée tient au plus 2048 éléments et 64 Ko) */
	constexpr int32 PropsPerChunk = 1000;

	double GetServerTime(const UWorld* World)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
	}
}

AYSIsland::AYSIsland()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	RootComponent = TerrainMesh;
	TerrainMesh->bUseComplexAsSimpleCollision = true;
	TerrainMesh->bUseAsyncCooking = false;
	TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TerrainMesh->SetCollisionObjectType(ECC_WorldStatic);
	TerrainMesh->SetCollisionResponseToAllChannels(ECR_Block);
	TerrainMesh->SetCanEverAffectNavigation(false);
	// statique tant que l'île ne coule pas : les props (statiques) ne peuvent pas s'accrocher à un parent mobile
	TerrainMesh->SetMobility(EComponentMobility::Static);
}

void AYSIsland::InitIsland(UYSIslandData* InData, int32 InSeed, bool bInStartIsland)
{
	Data = InData;
	RequestedSeed = InSeed;
	bStartIsland = bInStartIsland;
}

void AYSIsland::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSIsland, Data);
	DOREPLIFETIME(AYSIsland, Plan);
	DOREPLIFETIME(AYSIsland, Phase);
	DOREPLIFETIME(AYSIsland, OpenDeliveryClasses);
	DOREPLIFETIME(AYSIsland, StartBoat);
	DOREPLIFETIME(AYSIsland, SinkStartServerTime);
}

FVector AYSIsland::LocalToWorld(const FVector2f& Local, float LocalZ) const
{
	return GetActorTransform().TransformPosition(FVector(Local.X, Local.Y, LocalZ));
}

FVector2f AYSIsland::WorldToLocal(const FVector& World) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(World);
	return FVector2f(Local.X, Local.Y);
}

float AYSIsland::GetRadius() const
{
	return Data ? Data->Generation.Radius : 0.0f;
}

void AYSIsland::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		// le plan peut être arrivé avant le BeginPlay
		if (Plan.Seed != 0)
		{
			BuildLocally();
		}
		return;
	}

	if (!Data)
	{
		UE_LOG(LogTemp, Error, TEXT("[YSIsland] %s : pas de Data Asset"), *GetName());
		return;
	}

	// différente à chaque partie, sauf graine fixe du Data Asset ou graine demandée
	int32 Seed = RequestedSeed != 0 ? RequestedSeed : Data->FixedSeed;
	if (Seed == 0)
	{
		Seed = FMath::RandRange(1, 1000000);
	}
	ServerRng.Initialize(Seed);

	// biome : tiré au hasard si le Data Asset en propose plusieurs
	const EYSIslandGeneration Biome = Data->RandomGenerationTypes.Num() > 0
		? Data->RandomGenerationTypes[ServerRng.RandRange(0, Data->RandomGenerationTypes.Num() - 1)] : Data->GenerationType;

	// île de départ : tout de suite (le GameBoat et l'apparition des joueurs en dépendent) ; les autres : sur un autre fil, la partie ne gèle pas
	if (bStartIsland)
	{
		FString Report;
		YSIslandGenerator::GeneratePlan(Data->Generation, Biome, Seed, Plan, Report);
		OnPlanReady(Report);
		return;
	}
	AsyncPlanStartTime = FPlatformTime::Seconds();
	AsyncPlan = Async(EAsyncExecution::ThreadPool, [Rules = Data->Generation, Biome, Seed]()
	{
		FAsyncPlanResult Result;
		YSIslandGenerator::GeneratePlan(Rules, Biome, Seed, Result.Plan, Result.Report);
		return Result;
	});
}

void AYSIsland::OnPlanReady(const FString& Report)
{
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s générée en %s (yaw %.0f°, biome %s) : %s"), *Data->GetName(), *GetActorLocation().ToCompactString(), GetActorRotation().Yaw,
		*StaticEnum<EYSIslandGeneration>()->GetNameStringByValue(static_cast<int64>(Plan.Biome)), *Report);
	BuildLocally();
	SpawnPropChunks();
	SpawnGameplayActors();
	ForceNetUpdate();
}

void AYSIsland::TickPendingProps()
{
	if (PendingPropIndex >= PendingProps.Num())
	{
		return;
	}
	constexpr int32 PropsPerTick = 120;
	const int32 End = FMath::Min(PendingPropIndex + PropsPerTick, PendingProps.Num());
	const FYSIslandGenerationRules& Rules = Data->Generation;
	const float IslandYaw = GetActorRotation().Yaw;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	for (; PendingPropIndex < End; ++PendingPropIndex)
	{
		const FYSIslandPropSpawn& Prop = PendingProps[PendingPropIndex];
		const FYSIslandPropDef& Def = YSIslandGenerator::GetPropDef(Rules, Prop.Type);
		if (!Def.Class)
		{
			continue;
		}
		const FVector Location = LocalToWorld(Prop.Location, Terrain.Height(Prop.Location.X, Prop.Location.Y) - Def.Sink);
		if (AActor* Actor = GetWorld()->SpawnActor<AActor>(Def.Class, Location, FRotator(0.0f, Prop.Yaw + IslandYaw, 0.0f), Params))
		{
			Actor->SetActorScale3D(FVector(Prop.Scale));
			Actor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		}
	}
	if (PendingPropIndex >= PendingProps.Num())
	{
		UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : %d props posés (%s)"), *Data->GetName(), PendingProps.Num(), HasAuthority() ? TEXT("serveur") : TEXT("client"));
		PendingProps.Empty();
		PendingPropIndex = 0;
	}
}

void AYSIsland::OnRep_Plan()
{
	if (HasActorBegunPlay() && Plan.Seed != 0)
	{
		BuildLocally();
	}
}

void AYSIsland::BuildLocally()
{
	if (bBuiltLocally || !Data)
	{
		return;
	}
	bBuiltLocally = true;
	const double StartTime = FPlatformTime::Seconds();
	Terrain = FYSIslandTerrain(Data->Generation, Plan.Seed, Plan.Biome);
	BuildTerrain();
	const double TerrainTime = FPlatformTime::Seconds();

	// pontons de Port Town : sur chaque machine, accrochés à l'île
	if (Plan.PierSegments.Num() > 0)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		Pier = GetWorld()->SpawnActor<AYSIslandPier>(AYSIslandPier::StaticClass(), GetActorTransform(), Params);
		if (Pier)
		{
			Pier->Build(Plan.PierSegments, Data->Generation);
			Pier->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	if (HasAuthority())
	{
		SpawnPropList(Plan.Props);
	}
	else
	{
		// clients : props reçus par parts
		for (TActorIterator<AYSIslandPropChunk> It(GetWorld()); It; ++It)
		{
			if (It->GetIsland() == this)
			{
				It->TrySpawnProps();
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : terrain construit en %.0f ms (+ %.0f ms pontons, %s)"), *Data->GetName(), (TerrainTime - StartTime) * 1000.0,
		(FPlatformTime::Seconds() - TerrainTime) * 1000.0, HasAuthority() ? TEXT("serveur") : TEXT("client"));

	// le sol existe maintenant : ce qui s'est posé avant (client) se repose dessus
	for (TActorIterator<AYSBrookerNPC> It(GetWorld()); It; ++It)
	{
		if (!HasAuthority())
		{
			It->Resettle();
		}
	}
}

void AYSIsland::BuildTerrain()
{
	const FYSIslandGenerationRules& Rules = Data->Generation;
	const float Q = Rules.QuadSize;
	const float Half = Terrain.GetHalfSize();
	const int32 Size = FMath::RoundToInt32(Half * 2.0f / Q) + 1;

	TArray<float> Heights;
	Heights.SetNumUninitialized(Size * Size);
	for (int32 J = 0; J < Size; ++J)
	{
		for (int32 I = 0; I < Size; ++I)
		{
			Heights[J * Size + I] = Terrain.Height(-Half + I * Q, -Half + J * Q);
		}
	}

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	Vertices.SetNumUninitialized(Size * Size);
	Normals.SetNumUninitialized(Size * Size);
	for (int32 J = 0; J < Size; ++J)
	{
		for (int32 I = 0; I < Size; ++I)
		{
			const float H = Heights[J * Size + I];
			Vertices[J * Size + I] = FVector(-Half + I * Q, -Half + J * Q, H);
			const float HX0 = Heights[J * Size + FMath::Max(I - 1, 0)];
			const float HX1 = Heights[J * Size + FMath::Min(I + 1, Size - 1)];
			const float HY0 = Heights[FMath::Max(J - 1, 0) * Size + I];
			const float HY1 = Heights[FMath::Min(J + 1, Size - 1) * Size + I];
			Normals[J * Size + I] = FVector(HX0 - HX1, HY0 - HY1, 2.0f * Q).GetSafeNormal();
		}
	}

	// rien au fond de la mer, loin de l'île (sous la deadzone)
	TArray<int32> Triangles;
	Triangles.Reserve((Size - 1) * (Size - 1) * 6);
	const float FloorZ = Rules.SeaFloor + 1.0f;
	for (int32 J = 0; J + 1 < Size; ++J)
	{
		for (int32 I = 0; I + 1 < Size; ++I)
		{
			const int32 V00 = J * Size + I;
			const int32 V10 = J * Size + I + 1;
			const int32 V01 = (J + 1) * Size + I;
			const int32 V11 = (J + 1) * Size + I + 1;
			if (Heights[V00] <= FloorZ && Heights[V10] <= FloorZ && Heights[V01] <= FloorZ && Heights[V11] <= FloorZ)
			{
				continue;
			}
			Triangles.Append({ V00, V11, V10, V00, V01, V11 });
		}
	}

	TerrainMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
	if (Rules.GroundMaterial)
	{
		TerrainMesh->SetMaterial(0, Rules.GroundMaterial);
	}
}

void AYSIsland::SpawnPropList(const TArray<FYSIslandPropSpawn>& Props)
{
	// posés par paquets dans le Tick (TickPendingProps)
	PendingProps.Append(Props);
}

void AYSIsland::SpawnPropChunks()
{
	// seul : le serveur pose tout lui-même
	if (GetNetMode() == NM_Standalone)
	{
		return;
	}
	int32 Chunks = 0;
	for (int32 Start = 0; Start < Plan.Props.Num(); Start += YSIslandActor::PropsPerChunk)
	{
		TArray<FYSIslandPropSpawn> Slice;
		Slice.Append(Plan.Props.GetData() + Start, FMath::Min(YSIslandActor::PropsPerChunk, Plan.Props.Num() - Start));
		AYSIslandPropChunk* Chunk = GetWorld()->SpawnActorDeferred<AYSIslandPropChunk>(AYSIslandPropChunk::StaticClass(), GetActorTransform(), this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Chunk)
		{
			Chunk->InitChunk(this, MoveTemp(Slice));
			Chunk->FinishSpawning(GetActorTransform());
			++Chunks;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : %d props répliqués en %d part(s)"), *Data->GetName(), Plan.Props.Num(), Chunks);
}

void AYSIsland::AttachToIsland(AActor* Actor)
{
	if (Actor)
	{
		Actor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	}
}

void AYSIsland::SpawnStartBoat(const FVector& Location, float Yaw)
{
	if (!Data->StartBoatClass || StartBoat)
	{
		return;
	}
	const FTransform Transform(FRotator(0.0f, Yaw, 0.0f), Location);
	StartBoat = GetWorld()->SpawnActorDeferred<AYSStartBoat>(Data->StartBoatClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (StartBoat)
	{
		StartBoat->InitBoat(this, Data->BoatZoneRadius);
		StartBoat->FinishSpawning(Transform);
		AttachToIsland(StartBoat);
		ForceNetUpdate();
	}
}

FVector AYSIsland::GetGameBoatStartLocation(float Distance) const
{
	const AYSOcean* Ocean = AYSOcean::Find(GetWorld());
	const float Sea = Ocean ? Ocean->GetSeaLevel() : 0.0f;
	// depuis la plage, vers le large (-X de l'île) jusqu'au bord de l'eau
	float X = Plan.BoatLocation.X;
	while (X > -2.0f * Terrain.GetHalfSize() && Terrain.Height(X, Plan.BoatLocation.Y) > Sea)
	{
		X -= YSIslandActor::CircleSearchStep;
	}
	FVector Location = LocalToWorld(FVector2f(X - Distance, Plan.BoatLocation.Y), 0.0f);
	Location.Z = Sea;
	return Location;
}

void AYSIsland::SpawnGameplayActors()
{
	UWorld* World = GetWorld();

	// île de départ du GameBoat : la chaloupe attend sur l'eau, la zone de dépôt viendra à son arrivée ; sinon zone de dépôt sur la plage
	AYSFlowMode* FlowMode = World->GetAuthGameMode<AYSFlowMode>();
	if (!bStartIsland || !FlowMode || !FlowMode->SpawnGameBoat(this))
	{
		SpawnStartBoat(LocalToWorld(Plan.BoatLocation, Terrain.Height(Plan.BoatLocation.X, Plan.BoatLocation.Y)), Plan.BoatYaw + GetActorRotation().Yaw);
	}

	// tous les objectifs comptent dès le début ; les objets n'apparaissent qu'à leur jour
	for (const FYSIslandObjectiveDef& Def : Data->MainObjectives)
	{
		FYSIslandObjectiveState& State = Objectives.AddDefaulted_GetRef();
		State.Def = Def;
		State.bMain = true;
	}
	for (const FYSIslandObjectiveDef& Def : Data->SecondaryObjectives)
	{
		FYSIslandObjectiveState& State = Objectives.AddDefaulted_GetRef();
		State.Def = Def;
	}
	for (FYSIslandObjectiveState& State : Objectives)
	{
		State.DisplayName = State.Def.DisplayName;
		State.DeliveryClass = State.Def.DeliveryClass ? State.Def.DeliveryClass : State.Def.ItemClass;
	}
	SpawnDueObjectives();
	RefreshOpenDeliveryClasses();
}

void AYSIsland::SpawnDueObjectives()
{
	const int32 Day = GetIslandDay();
	for (FYSIslandObjectiveState& State : Objectives)
	{
		if (!State.bSpawned && !State.bDone && State.Def.SpawnDay <= Day)
		{
			SpawnObjective(State);
		}
	}
}

bool AYSIsland::IsSpotFree(const FVector2f& Spot, float Margin) const
{
	for (const FYSIslandMassif& Massif : Plan.Massifs)
	{
		if (YSIslandGenerator::MassifDistance(Massif, Spot) < Margin)
		{
			return false;
		}
	}
	for (const FYSIslandPropSpawn& Prop : Plan.Props)
	{
		if ((Prop.Role == EYSIslandPropRole::Trio || Prop.Role == EYSIslandPropRole::Beach) && (Prop.Location - Spot).SizeSquared() < FMath::Square(Margin + 100.0f))
		{
			return false;
		}
	}
	return true;
}

bool AYSIsland::FindFreeSpot(float MinDistanceFromBoat, float MaxDistanceFromBoat, bool bInTown, FVector2f& OutSpot)
{
	const float Reach = Data->Generation.Radius;
	for (int32 Attempt = 0; Attempt < YSIslandActor::SpotAttempts * (bInTown ? 4 : 1); ++Attempt)
	{
		const FVector2f Spot(ServerRng.FRandRange(-Reach, Reach), ServerRng.FRandRange(-Reach, Reach));
		const float Distance = (Spot - Plan.BoatLocation).Size();
		const float Theta = FMath::Atan2(Spot.Y, Spot.X);
		// sur le plateau : ni sur la plage ni au bord de la falaise ; ville : dans son secteur
		if (Distance >= MinDistanceFromBoat && Distance <= MaxDistanceFromBoat && Terrain.Inland(Spot.X, Spot.Y) >= FMath::Max(300.0f, Terrain.BeachDepth(Theta) * Terrain.BeachWeight(Theta))
			&& (!bInTown || Terrain.IsInTownSector(Spot.X, Spot.Y)) && IsSpotFree(Spot, YSIslandActor::SpotMargin))
		{
			OutSpot = Spot;
			return true;
		}
	}
	return false;
}

void AYSIsland::SpawnObjective(FYSIslandObjectiveState& State)
{
	const FYSIslandObjectiveDef& Def = State.Def;
	State.bSpawned = true;
	if (!Def.ItemClass)
	{
		return;
	}
	UWorld* World = GetWorld();
	AYSInteractiveObject* Item = nullptr;
	const AYSTreasureMap* MapSettings = Cast<AYSTreasureMap>(Def.ItemClass->GetDefaultObject());
	for (int32 Attempt = 0; Attempt < 30 && !Item; ++Attempt)
	{
		FVector2f Spot;
		if (!FindFreeSpot(Def.DistanceFromBoat.X, Def.DistanceFromBoat.Y, Def.bInTown, Spot))
		{
			break;
		}
		const FVector Ground = LocalToWorld(Spot, Terrain.Height(Spot.X, Spot.Y));
		if (MapSettings)
		{
			// carte au trésor : il lui faut un chemin complet
			TArray<FVector> Path;
			if (MapSettings->BuildPath(World, Ground, Path))
			{
				const FTransform Transform(FRotator(0.0f, ServerRng.FRandRange(0.0f, 360.0f), 0.0f), Ground);
				AYSTreasureMap* TreasureMap = World->SpawnActorDeferred<AYSTreasureMap>(Def.ItemClass.Get(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (TreasureMap)
				{
					TreasureMap->SetPath(Path);
					TreasureMap->FinishSpawning(Transform);
					Item = TreasureMap;
				}
			}
		}
		else
		{
			Item = AYSInteractiveObject::SpawnOnGround(World, Def.ItemClass, Ground, ServerRng.FRandRange(0.0f, 360.0f));
		}
	}

	State.Item = Item;
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s, jour %d : objectif %s « %s » : %s en %s"), *Data->GetName(), GetIslandDay(), State.bMain ? TEXT("principal") : TEXT("secondaire"),
		*Def.DisplayName.ToString(), Item ? *Item->GetName() : TEXT("AUCUN EMPLACEMENT"), Item ? *Item->GetActorLocation().ToCompactString() : TEXT("-"));
}

FTransform AYSIsland::GetPlayerSpawnTransform(int32 Index) const
{
	const FVector2f Spot = Plan.PlayerSpots.Num() > 0 ? Plan.PlayerSpots[Index % Plan.PlayerSpots.Num()] : FVector2f::ZeroVector;
	return FTransform(FRotator::ZeroRotator, LocalToWorld(Spot, Terrain.Height(Spot.X, Spot.Y) + YSIslandActor::PlayerSpawnHeight));
}

void AYSIsland::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// descente : sur chaque machine, depuis l'heure serveur
	UpdateSink();
	if (Data)
	{
		TickPendingProps();
	}

	if (!HasAuthority() || !Data)
	{
		return;
	}

	// plan calculé sur un autre fil : construction sur le fil du jeu
	if (AsyncPlan.IsValid())
	{
		if (!AsyncPlan.IsReady())
		{
			return;
		}
		FAsyncPlanResult Result = AsyncPlan.Get();
		AsyncPlan.Reset();
		Plan = MoveTemp(Result.Plan);
		OnPlanReady(FString::Printf(TEXT("%s (prêt %.0f ms après l'apparition)"), *Result.Report, (FPlatformTime::Seconds() - AsyncPlanStartTime) * 1000.0));
		return;
	}
	if (Plan.Seed == 0)
	{
		return;
	}

	if (Phase == EYSIslandPhase::Inactive)
	{
		TickActivation();
		return;
	}

	PhaseElapsed += DeltaSeconds;
	TickPhaseEvents();
	TickPendingCenterSpawns(DeltaSeconds);

	if (Phase == EYSIslandPhase::Day && PhaseElapsed >= Data->DayDuration)
	{
		StartNightPhase();
	}
	else if (Phase == EYSIslandPhase::Sinking && !bSinkStarted && PhaseElapsed >= Data->SinkDelay)
	{
		StartSinking();
	}
}

void AYSIsland::StartSinking()
{
	bSinkStarted = true;
	// ce qui est posé coule avec l'île
	for (TActorIterator<AYSInteractiveObject> It(GetWorld()); It; ++It)
	{
		if (!It->GetCarrier() && !It->GetAttachParentActor() && FindAt(GetWorld(), It->GetActorLocation()) == this)
		{
			AttachToIsland(*It);
		}
	}
	SinkStartServerTime = FMath::Max(YSIslandActor::GetServerTime(GetWorld()), 0.001);
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s coule (%.0f cm en %.0f s)"), *Data->GetName(), Data->SinkDepth, Data->SinkDuration);

	if (AYSFlowMode* FlowMode = GetWorld()->GetAuthGameMode<AYSFlowMode>())
	{
		FlowMode->OnIslandSinkStarted(this);
	}
}

bool AYSIsland::IsSinking() const
{
	if (!Data || SinkStartServerTime <= 0.0)
	{
		return false;
	}
	return YSIslandActor::GetServerTime(GetWorld()) - SinkStartServerTime < Data->SinkDuration;
}

void AYSIsland::UpdateSink()
{
	if (SinkStartServerTime <= 0.0 || !Data)
	{
		return;
	}

	// une fois : l'île et tout ce qui y est accroché (props, pontons, PNJ, objets), rendus mobiles
	if (!bSinkGathered)
	{
		bSinkGathered = true;
		SinkOrigin = GetActorLocation();
		TArray<AActor*> Attached;
		GetAttachedActors(Attached, false, true);
		Attached.Insert(this, 0);
		for (AActor* Actor : Attached)
		{
			TInlineComponentArray<USceneComponent*> Components(Actor);
			for (USceneComponent* Component : Components)
			{
				Component->SetMobility(EComponentMobility::Movable);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s coule : %d acteur(s) (%s)"), *Data->GetName(), Attached.Num(), HasAuthority() ? TEXT("serveur") : TEXT("client"));
	}

	// même descente sur toutes les machines (temps serveur) ; les acteurs accrochés suivent l'île
	const float Alpha = FMath::Clamp(static_cast<float>(YSIslandActor::GetServerTime(GetWorld()) - SinkStartServerTime) / FMath::Max(Data->SinkDuration, 0.1f), 0.0f, 1.0f);
	const FVector Target = SinkOrigin - FVector(0.0f, 0.0f, Data->SinkDepth * FMath::Pow(Alpha, Data->SinkEaseExponent));
	if (!GetActorLocation().Equals(Target, 0.1f))
	{
		SetActorLocation(Target);
	}
}

bool AYSIsland::IsSinkingAt(const UWorld* World, const FVector& Location)
{
	for (TActorIterator<AYSIsland> It(World); It; ++It)
	{
		if (It->IsSinking() && FVector::DistSquared2D(It->GetActorLocation(), Location) <= FMath::Square(It->GetReach()))
		{
			return true;
		}
	}
	return false;
}

void AYSIsland::TickActivation()
{
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	if (!YSGameState || !YSGameState->IsMatchStarted())
	{
		return;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AYSPlayerCharacter* Character = It->Get() ? Cast<AYSPlayerCharacter>(It->Get()->GetPawn()) : nullptr;
		const UPrimitiveComponent* Base = Character ? Character->GetCharacterMovement()->GetMovementBase() : nullptr;
		// sur le terrain, ou sur ce qui y est accroché (bateau de départ, rocher, ponton...)
		const AActor* BaseActor = Base ? Base->GetOwner() : nullptr;
		if (BaseActor && (BaseActor == this || BaseActor->GetAttachParentActor() == this))
		{
			UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s pose le pied sur %s"), *Character->GetName(), *Data->GetName());
			Activate();
			return;
		}
	}
}

void AYSIsland::Activate()
{
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	ReferenceDay = YSGameState ? YSGameState->GetDayNumber() : 1;
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s activée : jour de référence %d, %d objectif(s) principal(aux), %d jour(s) max"), *Data->GetName(), ReferenceDay, CountOpenMainObjectives(), Data->MaxDays);
	StartDayPhase();
}

int32 AYSIsland::GetIslandDay() const
{
	if (Phase == EYSIslandPhase::Inactive)
	{
		return 1;
	}
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	return (YSGameState ? YSGameState->GetDayNumber() : ReferenceDay) - ReferenceDay + 1;
}

void AYSIsland::StartDayPhase()
{
	Phase = EYSIslandPhase::Day;
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : début du jour %d de l'île (nuit dans %.0f s)"), *Data->GetName(), GetIslandDay(), Data->DayDuration);
	StartPhaseEvents(&Data->DayEvents);
	SpawnDueObjectives();
	RefreshOpenDeliveryClasses();
	ForceNetUpdate();
}

void AYSIsland::StartNightPhase()
{
	Phase = EYSIslandPhase::Night;
	NightElites.Reset();
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : tombée de la nuit %d de l'île"), *Data->GetName(), GetIslandDay());
	StartPhaseEvents(&Data->NightEvents);
	ForceNetUpdate();
}

void AYSIsland::StartSinkRoutine(const TCHAR* Reason)
{
	if (Phase == EYSIslandPhase::Sinking)
	{
		return;
	}
	Phase = EYSIslandPhase::Sinking;
	PendingCenterSpawns.Reset();
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : routine de coulaison (%s), l'île coule dans %.0f s"), *Data->GetName(), Reason, Data->SinkDelay);
	StartPhaseEvents(&Data->SinkEvents);
	ForceNetUpdate();
}

void AYSIsland::StartPhaseEvents(const TArray<FYSIslandSpawnEvent>* Events)
{
	PhaseElapsed = 0.0f;
	PhaseEvents = Events;
	EventsFired.Reset();
	EventsFired.SetNumZeroed(Events ? Events->Num() : 0);
}

void AYSIsland::TickPhaseEvents()
{
	if (!PhaseEvents)
	{
		return;
	}
	const int32 Day = GetIslandDay();
	for (int32 Index = 0; Index < PhaseEvents->Num(); ++Index)
	{
		const FYSIslandSpawnEvent& Event = (*PhaseEvents)[Index];
		// vague d'un seul jour (ou d'une seule nuit) de l'île
		if (Event.IslandDay > 0 && Event.IslandDay != Day)
		{
			continue;
		}
		while ((Event.RepeatCount == 0 || EventsFired[Index] < Event.RepeatCount) && PhaseElapsed >= Event.Time + EventsFired[Index] * Event.RepeatInterval)
		{
			FireEvent(Event, EventsFired[Index]);
			++EventsFired[Index];
			// jusqu'à la fin de la phase sans intervalle : une seule fois
			if (Event.RepeatInterval <= 0.0f)
			{
				EventsFired[Index] = FMath::Max(EventsFired[Index], Event.RepeatCount);
				if (Event.RepeatCount == 0)
				{
					break;
				}
			}
		}
	}
}

void AYSIsland::FireEvent(const FYSIslandSpawnEvent& Event, int32 Repeat)
{
	AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	if (!GameMode || !YSGameState)
	{
		return;
	}

	TArray<UClass*> Classes;
	for (const FYSIslandFoeEntry& Entry : Event.Foes)
	{
		if (!Entry.FoeClass && Entry.RandomFoeClasses.Num() == 0)
		{
			continue;
		}
		const int32 Count = Event.bScaleWithPlayers ? GameMode->ScaleEnemyCount(Entry.Count) : Entry.Count;
		for (int32 K = 0; K < Count; ++K)
		{
			// au hasard dans la liste (ex. élite Skull ou Gun)
			UClass* Class = Entry.RandomFoeClasses.Num() > 0 ? Entry.RandomFoeClasses[ServerRng.RandRange(0, Entry.RandomFoeClasses.Num() - 1)].Get() : Entry.FoeClass.Get();
			if (Class)
			{
				Classes.Add(Class);
			}
		}
	}
	for (int32 K = Classes.Num() - 1; K > 0; --K)
	{
		Classes.Swap(K, ServerRng.RandRange(0, K));
	}

	const int32 ForcedLevel = Event.bOvertimeLevel ? YSGameState->GetDayNumber() + Event.LevelOffset + Event.LevelStepPerRepeat * Repeat : INDEX_NONE;
	const int32 LevelBonus = Event.bOvertimeLevel ? 0 : Event.LevelStepPerRepeat * Repeat;

	int32 Spawned = 0;
	switch (Event.Shape)
	{
	case EYSIslandSpawnShape::Circle:
		Spawned = SpawnCircle(Classes, ForcedLevel, LevelBonus);
		break;
	case EYSIslandSpawnShape::Center:
		for (UClass* Class : Classes)
		{
			if (AYSFoeCharacter* Foe = TrySpawnAtCenter(Class, ForcedLevel, LevelBonus))
			{
				TrackNightElite(Foe);
				++Spawned;
			}
			else
			{
				PendingCenterSpawns.Add({ Class, ForcedLevel, LevelBonus });
			}
		}
		break;
	default:
		Spawned = SpawnOffScreen(Classes, ForcedLevel, LevelBonus);
		break;
	}
	FString Names;
	for (const UClass* Class : Classes)
	{
		if (Names.Len() < 120)
		{
			Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + Class->GetName();
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s, jour %d, %.0f s de phase %d : vague %s, répétition %d, %d / %d NME%s (%s)"), *Data->GetName(), GetIslandDay(), PhaseElapsed, static_cast<int32>(Phase),
		Event.Shape == EYSIslandSpawnShape::Circle ? TEXT("circle") : Event.Shape == EYSIslandSpawnShape::Center ? TEXT("centre") : TEXT("hors écran"), Repeat, Spawned, Classes.Num(),
		ForcedLevel != INDEX_NONE ? *FString::Printf(TEXT(", niveau %d"), ForcedLevel) : TEXT(""), *Names);
}

void AYSIsland::TrackNightElite(AYSFoeCharacter* Foe)
{
	if (Foe && Foe->IsElite() && Phase == EYSIslandPhase::Night)
	{
		NightElites.Add(Foe);
	}
}

AYSFoeCharacter* AYSIsland::TrySpawnAtCenter(UClass* Class, int32 ForcedLevel, int32 LevelBonus)
{
	AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	const FVector Facing = LocalToWorld(Plan.BoatLocation, Data->Generation.PlateauHeight);
	for (int32 Ring = 0; Ring <= YSIslandActor::CenterRingCount; ++Ring)
	{
		const int32 Spots = Ring == 0 ? 1 : YSIslandActor::CenterRingSpots;
		for (int32 Index = 0; Index < Spots; ++Index)
		{
			const float Angle = 2.0f * PI * Index / Spots;
			const FVector2f Spot = Plan.CenterSpot + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * (Ring * YSIslandActor::CenterRingStep);
			FVector Ground;
			if (IsSpotFree(Spot, YSIslandActor::SpotMargin) && GameMode->FindIslandGround(LocalToWorld(Spot, Data->Generation.PlateauHeight), Ground))
			{
				if (AYSFoeCharacter* Foe = GameMode->SpawnFoeOnGround(Class, Ground, Facing, ForcedLevel, LevelBonus))
				{
					return Foe;
				}
			}
		}
	}
	return nullptr;
}

void AYSIsland::TickPendingCenterSpawns(float DeltaSeconds)
{
	PendingRetryTimer -= DeltaSeconds;
	if (PendingCenterSpawns.Num() == 0 || PendingRetryTimer > 0.0f)
	{
		return;
	}
	PendingRetryTimer = YSIslandActor::CenterRetryInterval;
	for (int32 Index = PendingCenterSpawns.Num() - 1; Index >= 0; --Index)
	{
		const FPendingCenterSpawn& Pending = PendingCenterSpawns[Index];
		if (AYSFoeCharacter* Foe = TrySpawnAtCenter(Pending.Class, Pending.ForcedLevel, Pending.LevelBonus))
		{
			TrackNightElite(Foe);
			UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : %s apparu après avoir attendu une place"), *Data->GetName(), *Pending.Class->GetName());
			PendingCenterSpawns.RemoveAt(Index);
		}
	}
}

int32 AYSIsland::SpawnOffScreen(const TArray<UClass*>& Classes, int32 ForcedLevel, int32 LevelBonus)
{
	AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	TArray<const APawn*> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APawn* Pawn = It->Get() ? Cast<AYSPlayerCharacter>(It->Get()->GetPawn()) : nullptr)
		{
			Players.Add(Pawn);
		}
	}
	if (Players.Num() == 0)
	{
		return 0;
	}

	int32 Spawned = 0;
	for (UClass* Class : Classes)
	{
		for (int32 Attempt = 0; Attempt < YSIslandActor::OffScreenAttempts; ++Attempt)
		{
			const FVector Center = Players[ServerRng.RandRange(0, Players.Num() - 1)]->GetActorLocation();
			const float Angle = ServerRng.FRandRange(0.0f, 2.0f * PI);
			const float Distance = ServerRng.FRandRange(Data->OffScreenRadius, Data->OffScreenRadius + Data->OffScreenBand);
			const FVector World(Center.X + FMath::Cos(Angle) * Distance, Center.Y + FMath::Sin(Angle) * Distance, GetActorLocation().Z + Data->Generation.PlateauHeight);
			const FVector2f Spot = WorldToLocal(World);
			bool bFree = Terrain.Inland(Spot.X, Spot.Y) > YSIslandActor::CircleShoreInset && IsSpotFree(Spot, YSIslandActor::SpotMargin);
			for (const APawn* Player : Players)
			{
				bFree = bFree && FVector::DistSquared2D(Player->GetActorLocation(), World) >= FMath::Square(Data->OffScreenRadius);
			}
			FVector Ground;
			if (bFree && GameMode->FindIslandGround(World, Ground) && GameMode->SpawnFoeOnGround(Class, Ground, Center, ForcedLevel, LevelBonus))
			{
				++Spawned;
				break;
			}
		}
	}
	return Spawned;
}

bool AYSIsland::FindCirclePoint(float Angle, FVector& OutGround) const
{
	const AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	const float Coast = Terrain.CoastRadius(Angle);
	// depuis la falaise vers l'intérieur : première place libre (devant la ceinture de massifs, ou au bord dans une ouverture)
	for (float Inset = YSIslandActor::CircleShoreInset; Inset < YSIslandActor::CircleSearchMax; Inset += YSIslandActor::CircleSearchStep)
	{
		const FVector2f Spot((Coast - Inset) * FMath::Cos(Angle), (Coast - Inset) * FMath::Sin(Angle));
		if (IsSpotFree(Spot, YSIslandActor::SpotMargin) && GameMode && GameMode->FindIslandGround(LocalToWorld(Spot, Data->Generation.PlateauHeight), OutGround))
		{
			return true;
		}
	}
	return false;
}

int32 AYSIsland::SpawnCircle(const TArray<UClass*>& Classes, int32 ForcedLevel, int32 LevelBonus)
{
	AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	if (Classes.Num() == 0)
	{
		return 0;
	}
	// tour de l'île, sauf la plage
	const float EndAngle = FMath::DegreesToRadians(Data->Generation.BeachAngleEnd);
	const float Start = PI + EndAngle;
	const float Arc = 2.0f * PI - 2.0f * EndAngle;
	const FVector Facing = LocalToWorld(Plan.CenterSpot, Data->Generation.PlateauHeight);
	int32 Spawned = 0;
	for (int32 Index = 0; Index < Classes.Num(); ++Index)
	{
		FVector Ground;
		if (FindCirclePoint(Start + Arc * (Index + 0.5f) / Classes.Num(), Ground))
		{
			Spawned += GameMode->SpawnFoeOnGround(Classes[Index], Ground, Facing, ForcedLevel, LevelBonus) ? 1 : 0;
		}
	}
	return Spawned;
}

void AYSIsland::OnGlobalDayChanged(int32 Day)
{
	if (!Data || Phase == EYSIslandPhase::Inactive || Phase == EYSIslandPhase::Sinking)
	{
		return;
	}
	if (CountOpenMainObjectives() == 0)
	{
		StartSinkRoutine(TEXT("objectifs principaux accomplis"));
	}
	else if (Day - ReferenceDay + 1 >= Data->MaxDays)
	{
		StartSinkRoutine(TEXT("nombre de jours maximum"));
	}
	else
	{
		StartDayPhase();
	}
}

bool AYSIsland::OwnsNightElite(const AYSFoeCharacter* Elite) const
{
	return NightElites.ContainsByPredicate([Elite](const TWeakObjectPtr<AYSFoeCharacter>& Tracked) { return Tracked.Get() == Elite; });
}

void AYSIsland::OnEliteKilled(AYSFoeCharacter* Elite)
{
	if (!IsNight())
	{
		return;
	}
	// plusieurs élites (duo) : la nuit se termine quand le dernier meurt
	const bool bTracked = OwnsNightElite(Elite);
	NightElites.RemoveAll([Elite](const TWeakObjectPtr<AYSFoeCharacter>& Tracked)
	{
		return !Tracked.IsValid() || Tracked.Get() == Elite || Tracked->IsDying() || Tracked->GetHealth() <= 0.0f;
	});
	if (!bTracked && NightElites.Num() > 0)
	{
		return;
	}
	if (NightElites.Num() > 0 || PendingCenterSpawns.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : élite %s tué, encore %d élite(s) cette nuit"), *Data->GetName(), Elite ? *Elite->GetName() : TEXT("?"), NightElites.Num() + PendingCenterSpawns.Num());
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : élite %s tué, la nuit se termine"), *Data->GetName(), Elite ? *Elite->GetName() : TEXT("?"));
	if (AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>())
	{
		GameMode->AdvanceDay();
	}
}

int32 AYSIsland::CountOpenMainObjectives() const
{
	int32 Count = 0;
	for (const FYSIslandObjectiveState& State : Objectives)
	{
		Count += State.bMain && !State.bDone ? 1 : 0;
	}
	return Count;
}

void AYSIsland::RefreshOpenDeliveryClasses()
{
	OpenDeliveryClasses.Reset();
	for (const FYSIslandObjectiveState& State : Objectives)
	{
		if (!State.bDone && State.DeliveryClass)
		{
			OpenDeliveryClasses.AddUnique(State.DeliveryClass);
		}
	}
	ForceNetUpdate();
}

bool AYSIsland::IsObjectExpected(const AYSInteractiveObject* Object) const
{
	if (!Object || Object->IsDelivered())
	{
		return false;
	}
	for (const TSubclassOf<AYSInteractiveObject>& Class : OpenDeliveryClasses)
	{
		if (Class && Object->IsA(Class))
		{
			return true;
		}
	}
	return false;
}

void AYSIsland::NotifyObjectDelivered(AYSInteractiveObject* Object)
{
	for (FYSIslandObjectiveState& State : Objectives)
	{
		if (!State.bDone && State.bSpawned && Object && State.DeliveryClass && Object->IsA(State.DeliveryClass))
		{
			State.bDone = true;
			UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : objectif %s accompli : %s (%d principal(aux) restant(s))"), *Data->GetName(), State.bMain ? TEXT("principal") : TEXT("secondaire"),
				*State.DisplayName.ToString(), CountOpenMainObjectives());
			RefreshOpenDeliveryClasses();
			if (State.bMain && CountOpenMainObjectives() == 0)
			{
				// le GameBoat passe à l'étape suivante et les îles suivantes apparaissent, même si la routine de coulaison
				// a déjà commencé (jour max atteint avant la livraison : avant, le bateau ne changeait plus)
				if (AYSFlowMode* FlowMode = GetWorld()->GetAuthGameMode<AYSFlowMode>())
				{
					FlowMode->OnIslandMainObjectivesDone(this);
				}
				// plus d'objectif principal : on passe au jour suivant, ce qui lance la routine de coulaison (OnGlobalDayChanged)
				AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
				if (GameMode && Phase != EYSIslandPhase::Sinking)
				{
					GameMode->AdvanceDay();
				}
			}
			return;
		}
	}
}

AYSIsland* AYSIsland::FindAt(const UWorld* World, const FVector& Location)
{
	AYSIsland* Nearest = nullptr;
	double NearestDistSq = TNumericLimits<double>::Max();
	for (TActorIterator<AYSIsland> It(World); It; ++It)
	{
		const double DistSq = FVector::DistSquared2D(It->GetActorLocation(), Location);
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = *It;
		}
	}
	return Nearest;
}

bool AYSIsland::IsBlockedAt(const UWorld* World, const FVector& Location, float Margin)
{
	const AYSIsland* Island = FindAt(World, Location);
	if (!Island)
	{
		return false;
	}
	const FVector2f Point = Island->WorldToLocal(Location);
	for (const FYSIslandMassif& Massif : Island->Plan.Massifs)
	{
		if (YSIslandGenerator::MassifDistance(Massif, Point) < Margin)
		{
			return true;
		}
	}
	return false;
}

void AYSIsland::LogStatus() const
{
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s en %s : graine %d, biome %s, phase %d (%.0f s), jour global %d (référence %d, jour de l'île %d), bateau %s, %d élite(s) de nuit"),
		Data ? *Data->GetName() : TEXT("?"), *GetActorLocation().ToCompactString(), Plan.Seed, *StaticEnum<EYSIslandGeneration>()->GetNameStringByValue(static_cast<int64>(Plan.Biome)),
		static_cast<int32>(Phase), PhaseElapsed, YSGameState ? YSGameState->GetDayNumber() : 0, ReferenceDay, GetIslandDay(), StartBoat ? *StartBoat->GetActorLocation().ToCompactString() : TEXT("-"), NightElites.Num());
	for (int32 Index = 0; Index < Objectives.Num(); ++Index)
	{
		const FYSIslandObjectiveState& State = Objectives[Index];
		UE_LOG(LogTemp, Log, TEXT("[YSIsland]   %d %s « %s » jour %d %s : %s"), Index, State.bMain ? TEXT("principal") : TEXT("secondaire"), *State.DisplayName.ToString(), State.Def.SpawnDay,
			State.bDone ? TEXT("FAIT") : State.bSpawned ? TEXT("en cours") : TEXT("pas encore apparu"), State.Item ? *State.Item->GetActorLocation().ToCompactString() : TEXT("-"));
	}
}

void AYSIsland::DebugSkipTime(float Seconds)
{
	PhaseElapsed += Seconds;
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] Temps de phase avancé de %.0f s : %.0f s"), Seconds, PhaseElapsed);
}

void AYSIsland::DebugSinkNow()
{
	StartSinkRoutine(TEXT("debug"));
	if (!bSinkStarted)
	{
		StartSinking();
	}
}

bool AYSIsland::GetPierSegmentLocation(int32 Index, FVector& OutLocation) const
{
	if (!Plan.PierSegments.IsValidIndex(Index))
	{
		return false;
	}
	const FYSIslandPierSegment& Segment = Plan.PierSegments[Index];
	const FVector3f Middle = (Segment.A + Segment.B) * 0.5f;
	OutLocation = LocalToWorld(FVector2f(Middle.X, Middle.Y), Middle.Z);
	return true;
}

AYSInteractiveObject* AYSIsland::GetObjectiveItem(int32 Index) const
{
	return Objectives.IsValidIndex(Index) ? Objectives[Index].Item.Get() : nullptr;
}
