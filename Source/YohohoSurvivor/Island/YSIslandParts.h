#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Island/YSIslandGenerator.h"
#include "YSIslandParts.generated.h"

class AYSIsland;
class UInstancedStaticMeshComponent;

/**
 *  Une part des props d'une île, répliquée à part : une grande île (Port Town, 100 m) a trop de props pour une seule propriété répliquée.
 *  Le serveur pose ses props lui-même ; chaque client pose ceux de chaque part dès qu'il a construit le terrain de l'île.
 */
UCLASS(NotPlaceable)
class AYSIslandPropChunk : public AActor
{
	GENERATED_BODY()

public:

	AYSIslandPropChunk();

	/** Avant FinishSpawning (serveur) */
	void InitChunk(AYSIsland* InIsland, TArray<FYSIslandPropSpawn>&& InProps);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	/** Client : pose les props si l'île est construite (une seule fois) */
	void TrySpawnProps();

	AYSIsland* GetIsland() const { return Island; }

private:

	UPROPERTY(ReplicatedUsing=OnRep_Chunk)
	TObjectPtr<AYSIsland> Island;

	UPROPERTY(ReplicatedUsing=OnRep_Chunk)
	TArray<FYSIslandPropSpawn> Props;

	UFUNCTION()
	void OnRep_Chunk();

	bool bSpawned = false;
};

/**
 *  Pontons de Port Town (rampes, quais, ponton de la côte, accès depuis l'eau) : construits sur chaque machine à partir du plan, accrochés à l'île
 *  (ils coulent avec elle). Planches avec collision (WorldStatic, on marche dessus), poteaux décoratifs.
 */
UCLASS(NotPlaceable)
class AYSIslandPier : public AActor
{
	GENERATED_BODY()

public:

	AYSIslandPier();

	/** Segments en coordonnées de l'île (l'acteur est posé sur l'île, même transform) */
	void Build(const TArray<FYSIslandPierSegment>& Segments, const FYSIslandGenerationRules& Rules);

private:

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Planks;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Posts;
};
