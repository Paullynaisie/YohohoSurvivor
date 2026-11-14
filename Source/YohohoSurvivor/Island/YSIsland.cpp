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
#include "Interaction/YSObjectiveTreasureMap.h"
#include "Island/YSIslandData.h"
#include "Island/YSIslandNPC.h"
#include "Island/YSIslandParts.h"
#include "Island/YSPierLadder.h"
#include "Map/YSOcean.h"
#include "Net/UnrealNetwork.h"
#include "NPC/YSBrookerNPC.h"
#include "Player/YSPlayerCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Ship/YSPirateShip.h"
#include "Ship/YSShipHelm.h"
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
	constexpr float OffScreenFallbackSpacing = 200.0f;
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
	DOREPLIFETIME(AYSIsland, bForceSunk);
	DOREPLIFETIME(AYSIsland, bMainObjectivesDone);
	DOREPLIFETIME(AYSIsland, SubmergedDepth);
	DOREPLIFETIME(AYSIsland, RiseStartServerTime);
	DOREPLIFETIME(AYSIsland, RiseDuration);
	DOREPLIFETIME(AYSIsland, PropDespawnStartServerTime);
	DOREPLIFETIME(AYSIsland, PropDespawnRushServerTime);
}

void AYSIsland::StartRise(float Duration)
{
	if (!HasAuthority() || !IsSubmergedWaiting())
	{
		return;
	}
	RiseDuration = FMath::Max(Duration, 0.1f);
	RiseStartServerTime = FMath::Max(YSIslandActor::GetServerTime(GetWorld()), 0.001);
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s remonte à la surface (%.0f cm en %.0f s)"), Data ? *Data->GetName() : *GetName(), SubmergedDepth, RiseDuration);
}

bool AYSIsland::IsEmerged() const
{
	return SubmergedDepth <= 0.0f || bRiseDone;
}

