#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "YSGameState.generated.h"

class AYSDamageNumber;
class AYSPlayerCharacter;
class AYSFoeCharacter;

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

public:

	AYSGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const TArray<TSubclassOf<AYSPlayerCharacter>>& GetSelectableCharacters() const { return SelectableCharacters; }

	bool IsMatchStarted() const { return bMatchStarted; }

	/** Serveur uniquement (AYSGameMode) */
	void SetSelectableCharacters(const TArray<TSubclassOf<AYSPlayerCharacter>>& Characters) { SelectableCharacters = Characters; }
	void SetMatchStarted();

	int32 GetTeamLevel() const { return TeamLevel; }
	float GetTeamExperience() const { return TeamExperience; }
	float GetExperienceToNextLevel() const { return ExperienceToNextLevel; }
	bool IsUpgradeSelectionActive() const { return bUpgradeSelectionActive; }

	/** Serveur uniquement (AYSGameMode) */
	void SetTeamProgress(int32 Level, float Experience, float ExperienceNeeded) { TeamLevel = Level; TeamExperience = Experience; ExperienceToNextLevel = ExperienceNeeded; }
	void SetUpgradeSelectionActive(bool bActive) { bUpgradeSelectionActive = bActive; }
	void SetUpgradeSelectionLevel(int32 Level) { UpgradeSelectionLevel = Level; }
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

	/** Secondes écoulées depuis le démarrage de la partie (0 avant ; valable sur serveur et clients) */
	UFUNCTION(BlueprintPure, Category="Match")
	float GetMatchElapsedTime() const;

	/**
	 *  À appeler sur le serveur quand des dégâts sont infligés : affiche le chiffre chez tous les joueurs.
	 *  InstigatorController / Victim servent à déterminer la couleur selon chaque point de vue.
	 */
	static void ReportDamage(const AActor* Victim, float Amount, const FVector& ImpactLocation, const AController* InstigatorController);

protected:

	/** Les joueurs sont identifiés par leur PlayerState (toujours valide, contrairement au pawn qui peut mourir) */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDamageNumber(float Amount, FVector_NetQuantize Location, APlayerState* InstigatorPlayer, APlayerState* VictimPlayer);
};
