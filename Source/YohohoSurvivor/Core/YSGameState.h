#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "YSGameState.generated.h"

class AYSDamageNumber;
class AYSPlayerCharacter;

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

public:

	AYSGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const TArray<TSubclassOf<AYSPlayerCharacter>>& GetSelectableCharacters() const { return SelectableCharacters; }

	bool IsMatchStarted() const { return bMatchStarted; }

	/** Serveur uniquement (AYSGameMode) */
	void SetSelectableCharacters(const TArray<TSubclassOf<AYSPlayerCharacter>>& Characters) { SelectableCharacters = Characters; }
	void SetMatchStarted();

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
