#pragma once

#include "CoreMinimal.h"
#include "Interaction/YSInteractiveObject.h"
#include "YSCombatPost.generated.h"

class AYSPirateShip;
class AYSShipStation;
class UStaticMeshComponent;

/**
 *  « Poste de combat [item] » : objet ramassable (porté comme un coffre) qui personnalise le bateau. Posé sur un réceptacle du bateau (E à côté,
 *  l'objet dans les mains), il devient le poste actif Post Class (canon, Gatling...). Blueprints : /Game/Placeable/InteractiveObjects/CombatPosts.
 */
UCLASS(meta=(PrioritizeCategories="Combat Post Interaction"))
class AYSCombatPostItem : public AYSInteractiveObject
{
	GENERATED_BODY()

protected:

	/** Poste actif posé sur le réceptacle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat Post")
	TSubclassOf<AYSShipStation> PostClass;

public:

	AYSCombatPostItem();

	TSubclassOf<AYSShipStation> GetPostClass() const { return PostClass; }
};

/**
 *  Réceptacle à poste de combat (acteur enfant du bateau, à la place des canons fixes du GameBoat) : un joueur qui porte un poste de combat
 *  à côté appuie sur E pour le poser (le poste devient actif, accroché au réceptacle). À côté d'un poste actif libre et en état, E maintenu
 *  Dismount Hold Duration le démonte : le joueur l'obtient dans ses mains. Un simple appui sur E s'installe au poste comme avant.
 *  Serveur autoritaire ; poste posé répliqué.
 */
UCLASS(meta=(PrioritizeCategories="Combat Post Interaction"))
class AYSCombatPostReceptacle : public AYSInteractiveObject
{
	GENERATED_BODY()

private:

	/** Socle visible tant qu'aucun poste n'est posé */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Plate;

	UPROPERTY(ReplicatedUsing=OnRep_InstalledPost, Transient)
	TObjectPtr<AYSShipStation> InstalledPost;

	UFUNCTION()
	void OnRep_InstalledPost();

protected:

	/** Clic droit maintenu pendant cette durée au poste (en état) : démonté, le joueur repart avec l'objet */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat Post", meta=(ClampMin="0", Units="s"))
	float DismountHoldDuration = 3.0f;

	/** Poste posé au lancement de la partie (vide = réceptacle libre) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat Post")
	TSubclassOf<AYSShipStation> StartingPostClass;

public:

	AYSCombatPostReceptacle();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Disque au sol : pas besoin de le regarder */
	virtual bool RequiresFacing() const override { return false; }

	/** Un joueur qui porte un poste de combat, réceptacle libre, bateau utilisable */
	virtual bool CanInteract(const AYSPlayerCharacter* Character) const override;

	/** Pose le poste porté (serveur) */
	virtual void Interact(AYSPlayerCharacter* Character) override;

	/** Pose un poste de cette classe (serveur) ; faux si déjà occupé */
	bool InstallPost(TSubclassOf<AYSShipStation> PostClass, const TCHAR* Reason);

	/** Retire le poste posé (serveur) : la classe du poste, nullptr s'il n'y en a pas */
	TSubclassOf<AYSShipStation> RemovePost(const TCHAR* Reason);

	/** Le joueur peut démonter le poste posé : il y est installé, poste en état et pas en réparation, mains libres */
	bool CanDismount(const AYSPlayerCharacter* Character) const;

	/** Démonte le poste posé (serveur) : le joueur l'obtient dans ses mains */
	void Dismount(AYSPlayerCharacter* Character);

	AYSShipStation* GetInstalledPost() const { return InstalledPost; }

	float GetDismountHoldDuration() const { return DismountHoldDuration; }

	AYSPirateShip* GetShip() const;
};
