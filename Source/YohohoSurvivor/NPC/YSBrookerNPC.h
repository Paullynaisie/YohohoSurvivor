#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YSBrookerNPC.generated.h"

class AYSInteractiveObject;
class UTexture2D;

/**
 *  « Brooker » : PNJ qui rachète les objets ayant une valeur en doublons (coffres). Un objet reposé à moins de
 *  AcceptRange de lui est pris (l'objet disparaît) et l'équipe gagne ses doublons (objectif commun, AYSGameState).
 *  Quand le joueur local porte un objet, une bulle indique le Brooker le plus proche (au-dessus de lui à l'écran,
 *  ou au bord de l'écran dans sa direction).
 *  Origine de l'acteur = ses pieds : il se pose au sol au démarrage. Visuel (primitives) dans le Blueprint enfant.
 *  Classe de base du Blueprint BP_NPC_Brooker.
 */
UCLASS(meta=(PrioritizeCategories="Brooker"))
class AYSBrookerNPC : public AActor
{
	GENERATED_BODY()

private:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Root;

protected:

	/** Nom affiché */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brooker")
	FText BrookerName;

	/** Distance (au sol) à laquelle un objet reposé est racheté */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brooker", meta=(ClampMin="0", Units="Centimeters"))
	float AcceptRange = 2500.0f;

	/** Bulle d'indication : couleur du rond et image (optionnelle) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brooker|Indicator")
	FLinearColor IndicatorColor = FLinearColor(1.0f, 0.75f, 0.1f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brooker|Indicator")
	TObjectPtr<UTexture2D> IndicatorIcon;

	/** Hauteur de la bulle au-dessus des pieds du Brooker quand il est à l'écran */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brooker|Indicator", meta=(Units="Centimeters"))
	float IndicatorHeight = 260.0f;

public:

	AYSBrookerNPC();

	virtual void BeginPlay() override;

	/** Rachète l'objet s'il a une valeur et qu'il est à portée (serveur) : doublons à l'équipe, objet détruit */
	bool TryBuy(AYSInteractiveObject* Object);

	/** Brooker le plus proche de Location (nullptr si aucun) */
	static AYSBrookerNPC* FindNearest(const UWorld* World, const FVector& Location);

	/** Fait racheter l'objet par le Brooker le plus proche s'il est à portée (serveur) */
	static bool TrySell(AYSInteractiveObject* Object);

	float GetAcceptRange() const { return AcceptRange; }

	FLinearColor GetIndicatorColor() const { return IndicatorColor; }

	UTexture2D* GetIndicatorIcon() const { return IndicatorIcon; }

	FVector GetIndicatorLocation() const { return GetActorLocation() + FVector(0.0f, 0.0f, IndicatorHeight); }
};
