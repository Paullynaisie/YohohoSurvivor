#pragma once

#include "CoreMinimal.h"
#include "YSMatchStats.generated.h"

class AActor;
class AController;
class AYSPlayerCharacter;
class AYSPlayerState;
class UTexture2D;

/** Statistiques d'un joueur sur la partie (serveur, répliquées par son PlayerState ; écran des scores) */
USTRUCT(BlueprintType)
struct FYSMatchStats
{
	GENERATED_BODY()

	/** Dégâts infligés aux NME et aux bateaux de NME (lui, ses minions, ses bombes, les postes qu'il utilise) */
	UPROPERTY(BlueprintReadOnly, Category="Stats")
	float DamageDealt = 0.0f;

	/** Plus gros coup infligé */
	UPROPERTY(BlueprintReadOnly, Category="Stats")
	float BiggestHit = 0.0f;

	/** NME tués (coup mortel) */
	UPROPERTY(BlueprintReadOnly, Category="Stats")
	int32 Kills = 0;

	/** Dégâts subis (après l'armure) */
	UPROPERTY(BlueprintReadOnly, Category="Stats")
	float DamageTaken = 0.0f;

	/** PV retirés des coups reçus par l'armure (%Armor compris) */
	UPROPERTY(BlueprintReadOnly, Category="Stats")
	float ArmorReduced = 0.0f;

	/** Réanimations : cadavres d'alliés déposés chez un Medic */
	UPROPERTY(BlueprintReadOnly, Category="Stats")
	int32 Revives = 0;

	/** Objets déposés (coffres, CoreBoats, boussoles, équipage...) */
	UPROPERTY(BlueprintReadOnly, Category="Stats")
	int32 ItemsDelivered = 0;

	/** Morts du joueur */
	UPROPERTY(BlueprintReadOnly, Category="Stats")
	int32 Deaths = 0;
};

/** Une ligne d'upgrade de l'écran des scores (icône + nom) */
USTRUCT(BlueprintType)
struct FYSScoreUpgrade
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Stats")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category="Stats")
	FText Name;
};

/** Une shard de l'écran des scores (« 12 / 30 ») */
USTRUCT(BlueprintType)
struct FYSScoreShard
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Stats")
	FText Name;

	UPROPERTY(BlueprintReadOnly, Category="Stats")
	int32 Level = 0;

	UPROPERTY(BlueprintReadOnly, Category="Stats")
	int32 MaxLevel = 30;
};

/** Upgrades d'un joueur à la fin de la partie (copiées du personnage par le serveur : il peut être mort) */
USTRUCT(BlueprintType)
struct FYSScoreUpgrades
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Stats")
	bool bCaptured = false;

	UPROPERTY(BlueprintReadOnly, Category="Stats")
	TArray<FYSScoreUpgrade> ClassUpgrades;

	/** Upgrades d'arme débloquées (palier Déblocage) */
	UPROPERTY(BlueprintReadOnly, Category="Stats")
	TArray<FYSScoreUpgrade> WeaponUpgrades;

	/** Golden des upgrades d'arme (palier Super) */
	UPROPERTY(BlueprintReadOnly, Category="Stats")
	TArray<FYSScoreUpgrade> GoldenUpgrades;

	/** Shards possédées, de la plus haute à la plus basse */
	UPROPERTY(BlueprintReadOnly, Category="Stats")
	TArray<FYSScoreShard> Shards;
};

/** Suivi des statistiques (serveur) */
namespace YSMatchStats
{
	/** Joueur à créditer : PlayerState du contrôleur, sinon le joueur dont vient Causer / le pion du contrôleur (minion -> son Captain) */
	AYSPlayerState* FindPlayerState(const AController* InstigatorController, const AActor* Causer);

	/** Dégâts infligés (et plus gros coup) ; rien si la victime est un personnage joueur / minion */
	void RecordDamageDealt(const AActor* Victim, float Amount, const AController* InstigatorController, const AActor* Causer = nullptr);

	void RecordKill(const AController* InstigatorController, const AActor* Causer);

	/** Coup reçu par un joueur : dégâts subis (après armure) et PV retirés par l'armure */
	void RecordDamageTaken(AYSPlayerCharacter* Victim, float DamageBeforeArmor, float DamageAfterArmor);

	/** Objet déposé par Carrier (cadavre chez un Medic = réanimation) */
	void RecordDelivery(AYSPlayerCharacter* Carrier, bool bCorpse);

	/** Mort d'un joueur (pas les minions ni le Hired crew : sans PlayerState) */
	void RecordDeath(AYSPlayerCharacter* Victim);

	/** Upgrades du personnage pour l'écran des scores */
	FYSScoreUpgrades CaptureUpgrades(const AYSPlayerCharacter* Character);
}
