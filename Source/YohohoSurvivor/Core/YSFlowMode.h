#pragma once

#include "CoreMinimal.h"
#include "Core/YSGameMode.h"
#include "YSFlowMode.generated.h"

class AYSIsland;
class UYSIslandData;

/**
 *  Gamemode final (BP_FlowMod) : le déroulement de la partie dépend des îles. L'île de départ est générée à chaque lancement ;
 *  chaque île a son cycle (jour, nuit, coulaison), le jour global passe au suivant dès qu'une nuit se termine quelque part.
 *  Graine imposée pour les tests : -YSIslandSeed=<n>
 */
UCLASS()
class AYSFlowMode : public AYSGameMode
{
	GENERATED_BODY()

public:

	/** Île générée au démarrage de la map */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow")
	TObjectPtr<UYSIslandData> StartIsland;

	virtual void BeginPlay() override;

	virtual void OnEliteKilled(AYSFoeCharacter* Elite) override;

	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

protected:

	virtual void OnDayStarted(int32 Day) override;

private:

	UPROPERTY(Transient)
	TObjectPtr<AYSIsland> Island;

	/** Rang du prochain joueur qui apparaît */
	int32 NextPlayerSpot = 0;
};
