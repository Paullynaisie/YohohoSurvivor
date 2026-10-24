#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/YSCustomization.h"
#include "YSGameState.generated.h"

class AYSDamageNumber;
class AYSPlayerCharacter;
class AYSFoeCharacter;
class UTexture2D;
class UMaterialInterface;

/**
 *  État de partie partagé. Sert de canal pour les événements visuels globaux
 *  (chiffres de dégâts) : il existe toujours, même quand la victime vient d'être détruite.
 *  Porte aussi l'état de la sélection de personnage (liste proposée, partie démarrée).
 */
UCLASS()
class AYSGameState : public AGameStateBase
{
	GENERATED_BODY()

protected:

	/** Acteur utilisé pour afficher les chiffres de dégâts */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<AYSDamageNumber> DamageNumberClass;

	/** Décalage horizontal aléatoire max des chiffres, pour éviter qu'ils se superposent */
	UPROPERTY(EditAnywhere, Category="UI", meta=(ClampMin="0"))
	float DamageNumberJitter = 25.0f;

	/** Personnages proposés dans le menu de sélection (copiés depuis AYSGameMode) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Character Select")
	TArray<TSubclassOf<AYSPlayerCharacter>> SelectableCharacters;

	/** Couleurs et visages proposés à l'écran de sélection (copiés depuis AYSGameMode) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Customization")
	TArray<FYSCustomizationColor> CustomizationColors;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Customization")
	TArray<TObjectPtr<UTexture2D>> CustomizationFaces;

	/** Vrai une fois que tous les joueurs ont choisi leur personnage */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Character Select")
	bool bMatchStarted = false;

	/** Temps serveur (GetServerWorldTimeSeconds) du démarrage de la partie */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Match")
	double MatchStartServerTime = 0.0;

	/** Niveau de l'équipe (partagé par tous les joueurs) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Level")
	int32 TeamLevel = 0;

	/** XP accumulée dans le niveau en cours (partagée par tous les joueurs) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Level")
	float TeamExperience = 0.0f;

	/** XP nécessaire pour passer au niveau suivant (calculée par le GameMode) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Level")
	float ExperienceToNextLevel = 10.0f;

	/** Sélection d'upgrade en cours (partie en pause) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Level")
	bool bUpgradeSelectionActive = false;

	/** Niveau atteint par la sélection d'upgrade en cours (plusieurs niveaux gagnés d'un coup : sélections enchaînées) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Level")
	int32 UpgradeSelectionLevel = 0;

	/** Jour en cours (1 au départ) : sert aussi de niveau aux NME et de multiplicateur aux vagues */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="DayNight")
	int32 DayNumber = 1;

	/** Nuit en cours (dure tant que l'élite est en vie) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="DayNight")
	bool bNight = false;

	/** Temps serveur du début du jour en cours */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="DayNight")
	double DayStartServerTime = 0.0;

	/** Élite de la nuit en cours (nullptr hors nuit ou une fois mort) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="DayNight")
	TObjectPtr<AYSFoeCharacter> CurrentElite;

	/** Objectif commun : doublons gagnés par l'équipe et objectif à atteindre */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Objective")
	int32 Doubloons = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Objective")
	int32 DoubloonGoal = 100;

	/** Le bateau est là (GameMap : seulement une fois l'objectif de doublons atteint) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Ship")
	bool bShipPresent = true;

	/** Joueurs vivants à bord du bateau / joueurs vivants (« X/Y sur le bateau ») */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Ship")
	int32 ShipAboardPlayers = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Ship")
	int32 ShipTotalPlayers = 0;

	/** Fin de jeu temporaire : tous les joueurs sont à bord (« BRAVO » puis retour au menu après VictoryReturnDelay) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Ship")
	bool bVictory = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Ship")
	float VictoryReturnDelay = 5.0f;

	/** L'île est en overtime */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Overtime")
	bool bOvertime = false;

	/** L'île coule : temps serveur du début (0 = non), durée, profondeur, courbe ; chaque machine déplace localement l'île et ce qui y est accroché */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Overtime")
	double IslandSinkStartServerTime = 0.0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Overtime")
	float IslandSinkDuration = 30.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Overtime")
	float IslandSinkDepth = 1200.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Overtime")
	float IslandSinkEaseExponent = 1.5f;

	/** Acteurs qui coulent (Landscape et acteurs accrochés, récursivement) et leur position de départ (local) */
	TArray<TPair<TWeakObjectPtr<AActor>, FVector>> SinkingActors;
	bool bSinkingActorsGathered = false;

	/** Descente de l'île appliquée sur cette machine */
	void UpdateIslandSink();

