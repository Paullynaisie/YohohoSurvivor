#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Ship/YSLongboat.h"
#include "YSGameBoat.generated.h"

class UCameraComponent;
class UChildActorComponent;
class AYSGameBoatSeat;

/**
 *  Le GameBoat, bateau des joueurs de toute la partie (FlowMod) : la liste de ses formes, de la chaloupe de départ au galion final.
 *  Chaque étape est un Blueprint de bateau ; passer à l'étape suivante (AYSFlowMode::UpgradeGameBoat) remplace le bateau par le suivant.
 */
UCLASS(BlueprintType)
class UYSGameBoatData : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Étapes dans l'ordre : la première doit être une chaloupe de départ (AYSGameBoatLongboat) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Game Boat")
	TArray<TSubclassOf<AYSPirateShip>> Stages;

	/** Départ de la chaloupe : distance au sud du bord de l'eau, en face de la plage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Game Boat|Start", meta=(ClampMin="0", Units="Centimeters"))
	float StartDistanceFromShore = 6000.0f;

	/** Nouvelle étape : tournée de cet angle par rapport à la chaloupe (90 = flanc gauche, celui de l'accès à bord, côté plage) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Game Boat|Upgrade", meta=(Units="Degrees"))
	float UpgradeYawOffset = 90.0f;

	/** Nouvelle étape : éloignée de la plage par pas de Water Search Step jusqu'à avoir assez de fond, au plus Water Search Distance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Game Boat|Upgrade", meta=(ClampMin="1", Units="Centimeters"))
	float WaterSearchDistance = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Game Boat|Upgrade", meta=(ClampMin="10", Units="Centimeters"))
	float WaterSearchStep = 100.0f;
};

/**
 *  Chaloupe de départ du GameBoat : attend sur l'eau pendant la sélection des personnages (4 places, un personnage local par joueur qui a choisi),
 *  part toute seule quand tout le monde est prêt (mêmes commandes qu'un pilote : Ship|Helm), débarque les joueurs en touchant le fond, puis reste là, inutilisable.
 *  Visuel dans le Blueprint /Game/Placeable/Ship/BP_GameBoat_Longboat (copie de BP_Longboat ; places et caméra = ses composants).
 */
UCLASS(meta=(PrioritizeCategories="Longboat Ship"))
class AYSGameBoatLongboat : public AYSLongboat
{
	GENERATED_BODY()

private:

	/** Places des joueurs (AYSGameBoatSeat) : à déplacer dans le Blueprint */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> Seat1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> Seat2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> Seat3;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> Seat4;

	/** Vue pendant la sélection des personnages */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> IntroCamera;

public:

	AYSGameBoatLongboat(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static constexpr int32 SeatCount = 4;

	/** Pieds d'un joueur à cette place, tourné vers son +X (toutes machines : le composant existe avant l'acteur enfant répliqué) */
	FTransform GetSeatFeetTransform(int32 Seat) const;

	AYSGameBoatSeat* GetSeat(int32 Seat) const;

	/** Tout le monde est prêt : en route (serveur) */
	void StartSailing();

	bool HasLanded() const { return bLanded; }

protected:

	/** Tout droit à fond pendant la traversée */
	virtual bool ComputeAutoPilot(float& OutThrottle, float& OutSteer) override;

	/** Touche le fond : joueurs débarqués, la chaloupe reste */
	virtual void OnHullBlocked() override;

	virtual void OnShipCollision(AYSPirateShip* Other) override {}

	virtual bool DisappearsWithoutPilot() const override { return false; }

private:

	UChildActorComponent* GetSeatComponent(int32 Seat) const;

	bool bSailing = false;

	UPROPERTY(Replicated)
	bool bLanded = false;
};

/**
 *  Place d'un joueur sur la chaloupe du GameBoat : armes visibles mais rangées (pas de capacité), le regard suit le curseur, on n'en descend pas avec E.
 *  Réglages (caméra pendant la traversée) : /Game/Placeable/Ship/Stations/BP_GameBoatSeat.
 */
UCLASS(meta=(PrioritizeCategories="Station Interaction"))
class AYSGameBoatSeat : public AYSShipStation
{
	GENERATED_BODY()

public:

	AYSGameBoatSeat();

	virtual bool CanInteract(const AYSPlayerCharacter* Character) const override { return false; }

	virtual bool KeepsWeaponsVisible() const override { return true; }

	virtual bool KeepsCameraNorthUp() const override { return true; }

protected:

	virtual const TCHAR* GetLogTag() const override { return TEXT("[YSGameBoat]"); }
};

/**
 *  Sloop : 2e étape du GameBoat, miniature du bateau pirate avec 2 canons à l avant (écartés, à 15°), core, barre, nid d'pie et chalouperie.
 *  Visuel dans le Blueprint /Game/Placeable/Ship/BP_GameBoat_Sloop (Tools/EditorScripts/setup_game_boat.py).
 */
UCLASS(meta=(PrioritizeCategories="Ship"))
class AYSSloop : public AYSPirateShip
{
	GENERATED_BODY()

public:

	AYSSloop(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/**
 *  Slooplus : 3e étape du GameBoat (entre le sloop et le galion), 1 canon à l'avant (centré), 1 à bâbord, 1 à tribord.
 *  Visuel dans le Blueprint /Game/Placeable/Ship/BP_GameBoat_Slooplus (Tools/EditorScripts/setup_game_boat_stages.py).
 */
UCLASS(meta=(PrioritizeCategories="Ship"))
class AYSSlooplus : public AYSPirateShip
{
	GENERATED_BODY()

public:

	AYSSlooplus(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/**
 *  Brigantin : 4e étape du GameBoat (avant le galion), 2 canons à l'avant à 45°, 1 à bâbord, 1 à tribord.
 *  Visuel dans le Blueprint /Game/Placeable/Ship/BP_GameBoat_Brigantin (Tools/EditorScripts/setup_game_boat_stages.py).
 */
UCLASS(meta=(PrioritizeCategories="Ship"))
class AYSBrigantin : public AYSPirateShip
{
	GENERATED_BODY()

public:

	AYSBrigantin(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
