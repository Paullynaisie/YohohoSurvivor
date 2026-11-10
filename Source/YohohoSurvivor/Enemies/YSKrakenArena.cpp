#include "Enemies/YSKrakenArena.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/YSFlowMode.h"
#include "Core/YSGameState.h"
#include "Enemies/YSKraken.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "Map/YSOcean.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "Ship/YSPirateShip.h"
#include "Ship/YSShipStation.h"
#include "UObject/ConstructorHelpers.h"

AYSKrakenArena::AYSKrakenArena()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PreviewMaterial(TEXT("/Game/Core/Ship/M_ShipPreview.M_ShipPreview"));
	if (PreviewMaterial.Succeeded())
	{
		WaterMaterial = PreviewMaterial.Object;
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		DashMesh = CubeMesh.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterial.Succeeded())
	{
		DashMaterial = BasicMaterial.Object;
	}
}

void AYSKrakenArena::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AYSKrakenArena, CurrentStage);
}

AYSKrakenArena* AYSKrakenArena::Find(const UWorld* World)
{
	for (TActorIterator<AYSKrakenArena> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AYSKrakenArena::BeginPlay()
{
	Super::BeginPlay();

	// eau sombre et pointillés : visuels locaux (toutes machines)
	WaterDisc = NewObject<UProceduralMeshComponent>(this, TEXT("WaterDisc"));
	WaterDisc->SetupAttachment(GetRootComponent());
	WaterDisc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaterDisc->SetCastShadow(false);
	// translucide trié en dernier recours par la distance du centre (disque de 200 m) : toujours dessous les signes au sol du combat
	WaterDisc->SetTranslucentSortPriority(YSKraken::WaterSortPriority);
	WaterDisc->RegisterComponent();
	BuildWaterDisc();

	GuideDashes = NewObject<UInstancedStaticMeshComponent>(this, TEXT("GuideDashes"));
	GuideDashes->SetupAttachment(GetRootComponent());
	GuideDashes->SetUsingAbsoluteLocation(true);
	GuideDashes->SetUsingAbsoluteRotation(true);
	GuideDashes->SetUsingAbsoluteScale(true);
	GuideDashes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GuideDashes->SetCastShadow(false);
	GuideDashes->SetStaticMesh(DashMesh);
	GuideDashes->RegisterComponent();
	if (DashMaterial)
	{
		DashMaterialInstance = UMaterialInstanceDynamic::Create(DashMaterial, this);
		DashMaterialInstance->SetVectorParameterValue(DashColorParameterName, DashColor);
		GuideDashes->SetMaterial(0, DashMaterialInstance);
	}
	GuideDashes->SetVisibility(false);

	UE_LOG(LogTemp, Log, TEXT("[YSKrakenArena] Boss fight final en %s (rayon %.0f m)"), *GetActorLocation().ToCompactString(), Radius / 100.0f);
}

void AYSKrakenArena::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && BoatGrabs > 0)
	{
		if (AYSPirateShip* Boat = GetGameBoat())
		{
			Boat->SetGrappled(false);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AYSKrakenArena::BuildWaterDisc()
{
	// disque opaque bleu foncé jusqu'à Inner Radius, puis anneaux de plus en plus transparents jusqu'au bord (eau de la mer)
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FLinearColor> Colors;
	const FVector Center(0.0f, 0.0f, WaterHeightAboveSea);
	const int32 Segments = FMath::Max(WaterSegments, 8);

	// centre plein
	const int32 CenterIndex = Vertices.Add(Center);
	Colors.Add(WaterColor);
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const float Angle = 2.0f * PI * Index / Segments;
		Vertices.Add(Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * InnerRadius);
		Colors.Add(WaterColor);
	}
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		Triangles.Append({ CenterIndex, CenterIndex + 2 + Index, CenterIndex + 1 + Index });
	}

	// dégradé
	const int32 Rings = FMath::Max(WaterFadeRings, 1);
	for (int32 Ring = 0; Ring < Rings; ++Ring)
	{
		const float T0 = static_cast<float>(Ring) / Rings;
		const float T1 = static_cast<float>(Ring + 1) / Rings;
		const float R0 = FMath::Lerp(InnerRadius, Radius, T0);
		const float R1 = FMath::Lerp(InnerRadius, Radius, T1);
		FLinearColor C0 = WaterColor;
		FLinearColor C1 = WaterColor;
		// estompé en douceur (courbe) jusqu'à 0 au bord
		C0.A = WaterColor.A * FMath::Square(1.0f - T0);
		C1.A = WaterColor.A * FMath::Square(1.0f - T1);
		const int32 Base = Vertices.Num();
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const float Angle = 2.0f * PI * Index / Segments;
			const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
			Vertices.Add(Center + Dir * R0);
			Colors.Add(C0);
			Vertices.Add(Center + Dir * R1);
			Colors.Add(C1);
		}
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const int32 A = Base + Index * 2;
			Triangles.Append({ A, A + 1, A + 2, A + 1, A + 3, A + 2 });
		}
	}
	WaterDisc->CreateMeshSection_LinearColor(0, Vertices, Triangles, TArray<FVector>(), TArray<FVector2D>(), Colors, TArray<FProcMeshTangent>(), false);
	WaterDisc->SetMaterial(0, WaterMaterial);
}

