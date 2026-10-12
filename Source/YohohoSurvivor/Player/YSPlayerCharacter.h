#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "YSPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 *  Personnage joueur top-down, déplacé au clavier (ZQSD sur AZERTY, WASD sur QWERTY).
 *  Le déplacement est répliqué par le CharacterMovementComponent (prédiction client incluse),
 *  donc il fonctionne tel quel en multijoueur.
 *  Classe de base du Blueprint PlayerCharacter_mouss : les réglages se font dans le Blueprint.
 */
UCLASS(abstract)
class AYSPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

private:

	/** Bras de caméra au-dessus du personnage (rotation absolue : ne tourne pas avec lui) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** Caméra top-down */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> TopDownCamera;

	/** Action de déplacement (Axis2D), créée à l'exécution */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	/** Contexte d'input clavier par défaut, créé à l'exécution */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

protected:

	/** Vitesse de déplacement maximale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mouss|Movement", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float MoveSpeed = 600.0f;

public:

	AYSPlayerCharacter();

	virtual void BeginPlay() override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UCameraComponent* GetTopDownCamera() const { return TopDownCamera.Get(); }

	USpringArmComponent* GetCameraBoom() const { return CameraBoom.Get(); }

protected:

	/** Gère l'input de déplacement */
	void Move(const FInputActionValue& Value);

private:

	/** Crée l'action et le contexte ZQSD/WASD + flèches */
	void CreateDefaultInput();
};
