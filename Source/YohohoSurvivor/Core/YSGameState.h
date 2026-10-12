#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "YSGameState.generated.h"

class AYSDamageNumber;

/**
 *  État de partie partagé. Sert de canal pour les événements visuels globaux
 *  (chiffres de dégâts) : il existe toujours, même quand la victime vient d'être détruite.
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

public:

	AYSGameState();

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
