#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YSOcean.generated.h"

class UStaticMeshComponent;

/**
 *  Océan d'une map : surface bleue sans collision (on passe au travers et on tombe dans la KillZ),
 *  dont la hauteur donne le niveau de la mer. Le sol sous l'eau n'est pas jouable : pas de NME ni de carte au trésor
 *  (IsUnderwater), et les balles et zones au sol restent au-dessus de la surface (ClampAboveSea).
 *  Placé une fois par map (GameMap : Tools/EditorScripts/setup_island.py).
 */
UCLASS(meta=(PrioritizeCategories="Ocean"))
class AYSOcean : public AActor
{
	GENERATED_BODY()

private:

	/** Surface de l'eau (plane moteur mis à l'échelle de l'acteur), sans collision */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Surface;

protected:

	/** Un sol à moins de cette hauteur au-dessus de la mer compte comme dans l'eau (apparitions des NME, carte au trésor) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ocean", meta=(ClampMin="0", Units="Centimeters"))
	float ShoreMargin = 20.0f;

public:

	AYSOcean();

	/** Niveau de la mer = hauteur de l'acteur */
	float GetSeaLevel() const { return GetActorLocation().Z; }

	/** Océan de la map, ou nullptr */
	static const AYSOcean* Find(const UWorld* World);

	/** Vrai si ce point du sol est sous la mer (ou à moins de ShoreMargin au-dessus) ; faux sans océan */
	static bool IsUnderwater(const UWorld* World, const FVector& GroundPoint);

	/** Hauteur ramenée au niveau de la mer si elle est dessous (inchangée sans océan) */
	static float ClampAboveSea(const UWorld* World, float Z);
};
