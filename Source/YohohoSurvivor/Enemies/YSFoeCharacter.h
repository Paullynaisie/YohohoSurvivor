#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "YSFoeCharacter.generated.h"

/**
 *  Ennemi de base : attiré en ligne droite vers le joueur le plus proche, sans pathfinding,
 *  et toujours tourné vers lui.
 *  La logique tourne uniquement sur le serveur ; position et rotation sont répliquées aux clients.
 *  Classe de base du Blueprint FoeCharacter_Skull : les réglages se font dans le Blueprint.
 */
UCLASS(abstract)
class AYSFoeCharacter : public ACharacter
{
	GENERATED_BODY()

protected:

	/** Vitesse de déplacement maximale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Movement", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float MoveSpeed = 400.0f;

	/** Vitesse à laquelle l'ennemi se tourne vers sa cible (0 = instantané) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Movement", meta=(ClampMin="0"))
	float TurnSpeed = 10.0f;

	/** Couleur appliquée au mesh */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Appearance")
	FLinearColor BodyColor = FLinearColor::Red;

	/** Paramètre vectoriel des matériaux du mesh qui reçoit BodyColor */
	UPROPERTY(EditAnywhere, Category="Foe|Appearance")
	FName ColorParameterName = TEXT("Paint Tint");

	/** Joueur actuellement ciblé (renseigné sur le serveur uniquement) */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Foe")
	TObjectPtr<APawn> Target;

public:

	AYSFoeCharacter();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Retourne le pawn joueur le plus proche (sur le plan horizontal), ou nullptr */
	APawn* FindNearestPlayer() const;

	/** Applique BodyColor sur tous les matériaux du mesh */
	void ApplyBodyColor();
};