void AYSKrakenArena::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateGuideLine(DeltaSeconds);
	if (HasAuthority())
	{
		TickFight();
	}
}

AYSPirateShip* AYSKrakenArena::GetGameBoat() const
{
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	AYSPirateShip* Boat = YSGameState ? YSGameState->GetGameBoat() : nullptr;
	return IsValid(Boat) && !Boat->IsSinking() ? Boat : nullptr;
}

float AYSKrakenArena::GetSeaLevel() const
{
	const AYSOcean* Ocean = AYSOcean::Find(GetWorld());
	return Ocean ? Ocean->GetSeaLevel() : GetActorLocation().Z;
}

void AYSKrakenArena::UpdateGuideLine(float DeltaSeconds)
{
	// « Trouvez la créature » : toutes les boussoles ramenées, pas encore en partie 3 -> pointillés du bateau au centre de la zone
	GuideRefreshTimer -= DeltaSeconds;
	if (GuideRefreshTimer > 0.0f || !GuideDashes)
	{
		return;
	}
	GuideRefreshTimer = 0.2f;
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	const AYSPirateShip* Boat = GetGameBoat();
	const bool bShow = bShowGuideLine && YSGameState && Boat && YSGameState->AreAllCompassesFound() && YSGameState->GetFlowPart() < 3;
	if (!bShow)
	{
		if (bGuideVisible)
		{
			bGuideVisible = false;
			GuideDashes->ClearInstances();
			GuideDashes->SetVisibility(false);
			UE_LOG(LogTemp, Log, TEXT("[YSKrakenArena] Pointillés vers le Kraken retirés"));
		}
		return;
	}
	if (!bGuideVisible)
	{
		bGuideVisible = true;
		GuideDashes->SetVisibility(true);
		UE_LOG(LogTemp, Log, TEXT("[YSKrakenArena] Pointillés vers le Kraken affichés (toutes les boussoles trouvées)"));
	}

	const float Sea = GetSeaLevel();
	FVector Start = Boat->GetHullCenter();
	Start.Z = Sea + DashHeightAboveSea;
	FVector End = GetActorLocation();
	End.Z = Start.Z;
	const FVector Delta = End - Start;
	const float Length = Delta.Size2D();
	const FVector Direction = Delta.GetSafeNormal2D();
	const float Yaw = Direction.Rotation().Yaw;
	const FVector Scale(DashLength / 100.0f, DashWidth / 100.0f, DashThickness / 100.0f);
	TArray<FTransform> Transforms;
	// grille fixe le long de la ligne depuis le centre de la zone : les pointillés ne glissent pas quand le bateau avance
	for (float Distance = FMath::Fmod(Length - DashStartOffset, DashSpacing); Distance <= Length - DashStartOffset && Transforms.Num() < 2000; Distance += DashSpacing)
	{
		Transforms.Emplace(FRotator(0.0f, Yaw, 0.0f), End - Direction * Distance, Scale);
	}
	GuideDashes->ClearInstances();
	GuideDashes->AddInstances(Transforms, false, true);
}

