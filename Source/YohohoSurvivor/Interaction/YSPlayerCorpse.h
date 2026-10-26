#pragma once

#include "CoreMinimal.h"
#include "Interaction/YSInteractiveObject.h"
#include "YSPlayerCorpse.generated.h"

class APlayerState;

/**
 *  Cadavre d'un joueur : objet portable (touche E, comme le coffre : armes rangées, plus lent, NME attirés), invendable.
 *  Créé par le serveur quand le ragdoll du joueur mort s'est posé (AYSPlayerCharacter::SpawnCorpse) ; le personnage mort reste
 *  dans le monde et son corps (tête, mains, armes, visage) suit cet objet sur chaque machine (AttachBodyTo), avec la pose du
 *  serveur. Une fois pris en main, le corps est couché en travers des bras (Carried Body Location / Rotation).
 *  Le joueur mort peut regarder son cadavre en spectateur (vue à View Offset, comme sa caméra au moment de sa mort).
 */
UCLASS()
class AYSPlayerCorpse : public AYSInteractiveObject
{
	GENERATED_BODY()

public:

	/** Pose du corps par rapport à l'objet (origine = au sol) dès qu'il a été porté une fois : couché en travers des bras */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Corpse")
	FVector CarriedBodyLocation = FVector(0.0f, 0.0f, 45.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Corpse")
	FRotator CarriedBodyRotation = FRotator(0.0f, 0.0f, 90.0f);

	AYSPlayerCorpse();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	/** Avant FinishSpawning (serveur) : personnage mort, pose de son corps, joueur, vue du spectateur */
	void InitCorpse(AYSPlayerCharacter* InBody, const FVector& InBodyLocation, const FRotator& InBodyRotation, APlayerState* InOwnerPlayerState, const FVector& InViewOffset);

	virtual void Interact(AYSPlayerCharacter* Character) override;

	/** Joueur dont c'est le cadavre */
	APlayerState* GetOwnerPlayerState() const { return OwnerPlayerState; }

	/** Point de vue du spectateur qui regarde ce cadavre */
	FVector GetViewLocation() const { return GetActorLocation() + ViewOffset; }

protected:

	virtual void OnCarrierChanged() override;

private:

	/** Personnage mort dont le corps suit le cadavre */
	UPROPERTY(ReplicatedUsing=OnRep_Body)
	TObjectPtr<AYSPlayerCharacter> Body;

	/** Pose du corps au sol à la création (ragdoll du serveur) */
	UPROPERTY(ReplicatedUsing=OnRep_Body)
	FVector BodyLocation = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing=OnRep_Body)
	FRotator BodyRotation = FRotator::ZeroRotator;

	/** Déjà porté une fois : pose Carried Body (sinon la pose du ragdoll) */
	UPROPERTY(ReplicatedUsing=OnRep_Body)
	bool bEverCarried = false;

	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> OwnerPlayerState;

	UPROPERTY(Replicated)
	FVector ViewOffset = FVector::ZeroVector;

	UFUNCTION()
	void OnRep_Body();

	/** Le corps suit l'objet avec la pose actuelle (toutes machines) */
	void ApplyBodyPose();
};
