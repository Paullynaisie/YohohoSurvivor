#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YSBoardingPlank.generated.h"

class UMaterialInterface;
class UStaticMeshComponent;

/** Planche d'abordage : du dessus de Start (pont du Galion) au dessus de End (pont abordé), largeur Width (répliqué à l'apparition) */
USTRUCT()
struct FYSBoardingPlankSpan
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize10 Start = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantize10 End = FVector::ZeroVector;

	UPROPERTY()
	float Width = 260.0f;

	UPROPERTY()
	float Thickness = 20.0f;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> Material;
};

/**
 *  Planche d'abordage posée par un NME Galion qui a percuté le bateau des joueurs (AYSFoeGalion::StartBoarding) : un chemin de 3 joueurs de
 *  large entre les deux ponts, sol bloquant (WorldStatic) pour les joueurs et les NME. Retirée quand le capitaine meurt ou que le Galion coule.
 */
UCLASS()
class AYSBoardingPlank : public AActor
{
	GENERATED_BODY()

public:

	AYSBoardingPlank();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Avant FinishSpawning (serveur) */
	void InitPlank(const FYSBoardingPlankSpan& InSpan);

	virtual void BeginPlay() override;

protected:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Plank;

	UPROPERTY(ReplicatedUsing=OnRep_Span)
	FYSBoardingPlankSpan Span;

	UFUNCTION()
	void OnRep_Span();

private:

	/** Forme de la planche d'après Span (toutes machines) */
	void ApplySpan();
};
