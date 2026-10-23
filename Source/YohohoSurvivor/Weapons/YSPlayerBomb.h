#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "YSPlayerBomb.generated.h"

class AYSFoeCharacter;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;
class UYSGroundZoneComponent;

/** Lancer en cloche d'une bombe (BombThrower) : même trajectoire sur toutes les machines */
USTRUCT()
struct FYSPlayerBombFlight
{
	GENERATED_BODY()

	/** Bombe lancée (sinon posée sur place) */
	UPROPERTY()
	bool bThrown = false;

	/** Départ et point d'impact (au sol) */
	UPROPERTY()
	FVector Start = FVector::ZeroVector;

	UPROPERTY()
	FVector Target = FVector::ZeroVector;

	/** Durée du vol (la bombe explose en touchant le sol) */
	UPROPERTY()
	float Duration = 1.0f;

	/** Hauteur du sommet de la cloche au-dessus du plus haut des deux points */
	UPROPERTY()
	float ApexHeight = 300.0f;
};

/**
 *  Bombe posée au sol par un joueur (Roll and Bomb du Shotgun). Visuel = composants du Blueprint (bombe en primitives, comme
 *  celle des NME) ; le mesh principal (Mesh) passe du noir au rouge lumineux pendant la mèche (Fuse Duration), un disque posé
 *  sur le relief montre le rayon de l'explosion. Fin de la mèche (serveur) : Damage (fixé par l'action, avec ses upgrades)
 *  aux acteurs portant un des HitTags (NME) dont la hurtbox touche la sphère Explosion Radius ; jamais aux joueurs.
 *  Sphère d'explosion qui grossit puis s'efface (toutes les machines). Origine de l'acteur = le sol.
 *  Lancée (BombThrower, SetThrow) : vol en cloche calculé (pas de physique) vers un point du sol, explosion en touchant le sol (pas de mèche),
 *  le disque montre le point d'impact pendant le vol.
 *  Classe de base de /Game/Placeable/AMMO/BP_CannoneerBomb.
 */
UCLASS(meta=(PrioritizeCategories="Bomb"))
class AYSPlayerBomb : public AActor
{
	GENERATED_BODY()

private:

	/** Racine, posée au sol */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Root;

	/** Corps de la bombe : colorié par la mèche (Bomb Material) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Disque posé sur le relief, du rayon de l'explosion (coordonnées monde) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UYSGroundZoneComponent> ExplosionZone;

	/** Sphère de l'explosion */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> ExplosionSphere;

protected:

	/** Mèche : temps entre la pose et l'explosion */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb", meta=(ClampMin="0", Units="Seconds"))
	float FuseDuration = 1.0f;

	/** Rayon de l'explosion */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb", meta=(ClampMin="1", Units="Centimeters"))
	float ExplosionRadius = 300.0f;

	/** Cibles blessées par l'explosion : acteurs portant un de ces tags (NME) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb")
	FGameplayTagContainer HitTags;

	/** Matériau du corps (M_FoeBomb : paramètres Color et Emissive) ; vide = matériau du Mesh, sans effet de mèche */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Fuse")
	TObjectPtr<UMaterialInterface> BombMaterial;

	/** Couleur à la pose, couleur en fin de mèche et intensité de sa lueur (émissif) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Fuse")
	FLinearColor BombColor = FLinearColor(0.02f, 0.02f, 0.02f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Fuse")
	FLinearColor FuseColor = FLinearColor(1.0f, 0.05f, 0.02f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Fuse", meta=(ClampMin="0"))
	float FuseGlow = 20.0f;

	/** Disque du rayon de l'explosion au sol */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Zone")
	bool bShowZone = true;

	/** Matériau du disque (M_BrookerZone : Color, Glow, FillOpacity, EdgeOpacity, Radius, EdgeWidth) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Zone", meta=(EditCondition="bShowZone"))
	TObjectPtr<UMaterialInterface> ZoneMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Zone", meta=(EditCondition="bShowZone"))
	FLinearColor ZoneColor = FLinearColor(1.0f, 0.55f, 0.05f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Zone", meta=(ClampMin="0", ClampMax="1", EditCondition="bShowZone"))
	float ZoneFillOpacity = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Zone", meta=(ClampMin="0", ClampMax="1", EditCondition="bShowZone"))
	float ZoneEdgeOpacity = 0.7f;

	/** Matériau de la sphère d'explosion (M_FoeExplosion : paramètres Color et Opacity) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Explosion")
	TObjectPtr<UMaterialInterface> ExplosionMaterial;

	/** Couleur de la sphère (A = opacité de départ) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Explosion")
	FLinearColor ExplosionColor = FLinearColor(1.0f, 0.45f, 0.0f, 0.35f);

	/** La sphère grossit de 0 au rayon en ce temps, puis s'efface en Explosion Fade Duration */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Explosion", meta=(ClampMin="0", Units="Seconds"))
	float ExplosionGrowDuration = 0.14f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Explosion", meta=(ClampMin="0", Units="Seconds"))
	float ExplosionFadeDuration = 0.2f;

