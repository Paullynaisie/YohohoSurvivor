#pragma once

#include "CoreMinimal.h"
#include "Core/YSGameMode.h"
#include "YSFlowMode.generated.h"

class AYSIsland;
class UYSIslandData;
class UYSGameBoatData;
class AYSGameBoatLongboat;

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

	/** Le bateau des joueurs et ses étapes ; vide = pas de mise en scène de départ (bateau échoué de l'île, joueurs sur la plage) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flow")
	TObjectPtr<UYSGameBoatData> GameBoatData;

	virtual void InitGameState() override;

	virtual void BeginPlay() override;

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	virtual void RestartPlayer(AController* NewPlayer) override;

	/** L'île vient de se construire (serveur) : chaloupe posée sur l'eau en face de la plage ; faux = pas de GameBoat */
	bool SpawnGameBoat(AYSIsland* ForIsland);

	/** La chaloupe a débarqué les joueurs : elle devient le point de dépôt des objectifs de l'île */
	void OnGameBoatLanded(AYSGameBoatLongboat* Boat, const FVector& Landing);

	/** GameBoat : étape suivante, sur l'eau à côté de l'étape actuelle (serveur) */
	void UpgradeGameBoat();

	virtual void OnEliteKilled(AYSFoeCharacter* Elite) override;

	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

protected:

	virtual void OnDayStarted(int32 Day) override;

	virtual void OnAllPlayersReady() override;

private:

	UPROPERTY(Transient)
	TObjectPtr<AYSIsland> Island;

	/** Rang du prochain joueur qui apparaît */
	int32 NextPlayerSpot = 0;

	/** Étape actuelle du GameBoat */
	int32 GameBoatStage = 0;

	/** Chaloupe de départ encore en route, ou nullptr */
	AYSGameBoatLongboat* GetArrivingBoat() const;
};
