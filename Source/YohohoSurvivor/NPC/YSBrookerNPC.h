#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YSBrookerNPC.generated.h"

class AYSInteractiveObject;
class UTexture2D;
class UStaticMeshComponent;
class UMaterialInterface;

/**
 *  « Brooker » : PNJ qui rachète les objets ayant une valeur en doublons (coffres). Un objet reposé à moins de
 *  AcceptRange de lui est pris (l'objet disparaît) et l'équipe gagne ses doublons (objectif commun, AYSGameState).
 *  Quand le joueur local porte un objet, une bulle indique le Brooker le plus proche (au-dessus de lui à l'écran,
 *  ou au bord de l'écran dans sa direction).
 *  Zone de rachat visible au sol (AcceptZone : disque translucide de rayon AcceptRange, aussi dans l'éditeur).
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

	/** Zone de rachat au sol : disque (cylindre aplati) mis à l'échelle de AcceptRange dans OnConstruction */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> AcceptZone;

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

	/** Zone de rachat : matériau (M_BrookerZone : paramètres Color, Glow, FillOpacity, EdgeOpacity, Radius, EdgeWidth) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brooker|Zone")
	TObjectPtr<UMaterialInterface> ZoneMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brooker|Zone")
	bool bShowZone = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brooker|Zone")
	FLinearColor ZoneColor = FLinearColor(1.0f, 0.75f, 0.1f);

	/** Lueur propre de la zone (visible même à l'ombre ou la nuit) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brooker|Zone", meta=(ClampMin="0"))
	float ZoneGlow = 0.3f;

	/** Opacité de l'intérieur de la zone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brooker|Zone", meta=(ClampMin="0", ClampMax="1"))
	float ZoneFillOpacity = 0.12f;

	/** Opacité ajoutée sur le bord */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brooker|Zone", meta=(ClampMin="0", ClampMax="1"))
	float ZoneEdgeOpacity = 0.6f;

	/** Largeur du bord plus opaque */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brooker|Zone", meta=(ClampMin="1", Units="Centimeters"))
	float ZoneEdgeWidth = 60.0f;

	/** Hauteur du disque au-dessus du sol (évite qu'il clignote avec le sol) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brooker|Zone", meta=(Units="Centimeters"))
	float ZoneHeight = 2.0f;

public:

	AYSBrookerNPC();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;

	/** Taille et matériau de la zone de rachat (AcceptRange, réglages Brooker|Zone) */
	void ApplyZone();

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
