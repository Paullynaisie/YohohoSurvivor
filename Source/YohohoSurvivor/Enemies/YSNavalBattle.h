#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "YSNavalBattle.generated.h"

class AYSFoeGalion;
class AYSFoeJollyShip;
class AYSPirateShip;

/** Une bataille navale (ligne de la Data Table DT_NavalBattles, réglable par Paul) */
USTRUCT(BlueprintType)
struct FYSNavalBattleRow : public FTableRowBase
{
	GENERATED_BODY()

	/** NME Galion (vide = Galion Class du GameMode) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Naval Battle")
	TSubclassOf<AYSFoeGalion> GalionClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Naval Battle", meta=(ClampMin="0"))
	int32 GalionCount = 1;

	/** NME Jolly Ship (vide = Jolly Ship Class du GameMode) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Naval Battle")
	TSubclassOf<AYSFoeJollyShip> JollyShipClass;

	/** Jolly Ships présents dès le début (en sommeil avec le Galion) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Naval Battle", meta=(ClampMin="0"))
	int32 InitialJollyShips = 3;

	/** Une fois la bataille activée : Reinforcement Jolly Ships toutes les Reinforcement Interval */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Naval Battle", meta=(ClampMin="0"))
	int32 ReinforcementJollyShips = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Naval Battle", meta=(ClampMin="1", Units="s"))
	float ReinforcementInterval = 30.0f;

	/** Plus de renforts quand tous les Galions de la bataille sont vaincus (coulés ou capitaine tué) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Naval Battle")
	bool bStopReinforcementsWhenGalionsDefeated = true;

	/** Le bateau des joueurs à moins de cette distance du centre : la bataille s'active (75 m) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Naval Battle", meta=(ClampMin="0", Units="Centimeters"))
	float ActivationDistance = 7500.0f;

	/** Les bateaux apparaissent à moins de cette distance du centre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Naval Battle", meta=(ClampMin="0", Units="Centimeters"))
	float SpawnRadius = 4000.0f;

	/** Galion : il fonce sur le bateau des joueurs ce délai après l'activation (0 = jamais) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Naval Battle", meta=(ClampMin="0", Units="s"))
	float GalionRamDelay = 60.0f;
};

/**
 *  Espace de bataille navale (répliqué pour la bulle du nid d'pie, logique serveur) : à son apparition, ses Galions et Jolly Ships apparaissent autour de son centre,
 *  immobiles (en sommeil). Le bateau des joueurs à moins d'Activation Distance : ils se réveillent et prennent les joueurs en chasse, et des
 *  renforts de Jolly Ships arrivent régulièrement. Créé par AYSFlowMode::SpawnNavalBattles entre deux îles.
 */
UCLASS()
class AYSNavalBattle : public AActor
{
	GENERATED_BODY()

public:

	AYSNavalBattle();

	/** Avant FinishSpawning */
	void InitBattle(const FYSNavalBattleRow& InSettings, FName InRowName);

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool IsActive() const { return bActive; }

	/** Tous ses Galions vaincus (coulés ou capitaine tué) : bataille gagnée (répliqué : bulle du nid d'pie) */
	bool IsWon() const { return bWon; }

	/** Debug : activée tout de suite */
	void Activate(const TCHAR* Reason);

	/** Objectifs de la bataille (serveur, après l'apparition) : posés sur le pont de son premier Galion, à côté du capitaine */
	TArray<class AYSInteractiveObject*> SpawnObjectiveItems(const TArray<TSubclassOf<class AYSInteractiveObject>>& Classes);

	const TArray<TObjectPtr<AYSFoeGalion>>& GetGalions() const { return Galions; }

private:

	UPROPERTY(Replicated)
	bool bWon = false;

	/** Serveur : met Won à jour */
	void UpdateWon();

	FYSNavalBattleRow Settings;
	FName RowName;
	bool bActive = false;
	bool bReinforcementsStopped = false;
	float ReinforcementTimer = 0.0f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AYSFoeGalion>> Galions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AYSFoeJollyShip>> JollyShips;

	/** Un bateau du camp NME sur l'eau près du centre ; bDormant = en sommeil */
	AYSPirateShip* SpawnShip(UClass* ShipClass, bool bDormant);

	/** Un bateau des joueurs (pas un bateau de NME) à moins d'Activation Distance du centre */
	bool IsPlayerShipNear() const;
};
