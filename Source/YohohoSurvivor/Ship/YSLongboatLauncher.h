#pragma once

#include "CoreMinimal.h"
#include "Ship/YSShipStation.h"
#include "YSLongboatLauncher.generated.h"

class AYSLongboat;

/**
 *  Chalouperie (acteur enfant LongboatStation du bateau, devant la porte de la cabine) : E -> une chaloupe (Longboat Class)
 *  apparaît sur l'eau derrière le bateau (Spawn Distance Behind Helm derrière la barre) et le joueur en est le pilote (mêmes
 *  commandes que la barre ; il ne peut pas en descendre avec E). Une seule chaloupe à la fois : poste indisponible tant qu'elle
 *  existe. PV propres (à venir).
 */
UCLASS(meta=(PrioritizeCategories="Longboat Station Interaction"))
class AYSLongboatLauncher : public AYSShipStation
{
	GENERATED_BODY()

private:

	/** Chaloupe en mer (une seule à la fois), répliquée pour l'invite des clients */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Longboat", meta=(AllowPrivateAccess="true"))
	TObjectPtr<AYSLongboat> ActiveLongboat;

protected:

	/** Chaloupe créée (Blueprint BP_Longboat ; vide ou introuvable = classe C++ sans visuel) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Longboat")
	TSoftClassPtr<AYSLongboat> LongboatClass;

	/** Distance derrière la barre (vers la poupe) où la chaloupe apparaît, sur l'eau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Longboat", meta=(ClampMin="0", Units="Centimeters"))
	float SpawnDistanceBehindHelm = 500.0f;

	/** La chaloupe apparaît tournée vers l'arrière (elle s'éloigne du bateau en avançant) ; sinon dans le sens du bateau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Longboat")
	bool bSpawnFacingAway = true;

	virtual const TCHAR* GetLogTag() const override { return TEXT("[YSLongboat]"); }

public:

	AYSLongboatLauncher();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Bateau présent, pas de chaloupe en mer, joueur libre */
	virtual bool CanInteract(const AYSPlayerCharacter* Character) const override;

	/** Fait apparaître la chaloupe et y met le joueur à la barre (serveur) */
	virtual void Interact(AYSPlayerCharacter* Character) override;

	AYSLongboat* GetActiveLongboat() const { return ActiveLongboat; }
};
