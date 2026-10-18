#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YSProp.generated.h"

/**
 *  Prop de décor : assemblage de primitives (formes de base + matériaux PH), comme les armes.
 *  Juste une racine (Root) : les formes s'ajoutent comme composants dans le Blueprint enfant.
 *  Statique et non répliqué (posé dans la map, identique sur toutes les machines), sans Tick.
 *  Classe de base du Blueprint /Game/Art/Props/BP_Prop, parent de tous les props.
 */
UCLASS(meta=(PrioritizeCategories="Prop"))
class AYSProp : public AActor
{
	GENERATED_BODY()

private:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Root;

public:

	AYSProp();
};
