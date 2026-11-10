#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YSInteractiveObject.generated.h"

class AYSPlayerCharacter;
class UUserWidget;
class UWidgetComponent;
class UTexture2D;

/**
 *  Objet avec lequel le joueur interagit (touche E) : par défaut, un objet qu'il peut porter (ex. le coffre).
 *  Le joueur devant l'objet voit l'invite « E » au-dessus (InteractPrompt, local) ; E le prend dans ses mains
 *  (armes désactivées et cachées, mains sur LeftHandPoint / RightHandPoint, plus lent, cible préférée des NME),
 *  E à nouveau le repose au sol devant lui. Reposé près d'un Brooker, il est vendu s'il a une valeur en doublons.
 *  Serveur autoritaire ; porteur répliqué (OnRep_Carrier), attachement répliqué par le moteur.
 *  Origine de l'acteur = bas de l'objet (posé au sol). Les formes (primitives) s'ajoutent dans le Blueprint enfant.
 *  Classe de base du Blueprint BP_InteractiveObject.
 */
UCLASS(meta=(PrioritizeCategories="Interaction"))
class AYSInteractiveObject : public AActor
{
	GENERATED_BODY()

private:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Root;

	/** Point de placement de la main gauche du porteur (position et rotation monde de la main) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> LeftHandPoint;

	/** Point de placement de la main droite du porteur */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> RightHandPoint;

	/** Invite « E » au-dessus de l'objet, affichée seulement pour le joueur local qui peut interagir (hauteur = position du composant) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UWidgetComponent> InteractPrompt;

protected:

	/** Nom affiché */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	FText DisplayName;

	/** Nom dans le texte d'objectif du flow (« Ramène {0} au bateau » : « la boussole », « le Medic »...) ; vide = Display Name */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	FText ObjectiveName;

	/** Widget de l'invite : Widget Blueprint enfant de YSInteractPromptWidget (réglages dans Class Defaults, catégorie Layout) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	TSubclassOf<UUserWidget> PromptWidgetClass;

	/** Le joueur peut prendre l'objet dans ses mains */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Carry")
	bool bCarriable = true;

	/** Position de l'objet porté (son origine = son bas), par rapport au corps du porteur (BodyPivot) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Carry")
	FVector CarryLocation = FVector(70.0f, 0.0f, -36.0f);

	/** Rotation de l'objet porté, par rapport au corps du porteur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Carry")
	FRotator CarryRotation = FRotator::ZeroRotator;

	/** Vitesse de déplacement du porteur x ce multiplicateur (0.7 = 30 % plus lent) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Carry", meta=(ClampMin="0.05", ClampMax="2"))
	float CarrierSpeedMultiplier = 0.7f;

	/**
	 *  Choix de cible des NME : le porteur est considéré comme ce nombre de fois plus proche
	 *  (2 : à 20 m avec l'objet, il compte comme à 10 m)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Carry", meta=(ClampMin="0.01"))
	float FoeAttractionMultiplier = 2.0f;

	/** Échelle de l'objet (appliquée à l'apparition sur chaque machine, en plus de celle du Blueprint ; gardée portée et posée) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Carry", meta=(ClampMin="0.1"))
	float ItemScale = 1.0f;

	virtual void PostInitializeComponents() override;

	/** Distance devant le porteur où l'objet est reposé */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Carry", meta=(ClampMin="0", Units="Centimeters"))
	float DropDistance = 100.0f;

