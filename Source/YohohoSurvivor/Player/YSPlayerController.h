#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "YSPlayerController.generated.h"

class UYSAbilityIconWidget;

/**
 *  Contrôleur joueur : affiche un curseur de visée, oriente le pawn vers le point visé au sol
 *  et affiche l'icône de recharge de l'action de l'arme de la main gauche.
 *  La visée est calculée localement puis envoyée au serveur avec chaque mouvement
 *  (rotation de contrôle transmise par le CharacterMovementComponent).
 */
UCLASS()
class AYSPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Point visé au sol par la souris (valide uniquement sur le client local) */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Aim")
	FVector AimLocation = FVector::ZeroVector;

	/** Widget de l'icône de capacité (en bas au centre de l'écran) */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UYSAbilityIconWidget> AbilityIconClass;

	/** Distance entre l'icône et le bas de l'écran */
	UPROPERTY(EditAnywhere, Category="UI")
	float AbilityIconBottomMargin = 40.0f;

private:

	UPROPERTY(Transient)
	TObjectPtr<UYSAbilityIconWidget> AbilityIcon;

public:

	AYSPlayerController();

	virtual void BeginPlay() override;

	virtual void PlayerTick(float DeltaTime) override;

	FVector GetAimLocation() const { return AimLocation; }

	/**
	 *  Debug (console « ² ») : inflige Amount dégâts à tous les NME après Delay secondes.
	 *  bScreenshot = 1 prend une capture (avec UI) juste après.  Ex : YSDamageFoes 3 0
	 */
	UFUNCTION(Exec)
	void YSDamageFoes(float Amount = 3.0f, float Delay = 0.0f, bool bScreenshot = false);

protected:

	UFUNCTION(Server, Reliable)
	void ServerDamageFoes(float Amount);

	/** Applique les dégâts de debug (serveur) */
	void DamageAllFoes(float Amount);


	/** Projette la souris sur le plan horizontal du pawn et tourne la rotation de contrôle vers ce point */
	void UpdateAim();
};