public:

	AYSGameState();

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const TArray<TSubclassOf<AYSPlayerCharacter>>& GetSelectableCharacters() const { return SelectableCharacters; }

	/** Personnage unique (Unique Character) déjà choisi par un autre joueur qu'Except (serveur et clients : menu grisé) */
	bool IsCharacterTaken(int32 CharacterIndex, const APlayerState* Except) const;

	bool IsMatchStarted() const { return bMatchStarted; }

	/** Serveur uniquement (AYSGameMode) */
	void SetSelectableCharacters(const TArray<TSubclassOf<AYSPlayerCharacter>>& Characters) { SelectableCharacters = Characters; }
	void SetCustomizationOptions(const TArray<FYSCustomizationColor>& Colors, const TArray<TObjectPtr<UTexture2D>>& Faces) { CustomizationColors = Colors; CustomizationFaces = Faces; }

	const TArray<FYSCustomizationColor>& GetCustomizationColors() const { return CustomizationColors; }
	const TArray<TObjectPtr<UTexture2D>>& GetCustomizationFaces() const { return CustomizationFaces; }

	/** Matériau de couleur / texture de visage d'un index (nullptr si invalide) */
	UMaterialInterface* GetCustomizationMaterial(int32 ColorIndex) const { return CustomizationColors.IsValidIndex(ColorIndex) ? CustomizationColors[ColorIndex].Material.Get() : nullptr; }
	UTexture2D* GetCustomizationFace(int32 FaceIndex) const { return CustomizationFaces.IsValidIndex(FaceIndex) ? CustomizationFaces[FaceIndex].Get() : nullptr; }
	void SetMatchStarted();

	int32 GetTeamLevel() const { return TeamLevel; }
	float GetTeamExperience() const { return TeamExperience; }
	float GetExperienceToNextLevel() const { return ExperienceToNextLevel; }
	bool IsUpgradeSelectionActive() const { return bUpgradeSelectionActive; }

	/** Serveur uniquement (AYSGameMode) */
	void SetTeamProgress(int32 Level, float Experience, float ExperienceNeeded) { TeamLevel = Level; TeamExperience = Experience; ExperienceToNextLevel = ExperienceNeeded; ForceNetUpdate(); }
	void SetUpgradeSelectionActive(bool bActive) { bUpgradeSelectionActive = bActive; ForceNetUpdate(); }
	void SetUpgradeSelectionLevel(int32 Level) { UpgradeSelectionLevel = Level; ForceNetUpdate(); }
	int32 GetUpgradeSelectionLevel() const { return UpgradeSelectionLevel; }

	int32 GetDayNumber() const { return DayNumber; }
	bool IsNight() const { return bNight; }
	AYSFoeCharacter* GetCurrentElite() const { return CurrentElite; }

	/** Secondes écoulées depuis le début du jour en cours */
	float GetDayElapsedTime() const;

	/** Serveur uniquement (AYSGameMode / élite) */
	void StartDay(int32 Day);
	void StartNight() { bNight = true; }
	void SetCurrentElite(AYSFoeCharacter* Elite) { CurrentElite = Elite; }

	int32 GetDoubloons() const { return Doubloons; }
	int32 GetDoubloonGoal() const { return DoubloonGoal; }

	/** Serveur uniquement (AYSGameMode) */
	void SetDoubloons(int32 Amount, int32 Goal) { Doubloons = Amount; DoubloonGoal = Goal; ForceNetUpdate(); }

	bool IsShipPresent() const { return bShipPresent; }
	int32 GetShipAboardPlayers() const { return ShipAboardPlayers; }
	int32 GetShipTotalPlayers() const { return ShipTotalPlayers; }
	bool IsVictory() const { return bVictory; }
	float GetVictoryReturnDelay() const { return VictoryReturnDelay; }
	bool IsOvertime() const { return bOvertime; }
	bool IsIslandSinking() const { return IslandSinkStartServerTime > 0.0; }

	/** Serveur uniquement (AYSGameMode) */
	void SetShipPresent(bool bPresent) { bShipPresent = bPresent; ForceNetUpdate(); }
	void SetShipBoarding(int32 Aboard, int32 Total) { ShipAboardPlayers = Aboard; ShipTotalPlayers = Total; ForceNetUpdate(); }
	void SetVictory(float ReturnDelay) { bVictory = true; VictoryReturnDelay = ReturnDelay; ForceNetUpdate(); }
	void SetOvertime() { bOvertime = true; ForceNetUpdate(); }
	void StartIslandSink(float Duration, float Depth, float EaseExponent);

	/** Secondes écoulées depuis le démarrage de la partie (0 avant ; valable sur serveur et clients) */
	UFUNCTION(BlueprintPure, Category="Match")
	float GetMatchElapsedTime() const;

	/**
	 *  À appeler sur le serveur quand des dégâts sont infligés : affiche le chiffre chez tous les joueurs.
	 *  InstigatorController / Victim servent à déterminer la couleur selon chaque point de vue.
	 */
	static void ReportDamage(const AActor* Victim, float Amount, const FVector& ImpactLocation, const AController* InstigatorController, bool bCritical = false);

protected:

	/** Les joueurs sont identifiés par leur PlayerState (toujours valide, contrairement au pawn qui peut mourir) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDamageNumber(float Amount, FVector_NetQuantize Location, APlayerState* InstigatorPlayer, APlayerState* VictimPlayer, bool bCritical);
};