void AYSKrakenArena::TickFight()
{
	if (CurrentStage >= 4)
	{
		return;
	}
	if (CurrentStage == 0)
	{
		// le bateau des joueurs entre dans la zone : partie 3 (si toutes les boussoles sont ramenées)
		const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
		const AYSPirateShip* Boat = GetGameBoat();
		if (Boat && YSGameState && FVector::Dist2D(Boat->GetHullCenter(), GetActorLocation()) <= Radius
			&& (!bRequireAllCompasses || YSGameState->AreAllCompassesFound()))
		{
			StartStage(1, TEXT("le bateau des joueurs entre dans la zone"));
		}
		return;
	}

	Parts.RemoveAll([](const TWeakObjectPtr<AYSKrakenPart>& Part)
	{
		return !Part.IsValid() || Part->GetHealth() <= 0.0f;
	});
	UpdateBossBar();

	// étape finie : tous ses morceaux principaux morts
	bool bStageDone = false;
	if (CurrentStage == 1)
	{
		bStageDone = !Parts.ContainsByPredicate([](const TWeakObjectPtr<AYSKrakenPart>& Part) { return Part->IsA<AYSKrakenHead>(); });
	}
	else
	{
		bStageDone = Parts.Num() == 0;
	}
	if (!bStageDone)
	{
		return;
	}
	if (CurrentStage < 3)
	{
		StartStage(CurrentStage + 1, CurrentStage == 1 ? TEXT("tête du Kraken vaincue") : TEXT("toutes les tentacules vaincues"));
		return;
	}

	// tête finale vaincue : BRAVO puis retour au menu
	CurrentStage = 4;
	ForceNetUpdate();
	if (AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>())
	{
		YSGameState->SetKrakenBossBar(FYSKrakenBossBar());
	}
	UE_LOG(LogTemp, Log, TEXT("[YSKrakenArena] Kraken vaincu : BRAVO, retour au menu dans %.0f s"), VictoryReturnDelay);
	if (AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>())
	{
		GameMode->TriggerVictory(TEXT("Kraken vaincu"), VictoryReturnDelay);
	}
}

void AYSKrakenArena::StartStage(int32 Stage, const TCHAR* Reason)
{
	const int32 Previous = CurrentStage;
	if (Previous == 0)
	{
		// partie 3 : toutes les îles coulent, « Affrontez le Kraken »
		if (AYSFlowMode* FlowMode = GetWorld()->GetAuthGameMode<AYSFlowMode>())
		{
			FlowMode->StartPart3(Reason);
		}
	}

	// morceaux restants retirés (tentacules de l'étape 1), bateau libéré
	for (const TWeakObjectPtr<AYSKrakenPart>& Part : Parts)
	{
		if (Part.IsValid())
		{
			Part->Vanish(TEXT("changement d'étape"));
		}
	}
	Parts.Reset();
	if (BoatGrabs > 0)
	{
		BoatGrabs = 0;
		if (AYSPirateShip* Boat = GetGameBoat())
		{
			Boat->SetGrappled(false);
		}
	}

	CurrentStage = Stage;
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[YSKrakenArena] Étape %d du combat contre le Kraken (%s)"), Stage, Reason);

	if (Previous > 0)
	{
		ForceLevelUpAndRepair();
	}
	SpawnStageParts(Stage);
	UpdateBossBar();
}

AYSKrakenPart* AYSKrakenArena::SpawnPart(UClass* Class, const FVector& Location, float EmergeStagger)
{
	if (!Class)
	{
		return nullptr;
	}
	const FTransform Transform(FRotator::ZeroRotator, Location);
	AYSKrakenPart* Part = GetWorld()->SpawnActorDeferred<AYSKrakenPart>(Class, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Part)
	{
		return nullptr;
	}
	Part->SetArena(this);
	if (AYSKrakenTentacle* Tentacle = Cast<AYSKrakenTentacle>(Part))
	{
		Tentacle->SetEmergeStagger(EmergeStagger);
	}
	Part->FinishSpawning(Transform);
	Parts.Add(Part);
	return Part;
}

void AYSKrakenArena::SpawnStageParts(int32 Stage)
{
	const float Sea = GetSeaLevel();
	const FVector Center(GetActorLocation().X, GetActorLocation().Y, Sea);
	// tentacules : sous l'eau autour du centre (elles sortent près du bateau après leur délai)
	// (première sortie décalée de Tentacle Stagger de l'une à l'autre : elles ne tirent pas toutes en même temps)
	int32 TentacleIndex = 0;
	const auto SpawnTentacles = [this, &Center, &TentacleIndex](UClass* Class, int32 Count, float AngleOffset)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Angle = FMath::DegreesToRadians(AngleOffset + 360.0f * Index / FMath::Max(Count, 1));
			SpawnPart(Class, Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * 4000.0f, TentacleStagger * TentacleIndex++);
		}
	};
	StageMaxHealth = 0.0f;
	switch (Stage)
	{
	case 1:
		SpawnPart(HeadClass, Center);
		SpawnTentacles(TentacleRangeClass, Stage1RangeTentacles, 0.0f);
		SpawnTentacles(TentacleMeleeClass, Stage1MeleeTentacles, 45.0f);
		break;
	case 2:
		SpawnTentacles(TentacleRangeClass, Stage2RangeTentacles, 0.0f);
		SpawnTentacles(TentacleMeleeClass, Stage2MeleeTentacles, 30.0f);
		for (const TWeakObjectPtr<AYSKrakenPart>& Part : Parts)
		{
			StageMaxHealth += Part.IsValid() ? Part->GetMaxHealth() : 0.0f;
		}
		break;
	case 3:
		SpawnPart(HeadFinalClass, Center);
		break;
	default:
		break;
	}
	UE_LOG(LogTemp, Log, TEXT("[YSKrakenArena] Étape %d : %d morceau(x) du Kraken"), Stage, Parts.Num());
}

