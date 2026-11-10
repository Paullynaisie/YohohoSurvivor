#pragma once

#include "CoreMinimal.h"
#include "Interaction/YSInteractiveObject.h"
#include "YSBoatCrew.generated.h"

class UChildActorComponent;
class UStaticMeshComponent;
class UYSFaceDecalComponent;
class AYSWeapon;
class AYSPirateShip;

/** Bonus passif d'un membre d'équipage, tant qu'il est à bord du bateau des joueurs */
UENUM(BlueprintType)
enum class EYSBoatCrewBonus : uint8
{
	None			UMETA(DisplayName="Aucun"),
	/** Charpentier : postes de combat, chalouperie et core réparés en continu */
	AutoRepair		UMETA(DisplayName="Réparation automatique"),
	/** Medic : une réanimation sans plume de phoenix, puis recharge */
	FreeRevive		UMETA(DisplayName="Réanimation gratuite"),
	/** Navigateur : vitesse du bateau */
	ShipSpeed		UMETA(DisplayName="Vitesse du bateau"),
	/** Cuisinier : soin des joueurs et minions sur le bateau */
	ShipHeal		UMETA(DisplayName="Soin à bord"),
	/** Flibustier : recharge des postes de canon */
	CannonCooldown	UMETA(DisplayName="Recharge des canons")
};

/**
 *  Membre d'équipage du bateau (Charpentier, Medic, Navigateur, Cuisinier, Flibustier) : à trouver dans une zone d'intérêt, se porte et se pose
 *  comme un objet (réglages Interaction|Carry du Blueprint : porté couché) ; posé dans une zone de dépôt (core du bateau des joueurs, zone de la
 *  plage) il REJOINT L'ÉQUIPAGE : il reste debout sur le pont (il voyage avec le bateau) et applique son bonus passif (catégorie Boat Crew).
 *  Visuel comme les joueurs : corps (cube), deux mains, decal de visage, Head Feature (chapeau de JohnnyPlaytest par défaut) et une arme
 *  purement visuelle (Weapon Class, non répliquée : créée sur chaque machine). Parent : BP_BoatCrew_Core.
 */
UCLASS(meta=(PrioritizeCategories="BoatCrew Interaction"))
class AYSBoatCrew : public AYSInteractiveObject
{
	GENERATED_BODY()

private:

	/** Centre du corps (comme le BodyPivot des joueurs : origine de la capsule, 96 au-dessus des pieds) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> BodyPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UYSFaceDecalComponent> FaceDecal;

	/** Coiffure (bord du chapeau de JohnnyPlaytest) ; ses sous-composants en font partie */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> HeadFeature;

	/** Haut du chapeau */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> HatCrown;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> LeftHand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> RightHand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> LeftHandMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> RightHandMesh;

	/** Arme tenue dans la main droite (Weapon Class) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UChildActorComponent> Weapon;

protected:

	/** Arme visuelle (Blueprint de /Game/Placeable/Weapons/BoatCrew, sans action, Replicates décoché) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BoatCrew")
	TSubclassOf<AYSWeapon> WeaponClass;

	/** Les mains prennent le matériau du corps */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BoatCrew")
	bool bHandsUseBodyMaterial = true;

	/** Bonus passif appliqué une fois qu'il a rejoint l'équipage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BoatCrew|Bonus")
	EYSBoatCrewBonus Bonus = EYSBoatCrewBonus::None;

	/** Charpentier : PV rendus par seconde à chaque poste de combat, à la chalouperie et au core du bateau des joueurs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BoatCrew|Bonus", meta=(ClampMin="0", EditCondition="Bonus == EYSBoatCrewBonus::AutoRepair", EditConditionHides))
	float RepairPerSecond = 10.0f;

	/** Medic : recharge après une réanimation gratuite (sans plume de phoenix) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BoatCrew|Bonus", meta=(ClampMin="0", Units="s", EditCondition="Bonus == EYSBoatCrewBonus::FreeRevive", EditConditionHides))
	float FreeReviveCooldown = 30.0f;

	/** Navigateur : vitesses max du bateau des joueurs + cette part (0.5 = +50 %) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BoatCrew|Bonus", meta=(ClampMin="0", EditCondition="Bonus == EYSBoatCrewBonus::ShipSpeed", EditConditionHides))
	float ShipSpeedBonus = 0.5f;

	/** Cuisinier : PV rendus par seconde aux joueurs et minions sur le bateau (en plus de leur regen) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BoatCrew|Bonus", meta=(ClampMin="0", EditCondition="Bonus == EYSBoatCrewBonus::ShipHeal", EditConditionHides))
	float HealPerSecond = 10.0f;

	/** Flibustier : recharge des postes de canon (canon, Gatling) du bateau des joueurs x ce multiplicateur (0.5 = 2x plus vite) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BoatCrew|Bonus", meta=(ClampMin="0.05", ClampMax="1", EditCondition="Bonus == EYSBoatCrewBonus::CannonCooldown", EditConditionHides))
	float CannonCooldownMultiplier = 0.5f;

	/** Medic : heure serveur où la réanimation gratuite est de nouveau prête */
	UPROPERTY(Replicated, Transient)
	double FreeReviveReadyServerTime = 0.0;

public:

	AYSBoatCrew();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void Tick(float DeltaSeconds) override;

	/** Posé dans une zone de dépôt (serveur) : il rejoint l'équipage, son bonus s'applique */
	virtual void OnDelivered() override;

	bool HasJoined() const { return IsDelivered(); }

	EYSBoatCrewBonus GetBonus() const { return Bonus; }

	/** Medic à bord avec sa réanimation gratuite prête */
	bool IsFreeReviveReady() const;

	/** Réanimation gratuite utilisée (serveur) : recharge */
	void ConsumeFreeRevive();

	/** Premier membre d'équipage qui a rejoint le bateau avec ce bonus (nullptr sinon) */
	static AYSBoatCrew* FindJoined(const UWorld* World, EYSBoatCrewBonus InBonus);

	/** Medic à bord avec sa réanimation gratuite prête (nullptr sinon) */
	static AYSBoatCrew* FindFreeRevive(const UWorld* World);

	/** Vitesses max de Ship x ce multiplicateur (Navigateur ; 1 si ce n'est pas le bateau des joueurs) */
	static float GetShipSpeedMultiplier(const AYSPirateShip* Ship);

	/** Recharge des postes de canon de Ship x ce multiplicateur (Flibustier ; 1 si ce n'est pas le bateau des joueurs) */
	static float GetCannonCooldownMultiplier(const AYSPirateShip* Ship);

private:

	/** Charpentier (serveur) */
	void TickAutoRepair(AYSPirateShip* Ship, float DeltaSeconds);

	/** Cuisinier (serveur) */
	void TickShipHeal(AYSPirateShip* Ship, float DeltaSeconds);

	/** Bonus actif : a rejoint l'équipage et le bateau des joueurs existe */
	AYSPirateShip* GetCrewShip() const;
};