	/**
	 *  Mine (upgrade « Une Mine ! ») : au lieu d'exploser en fin de mèche (posée) ou en touchant le sol (lancée), elle s'ARME,
	 *  puis explose dès qu'un NME (HitTags) a sa hurtbox dans son rayon. Tous les autres réglages sont ceux de la bombe.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Mine")
	bool bMine = false;

	/** Rayon de la mine = Explosion Radius / ce diviseur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Mine", meta=(ClampMin="0.1", EditCondition="bMine"))
	float MineRadiusDivisor = 1.5f;

	/** Mine armée : intervalle de recherche des NME dans son rayon (serveur) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Mine", meta=(ClampMin="0", Units="Seconds", EditCondition="bMine"))
	float MineCheckInterval = 0.1f;

	/** Mine armée : la lueur de la mèche pulse à cette fréquence entre Armed Pulse Min et le maximum */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Mine", meta=(ClampMin="0", Units="Hertz", EditCondition="bMine"))
	float ArmedPulseFrequency = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Mine", meta=(ClampMin="0", ClampMax="1", EditCondition="bMine"))
	float ArmedPulseMin = 0.4f;

	/** Mine armée : délai entre la détection d'un NME dans son rayon et l'explosion (les NME dans le rayon À L'EXPLOSION sont touchés) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Mine", meta=(ClampMin="0", Units="Seconds", EditCondition="bMine"))
	float MineTriggerDelay = 1.0f;

	/** Pendant ce délai : clignotement rapide (fréquence) pour prévenir */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Mine", meta=(ClampMin="0", EditCondition="bMine"))
	float TriggeredPulseFrequency = 8.0f;

	/** Mine armée : disparaît sans exploser après ce temps (0 = jamais) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bomb|Mine", meta=(ClampMin="0", Units="Seconds", EditCondition="bMine"))
	float MineLifetime = 0.0f;

	/** Dégâts de l'explosion, fixés par l'action qui pose la bombe (serveur, avant FinishSpawning) */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Bomb")
	float Damage = 0.0f;

	/** Pushback ajouté au pushback naturel des NME touchés (Mine repulsante ; serveur) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Bomb")
	float ExtraPushback = 0.0f;

public:

	AYSPlayerBomb();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	void SetDamage(float NewDamage) { Damage = NewDamage; }

	void SetExtraPushback(float NewExtraPushback) { ExtraPushback = NewExtraPushback; }

	bool IsMine() const { return bMine; }

	/** Rayon effectif (mine : Explosion Radius / Mine Radius Divisor) */
	float GetExplosionRadius() const { return bMine ? ExplosionRadius / FMath::Max(MineRadiusDivisor, 0.1f) : ExplosionRadius; }

	/** Lancer en cloche de Start vers Target (sol) en Duration : la bombe explose en touchant le sol, sans mèche (serveur, avant FinishSpawning) */
	void SetThrow(const FVector& Start, const FVector& Target, float Duration, float ApexHeight);

	/** Position sur la trajectoire d'un lancer au temps Time */
	static FVector GetFlightLocation(const FYSPlayerBombFlight& Flight, float Time);

protected:

	/** Lancer (BombThrower) ; bThrown faux = bombe posée sur place (Roll and Bomb) */
	UPROPERTY(Replicated)
	FYSPlayerBombFlight Flight;

	/** Point d'explosion : la cible du lancer, sinon la position de la bombe */
	FVector GetExplosionCenter() const { return Flight.bThrown ? Flight.Target : GetActorLocation(); }

	/** Délai entre la pose (ou le lancer) et l'explosion */
	float GetTimeToExplode() const { return Flight.bThrown ? FMath::Max(Flight.Duration, 0.05f) : FuseDuration; }


	/** Explosion (serveur : dégâts) puis effet sur toutes les machines */
	void Explode();

	/** NME (HitTags) dont la hurtbox touche la sphère de l'explosion */
	TArray<AYSFoeCharacter*> FindFoesInRadius(const FVector& Center) const;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExploded();

	/** Mine déclenchée (NME détecté) : clignotement rapide sur toutes les machines jusqu'à l'explosion */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastMineTriggered();

	/** Mine déclenchée : explosion à ce moment (serveur, temps local) ; < 0 = pas déclenchée */
	double MineExplodeTime = -1.0;

	/** Mine déclenchée sur cette machine (clignotement rapide) */
	bool bMineTriggered = false;

private:

	/** Pose vue par cette machine (temps local) */
	double PlacedTime = 0.0;

	/** Début de l'explosion vue par cette machine (temps local ; < 0 = pas encore) */
	double ExplosionStartTime = -1.0;

	bool bExploded = false;

	/** Mine : prochaine recherche de NME (serveur, temps local) et armement déjà signalé dans le log */
	double NextMineCheckTime = 0.0;
	bool bArmedLogged = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BombMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ExplosionMaterialInstance;

	/** Couleur du corps : du noir au rouge lumineux pendant la mèche (Alpha 0 -> 1) */
	void SetFuseAlpha(float Alpha);

	void UpdateExplosion();
};
