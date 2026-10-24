#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "YSGameInstance.generated.h"

class FOnlineSessionSearch;
class UNetDriver;

/** Partie Steam trouvée par FindOnlineGames (affichée par le menu principal) */
struct FYSOnlineGame
{
	FString HostName;
	int32 Players = 0;
	int32 MaxPlayers = 0;
	int32 PingMs = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FYSOnOnlineGamesFound, bool /*bSuccess*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FYSOnOnlineStatus, const FText& /*Status*/, bool /*bError*/);

/**
 *  GameInstance du jeu : héberger / rejoindre une partie (listen server) et mémoire des erreurs réseau.
 *  Deux modes :
 *   - Steam (si Steam est lancé et connecté) : la partie est un lobby Steam (session NAME_GameSession) ; on la rejoint
 *     par invitation d'un ami (overlay Steam) ou par la liste des parties ; connexion par le réseau de relais Steam
 *     (driver SteamSockets, pas de port à ouvrir). AppID 480 (Spacewar) pour l'instant, voir DefaultEngine.ini.
 *   - IP (réseau local / VPN, toujours possible) : HostGame / JoinGame comme avant, driver IpNetDriver (port 7777).
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

	/** Joueurs max d'une partie Steam (lobby) */
	UPROPERTY(EditDefaultsOnly, Config, Category="Network|Steam", meta=(ClampMin="1"))
	int32 MaxOnlinePlayers = 4;

	/** Parties Steam max demandées à la recherche */
	UPROPERTY(EditDefaultsOnly, Config, Category="Network|Steam", meta=(ClampMin="1"))
	int32 MaxSearchResults = 50;

	virtual void Init() override;

	virtual void Shutdown() override;

	// ---- mode IP ----

	/** Charge HostMap en listen server : les autres joueurs peuvent rejoindre par l'IP de cette machine */
	void HostGame();

	/** Rejoint l'hôte à l'adresse donnée (« 192.168.1.20 » ou « 192.168.1.20:7777 »). Faux si l'adresse est vide. */
	bool JoinGame(const FString& Address);

	// ---- mode Steam ----

	/** Steam est lancé et le joueur y est connecté (sinon seul le mode IP marche) */
	bool IsSteamAvailable() const;

	/** Pseudo Steam du joueur local (vide sans Steam) */
	FString GetSteamPersonaName() const;

	/** Crée un lobby Steam puis charge HostMap en listen server (les amis rejoignent par invitation ou par la liste) */
	void HostOnlineGame();

	/** Cherche les parties Steam de ce jeu (lobbies) ; résultat par OnOnlineGamesFound puis GetFoundOnlineGames */
	void FindOnlineGames();

	/** Rejoint une partie trouvée par FindOnlineGames */
	void JoinOnlineGame(int32 Index);

	const TArray<FYSOnlineGame>& GetFoundOnlineGames() const { return FoundGames; }

	bool IsSearchingOnlineGames() const { return bSearching; }

	/** Ouvre la fenêtre d'invitation d'amis de l'overlay Steam (seulement dans une partie Steam) */
	bool ShowInviteFriendsUI();

	/** Une partie Steam (lobby) est en cours sur cette machine (hôte ou client) */
	bool HasOnlineSession() const;

	/** Quitte le lobby Steam en cours (sans rien faire s'il n'y en a pas) */
	void DestroyOnlineSession();

	/** Debug : HostOnlineGame (comme le bouton Héberger (Steam)) */
	UFUNCTION(Exec)
	void YSSteamHost();

	/** Debug : FindOnlineGames, puis rejoint la partie d'index JoinIndex si elle existe (-1 = liste seulement, dans le log) */
	UFUNCTION(Exec)
	void YSSteamFind(int32 JoinIndex = -1);

	/** Fin de la recherche de parties Steam */
	FYSOnOnlineGamesFound OnOnlineGamesFound;

	/** Étapes et erreurs d'une partie Steam (création, connexion) à afficher dans le menu */
	FYSOnOnlineStatus OnOnlineStatus;

	// ---- commun ----

	/** Quitte la partie en cours (hôte ou client, et son lobby Steam) et revient au menu */
	void ReturnToMenu();

	/** Dernière adresse rejointe (mémorisée sur ce PC) */
	FString GetLastJoinAddress() const;

	/** Adresses IPv4 locales de ce PC, à donner aux autres joueurs */
	static TArray<FString> GetLocalIPv4Addresses();

	/** Message d'erreur réseau en attente (vidé à la lecture) */
	FText ConsumePendingError();

private:

	/** Driver réseau du jeu (GameNetDriver) : SteamSockets pour une partie Steam, IpNetDriver pour une partie par IP */
	static void UseSteamNetDriver(bool bSteam);

	IOnlineSessionPtr GetSessionInterface() const;

	FUniqueNetIdPtr GetLocalUserId() const;

	/** Rejoint une session (résultat de recherche ou invitation) : quitte d'abord le lobby en cours s'il y en a un */
	void JoinSessionResult(const FOnlineSessionSearchResult& Result);

	void CreateOnlineSession();

	void ReportOnlineError(const FText& Error);

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);

	void HandleFindSessionsComplete(bool bWasSuccessful);

	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	void HandleSessionUserInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);

	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	FText PendingError;

	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;

	FDelegateHandle CreateSessionHandle;
	FDelegateHandle FindSessionsHandle;
	FDelegateHandle JoinSessionHandle;
	FDelegateHandle InviteAcceptedHandle;

	TSharedPtr<FOnlineSessionSearch> SessionSearch;

	TArray<FYSOnlineGame> FoundGames;

	bool bSearching = false;

	/** YSSteamFind : partie à rejoindre à la fin de la recherche (-1 = aucune) */
	int32 DebugJoinIndex = -1;
};
