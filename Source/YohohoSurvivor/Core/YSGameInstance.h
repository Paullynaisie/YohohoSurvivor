#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "YSGameInstance.generated.h"

class UNetDriver;

/**
 *  GameInstance du jeu : héberger / rejoindre une partie par IP (listen server), et mémoire des erreurs réseau.
 *  En cas de perte de connexion ou d'échec pour rejoindre, le moteur renvoie sur la GameDefaultMap (MainMenu) :
 *  le message est gardé ici pour que le menu l'affiche.
 *  Réglages dans DefaultGame.ini, section [/Script/YohohoSurvivor.YSGameInstance].
 */
UCLASS(Config=Game)
class UYSGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:

	/** Map chargée par l'hôte (en listen server) */
	UPROPERTY(EditDefaultsOnly, Config, Category="Network")
	TSoftObjectPtr<UWorld> HostMap;

	/** Map du menu principal (retour au menu) */
	UPROPERTY(EditDefaultsOnly, Config, Category="Network")
	TSoftObjectPtr<UWorld> MenuMap;

	virtual void Init() override;

	virtual void Shutdown() override;

	/** Charge HostMap en listen server : les autres joueurs peuvent rejoindre par l'IP de cette machine */
	void HostGame();

	/** Rejoint l'hôte à l'adresse donnée (« 192.168.1.20 » ou « 192.168.1.20:7777 »). Faux si l'adresse est vide. */
	bool JoinGame(const FString& Address);

	/** Quitte la partie en cours (hôte ou client) et revient au menu */
	void ReturnToMenu();

	/** Dernière adresse rejointe (mémorisée sur ce PC) */
	FString GetLastJoinAddress() const;

	/** Adresses IPv4 locales de ce PC, à donner aux autres joueurs */
	static TArray<FString> GetLocalIPv4Addresses();

	/** Message d'erreur réseau en attente (vidé à la lecture) */
	FText ConsumePendingError();

private:

	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	FText PendingError;

	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
};
