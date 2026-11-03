#pragma once

#include "CoreMinimal.h"
#include "Interaction/YSInteractiveObject.h"
#include "NPC/YSBrookerNPC.h"
#include "NPC/YSMedicNPC.h"
#include "YSIslandNPC.generated.h"

class AYSIsland;

/** Brooker créé en jeu sur une île générée : répliqué (les Brookers placés dans une map ne le sont pas), accroché à l'île. Ex. le Charpentier ramené au bateau */
UCLASS()
class AYSIslandNPC : public AYSBrookerNPC
{
	GENERATED_BODY()

public:

	AYSIslandNPC();

	virtual void BeginPlay() override;
};

/** Medic créé en jeu sur une île générée (ex. Medic de Port Town ramené au bateau) : répliqué, accroché à l'île, ressuscite comme le Medic d'une map */
UCLASS()
class AYSIslandMedicNPC : public AYSMedicNPC
{
	GENERATED_BODY()

public:

	AYSIslandMedicNPC();

	virtual void BeginPlay() override;
};

/**
 *  Bateau de départ échoué sur la plage : point d'arrivée des joueurs. Sa zone valide la pose des objets attendus par les objectifs de l'île.
 *  Visuel (primitives) dans le Blueprint enfant.
 */
UCLASS()
class AYSStartBoat : public AYSIslandNPC
{
	GENERATED_BODY()

public:

	/** Avant FinishSpawning (serveur) */
	void InitBoat(AYSIsland* InIsland, float ZoneRadius);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	virtual bool AcceptsObject(const AYSInteractiveObject* Object) const override;

	/** Pas un Brooker pour les bulles sans objet porté */
	virtual bool IsBrooker() const override { return false; }

protected:

	virtual void ReceiveObject(AYSInteractiveObject* Object) override;

	virtual const TCHAR* GetLogTag() const override { return TEXT("[YSStartBoat]"); }

private:

	UPROPERTY(Replicated)
	TObjectPtr<AYSIsland> Island;

	UPROPERTY(ReplicatedUsing=OnRep_ZoneRadius)
	float ZoneRadius = 0.0f;

	UFUNCTION()
	void OnRep_ZoneRadius();
};

/**
 *  Un PNJ à trouver et à ramener (le Charpentier, le Medic de Port Town) : se porte comme le cadavre d'un joueur, se pose et se reprend comme un objet
 *  (réglages Interaction|Carry du Blueprint). Il ne prend rien tant qu'il n'est pas au bateau ; posé dans la zone du bateau, il y reste :
 *  Delivered NPC Class (Brooker, Medic : PNJ répliqué d'île) prend sa place.
 */
UCLASS()
class AYSCarpenter : public AYSInteractiveObject
{
	GENERATED_BODY()

public:

	/** PNJ qui le remplace au bateau (AYSIslandNPC, AYSIslandMedicNPC : répliqués) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Carpenter")
	TSubclassOf<AYSBrookerNPC> DeliveredNPCClass;

	virtual void OnDelivered() override;
};
