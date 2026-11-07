#pragma once

#include "CoreMinimal.h"
#include "Ship/YSShipStation.h"
#include "YSCrowsNest.generated.h"

class APlayerState;
class UBoxComponent;
class UTexture2D;

/** Ping du nid d'pie, répliqué */
USTRUCT()
struct FYSShipPing
{
	GENERATED_BODY()

	UPROPERTY()
	bool bActive = false;

	/** Point visé (monde) */
	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	/** Joueur qui a pingé (son icône s'affiche) */
	UPROPERTY()
	TObjectPtr<APlayerState> Pinger;

	/** Heure serveur du ping */
	UPROPERTY()
	double ServerTime = 0.0;
};

/**
 *  Nid d'pie (acteur enfant CrowsNestStation du bateau, au pied du grand mât) : E au pied du mât -> le joueur monte dans le nid
 *  (Occupant Point, sur le plancher NestFloor), ne bouge plus et n'attaque plus, caméra encore plus dézoomée ; E le redescend au
 *  pied du mât (Exit Point). Clic gauche = PING : l'icône du joueur (couleur + visage) s'affiche là où il a cliqué chez les autres
 *  joueurs, qui clignote ; hors écran, au bord de l'écran dans sa direction. Un nouveau ping remplace l'ancien ; clic droit le
 *  retire, quitter le nid aussi. Un seul joueur à la fois.
 */
UCLASS(meta=(PrioritizeCategories="Crow's Nest Station Interaction"))
class AYSCrowsNest : public AYSShipStation
{
	GENERATED_BODY()

private:

	/** Plancher du nid (bloquant, sol du joueur là-haut) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> NestFloor;

	UPROPERTY(ReplicatedUsing=OnRep_Ping)
	FYSShipPing Ping;

	UFUNCTION()
	void OnRep_Ping();

protected:

	/** Clignotements du ping par seconde */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crow's Nest|Ping", meta=(ClampMin="0"))
	float PingBlinkFrequency = 2.5f;

	/** Part de chaque clignotement où l'icône est visible */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crow's Nest|Ping", meta=(ClampMin="0.05", ClampMax="1"))
	float PingVisibleRatio = 0.6f;

	/** Durée du ping (0 = jusqu'au ping suivant ou au départ du joueur) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crow's Nest|Ping", meta=(ClampMin="0", Units="s"))
	float PingDuration = 0.0f;

	/** Le joueur au nid voit aussi son propre ping */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crow's Nest|Ping")
	bool bShowPingToPinger = true;

	/** Hauteur de l'icône au-dessus du point cliqué */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crow's Nest|Ping", meta=(Units="Centimeters"))
	float PingHeight = 50.0f;

	/** Le joueur au nid voit une icône par île dont les objectifs principaux ne sont pas finis (au bord de l'écran hors écran) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crow's Nest|Islands")
	bool bShowIslandIndicators = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crow's Nest|Islands")
	FLinearColor IslandIndicatorColor = FLinearColor(0.2f, 0.85f, 0.35f);

	/** Icône dans la bulle (vide = rond seul) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crow's Nest|Islands")
	TObjectPtr<UTexture2D> IslandIndicatorIcon;

	/** Hauteur de la bulle au-dessus du centre de l'île (niveau de la mer) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crow's Nest|Islands", meta=(Units="Centimeters"))
	float IslandIndicatorHeight = 500.0f;

	/** Quitter le nid arrête le ping */
	virtual void OnOccupantChanged(AYSPlayerCharacter* Previous) override;

	virtual const TCHAR* GetLogTag() const override { return TEXT("[YSCrowsNest]"); }

public:

	AYSCrowsNest();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool HandlesAction(EYSStationAction Action) const override { return true; }

	/** Clic gauche : ping au point visé ; clic droit : ping retiré */
	virtual void ServerHandleAction(AYSPlayerCharacter* Character, EYSStationAction Action, const FVector& AimLocation) override;

	/** Ping en cours (toutes machines) : point de l'icône et joueur qui l'a placé */
	bool GetActivePing(FVector& OutLocation, APlayerState*& OutPinger) const;

	/** Icône visible à cet instant (clignotement, temps local) */
	bool IsPingBlinkVisible() const;

	bool ShowsPingToPinger() const { return bShowPingToPinger; }

	bool ShowsIslandIndicators() const { return bShowIslandIndicators; }
	const FLinearColor& GetIslandIndicatorColor() const { return IslandIndicatorColor; }
	UTexture2D* GetIslandIndicatorIcon() const { return IslandIndicatorIcon; }
	float GetIslandIndicatorHeight() const { return IslandIndicatorHeight; }
};