	/** Lâché dans la mer, l'objet flotte à la surface (cadavre) ; sinon il revient au dernier sol sûr de son porteur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Carry")
	bool bFloatsOnWater = false;

	/**
	 *  Objet portable (objectif) : s'il se retrouve sous la surface de la mer (île ou bateau qui coule, lâché à l'eau), il remonte et flotte
	 *  à la surface (demande de Paul) ; il flotte aussi quand on le lâche dans la mer
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Carry", meta=(EditCondition="bCarriable"))
	bool bRisesToSurface = true;

	/** Valeur de rachat par un Brooker, en doublons (0 = ne s'achète pas) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Brooker", meta=(ClampMin="0"))
	int32 DoubloonValue = 0;

	/** Plumes de phoenix ajoutées au stock de l'équipe quand un Brooker le rachète (coffre de boss : 1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Brooker", meta=(ClampMin="0"))
	int32 PhoenixFeatherValue = 0;

	/** Objectif d'île qui fait passer le GameBoat à l'étape suivante quand il est livré (core du bateau ; sauf sur l'île de départ, où il faut tous ses objectifs principaux) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Brooker")
	bool bUpgradesGameBoat = false;

	/** Bulle d'indication (comme celle du Brooker) : au bord de l'écran quand l'objet posé est hors écran (coffres : pour ne pas les perdre) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Indicator")
	bool bShowIndicator = false;

	/** Bulle aussi affichée au-dessus de l'objet quand il est à l'écran */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Indicator", meta=(EditCondition="bShowIndicator"))
	bool bIndicatorOnScreen = false;

	/** Couleur du rond et image (optionnelle) de la bulle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Indicator", meta=(EditCondition="bShowIndicator"))
	FLinearColor IndicatorColor = FLinearColor(0.85f, 0.55f, 0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Indicator", meta=(EditCondition="bShowIndicator"))
	TObjectPtr<UTexture2D> IndicatorIcon;

	/** Hauteur visée au-dessus de l'origine (bas) de l'objet */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Indicator", meta=(Units="Centimeters", EditCondition="bShowIndicator"))
	float IndicatorHeight = 120.0f;

	/**
	 *  Bulle cachée au départ, révélée (pour tous) quand un joueur vivant passe à moins d Indicator Reveal Distance de l'objet ; de nouveau
	 *  cachée quand l'objet coule (sous la mer) ou quand tous les joueurs vivants sont à plus d Indicator Hide Distance (demande de Paul)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Indicator", meta=(EditCondition="bShowIndicator"))
	bool bRevealIndicatorNearby = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Indicator", meta=(ClampMin="0", Units="Centimeters", EditCondition="bShowIndicator && bRevealIndicatorNearby"))
	float IndicatorRevealDistance = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Indicator", meta=(ClampMin="0", Units="Centimeters", EditCondition="bShowIndicator && bRevealIndicatorNearby"))
	float IndicatorHideDistance = 10000.0f;

	/** Bulle révélée (serveur -> clients) */
	UPROPERTY(Replicated, Transient)
	bool bIndicatorRevealed = false;

	FTimerHandle IndicatorTimer;

	/** Serveur, régulièrement : révèle / cache la bulle selon la distance des joueurs et la mer */
	void UpdateIndicatorReveal();

