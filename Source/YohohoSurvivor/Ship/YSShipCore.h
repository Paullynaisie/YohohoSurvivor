#pragma once

#include "CoreMinimal.h"
#include "GameplayTagAssetInterface.h"
#include "NPC/YSMedicNPC.h"
#include "YSShipCore.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 *  Core d'un bateau pirate (acteur enfant CoreStation, au milieu du pont, local sur chaque machine) : définit les stats du bateau
 *  (PV, multiplicateurs de vitesse, d'accélération et de rotation, catégorie Core|Ship Stats) et porte une zone qui est à la fois
 *  Brooker (coffre reposé dedans = doublons) et Medic (cadavre reposé dedans = joueur ressuscité). Zone, bulle et résurrection :
 *  réglages Brooker / Medic hérités. Indisponible tant que le bateau est absent.
 */
UCLASS(meta=(PrioritizeCategories="Core Brooker Medic"))
class AYSShipCore : public AYSMedicNPC, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

private:

	/** Bateau des joueurs : hurtbox ciblée par les NME (haute : de la mer au-dessus du core), ses dégâts vont aux PV du bateau */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> CoreHurtbox;

	/** Visuel du core (primitives, à retoucher dans le Blueprint) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> CoreBase;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> CoreOrb;

protected:

	/** PV du bateau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Core|Ship Stats", meta=(ClampMin="0"))
	float ShipMaxHealth = 5000.0f;

	/** Vitesses max (avant et arrière) du bateau x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Core|Ship Stats", meta=(ClampMin="0"))
	float SpeedMultiplier = 1.0f;

	/** Accélérations (Z / S) du bateau x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Core|Ship Stats", meta=(ClampMin="0"))
	float AccelerationMultiplier = 1.0f;

	/** Vitesse de rotation max du bateau x ce multiplicateur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Core|Ship Stats", meta=(ClampMin="0"))
	float TurnRateMultiplier = 1.0f;

	virtual void ReceiveObject(AYSInteractiveObject* Object) override;

	virtual const TCHAR* GetLogTag() const override { return TEXT("[YSShipCore]"); }

public:

	AYSShipCore();

	virtual void BeginPlay() override;

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UBoxComponent* GetHurtbox() const { return CoreHurtbox; }

	/** Coffre (valeur en doublons) ou cadavre */
	virtual bool AcceptsObject(const AYSInteractiveObject* Object) const override;

	/** Objet attendu par un objectif d'île, et ce core est celui d'un bateau des joueurs */
	bool IsIslandObjective(const AYSInteractiveObject* Object) const;

	virtual bool IsAvailable() const override;

	virtual bool IsBrooker() const override { return true; }

	float GetShipMaxHealth() const { return ShipMaxHealth; }
	float GetSpeedMultiplier() const { return SpeedMultiplier; }
	float GetAccelerationMultiplier() const { return AccelerationMultiplier; }
	float GetTurnRateMultiplier() const { return TurnRateMultiplier; }
};
