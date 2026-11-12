#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSMoveActionAbility.h"
#include "YSRunAbility.generated.h"

/**
 *  « Run » : MoveAction de Mouss. Coûte Stamina Cost (cran de la barre) au départ, puis Stamina Per Second tant que la course dure
 *  (pas de regen pendant la course) ; vitesse de déplacement x Run Speed Multiplier.
 *  Hold To Run : la course dure tant qu'Espace est maintenu (sinon : Espace la lance, un 2e appui l'arrête).
 *  S'arrête aussi à 0 de stamina, en portant un objet, dans l'eau, à un poste, à la mort.
 *  N'interrompt et ne bloque aucune action : on peut attaquer en courant (les ralentissements des attaques s'y multiplient).
 *  Réseau : le client qui contrôle lance / arrête sa course tout de suite (prédiction de la vitesse), le serveur fait foi.
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Run"))
class UYSRunAbility : public UYSMoveActionAbility
{
	GENERATED_BODY()

public:

	/** Stamina consommée par seconde de course, après le coût de départ */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Run", meta=(ClampMin="0"))
	float StaminaPerSecond = 5.0f;

	/** Vitesse de déplacement x ce multiplicateur pendant la course (1.3 = +30 %) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Run", meta=(ClampMin="0"))
	float RunSpeedMultiplier = 1.3f;

	/** Vrai : courir tant qu'Espace est maintenu ; faux : Espace lance la course, un 2e appui l'arrête */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Run")
	bool bHoldToRun = true;

	UYSRunAbility();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void OnInputPressed(const FVector& TargetLocation) override;

	virtual void OnInputReleased() override;

	/** Pendant la course : x Run Speed Multiplier */
	virtual float GetMoveSpeedMultiplier() const override;

	/** Pas de nouvelle course pendant la course */
	virtual bool CanActivateNow() const override;

	UFUNCTION(BlueprintPure, Category="Run")
	bool IsRunning() const { return bRunning; }

protected:

	virtual void OnActivationPredicted(const FVector& TargetLocation) override;

	virtual void ExecuteMoveAction(const FVector& TargetLocation) override;

	virtual void OnActivationRejected() override;

private:

	/** En train de courir (serveur ; client qui contrôle : sa prédiction ; autres machines : répliqué) */
	UPROPERTY(Replicated)
	bool bRunning = false;

	/** Machine qui contrôle : arrête la course (et le serveur) */
	void RequestStop();

	/** Serveur : fin de la course ; bNotifyOwner : le client qui contrôle l'arrête aussi (fin imposée par le serveur) */
	void StopRunning(bool bNotifyOwner, const TCHAR* Reason);

	UFUNCTION(Server, Reliable)
	void ServerStopRunning();

	UFUNCTION(Client, Reliable)
	void ClientStopRunning();
};