void AYSKrakenArena::UpdateBossBar()
{
	AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	if (!YSGameState)
	{
		return;
	}
	FYSKrakenBossBar Bar;
	Bar.bActive = CurrentStage >= 1 && CurrentStage <= 3;
	Bar.Stage = CurrentStage;
	if (CurrentStage == 2)
	{
		// étape 2 : PV cumulés des tentacules
		Bar.Name = TentaclesBarName;
		Bar.MaxHealth = StageMaxHealth;
		for (const TWeakObjectPtr<AYSKrakenPart>& Part : Parts)
		{
			Bar.Health += Part.IsValid() ? FMath::Max(Part->GetHealth(), 0.0f) : 0.0f;
		}
	}
	else
	{
		Bar.Name = CurrentStage == 3 ? FinalHeadBarName : HeadBarName;
		const TWeakObjectPtr<AYSKrakenPart>* Head = Parts.FindByPredicate([](const TWeakObjectPtr<AYSKrakenPart>& Part) { return Part.IsValid() && Part->IsA<AYSKrakenHead>(); });
		if (Head)
		{
			Bar.Health = (*Head)->GetHealth();
			Bar.MaxHealth = (*Head)->GetMaxHealth();
		}
	}
	YSGameState->SetKrakenBossBar(Bar);
}

void AYSKrakenArena::ForceLevelUpAndRepair()
{
	// bateau régénéré : core et postes au max
	if (AYSPirateShip* Boat = GetGameBoat(); Boat && bRepairBoatOnStageChange)
	{
		Boat->RepairCoreBy(Boat->GetMaxHealth() * 2.0f + FMath::Abs(Boat->GetHealth()));
		for (AYSShipStation* Station : Boat->GetStations())
		{
			if (Station)
			{
				Station->RestoreFullHealth();
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[YSKrakenArena] %s régénéré (%.0f / %.0f)"), *Boat->GetName(), Boat->GetHealth(), Boat->GetMaxHealth());
	}
	// level up forcé : l'XP qui manque pour le niveau suivant
	AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	const AYSGameState* YSGameState = GetWorld()->GetGameState<AYSGameState>();
	if (bLevelUpOnStageChange && GameMode && YSGameState)
	{
		const float Missing = FMath::Max(YSGameState->GetExperienceToNextLevel() - YSGameState->GetTeamExperience(), 0.01f);
		UE_LOG(LogTemp, Log, TEXT("[YSKrakenArena] Level up forcé (+%.1f XP)"), Missing);
		GameMode->AddTeamExperience(Missing);
	}
}

void AYSKrakenArena::OnPartKilled(AYSKrakenPart* Part)
{
	UE_LOG(LogTemp, Log, TEXT("[YSKrakenArena] %s vaincu (étape %d)"), *GetNameSafe(Part), CurrentStage);
}

void AYSKrakenArena::AddBoatGrab()
{
	if (++BoatGrabs == 1)
	{
		if (AYSPirateShip* Boat = GetGameBoat())
		{
			Boat->SetGrappled(true);
		}
	}
}

void AYSKrakenArena::RemoveBoatGrab()
{
	if (BoatGrabs > 0 && --BoatGrabs == 0)
	{
		if (AYSPirateShip* Boat = GetGameBoat())
		{
			Boat->SetGrappled(false);
		}
	}
}

void AYSKrakenArena::DebugStartStage(int32 Stage)
{
	StartStage(FMath::Clamp(Stage, 1, 3), TEXT("debug"));
}

void AYSKrakenArena::DebugKillStage()
{
	for (const TWeakObjectPtr<AYSKrakenPart>& Part : Parts)
	{
		if (Part.IsValid() && (CurrentStage != 1 || Part->IsA<AYSKrakenHead>()))
		{
			Part->TakeDamage(Part->GetHealth() + 1.0f, FDamageEvent(), nullptr, this);
		}
	}
}
