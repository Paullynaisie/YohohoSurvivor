#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YSDamageNumber.generated.h"

class UWidgetComponent;

/** Point de vue du joueur local sur un événement de dégâts (détermine la couleur) */
UENUM(BlueprintType)
enum class EYSDamageNumberKind : uint8
{
	DealtByMe,
	TakenByMe,
	DealtByOther,
	TakenByOther
};

/**
 *  Chiffre de dégâts flottant, purement local (non répliqué) : apparaît au point d'impact,
 *  monte légèrement et s'efface pendant Duration, puis se détruit.
 *  Créé sur chaque machine par AYSGameState::MulticastDamageNumber.
 */
UCLASS()
class AYSDamageNumber : public AActor
{
	GENERATED_BODY()

private:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UWidgetComponent> Widget;

protected:

	/** Durée d'affichage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage Number", meta=(ClampMin="0.1", Units="Seconds"))
	float Duration = 1.0f;

	/** Vitesse de montée du chiffre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage Number", meta=(Units="CentimetersPerSecond"))
	float RiseSpeed = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage Number")
	int32 FontSize = 24;

	/** Dégâts que j'inflige */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage Number|Colors")
	FLinearColor DealtByMeColor = FLinearColor::White;

	/** Dégâts que je subis */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage Number|Colors")
	FLinearColor TakenByMeColor = FLinearColor(1.0f, 0.1f, 0.1f);

	/** Dégâts infligés par un autre joueur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage Number|Colors")
	FLinearColor DealtByOtherColor = FLinearColor(0.55f, 0.55f, 0.55f);

	/** Dégâts subis par un autre joueur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage Number|Colors")
	FLinearColor TakenByOtherColor = FLinearColor(1.0f, 0.5f, 0.0f);

private:

	float Age = 0.0f;

public:

	AYSDamageNumber();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/** Renseigne le montant et la couleur (à appeler juste après le spawn) */
	void Init(float Amount, EYSDamageNumberKind Kind);
};