void AYSIsland::UpdateRise()
{
	if (SubmergedDepth <= 0.0f || RiseStartServerTime <= 0.0 || bRiseDone)
	{
		return;
	}

	// une fois : l'île et tout ce qui y est accroché (props, pontons) rendus mobiles ; ce qui était statique le redevient à l'arrivée
	if (!bRiseGathered)
	{
		bRiseGathered = true;
		RiseOrigin = GetActorLocation();
		TArray<AActor*> Attached;
		GetAttachedActors(Attached, false, true);
		Attached.Insert(this, 0);
		for (AActor* Actor : Attached)
		{
			TInlineComponentArray<USceneComponent*> Components(Actor);
			for (USceneComponent* Component : Components)
			{
				if (Component->Mobility == EComponentMobility::Static)
				{
					RiseStaticComponents.Add(Component);
				}
				Component->SetMobility(EComponentMobility::Movable);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s remonte : %d acteur(s) (%s)"), *Data->GetName(), Attached.Num(), HasAuthority() ? TEXT("serveur") : TEXT("client"));
	}

	// même remontée sur toutes les machines (temps serveur), ralentie à l'arrivée
	const float Alpha = FMath::Clamp(static_cast<float>(YSIslandActor::GetServerTime(GetWorld()) - RiseStartServerTime) / FMath::Max(RiseDuration, 0.1f), 0.0f, 1.0f);
	const float Eased = 1.0f - FMath::Square(1.0f - Alpha);
	const FVector Target = RiseOrigin + FVector(0.0f, 0.0f, SubmergedDepth * Eased);
	if (!GetActorLocation().Equals(Target, 0.1f))
	{
		SetActorLocation(Target);
	}
	if (Alpha < 1.0f)
	{
		return;
	}

	// arrivée : statique comme une île générée à la surface (props accrochés à un parent statique), puis objectifs (serveur)
	bRiseDone = true;
	for (const TWeakObjectPtr<USceneComponent>& Component : RiseStaticComponents)
	{
		if (Component.IsValid())
		{
			Component->SetMobility(EComponentMobility::Static);
		}
	}
	RiseStaticComponents.Empty();
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s à la surface (%s)"), *Data->GetName(), HasAuthority() ? TEXT("serveur") : TEXT("client"));
	if (HasAuthority() && bGameplayActorsPending)
	{
		bGameplayActorsPending = false;
		SpawnGameplayActors();
		ForceNetUpdate();
	}
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

FVector AYSIsland::GetCenterLocation() const
{
	return Data ? LocalToWorld(Plan.CenterSpot, Data->Generation.PlateauHeight) : GetActorLocation();
}

bool AYSIsland::FindFreeCenterGround(FVector& OutGround) const
{
	const AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	if (!GameMode || !Data)
	{
		return false;
	}
	for (int32 Ring = 0; Ring <= YSIslandActor::CenterRingCount; ++Ring)
	{
		const int32 Spots = Ring == 0 ? 1 : YSIslandActor::CenterRingSpots;
		for (int32 Index = 0; Index < Spots; ++Index)
		{
			const float Angle = 2.0f * PI * Index / Spots;
			const FVector2f Spot = Plan.CenterSpot + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * (Ring * YSIslandActor::CenterRingStep);
			if (IsSpotFree(Spot, YSIslandActor::SpotMargin) && GameMode->FindIslandGround(LocalToWorld(Spot, Data->Generation.PlateauHeight), OutGround))
			{
				return true;
			}
		}
	}
	return GameMode->FindIslandGround(GetCenterLocation(), OutGround);
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
	// prégénérée sous l'eau : objectifs et zone de dépôt une fois remontée
	if (SubmergedDepth > 0.0f && !bRiseDone)
	{
		bGameplayActorsPending = true;
	}
	else
	{
		SpawnGameplayActors();
	}
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
			// île en mouvement (remontée, coulaison) : un prop statique ne s'accroche pas à un parent mobile
			if (TerrainMesh->Mobility == EComponentMobility::Movable)
			{
				TInlineComponentArray<USceneComponent*> Components(Actor);
				for (USceneComponent* Component : Components)
				{
					Component->SetMobility(EComponentMobility::Movable);
				}
			}
			Actor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			LocalProps.Add(Actor);
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

	// échelles du quai de la plage : sur chaque machine (pas répliquées, comme le ponton), accrochées à l'île
	for (const FVector4f& Ladder : Plan.PierLadders)
	{
		const float DeckHeight = Data->Generation.PierDeckHeight;
		const FTransform Transform(FRotator(0.0f, GetActorRotation().Yaw + Ladder.W, 0.0f), LocalToWorld(FVector2f(Ladder.X, Ladder.Y), Ladder.Z - DeckHeight));
		AYSPierLadder* PierLadder = GetWorld()->SpawnActorDeferred<AYSPierLadder>(AYSPierLadder::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (PierLadder)
		{
			PierLadder->SetupPier(DeckHeight, Data->Generation.BeachQuayLadderLandingInset);
			PierLadder->FinishSpawning(Transform);
			PierLadder->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : échelle du quai %s en %s"), *GetName(), *PierLadder->GetName(), *Transform.GetLocation().ToCompactString());
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
	// demande de Paul : zone de dépôt accrochée à l'île seulement pour l'île de départ ; sur les îles suivantes, les objectifs se posent
	// dans la zone du core du bateau des joueurs (AYSShipCore)
	const bool bGameBoatFlow = FlowMode && FlowMode->GameBoatData;
	if (bStartIsland && bGameBoatFlow)
	{
		if (!FlowMode->SpawnGameBoat(this))
		{
			SpawnStartBoat(LocalToWorld(Plan.BoatLocation, Terrain.Height(Plan.BoatLocation.X, Plan.BoatLocation.Y)), Plan.BoatYaw + GetActorRotation().Yaw);
		}
	}
	else if (!bGameBoatFlow)
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
	// objets des zones d'intérêt répartis par le flow
	for (const TPair<FYSIslandObjectiveDef, bool>& Extra : ExtraObjectives)
	{
		FYSIslandObjectiveState& State = Objectives.AddDefaulted_GetRef();
		State.Def = Extra.Key;
		State.bMain = Extra.Value;
	}
	for (FYSIslandObjectiveState& State : Objectives)
	{
		State.DisplayName = State.Def.DisplayName;
		State.DeliveryClass = State.Def.bRandomCombatPost ? nullptr : State.Def.DeliveryClass ? State.Def.DeliveryClass : State.Def.ItemClass;
	}
	SpawnDueObjectives();
	RefreshOpenDeliveryClasses();
	// flow sans objectif principal : directement « Survivez »
	if (UsesObjectiveFlow() && CountOpenMainObjectives() == 0)
	{
		bMainObjectivesDone = true;
	}
}

void AYSIsland::AddExtraObjective(const FYSIslandObjectiveDef& Def, bool bMain)
{
	ExtraObjectives.Emplace(Def, bMain);
}

void AYSIsland::SpawnDueObjectives()
{
	const int32 Day = GetIslandDay();
	for (FYSIslandObjectiveState& State : Objectives)
	{
		// boussole d'une île des zones d'intérêt : posée par la carte au trésor d'objectif (accostage), pas sur l'île
		if (!State.bSpawned && !State.bDone && !State.bWaitsObjectiveMap && UsesObjectiveMap(State))
		{
			State.bWaitsObjectiveMap = true;
			UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : objectif « %s » posé par une carte au trésor d'objectif à l'accostage"), *Data->GetName(), *State.Def.DisplayName.ToString());
		}
		if (!State.bSpawned && !State.bDone && !State.bWaitsObjectiveMap && State.Def.SpawnDay <= Day)
		{
			SpawnObjective(State);
		}
	}
}

bool AYSIsland::UsesObjectiveMap(const FYSIslandObjectiveState& State) const
{
	const AYSFlowMode* FlowMode = GetWorld()->GetAuthGameMode<AYSFlowMode>();
	return !bStartIsland && FlowMode && FlowMode->ObjectiveTreasureMapClass && FlowMode->IsCompassClass(State.Def.ItemClass);
}

bool AYSIsland::FindObjectiveMapSpot(const FVector2f* AwayVirtual, float MinDistance, float MaxDistanceFromCenter, float Scale, FVector& OutWorld, FVector2f& OutVirtual)
{
	const float Reach = Data->Generation.Radius;
	const FVector2f AwayLocal = AwayVirtual ? *AwayVirtual : FVector2f::ZeroVector;
	const FVector2f AwayDirection = AwayLocal.GetSafeNormal();
	const float SpotScale = FMath::Clamp(Scale, 0.05f, 1.0f);
	for (int32 Attempt = 0; Attempt < YSIslandActor::SpotAttempts * 4; ++Attempt)
	{
		// tirage sur l'île pleine taille (directions de la carte), sur l'île
		const float Range = MaxDistanceFromCenter > 0.0f ? FMath::Min(MaxDistanceFromCenter, Reach) : Reach;
		const FVector2f Virtual(ServerRng.FRandRange(-Range, Range), ServerRng.FRandRange(-Range, Range));
		if ((MaxDistanceFromCenter > 0.0f && Virtual.SizeSquared() > FMath::Square(MaxDistanceFromCenter)) || Terrain.Inland(Virtual.X, Virtual.Y) < 0.0f)
		{
			continue;
		}
		// moitié opposée : de l'autre côté du centre par rapport au point précédent, et assez loin de lui
		if (AwayVirtual && (FVector2f::DotProduct(Virtual, AwayDirection) > 0.0f || (Virtual - AwayLocal).Size() < MinDistance))
		{
			continue;
		}
		// point réel : rapproché du centre (chemin raccourci, mêmes directions)
		const FVector2f Spot = Virtual * SpotScale;
		// sur le plateau : ni sur la plage ni au bord de la falaise, hors des massifs et des props
		const float Theta = FMath::Atan2(Spot.Y, Spot.X);
		if (Terrain.Inland(Spot.X, Spot.Y) < FMath::Max(300.0f, Terrain.BeachDepth(Theta) * Terrain.BeachWeight(Theta)) || !IsSpotFree(Spot, YSIslandActor::SpotMargin))
		{
			continue;
		}
		const FVector Ground = LocalToWorld(Spot, Terrain.Height(Spot.X, Spot.Y));
		if (AYSOcean::IsUnderwater(GetWorld(), Ground))
		{
			continue;
		}
		OutWorld = Ground;
		OutVirtual = Virtual;
		return true;
	}
	return false;
}

void AYSIsland::SpawnObjectiveMaps(const FVector& Landing, const TCHAR* Reason)
{
	const AYSFlowMode* FlowMode = GetWorld()->GetAuthGameMode<AYSFlowMode>();
	const UClass* MapClass = FlowMode ? FlowMode->ObjectiveTreasureMapClass.Get() : nullptr;
	const AYSObjectiveTreasureMap* Settings = MapClass ? MapClass->GetDefaultObject<AYSObjectiveTreasureMap>() : nullptr;
	if (!Settings)
	{
		return;
	}
	int32 MapRank = 0;
	for (int32 Index = 0; Index < Objectives.Num(); ++Index)
	{
		FYSIslandObjectiveState& State = Objectives[Index];
		if (!State.bWaitsObjectiveMap || State.bObjectiveMapSpawned || State.bDone)
		{
			continue;
		}
		State.bObjectiveMapSpawned = true;

		// sur la plage, à côté du point d'accostage (vers l'intérieur, décalée sur le côté de la zone de dépôt)
		const FVector2f LandingLocal = WorldToLocal(Landing);
		const FVector2f Inward = (-LandingLocal).GetSafeNormal();
		const FVector2f Side(-Inward.Y, Inward.X);
		const float SideOffset = (Data->BoatZoneRadius + 250.0f) * (MapRank % 2 == 0 ? 1.0f : -1.0f) * (1 + MapRank / 2);
		FVector2f StartLocal = LandingLocal + Side * SideOffset;
		if (Terrain.Inland(StartLocal.X, StartLocal.Y) < 0.0f)
		{
			StartLocal = FindShorePointToward(StartLocal) + Inward * 150.0f;
		}
		const FVector Start = LocalToWorld(StartLocal, Terrain.Height(StartLocal.X, StartLocal.Y));
		++MapRank;

		// 3 tracés : moitié opposée au départ, moitié opposée à la fin du 1er, autour du centre
		// (tirés sur l'île pleine taille puis rapprochés du centre x Path Scale : chemin raccourci, mêmes directions, fin au centre)
		FVector End1, End2, End3;
		FVector2f Virtual1, Virtual2, Virtual3;
		const float PathScale = Settings->GetPathScale();
		const bool bFound1 = FindObjectiveMapSpot(&StartLocal, Settings->GetMinLegLength(), 0.0f, PathScale, End1, Virtual1);
		const bool bFound2 = bFound1 && FindObjectiveMapSpot(&Virtual1, Settings->GetMinLegLength(), 0.0f, PathScale, End2, Virtual2);
		const bool bFound3 = FindObjectiveMapSpot(nullptr, 0.0f, Settings->GetFinalCenterRadius(), PathScale, End3, Virtual3);
		if (!bFound1 || !bFound2 || !bFound3)
		{
			// sans place : la boussole est posée normalement (l'objectif ne doit jamais être perdu)
			UE_LOG(LogTemp, Warning, TEXT("[YSIsland] %s : carte au trésor d'objectif sans emplacement (%d / %d / %d), objectif « %s » posé sur l'île"), *Data->GetName(),
				bFound1, bFound2, bFound3, *State.Def.DisplayName.ToString());
			State.bWaitsObjectiveMap = false;
			SpawnObjective(State);
			continue;
		}
		TArray<FVector> Path;
		Path.Add(Start);
		TArray<int32> LegEnds;
		for (const FVector& End : { End1, End2, End3 })
		{
			TArray<FVector> Leg;
			Settings->BuildLeg(GetWorld(), Path.Last(), End, Leg);
			Path.Append(Leg);
			LegEnds.Add(Path.Num() - 1);
		}

		const FTransform Transform(FRotator(0.0f, ServerRng.FRandRange(0.0f, 360.0f), 0.0f), Start);
		AYSObjectiveTreasureMap* Map = GetWorld()->SpawnActorDeferred<AYSObjectiveTreasureMap>(FlowMode->ObjectiveTreasureMapClass.Get(), Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Map)
		{
			State.bWaitsObjectiveMap = false;
			SpawnObjective(State);
			continue;
		}
		Map->InitObjectiveMap(Path, LegEnds, this, Index);
		Map->FinishSpawning(Transform);
		ObjectiveMaps.Add(Map);
		float PathLength = 0.0f;
		for (int32 Point = 1; Point < Path.Num(); ++Point)
		{
			PathLength += FVector::Dist2D(Path[Point - 1], Path[Point]);
		}
		UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : carte au trésor d'objectif %s (%s) en %s pour « %s » : %d points, %.0f m (échelle %.2f), fins des tracés %s / %s / %s"), *Data->GetName(), *Map->GetName(), Reason,
			*Start.ToCompactString(), *State.Def.DisplayName.ToString(), Path.Num(), PathLength / 100.0f, PathScale, *End1.ToCompactString(), *End2.ToCompactString(), *End3.ToCompactString());
	}
}

void AYSIsland::OnObjectiveMapCompleted(AYSObjectiveTreasureMap* Map, int32 ObjectiveIndex, const FVector& Location)
{
	ObjectiveMaps.Remove(Map);
	if (!Objectives.IsValidIndex(ObjectiveIndex))
	{
		return;
	}
	FYSIslandObjectiveState& State = Objectives[ObjectiveIndex];
	State.bWaitsObjectiveMap = false;
	State.bSpawned = true;
	if (State.Def.ItemClass)
	{
		State.Item = AYSInteractiveObject::SpawnOnGround(GetWorld(), State.Def.ItemClass, Location + FVector(0.0f, 0.0f, 100.0f), ServerRng.FRandRange(0.0f, 360.0f));
	}
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : carte au trésor d'objectif %s finie, objectif « %s » : %s en %s"), *Data->GetName(), *GetNameSafe(Map), *State.Def.DisplayName.ToString(),
		State.Item ? *State.Item->GetName() : TEXT("ÉCHEC"), *Location.ToCompactString());
}

AYSObjectiveTreasureMap* AYSIsland::GetObjectiveMap() const
{
	for (const TWeakObjectPtr<AYSObjectiveTreasureMap>& Map : ObjectiveMaps)
	{
		if (Map.IsValid())
		{
			return Map.Get();
		}
	}
	return nullptr;
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
	FYSIslandObjectiveDef Def = State.Def;
	State.bSpawned = true;
	UWorld* World = GetWorld();
	// poste de combat au hasard parmi tous ceux qui existent
	if (Def.bRandomCombatPost)
	{
		const AYSGameMode* GameMode = World->GetAuthGameMode<AYSGameMode>();
		Def.ItemClass = GameMode ? GameMode->PickRandomCombatPostItem() : nullptr;
	}
	if (!Def.ItemClass)
	{
		return;
	}
	AYSInteractiveObject* Item = nullptr;
	const AYSTreasureMap* MapSettings = Cast<AYSTreasureMap>(Def.ItemClass->GetDefaultObject());
	// le plus au nord possible dans l'espace accessible
	FVector2f NorthSpot;
	if (Def.bNorthMost && !MapSettings && FindNorthSpot(NorthSpot))
	{
		Item = AYSInteractiveObject::SpawnOnGround(World, Def.ItemClass, LocalToWorld(NorthSpot, Terrain.Height(NorthSpot.X, NorthSpot.Y)), ServerRng.FRandRange(0.0f, 360.0f));
	}
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

	// remontée (îles prégénérées) et descente : sur chaque machine, depuis l'heure serveur
	UpdateRise();
	UpdateSink();
	if (Data)
	{
		TickPendingProps();
		UpdatePropDespawn();
	}

	if (!HasAuthority() || !Data)
	{
		return;
	}

	// coulaison finie : le flow est prévenu (île de départ : la partie 2 commence)
	if (SinkStartServerTime > 0.0 && !bSinkFinished && !IsSinking())
	{
		bSinkFinished = true;
		UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s a fini de couler"), *Data->GetName());
		if (AYSFlowMode* SunkFlowMode = GetWorld()->GetAuthGameMode<AYSFlowMode>())
		{
			SunkFlowMode->OnIslandSunk(this);
		}
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

	// coulée de force (partie 3 du flow) : plus aucun cycle ni apparition
	if (bForceSunk)
	{
		return;
	}

	// îles de la partie 2 : le bateau des joueurs qui accoste devant la plage y reste bloqué (et active l'île)
	if (UsesObjectiveFlow() && Data->bLockHelmWhenDocked && !bBoatDocked && !bFlowVictory && IsEmerged()
		&& (Phase == EYSIslandPhase::Inactive || Phase == EYSIslandPhase::Day || Phase == EYSIslandPhase::Night))
	{
		TickDocking();
	}

	if (Phase == EYSIslandPhase::Inactive)
	{
		// sous l'eau ou en train de remonter : rien ne l'active
		if (IsEmerged())
		{
			TickActivation();
		}
		return;
	}

	PhaseElapsed += DeltaSeconds;
	TickPhaseEvents();
	TickPendingCenterSpawns(DeltaSeconds);

	// flow : overtime une fois la nuit écoulée (ses vagues comptées depuis), spawn constant, évacuation
	// mode Peaceful : ni timer de jour ni overtime, ni spawn constant ; la nuit tombe quand l'objectif de l'île est atteint
	const bool bPeaceful = IsPeacefulFlow();
	if (UsesObjectiveFlow())
	{
		if (!bPeaceful && Phase == EYSIslandPhase::Night && !bOvertime && PhaseElapsed >= Data->NightDuration)
		{
			StartOvertime();
		}
		if (bOvertime && Phase == EYSIslandPhase::Night)
		{
			TickEventList(Data->OvertimeEvents, OvertimeEventsFired, PhaseElapsed - Data->NightDuration);
		}
		if (!bPeaceful)
		{
			TickConstantSpawn(DeltaSeconds);
		}
		if (Phase == EYSIslandPhase::Evacuation)
		{
			TickEvacuation();
		}
	}

	if (Phase == EYSIslandPhase::Day && (bPeaceful ? bMainObjectivesDone : PhaseElapsed >= Data->DayDuration))
	{
		if (bPeaceful)
		{
			UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : Peaceful, objectif atteint : la nuit tombe"), *Data->GetName());
		}
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
	// la zone de dépôt disparaît quand l'île commence à couler
	if (IsValid(StartBoat))
	{
		StartBoat->Destroy();
		StartBoat = nullptr;
	}
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

	// coulée de force (partie 3, îles loin du combat) : disparaît d'un coup au lieu de descendre ; descendre déplaçait chaque image
	// les milliers de props accrochés (mesuré : ~50 ms du fil du jeu par image pour 3 îles, moins de 10 FPS au début du combat)
	if (bForceSunk)
	{
		if (!bForceSunkHidden)
		{
			bForceSunkHidden = true;
			TArray<AActor*> Attached;
			GetAttachedActors(Attached, false, true);
			Attached.Insert(this, 0);
			int32 Destroyed = 0;
			for (AActor* Actor : Attached)
			{
				if (!IsValid(Actor))
				{
					continue;
				}
				// objets posés (coffres, objectifs) : retirés par le serveur ; le reste (terrain, props locaux, pontons, PNJ) caché sans collision
				if (Actor->IsA<AYSInteractiveObject>() && Actor->GetIsReplicated())
				{
					if (HasAuthority())
					{
						Actor->Destroy();
						++Destroyed;
					}
					continue;
				}
				Actor->SetActorHiddenInGame(true);
				Actor->SetActorEnableCollision(false);
			}
			UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s coulée d'un coup : %d acteur(s) cachés, %d objet(s) retirés (%s)"), *Data->GetName(), Attached.Num() - Destroyed,
				Destroyed, HasAuthority() ? TEXT("serveur") : TEXT("client"));
		}
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
			// arrivé sans accoster (à la nage, en chaloupe) : la carte au trésor d'objectif apparaît sur la plage la plus proche de lui
			const FVector2f Shore = FindShorePointToward(WorldToLocal(Character->GetActorLocation()) * 1.5f);
			const FVector2f Inland = Shore - Shore.GetSafeNormal() * Data->BoatZoneRadius * 0.5f;
			SpawnObjectiveMaps(LocalToWorld(Inland, Terrain.Height(Inland.X, Inland.Y)), TEXT("joueur arrivé sans accoster"));
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
	bOvertime = false;
	UpdateFlowStatus();
	ForceNetUpdate();
}

void AYSIsland::StartNightPhase()
{
	Phase = EYSIslandPhase::Night;
	NightElites.Reset();
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : tombée de la nuit %d de l'île%s"), *Data->GetName(), GetIslandDay(),
		IsPeacefulFlow() ? TEXT(" (Peaceful : pas d'overtime)") : UsesObjectiveFlow() ? *FString::Printf(TEXT(" (overtime dans %.0f s)"), Data->NightDuration) : TEXT(""));
	StartPhaseEvents(&Data->NightEvents);

	// flow : les élites / boss de la nuit suivent la séquence de nuits de la partie (quelle que soit l'île), à la place des vagues « au centre »
	TArray<UClass*> SetupClasses;
	AYSFlowMode* NightFlowMode = UsesObjectiveFlow() ? GetWorld()->GetAuthGameMode<AYSFlowMode>() : nullptr;
	bNightSetupOverride = NightFlowMode && NightFlowMode->TakeNightSetup(this, ServerRng, SetupClasses);
	for (UClass* Class : SetupClasses)
	{
		if (AYSFoeCharacter* Foe = TrySpawnAtCenter(Class, INDEX_NONE, 0))
		{
			TrackNightElite(Foe);
		}
		else
		{
			PendingCenterSpawns.Add({ Class, INDEX_NONE, 0 });
		}
	}
	bOvertime = false;
	UpdateFlowStatus();
	ForceNetUpdate();
}

bool AYSIsland::UsesObjectiveFlow() const
{
	return Data && Data->bObjectiveFlow;
}

bool AYSIsland::IsPeacefulFlow() const
{
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	return UsesObjectiveFlow() && YSGameState && YSGameState->IsPeaceful();
}

void AYSIsland::StartOvertime()
{
	bOvertime = true;
	OvertimeEventsFired.Reset();
	OvertimeEventsFired.SetNumZeroed(Data->OvertimeEvents.Num());
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : OVERTIME (nuit %d écoulée, %d vague(s) d'overtime)"), *Data->GetName(), GetIslandDay(), Data->OvertimeEvents.Num());
	UpdateFlowStatus();
}

void AYSIsland::StartEvacuation()
{
	Phase = EYSIslandPhase::Evacuation;
	StartPhaseEvents(nullptr);
	PendingCenterSpawns.Reset();
	bOvertime = false;
	FlowBoarded = INDEX_NONE;
	FlowBoardingTotal = INDEX_NONE;
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : « %s » (%.0f s pour monter à bord)"), *Data->GetName(), *Data->EvacuateText.ToString(), Data->EvacuationDuration);
	if (Data->bDespawnPropsOnEvacuation && PropDespawnStartServerTime <= 0.0)
	{
		PropDespawnStartServerTime = FMath::Max(YSIslandActor::GetServerTime(GetWorld()), 0.001);
	}
	TickEvacuation();
	UpdateFlowStatus();
	ForceNetUpdate();
}

void AYSIsland::TickEvacuation()
{
	// joueurs vivants, et ceux d'entre eux qui sont sur le bateau des joueurs (debout dessus ou à un poste)
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	const AYSPirateShip* GameBoat = YSGameState ? YSGameState->GetGameBoat() : nullptr;
	int32 Total = 0;
	int32 Boarded = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AYSPlayerCharacter* Character = It->Get() ? Cast<AYSPlayerCharacter>(It->Get()->GetPawn()) : nullptr;
		if (!Character || !Character->IsAlive())
		{
			continue;
		}
		++Total;
		Boarded += GameBoat && Character->GetShipUnderFeet() == GameBoat ? 1 : 0;
	}
	if (Boarded != FlowBoarded || Total != FlowBoardingTotal)
	{
		FlowBoarded = Boarded;
		FlowBoardingTotal = Total;
		UE_LOG(LogTemp, Log, TEXT("[YSIsland] Évacuation : %d / %d joueur(s) à bord"), Boarded, Total);
		UpdateFlowStatus();
	}
	// île de départ : tant que le CoreBoat n'est pas ramené, il n'y a pas de bateau où monter, seule la fin du temps compte
	// (îles de la partie 2 : le bateau est déjà là, les objectifs ne sont pas obligatoires)
	const bool bCanLeave = bMainObjectivesDone || !Data->bGameOverWithoutMainObjectives;
	if (bCanLeave && Total > 0 && Boarded == Total)
	{
		FinishEvacuation(TEXT("tous les joueurs sont à bord"));
	}
	else if (PhaseElapsed >= Data->EvacuationDuration)
	{
		if (bCanLeave)
		{
			FinishEvacuation(TEXT("temps écoulé"));
		}
		else if (AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>())
		{
			bFlowVictory = false;
			PendingCenterSpawns.Reset();
			StartPhaseEvents(nullptr);
			Phase = EYSIslandPhase::Sinking;
			UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : fin de l'évacuation sans le CoreBoat : GAME OVER"), *Data->GetName());
			GameMode->GameOver(TEXT("CoreBoat pas ramené à la fin de l'évacuation"));
		}
	}
}

void AYSIsland::FinishEvacuation(const TCHAR* Reason)
{
	if (bFlowVictory)
	{
		return;
	}
	// le bateau devient pilotable et l'île coule tout de suite (pas de routine de coulaison) ; île de départ : la partie 2 commence quand elle a
	// fini de couler (AYSFlowMode::OnIslandSunk). Le chronomètre continue (la partie continue)
	bFlowVictory = true;
	Phase = EYSIslandPhase::Sinking;
	StartPhaseEvents(nullptr);
	PendingCenterSpawns.Reset();
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : fin de l'évacuation (%s, %d / %d à bord) : bateau pilotable, l'île coule%s"),
		*Data->GetName(), Reason, FlowBoarded, FlowBoardingTotal, bStartIsland ? TEXT(" (la partie 2 commencera quand elle aura coulé)") : TEXT(""));
	UpdateFlowStatus();
	// props pas encore partis (tous à bord avant la fin) : tous les restants rétrécissent maintenant
	if (PropDespawnStartServerTime > 0.0 && PropDespawnRushServerTime <= 0.0)
	{
		PropDespawnRushServerTime = FMath::Max(YSIslandActor::GetServerTime(GetWorld()), 0.001);
	}
	if (!bSinkStarted)
	{
		StartSinking();
	}
	ForceNetUpdate();
}

void AYSIsland::UpdatePropDespawn()
{
	if (PropDespawnStartServerTime <= 0.0 || (PropDespawnNext >= LocalProps.Num() && ShrinkingProps.IsEmpty()))
	{
		return;
	}
	const double Now = YSIslandActor::GetServerTime(GetWorld());
	const float ScaleDuration = FMath::Max(Data->PropDespawnScaleDuration, 0.05f);
	const float BatchInterval = FMath::Max(Data->PropDespawnBatchInterval, 0.05f);

	// ordre tiré au hasard, le même sur chaque machine (graine du plan, props posés dans l'ordre du plan) : les collisions restent d'accord
	if (!bPropDespawnShuffled)
	{
		bPropDespawnShuffled = true;
		FRandomStream Stream(Plan.Seed ^ 0x5EED);
		for (int32 Index = LocalProps.Num() - 1; Index > 0; --Index)
		{
			LocalProps.Swap(Index, Stream.RandRange(0, Index));
		}
		// le dernier paquet doit avoir fini de rétrécir à la fin de l'évacuation
		const int32 Batches = FMath::Max(1, FMath::FloorToInt((Data->EvacuationDuration - ScaleDuration) / BatchInterval) + 1);
		PropDespawnBatchSize = FMath::Max(1, FMath::DivideAndRoundUp(LocalProps.Num(), Batches));
		UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : évacuation, %d props retirés par paquets de %d toutes les %.1f s (%s)"), *Data->GetName(), LocalProps.Num(),
			PropDespawnBatchSize, BatchInterval, HasAuthority() ? TEXT("serveur") : TEXT("client"));
	}

	const bool bRush = PropDespawnRushServerTime > 0.0 && Now >= PropDespawnRushServerTime;
	const int32 BatchesStarted = FMath::FloorToInt(FMath::Max(0.0, Now - PropDespawnStartServerTime) / BatchInterval) + 1;
	const int32 Due = bRush ? LocalProps.Num() : FMath::Min(LocalProps.Num(), BatchesStarted * PropDespawnBatchSize);
	if (bRush && PropDespawnNext < Due)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : l'île coule, %d props restants retirés d'un coup"), *Data->GetName(), Due - PropDespawnNext);
	}
	for (; PropDespawnNext < Due; ++PropDespawnNext)
	{
		AActor* Actor = LocalProps[PropDespawnNext].Get();
		if (!IsValid(Actor))
		{
			continue;
		}
		// prop statique : rendu mobile pour pouvoir changer d'échelle
		TInlineComponentArray<USceneComponent*> Components(Actor);
		for (USceneComponent* Component : Components)
		{
			Component->SetMobility(EComponentMobility::Movable);
		}
		ShrinkingProps.Add({ Actor, Actor->GetActorScale3D(), Now });
	}

	for (int32 Index = ShrinkingProps.Num() - 1; Index >= 0; --Index)
	{
		const FShrinkingProp& Prop = ShrinkingProps[Index];
		AActor* Actor = Prop.Actor.Get();
		const float Alpha = static_cast<float>((Now - Prop.StartTime) / ScaleDuration);
		if (!IsValid(Actor) || Alpha >= 1.0f)
		{
			if (IsValid(Actor))
			{
				Actor->Destroy();
			}
			ShrinkingProps.RemoveAtSwap(Index);
			continue;
		}
		Actor->SetActorScale3D(Prop.StartScale * FMath::Max(1.0f - FMath::InterpEaseIn(0.0f, 1.0f, Alpha, 2.0f), 0.01f));
	}
	if (PropDespawnNext >= LocalProps.Num() && ShrinkingProps.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : tous les props retirés"), *Data->GetName());
		LocalProps.Empty();
		PropDespawnNext = 0;
	}
}

void AYSIsland::DebugFinishEvacuation()
{
	FinishEvacuation(TEXT("debug"));
}

void AYSIsland::UpdateFlowStatus()
{
	AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	if (!UsesObjectiveFlow() || !YSGameState || !HasAuthority())
	{
		return;
	}
	FYSFlowStatus Status;
	if (bFlowVictory)
	{
		// île évacuée : plus rien de l'île, bateau libre ; le HUD affiche l'objectif en mer (demande de Paul : plus de « Victoire »)
		YSGameState->SetFlowStatus(Status);
		return;
	}
	Status.Island = this;
	if (Phase == EYSIslandPhase::Evacuation)
	{
		Status.Objective = EYSFlowObjective::Evacuate;
		Status.ObjectiveText = Data->EvacuateText;
		Status.PhaseDuration = Data->EvacuationDuration;
		Status.BoardedPlayers = FMath::Max(FlowBoarded, 0);
		Status.BoardingTotal = FMath::Max(FlowBoardingTotal, 0);
	}
	else if (Phase == EYSIslandPhase::Day || Phase == EYSIslandPhase::Night)
	{
		Status.Objective = bMainObjectivesDone ? EYSFlowObjective::Defend : EYSFlowObjective::FindCore;
		Status.ObjectiveText = bMainObjectivesDone ? Data->DefendText : BuildFindText();
		// Peaceful : pas de timer de jour ni de nuit (barre cachée)
		Status.PhaseDuration = IsPeacefulFlow() ? 0.0f : Phase == EYSIslandPhase::Day ? Data->DayDuration : Data->NightDuration;
		Status.bNight = Phase == EYSIslandPhase::Night;
		Status.bOvertime = bOvertime;
	}
	else
	{
		return;
	}
	Status.PhaseStartServerTime = FMath::Max(YSIslandActor::GetServerTime(GetWorld()) - PhaseElapsed, 0.001);
	Status.bHelmLocked = !bFlowVictory && (Data->bLockHelmUntilEvacuation || bBoatDocked);
	Status.FinalMatchTime = FlowFinalMatchTime;
	YSGameState->SetFlowStatus(Status);
}

FVector AYSIsland::GetBeachApproachPoint(float Distance, float& OutYaw) const
{
	// plage au sud de l'île (-X local)
	const FVector2f Local(-(Terrain.CoastRadius(PI) + Distance), 0.0f);
	OutYaw = GetActorRotation().Yaw;
	return LocalToWorld(Local, 0.0f);
}

FText AYSIsland::BuildFindText() const
{
	if (Data->FindObjectiveFormat.IsEmpty())
	{
		return Data->FindText;
	}
	TArray<FText> Names;
	for (const FYSIslandObjectiveState& State : Objectives)
	{
		if (!State.bMain || State.bDone)
		{
			continue;
		}
		const UClass* Class = State.Item ? State.Item->GetClass() : State.Def.ItemClass.Get();
		const AYSInteractiveObject* Settings = Class ? Class->GetDefaultObject<AYSInteractiveObject>() : nullptr;
		const FText Name = Settings ? Settings->GetObjectiveName() : State.DisplayName;
		if (!Name.IsEmpty() && !Names.ContainsByPredicate([&Name](const FText& Other) { return Other.EqualTo(Name); }))
		{
			Names.Add(Name);
		}
	}
	return Names.Num() > 0 ? FText::Format(Data->FindObjectiveFormat, FText::Join(Data->FindObjectiveSeparator, Names)) : Data->FindText;
}

void AYSIsland::TickDocking()
{
	AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	AYSPirateShip* GameBoat = YSGameState ? YSGameState->GetGameBoat() : nullptr;
	if (!GameBoat || !GameBoat->IsHullGrounded() || GameBoat->IsSinking())
	{
		return;
	}
	// échoué devant la plage (pas contre une falaise, ni contre un autre bateau au large)
	const FVector2f Local = WorldToLocal(GameBoat->GetActorLocation());
	const float FromShore = -Terrain.Inland(Local.X, Local.Y);
	const float BeachWeight = Terrain.BeachWeight(FMath::Atan2(Local.Y, Local.X));
	if (FromShore > Data->DockMaxDistanceFromShore || BeachWeight < Data->DockMinBeachWeight)
	{
		return;
	}
	bBoatDocked = true;
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : %s accoste devant la plage (%.0f cm du bord de l'eau, plage %.2f) : bloqué à quai jusqu'à la fin de l'évacuation"),
		*Data->GetName(), *GameBoat->GetName(), FromShore, BeachWeight);
	// le pilote lâche la barre
	GameBoat->SetPilotInput(0.0f, 0.0f);
	if (AYSShipHelm* Helm = GameBoat->GetHelm())
	{
		if (AYSPlayerCharacter* Pilot = Helm->GetOccupant())
		{
			Pilot->LeaveStation(TEXT("bateau bloqué à quai"), true);
		}
	}
	// zone de dépôt sur la plage au point d'accostage (déposer coffres et objets sans monter à bord)
	if (!StartBoat)
	{
		const FVector2f Shore = FindShorePointToward(Local);
		const float Inset = Data->BoatZoneRadius * 0.5f;
		const FVector2f Inland = Shore - Shore.GetSafeNormal() * Inset;
		SpawnStartBoat(LocalToWorld(Inland, Terrain.Height(Inland.X, Inland.Y)), GameBoat->GetActorRotation().Yaw);
		UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : zone de dépôt posée au point d'accostage (%s)"), *Data->GetName(), StartBoat ? *StartBoat->GetActorLocation().ToCompactString() : TEXT("échec"));
	}
	// boussole de l'île : carte au trésor d'objectif sur la plage, à côté du point d'accostage
	{
		const FVector2f Shore = FindShorePointToward(Local);
		const FVector2f Inland = Shore - Shore.GetSafeNormal() * Data->BoatZoneRadius * 0.5f;
		SpawnObjectiveMaps(LocalToWorld(Inland, Terrain.Height(Inland.X, Inland.Y)), TEXT("accostage"));
	}
	if (Phase == EYSIslandPhase::Inactive)
	{
		Activate();
	}
	else
	{
		UpdateFlowStatus();
	}
}

FVector2f AYSIsland::FindShorePointToward(const FVector2f& FromLocal) const
{
	// du bateau vers le centre de l'île, premier point hors de l'eau (bord de la plage)
	const float Total = FromLocal.Size();
	const FVector2f Direction = Total > KINDA_SMALL_NUMBER ? -FromLocal / Total : FVector2f(1.0f, 0.0f);
	for (float Distance = 0.0f; Distance < Total; Distance += 50.0f)
	{
		const FVector2f Point = FromLocal + Direction * Distance;
		if (Terrain.Inland(Point.X, Point.Y) >= 0.0f)
		{
			return Point;
		}
	}
	return FVector2f::ZeroVector;
}

void AYSIsland::TickConstantSpawn(float DeltaSeconds)
{
	const FYSIslandConstantSpawn& Rule = Data->ConstantSpawn;
	const bool bActive = Rule.bEnabled && !bFlowVictory
		&& (Phase == EYSIslandPhase::Day || Phase == EYSIslandPhase::Night || (Phase == EYSIslandPhase::Evacuation && Rule.bDuringEvacuation));
	if (!bActive)
	{
		ConstantSpawnTimer = 0.0f;
		return;
	}
	ConstantSpawnTimer += DeltaSeconds;
	if (ConstantSpawnTimer < Rule.Interval)
	{
		return;
	}
	ConstantSpawnTimer -= Rule.Interval;

	const TArray<TSubclassOf<AYSFoeCharacter>>& Pool = bOvertime && Rule.OvertimeFoes.Num() > 0 ? Rule.OvertimeFoes : Rule.Foes;
	if (Pool.Num() == 0)
	{
		return;
	}
	AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	const int32 Count = Rule.bScaleWithPlayers && GameMode ? GameMode->ScaleEnemyCount(Rule.Count) : Rule.Count;
	TArray<UClass*> Classes;
	for (int32 K = 0; K < Count; ++K)
	{
		if (UClass* Class = Pool[ServerRng.RandRange(0, Pool.Num() - 1)].Get())
		{
			Classes.Add(Class);
		}
	}
	const int32 Spawned = SpawnFarSide(Classes, Rule.Spacing);
	UE_LOG(LogTemp, Verbose, TEXT("[YSIsland] Spawn constant%s : %d / %d %s"), bOvertime ? TEXT(" (overtime)") : TEXT(""), Spawned, Classes.Num(), Classes.Num() > 0 ? *Classes[0]->GetName() : TEXT("-"));
}

int32 AYSIsland::SpawnFarSide(const TArray<UClass*>& Classes, float Spacing, int32 ForcedLevel, int32 LevelBonus)
{
	AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	TArray<FVector> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AYSPlayerCharacter* Character = It->Get() ? Cast<AYSPlayerCharacter>(It->Get()->GetPawn()) : nullptr;
		if (Character && Character->IsAlive())
		{
			Players.Add(Character->GetActorLocation());
		}
	}
	if (!GameMode || Classes.Num() == 0 || Players.Num() == 0)
	{
		return 0;
	}

	// le bord de l'île (tour sauf la plage, comme le circle spawn) dont le joueur le plus proche est le plus loin
	constexpr int32 Samples = 72;
	const float EndAngle = FMath::DegreesToRadians(Data->Generation.BeachAngleEnd);
	const float Start = PI + EndAngle;
	const float Arc = 2.0f * PI - 2.0f * EndAngle;
	float BestAngle = Start + Arc * 0.5f;
	double BestScore = -1.0;
	for (int32 Index = 0; Index < Samples; ++Index)
	{
		const float Angle = Start + Arc * (Index + 0.5f) / Samples;
		const float Coast = Terrain.CoastRadius(Angle);
		const FVector Point = LocalToWorld(FVector2f(Coast * FMath::Cos(Angle), Coast * FMath::Sin(Angle)), 0.0f);
		double Nearest = TNumericLimits<double>::Max();
		for (const FVector& Player : Players)
		{
			Nearest = FMath::Min(Nearest, FVector::DistSquared2D(Player, Point));
		}
		if (Nearest > BestScore)
		{
			BestScore = Nearest;
			BestAngle = Angle;
		}
	}

	// côte à côte le long du bord, tournés vers le centre
	const float Coast = FMath::Max(Terrain.CoastRadius(BestAngle), 100.0f);
	const FVector Facing = LocalToWorld(Plan.CenterSpot, Data->Generation.PlateauHeight);
	int32 Spawned = 0;
	for (int32 Index = 0; Index < Classes.Num(); ++Index)
	{
		const float Offset = (Index - (Classes.Num() - 1) * 0.5f) * Spacing / Coast;
		FVector Ground;
		if (FindCirclePoint(BestAngle + Offset, Ground))
		{
			Spawned += GameMode->SpawnFoeOnGround(Classes[Index], Ground, Facing, ForcedLevel, LevelBonus) ? 1 : 0;
		}
	}
	UE_LOG(LogTemp, Verbose, TEXT("[YSIsland] Côté le plus loin des joueurs : %.0f° (joueur le plus proche à %.0f m)"), FMath::RadiansToDegrees(BestAngle), FMath::Sqrt(BestScore) / 100.0);
	return Spawned;
}

bool AYSIsland::FindNorthSpot(FVector2f& OutSpot) const
{
	if (!Plan.bHasNorthSpot)
	{
		return false;
	}
	// le point du plan s'il est libre (un trio peut y avoir été posé), sinon le plus au nord des anneaux autour
	for (int32 Ring = 0; Ring <= 12; ++Ring)
	{
		const int32 Spots = Ring == 0 ? 1 : 8 * Ring;
		bool bFound = false;
		FVector2f Best = Plan.NorthSpot;
		for (int32 Index = 0; Index < Spots; ++Index)
		{
			const float Angle = 2.0f * PI * Index / Spots;
			const FVector2f Spot = Plan.NorthSpot + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * (Ring * 100.0f);
			if (Terrain.Inland(Spot.X, Spot.Y) >= Data->Generation.NorthSpotShoreInset && IsSpotFree(Spot, YSIslandActor::SpotMargin) && (!bFound || Spot.X > Best.X))
			{
				Best = Spot;
				bFound = true;
			}
		}
		if (bFound)
		{
			OutSpot = Best;
			return true;
		}
	}
	return false;
}

bool AYSIsland::GetBerthTransform(FTransform& OutTransform) const
{
	if (!Plan.bHasBerth)
	{
		return false;
	}
	const AYSOcean* Ocean = AYSOcean::Find(GetWorld());
	FVector Location = LocalToWorld(Plan.BerthLocation, 0.0f);
	Location.Z = Ocean ? Ocean->GetSeaLevel() : 0.0f;
	float Yaw = GetActorRotation().Yaw + 180.0f;
	if (Plan.bBerthAlongQuay)
	{
		// le long du quai (Y local) : des deux sens, celui qui pointe le plus vers l'ouest (gauche de l'écran, nord en haut)
		const float YawA = GetActorRotation().Yaw + 90.0f;
		const float YawB = GetActorRotation().Yaw - 90.0f;
		Yaw = FRotator(0.0f, YawA, 0.0f).Vector().Y < FRotator(0.0f, YawB, 0.0f).Vector().Y ? YawA : YawB;
	}
	OutTransform = FTransform(FRotator(0.0f, Yaw, 0.0f), Location);
	return true;
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
	if (PhaseEvents)
	{
		TickEventList(*PhaseEvents, EventsFired, PhaseElapsed);
	}
}

void AYSIsland::TickEventList(const TArray<FYSIslandSpawnEvent>& Events, TArray<int32>& Fired, float Elapsed)
{
	if (Fired.Num() != Events.Num())
	{
		Fired.SetNumZeroed(Events.Num());
	}
	const int32 Day = GetIslandDay();
	// Peaceful : seulement les vagues répétées hors écran (ex. 7 Skull toutes les 20 s, sans fin si Peaceful Endless Waves) et les élites / boss
	// au centre (la nuit finit à leur mort) ; ni vagues uniques, ni cercles
	const bool bPeaceful = IsPeacefulFlow();
	const AYSGameMode* EventGameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	const bool bEndless = bPeaceful && EventGameMode && EventGameMode->UsesPeacefulEndlessWaves();
	for (int32 Index = 0; Index < Events.Num(); ++Index)
	{
		const FYSIslandSpawnEvent& Event = Events[Index];
		// vague d'un seul jour (ou d'une seule nuit) de l'île ; élites « au centre » de la nuit remplacés par la séquence de nuits du flow
		if (!Event.IsForIslandDay(Day) || (bNightSetupOverride && Phase == EYSIslandPhase::Night && &Events == &Data->NightEvents && Event.Shape == EYSIslandSpawnShape::Center))
		{
			continue;
		}
		const bool bRepeatedWave = Event.Shape == EYSIslandSpawnShape::OffScreen && Event.RepeatInterval > 0.0f && Event.RepeatCount != 1;
		if (bPeaceful && !bRepeatedWave && Event.Shape != EYSIslandSpawnShape::Center)
		{
			continue;
		}
		const int32 RepeatCount = bEndless && bRepeatedWave ? 0 : Event.RepeatCount;
		// jusqu'à la fin de la phase sans intervalle : une seule fois (sinon boucle sans fin)
		if (RepeatCount == 0 && Event.RepeatInterval <= 0.0f && Fired[Index] > 0)
		{
			continue;
		}
		while ((RepeatCount == 0 || Fired[Index] < RepeatCount) && Elapsed >= Event.Time + Fired[Index] * Event.RepeatInterval)
		{
			FireEvent(Event, Fired[Index]);
			++Fired[Index];
			if (Event.RepeatInterval <= 0.0f)
			{
				Fired[Index] = FMath::Max(Fired[Index], Event.RepeatCount);
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
			// au hasard dans la liste (ex. élite Skull ou Gun) ; au centre (élites, boss) : tous différents tant que possible
			UClass* Class = Entry.PickClass(ServerRng, Event.Shape == EYSIslandSpawnShape::Center ? &Classes : nullptr);
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

	const int32 ForcedLevel = Event.bOvertimeLevel ? YSGameState->GetFoeDayNumber() + Event.LevelOffset + Event.LevelStepPerRepeat * Repeat : INDEX_NONE;
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
	TArray<UClass*> Failed;
	for (UClass* Class : Classes)
	{
		bool bPlaced = false;
		for (int32 Attempt = 0; Attempt < YSIslandActor::OffScreenAttempts && !bPlaced; ++Attempt)
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
				bPlaced = true;
			}
		}
		if (!bPlaced)
		{
			Failed.Add(Class);
		}
	}
	// joueurs dispersés (souvent à 3-4) : plus aucune place à OffScreenRadius de TOUS les joueurs -> au bord de l'île le plus loin
	// des joueurs plutôt que de perdre ces NME (ils étaient abandonnés : moins de NME à 4 joueurs qu'à 2-3)
	if (Failed.Num() > 0)
	{
		const int32 Fallback = SpawnFarSide(Failed, YSIslandActor::OffScreenFallbackSpacing, ForcedLevel, LevelBonus);
		Spawned += Fallback;
		UE_LOG(LogTemp, Log, TEXT("[YSIsland] Hors écran : %d NME sans place loin de tous les joueurs -> %d au bord le plus loin"), Failed.Num(), Fallback);
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
	if (!Data || Phase == EYSIslandPhase::Inactive || Phase == EYSIslandPhase::Sinking || Phase == EYSIslandPhase::Evacuation)
	{
		return;
	}
	// flow : nuit finie (élites morts) = « Quittez l'île ! », objectifs livrés ou non (demande de Paul : sans le CoreBoat à la fin, game over)
	if (UsesObjectiveFlow())
	{
		StartEvacuation();
		return;
	}
	if (Data->bInfiniteDays)
	{
		StartDayPhase();
	}
	else if (CountOpenMainObjectives() == 0)
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
	// l'objectif qui a posé cet objet d'abord, sinon le premier qui attend ce type d'objet
	auto Matches = [Object](const FYSIslandObjectiveState& State) { return !State.bDone && State.bSpawned && Object && State.DeliveryClass && Object->IsA(State.DeliveryClass); };
	int32 Found = Objectives.IndexOfByPredicate([&Matches, Object](const FYSIslandObjectiveState& State) { return Matches(State) && State.Item == Object; });
	if (Found == INDEX_NONE)
	{
		Found = Objectives.IndexOfByPredicate(Matches);
	}
	if (Objectives.IsValidIndex(Found))
	{
		FYSIslandObjectiveState& State = Objectives[Found];
		{
			State.bDone = true;
			UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : objectif %s accompli : %s (%d principal(aux) restant(s))"), *Data->GetName(), State.bMain ? TEXT("principal") : TEXT("secondaire"),
				*State.DisplayName.ToString(), CountOpenMainObjectives());
			RefreshOpenDeliveryClasses();
			// core du bateau ramené d'une île suivante : le GameBoat passe à l'étape suivante (île de départ : quand tous ses objectifs principaux sont livrés)
			AYSFlowMode* DeliveryFlowMode = GetWorld()->GetAuthGameMode<AYSFlowMode>();
			// (zones d'intérêt : il part dans le stock, l'étape se fait quand il y en a assez)
			if (DeliveryFlowMode && Object && Object->UpgradesGameBoat() && DeliveryFlowMode->GetStartIslandActor() != this && !DeliveryFlowMode->StocksCoreBoats())
			{
				DeliveryFlowMode->RequestGameBoatUpgrade(*FString::Printf(TEXT("%s livré (%s)"), *Object->GetName(), *Data->GetName()));
			}
			if (State.bMain && CountOpenMainObjectives() == 0)
			{
				bMainObjectivesDone = true;
				ForceNetUpdate();
				// le GameBoat passe à l'étape suivante et les îles suivantes apparaissent, même si la routine de coulaison
				// a déjà commencé (jour max atteint avant la livraison : avant, le bateau ne changeait plus)
				if (AYSFlowMode* FlowMode = GetWorld()->GetAuthGameMode<AYSFlowMode>())
				{
					FlowMode->OnIslandMainObjectivesDone(this);
				}
				// flow : « Survivez et défendez le navire », la phase ne change pas
				if (UsesObjectiveFlow())
				{
					// île de départ : le CoreBoat livré disparaît (à l'image suivante : on est dans l'appel de la zone) ; la zone de dépôt reste
					// jusqu'à la coulaison (demande de Paul du 2026-11-10 ; StartSinking)
					TWeakObjectPtr<AActor> Delivered = bStartIsland && StartBoat ? Object : nullptr;
					GetWorldTimerManager().SetTimerForNextTick([Delivered]()
					{
						if (Delivered.IsValid()) { Delivered->Destroy(); }
					});
					UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : « %s » (phase %d gardée)%s"), *Data->GetName(), *Data->DefendText.ToString(),
						static_cast<int32>(Phase), Delivered.IsValid() ? *FString::Printf(TEXT(", %s retiré"), *Object->GetName()) : TEXT(""));
					// îles de la partie 2 : l'objectif ramené (boussole, CoreBoat) finit le jour, la nuit tombe tout de suite (demande de Paul, 2026-11-14)
					if (Data->bEndDayOnObjectiveDelivered && !bStartIsland && Phase == EYSIslandPhase::Day)
					{
						UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s : objectif livré pendant le jour : la nuit tombe"), *Data->GetName());
						StartNightPhase();
					}
					UpdateFlowStatus();
					return;
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

AYSIsland* AYSIsland::FindExpecting(const UWorld* World, const AYSInteractiveObject* Object, const FVector& Location)
{
	// d'abord l'île d'où vient cet objet (serveur : objet posé par un de ses objectifs), sinon la plus proche qui attend ce type d'objet
	for (TActorIterator<AYSIsland> It(World); It && Object; ++It)
	{
		if (It->IsObjectExpected(Object) && It->Objectives.ContainsByPredicate([Object](const FYSIslandObjectiveState& State) { return !State.bDone && State.Item == Object; }))
		{
			return *It;
		}
	}
	// par type d'objet : seulement les objectifs dont l'objet à ramener n'est pas celui posé (coffre d'une carte au trésor) ; un objet posé par
	// un objectif (CoreBoat, boussole...) ne valide que le sien, pas celui d'une autre île
	const auto ExpectsByClass = [Object](const AYSIsland* Island)
	{
		return Island->Objectives.ContainsByPredicate([Object](const FYSIslandObjectiveState& State)
		{
			return !State.bDone && State.DeliveryClass && Object->IsA(State.DeliveryClass) && (State.Def.DeliveryClass || !State.bSpawned);
		});
	};
	AYSIsland* Nearest = nullptr;
	double NearestDistSq = TNumericLimits<double>::Max();
	for (TActorIterator<AYSIsland> It(World); It && Object; ++It)
	{
		const double DistSq = FVector::DistSquared2D(It->GetActorLocation(), Location);
		// (objectifs connus du serveur seulement : un client garde le test par type)
		if (DistSq < NearestDistSq && It->IsObjectExpected(Object) && (!World->GetAuthGameMode() || ExpectsByClass(*It)))
		{
			NearestDistSq = DistSq;
			Nearest = *It;
		}
	}
	return Nearest;
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
	UpdateFlowStatus();
}

void AYSIsland::DebugSinkNow()
{
	StartSinkRoutine(TEXT("debug"));
	if (!bSinkStarted)
	{
		StartSinking();
	}
}

void AYSIsland::ForceSinkNow(const TCHAR* Reason)
{
	if (!HasAuthority() || bSinkStarted)
	{
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSIsland] %s coule tout de suite (%s)"), *GetName(), Reason);
	bForceSunk = true;
	StartSinking();
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

UClass* FYSIslandFoeEntry::PickClass(FRandomStream& Rng, const TArray<UClass*>* Avoid) const
{
	if (RandomFoeClasses.Num() == 0)
	{
		return FoeClass.Get();
	}
	TArray<UClass*> Candidates;
	for (const TSubclassOf<AYSFoeCharacter>& Class : RandomFoeClasses)
	{
		if (Class && (!Avoid || !Avoid->Contains(Class.Get())))
		{
			Candidates.Add(Class.Get());
		}
	}
	if (Candidates.Num() == 0)
	{
		return RandomFoeClasses[Rng.RandRange(0, RandomFoeClasses.Num() - 1)].Get();
	}
	return Candidates[Rng.RandRange(0, Candidates.Num() - 1)];
}
