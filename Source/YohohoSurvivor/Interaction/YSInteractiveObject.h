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

	/** Distance devant le porteur où l'objet est reposé */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Carry", meta=(ClampMin="0", Units="Centimeters"))
	float DropDistance = 100.0f;

	/** Valeur de rachat par un Brooker, en doublons (0 = ne s'achète pas) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Brooker", meta=(ClampMin="0"))
	int32 DoubloonValue = 0;

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

	/** Pris par un joueur, ou reposé (nullptr) au sol à DropLocation (serveur, appelé par le personnage) */
	void SetCarrier(AYSPlayerCharacter* NewCarrier, const FVector& DropLocation = FVector::ZeroVector, float DropYaw = 0.0f);

	AYSPlayerCharacter* GetCarrier() const { return Carrier; }

	/** Invite « E » visible ou non (local) */
	void SetPromptVisible(bool bVisible);

	USceneComponent* GetHandPoint(bool bLeft) const { return bLeft ? LeftHandPoint.Get() : RightHandPoint.Get(); }

	float GetCarrierSpeedMultiplier() const { return CarrierSpeedMultiplier; }

	float GetFoeAttractionMultiplier() const { return FoeAttractionMultiplier; }

	float GetDropDistance() const { return DropDistance; }

	int32 GetDoubloonValue() const { return DoubloonValue; }

	bool ShowsIndicator() const { return bShowIndicator; }
	bool ShowsIndicatorOnScreen() const { return bIndicatorOnScreen; }
	FLinearColor GetIndicatorColor() const { return IndicatorColor; }
	UTexture2D* GetIndicatorIcon() const { return IndicatorIcon; }
	FVector GetIndicatorLocation() const { return GetActorLocation() + FVector(0.0f, 0.0f, IndicatorHeight); }

	FText GetDisplayName() const { return DisplayName; }

	/** Point au sol (WorldStatic) sous Location, ou Location s'il n'y en a pas */
	static FVector FindGround(const UWorld* World, const FVector& Location, const AActor* IgnoredActor);

	/** Fait apparaître un objet posé au sol sous Location (serveur) */
	static AYSInteractiveObject* SpawnOnGround(UWorld* World, TSubclassOf<AYSInteractiveObject> ObjectClass, const FVector& Location, float Yaw);

	/** Dernier sol sûr (hors de l'eau) où l'objet a été posé ou porté (serveur) : il y revient s'il est lâché dans la mer */
	void SetLastSafeLocation(const FVector& Location) { LastSafeLocation = Location; }
	const FVector& GetLastSafeLocation() const { return LastSafeLocation; }

	/** Point de pose valable : au sol, hors de l'eau, au-dessus de la deadzone */
	static bool IsSafeDropLocation(const UWorld* World, const FVector& Ground);

private:

	FVector LastSafeLocation = FVector::ZeroVector;

	/** Collisions et invite selon l'état porté / posé (toutes machines) */
	void ApplyCarryState();

	/** Échelle de l'objet à son apparition : gardée quand il est porté (un porteur plus grand, ex. Quartermaster, ne le fait pas grossir) */
	FVector OriginalScale = FVector::OneVector;
};
