#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "YSPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UBlendSpace;
class UInputAction;
class UInputMappingContext;
class UYSCoupDPetoirAbility;
struct FInputActionValue;

/**
 *  Personnage joueur top-down, déplacé au clavier (ZQSD sur AZERTY, WASD sur QWERTY).
 *  Il regarde toujours vers le curseur (rotation de contrôle fournie par AYSPlayerController)
 *  et strafe : l'animation dépend de la direction du mouvement par rapport au regard.
 *  Clic gauche : Coup d'Petoir (tir du pistolet tenu dans la main gauche).
 *  Déplacement et rotation sont répliqués par le CharacterMovementComponent (prédiction client incluse).
 *  Classe de base du Blueprint PlayerCharacter_mouss : les réglages se font dans le Blueprint.
 */
UCLASS(abstract)
class AYSPlayerCharacter : public ACharacter, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

private:

	/** Bras de caméra au-dessus du personnage (rotation absolue : ne tourne pas avec lui) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** Caméra top-down */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> TopDownCamera;

	/** Pistolet, attaché au socket PistolSocketName du mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> PistolMesh;

	/** Point de départ des projectiles (bout du pistolet) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> MuzzlePoint;

	/** Capacité de tir principale */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UYSCoupDPetoirAbility> CoupDPetoir;

	/** Action de déplacement (Axis2D), créée à l'exécution */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	/** Action de tir (clic gauche), créée à l'exécution */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FireAction;

	/** Contexte d'input clavier/souris par défaut, créé à l'exécution */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

protected:

	/** Vitesse de déplacement maximale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mouss|Movement", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float MoveSpeed = 600.0f;

	/**
	 *  BlendSpace 2D de strafe (X = direction -180..180 relative au regard, Y = vitesse).
	 *  S'il est renseigné, il remplace l'Anim Blueprint du mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mouss|Animation")
	TObjectPtr<UBlendSpace> StrafeBlendSpace;

	/** Lissage des paramètres envoyés au BlendSpace (0 = aucun lissage) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mouss|Animation", meta=(ClampMin="0"))
	float StrafeBlendSmoothing = 10.0f;

	/** Tags portés par le personnage (camp, etc.), lus par les projectiles */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mouss|Tags")
	FGameplayTagContainer GameplayTags;

private:

	/** Position actuelle (lissée) dans le BlendSpace : X = direction, Y = vitesse */
	FVector2D StrafeBlendPosition = FVector2D::ZeroVector;

public:

	/** Socket du squelette où est attaché le pistolet (main gauche) */
	static const FName PistolSocketName;

	AYSPlayerCharacter();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override { TagContainer.AppendTags(GameplayTags); }

	UCameraComponent* GetTopDownCamera() const { return TopDownCamera.Get(); }

	USpringArmComponent* GetCameraBoom() const { return CameraBoom.Get(); }

	/** Position de départ des projectiles */
	FVector GetMuzzleLocation() const;

protected:

	/** Gère l'input de déplacement */
	void Move(const FInputActionValue& Value);

	/** Clic gauche : déclenche le Coup d'Petoir vers le curseur */
	void Fire();

	/** Met à jour la position du BlendSpace de strafe (sur toutes les machines) */
	void UpdateStrafeAnimation(float DeltaSeconds);

private:

	/** Crée les actions et le contexte ZQSD/WASD + flèches + clic gauche */
	void CreateDefaultInput();
};