	/** Joueur qui porte l'objet (nullptr = au sol) */
	UPROPERTY(ReplicatedUsing=OnRep_Carrier, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Interaction")
	TObjectPtr<AYSPlayerCharacter> Carrier;

	UFUNCTION()
	void OnRep_Carrier();

	/** Pris ou reposé (toutes machines, après la mise à jour des collisions) ; ex. cadavre : pose du corps */
	virtual void OnCarrierChanged() {}

public:

	AYSInteractiveObject();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	/** Le joueur peut interagir avec l'objet maintenant (par défaut : objet portable, pas déjà porté, mains libres) */
	virtual bool CanInteract(const AYSPlayerCharacter* Character) const;

	/** Interaction (serveur) : par défaut, le joueur prend l'objet dans ses mains */
	virtual void Interact(AYSPlayerCharacter* Character);

	/** Point de l'objet le plus proche de From pour la portée d'interaction (défaut : son origine ; accès du bateau : sa zone) */
	virtual FVector GetInteractLocation(const FVector& From) const { return GetActorLocation(); }

	/**
	 *  Point qui départage plusieurs objets à portée (le plus proche l'emporte) ; défaut = Get Interact Location ; accès du bateau : le flanc
	 *  du bateau (sa zone ne compte que pour la portée, sinon il passait toujours avant un coffre voisin)
	 */
	virtual FVector GetInteractPriorityLocation(const FVector& From) const { return GetInteractLocation(From); }

	/** Le joueur doit être tourné vers l'objet (cône Interaction Angle) pour interagir */
	virtual bool RequiresFacing() const { return true; }

	/** Pris par un joueur, ou reposé (nullptr) au sol à DropLocation (serveur, appelé par le personnage) */
	void SetCarrier(AYSPlayerCharacter* NewCarrier, const FVector& DropLocation = FVector::ZeroVector, float DropYaw = 0.0f);

	AYSPlayerCharacter* GetCarrier() const { return Carrier; }

	/** Invite « E » visible ou non (local) */
	void SetPromptVisible(bool bVisible);

	USceneComponent* GetHandPoint(bool bLeft) const { return bLeft ? LeftHandPoint.Get() : RightHandPoint.Get(); }

	float GetCarrierSpeedMultiplier() const { return CarrierSpeedMultiplier; }

	float GetFoeAttractionMultiplier() const { return FoeAttractionMultiplier; }

	float GetDropDistance() const { return DropDistance; }

	bool FloatsOnWater() const { return bFloatsOnWater || RisesToSurface(); }

	bool RisesToSurface() const { return bRisesToSurface && bCarriable; }

	int32 GetDoubloonValue() const { return DoubloonValue; }
	int32 GetPhoenixFeatherValue() const { return PhoenixFeatherValue; }
	bool UpgradesGameBoat() const { return bUpgradesGameBoat; }

	bool ShowsIndicator() const { return bShowIndicator && (!bRevealIndicatorNearby || bIndicatorRevealed); }
	bool ShowsIndicatorOnScreen() const { return bIndicatorOnScreen; }
	FLinearColor GetIndicatorColor() const { return IndicatorColor; }
	UTexture2D* GetIndicatorIcon() const { return IndicatorIcon; }
	FVector GetIndicatorLocation() const { return GetActorLocation() + FVector(0.0f, 0.0f, IndicatorHeight); }

	FText GetDisplayName() const { return DisplayName; }
	FText GetObjectiveName() const { return ObjectiveName.IsEmpty() ? DisplayName : ObjectiveName; }

	/** Point au sol (WorldStatic) sous Location, ou Location s'il n'y en a pas */
	static FVector FindGround(const UWorld* World, const FVector& Location, const AActor* IgnoredActor);

	/** Fait apparaître un objet posé au sol sous Location (serveur) */
	static AYSInteractiveObject* SpawnOnGround(UWorld* World, TSubclassOf<AYSInteractiveObject> ObjectClass, const FVector& Location, float Yaw);

	/** Posé dans la zone de dépôt qui l'attendait (serveur) : avec une valeur en doublons, il est vendu ; sinon il y reste et ne se porte plus */
	virtual void OnDelivered();

	bool IsDelivered() const { return bDelivered; }

	/** Dernier sol sûr (hors de l'eau) où l'objet a été posé ou porté (serveur) : il y revient s'il est lâché dans la mer */
	void SetLastSafeLocation(const FVector& Location) { LastSafeLocation = Location; }
	const FVector& GetLastSafeLocation() const { return LastSafeLocation; }

	/** Point de pose valable : au sol, hors de l'eau, au-dessus de la deadzone */
	static bool IsSafeDropLocation(const UWorld* World, const FVector& Ground);

	/** Posé sur le pont d'un bateau (serveur) : accroché à lui, il voyage avec lui */
	void AttachToShipBelow();

private:

	UPROPERTY(Replicated)
	bool bDelivered = false;

	FVector LastSafeLocation = FVector::ZeroVector;

	/** Collisions et invite selon l'état porté / posé (toutes machines) */
	void ApplyCarryState();

	/** Serveur, régulièrement : sous la surface de la mer et pas porté -> détaché, posé à la surface (Rises To Surface) */
	void CheckUnderwater();

	FTimerHandle UnderwaterTimer;

	/** Échelle de l'objet à son apparition : gardée quand il est porté (un porteur plus grand, ex. Quartermaster, ne le fait pas grossir) */
	FVector OriginalScale = FVector::OneVector;
};
