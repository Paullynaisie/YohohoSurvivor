#pragma once

#include "CoreMinimal.h"
#include "Abilities/YSSummonCrewAbility.h"
#include "YSGoMinionsAbility.generated.h"

/**
 *  « Allez-y mes minions ! » : Ability du Cursed Hook (set Cursed Captain ; sous-classe de Summon the crew : même coup de crochet,
 *  mêmes minions, même maximum, mais elle n'invoque pas : les minions du Cursed Captain apparaissent là où meurt le NME marqué
 *  par le Cursed Captain Gun, via SpawnMinionAt).
 *  Au sommet du crochet (serveur), l'Ability de chaque minion du Captain est FORCÉE (Dashing Slash des épées, Rolling shot des
 *  guns : les minions ne les utilisent jamais d'eux-mêmes, sans recharge) : AYSPlayerCharacter::ForceAbility.
 *  Upgrade d'arme « You're all my minions » : l'Ability des joueurs à moins de All My Minions Range du Captain est forcée aussi
 *  (priorité sur tout, recharge ignorée et pas déclenchée) ; ++ : dégâts des actions forcées ; Golden : recharge des joueurs rendue.
 *  Sans personne à forcer, le clic ne fait rien (pas de recharge).
 */
UCLASS(ClassGroup=(YS), meta=(BlueprintSpawnableComponent, DisplayName="Allez-y mes minions"))
class UYSGoMinionsAbility : public UYSSummonCrewAbility
{
	GENERATED_BODY()

public:

	/** You're all my minions : les joueurs à moins de cette distance du Captain voient aussi leur Ability forcée */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Allez-y mes minions", meta=(ClampMin="0", Units="Centimeters"))
	float AllMyMinionsRange = 2500.0f;

	UYSGoMinionsAbility();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Au moins un minion (ou, avec You're all my minions, un joueur à portée) dont l'Ability sera forcée */
	virtual bool CanActivateNow() const override;

	/** Personnages dont l'Ability est forcée : minions du Captain, + joueurs vivants à portée avec You're all my minions */
	void GetForcedCharacters(TArray<AYSPlayerCharacter*>& OutCharacters) const;

#if WITH_EDITOR
	virtual void GetUnusedProperties(TArray<FName>& OutNames) const override;
#endif

protected:

	/** Sommet du crochet (serveur) : Abilities forcées */
	virtual void OnHookStrike() override;

	/** You're all my minions débloqué (copie répliquée de la propriété d'arme : le client vérifie s'il peut activer) */
	UPROPERTY(Replicated)
	bool bForcesPlayers = false;
};
