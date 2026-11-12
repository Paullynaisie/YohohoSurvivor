#pragma once

#include "CoreMinimal.h"
#include "Interaction/YSInteractiveObject.h"
#include "YSSuperTreasureMap.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;

/**
 *  Super carte au trésor (Blueprint BP_InteractiveObject_SuperTreasureMap, posée sur une zone d'intérêt) : tant qu'un joueur la tient,
 *  une ligne de pointillés dorés part de lui vers la zone du world boss (AYSDavyJonesArena), comme celle qui indique le Kraken.
 *  Seul le joueur qui la tient la voit ; lâchée, la ligne disparaît. Le combat contre Davy Jones commencé, toutes les super cartes disparaissent.
 */
UCLASS(meta=(PrioritizeCategories="Interaction SuperTreasureMap"))
class AYSSuperTreasureMap : public AYSInteractiveObject
{
	GENERATED_BODY()

public:

	AYSSuperTreasureMap();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/**
	 *  Toujours reprenable, même livrée au core du bateau (objectif secondaire validé une seule fois : une carte livrée ne valide plus
	 *  rien), tant qu'elle n'a pas disparu au début du combat contre Davy Jones (demande de Paul du 2026-11-12)
	 */
	virtual bool CanInteract(const AYSPlayerCharacter* Character) const override;

protected:

	/** Pointillés (locaux, coordonnées monde, seulement chez le joueur qui tient la carte) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UInstancedStaticMeshComponent> GuideDashes;

	/** Rafraîchissement de la ligne */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SuperTreasureMap|Dashes", meta=(ClampMin="0", Units="s"))
	float RefreshInterval = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SuperTreasureMap|Dashes", meta=(ClampMin="1", Units="Centimeters"))
	float DashSpacing = 210.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SuperTreasureMap|Dashes", meta=(ClampMin="1", Units="Centimeters"))
	float DashLength = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SuperTreasureMap|Dashes", meta=(ClampMin="1", Units="Centimeters"))
	float DashWidth = 42.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SuperTreasureMap|Dashes", meta=(ClampMin="0.1", Units="Centimeters"))
	float DashThickness = 6.0f;

	/** Hauteur des pointillés au-dessus de ce qu'il y a dessous (sol, pont, mer) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SuperTreasureMap|Dashes", meta=(Units="Centimeters"))
	float DashHeightOffset = 20.0f;

	/** Pas de pointillés à moins de cette distance du joueur (sous ses pieds) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SuperTreasureMap|Dashes", meta=(ClampMin="0", Units="Centimeters"))
	float DashStartOffset = 250.0f;

	/** Pointillés posés sur le sol / le pont jusqu'à cette distance du joueur ; au-delà, à la surface de la mer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SuperTreasureMap|Dashes", meta=(ClampMin="0", Units="Centimeters"))
	float GroundFollowRadius = 6000.0f;

	/** Nombre max de pointillés (au-delà, la ligne s'arrête) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SuperTreasureMap|Dashes", meta=(ClampMin="1"))
	int32 MaxDashes = 2000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SuperTreasureMap|Dashes")
	TObjectPtr<UStaticMesh> DashMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SuperTreasureMap|Dashes")
	TObjectPtr<UMaterialInterface> DashMaterial;

	/** Doré */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SuperTreasureMap|Dashes")
	FLinearColor DashColor = FLinearColor(1.0f, 0.75f, 0.1f);

	UPROPERTY(EditAnywhere, Category="SuperTreasureMap|Dashes")
	FName DashColorParameterName = TEXT("Color");

private:

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DashMaterialInstance;

	float RefreshTimer = 0.0f;
	bool bDashesVisible = false;

	void UpdateDashes();
};
