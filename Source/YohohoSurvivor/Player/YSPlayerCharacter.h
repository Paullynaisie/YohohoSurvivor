#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "Kismet/KismetMathLibrary.h"
#include "YSPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UYSFaceDecalComponent;
class UBlendSpace;
class UInputAction;
class UInputMappingContext;
class UYSCoupDPetoirAbility;
struct FInputActionValue;

/**
 *  Personnage joueur top-down, déplacé au clavier (ZQSD sur AZERTY, WASD sur QWERTY).
 *  Il regarde toujours vers le curseur (rotation de contrôle fournie par AYSPlayerController)
 *  et strafe : l'animation dépend de la direction du mouvement par rapport au regard.
 *  DA « primitives » : le corps est un cylindre (BodyMesh) avec un visage en decal (FaceDecal),
 *  portés par BodyPivot qui s'incline dans la direction du mouvement (penché autour des pieds).
 *  Clic gauche : Coup d'Petoir (tir du pistolet fixé sur le flanc gauche).
 *  Déplacement et rotation sont répliqués par le CharacterMovementComponent (prédiction client incluse).
 *  Classe de base du Blueprint PlayerCharacter_mouss : les réglages se font dans le Blueprint.
 */
UCLASS(abstract, meta=(PrioritizeCategories="3C"))
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

	/** Pivot visuel du corps : s'incline pendant le mouvement (corps, visage et pistolet le suivent) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> BodyPivot;

	/** Corps du personnage (cylindre par défaut) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** Visage projeté sur l'avant du corps (texture réglable via Face Texture sur le composant) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UYSFaceDecalComponent> FaceDecal;

	/** Pistolet, fixé sur le flanc gauche du corps */
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Movement", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float MoveSpeed = 600.0f;

	/**
	 *  BlendSpace 2D de strafe (X = direction -180..180 relative au regard, Y = vitesse).
	 *  S'il est renseigné, il remplace l'Anim Blueprint du mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Animation")
	TObjectPtr<UBlendSpace> StrafeBlendSpace;

	/** Lissage des paramètres envoyés au BlendSpace (0 = aucun lissage) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Animation", meta=(ClampMin="0"))
	float StrafeBlendSmoothing = 10.0f;

	/** Inclinaison maximale du corps dans la direction du mouvement */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Lean", meta=(ClampMin="0", ClampMax="89", Units="Degrees"))
	float LeanMaxAngle = 30.0f;

	/** Durée de la transition vers une nouvelle inclinaison */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Lean", meta=(ClampMin="0", Units="Seconds"))
	float LeanDuration = 0.14f;

	/** Courbe de la transition */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Lean")
	TEnumAsByte<EEasingFunc::Type> LeanEasing = EEasingFunc::EaseIn;

	/** Exposant de la courbe (EaseIn / EaseOut / EaseInOut) : plus il est grand, plus le départ est lent */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Lean", meta=(ClampMin="1"))
	float LeanEaseExponent = 2.0f;

	/** Hauteur du sautillement du corps pendant le déplacement */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Hop", meta=(ClampMin="0", Units="Centimeters"))
	float HopHeight = 10.0f;

	/**
	 *  Durée d'un sautillement (montée + retour) quand le personnage avance à MoveSpeed.
	 *  Proportionnelle à la vitesse réelle : 2x plus lent à MoveSpeed / 2, 2x plus rapide à MoveSpeed x 2.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Hop", meta=(ClampMin="0.01", Units="Seconds"))
	float HopDuration = 0.2f;

	/** Courbe du sautillement */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Hop")
	TEnumAsByte<EEasingFunc::Type> HopEasing = EEasingFunc::EaseInOut;

	/** Exposant de la courbe (EaseIn / EaseOut / EaseInOut) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Hop", meta=(ClampMin="1"))
	float HopEaseExponent = 2.0f;

	/** Tags portés par le personnage (camp, etc.), lus par les projectiles */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3C|Tags")
	FGameplayTagContainer GameplayTags;

private:

	/** Position actuelle (lissée) dans le BlendSpace : X = direction, Y = vitesse */
	FVector2D StrafeBlendPosition = FVector2D::ZeroVector;

	/** Inclinaison en espace monde (X, Y) en degrés : direction = sens du penché, norme = angle */
	FVector2D CurrentLean = FVector2D::ZeroVector;

	/** Transition d'inclinaison en cours : départ, cible et progression (0..1) */
	FVector2D LeanStart = FVector2D::ZeroVector;
	FVector2D LeanTarget = FVector2D::ZeroVector;
	float LeanAlpha = 1.0f;

	/** Progression du sautillement en cours (0..1, 0 = au sol) */
	float HopPhase = 0.0f;

public:

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

	/** Incline BodyPivot vers la direction du mouvement et le fait sautiller (visuel, sur toutes les machines) */
	void UpdateBodyPivot(float DeltaSeconds);

	/** Avance l'inclinaison lissée et retourne la rotation monde correspondante */
	FQuat UpdateLean(float DeltaSeconds, const FVector& Velocity);

	/** Avance le sautillement et retourne la hauteur actuelle */
	float UpdateHop(float DeltaSeconds, float Speed);

private:

	/** Crée les actions et le contexte ZQSD/WASD + flèches + clic gauche */
	void CreateDefaultInput();
};
