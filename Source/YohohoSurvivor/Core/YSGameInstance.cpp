#include "Core/YSGameInstance.h"
#include "Core/YSGamepad.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "IPAddress.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "SocketSubsystem.h"

namespace YSGameInstance
{
	const TCHAR* NetworkConfigSection = TEXT("YohohoSurvivor.Network");
	const TCHAR* LastJoinAddressKey = TEXT("LastJoinAddress");

	const FName IpNetDriverClass(TEXT("/Script/OnlineSubsystemUtils.IpNetDriver"));
	const FName SteamNetDriverClass(TEXT("/Script/SteamSockets.SteamSocketsNetDriver"));

	/** Clé posée sur nos lobbies : l'AppID 480 est partagé par tous les développeurs, la recherche ne garde que les nôtres */
	const FName LobbyGameKey(TEXT("YOHOHOSURVIVOR"));
	constexpr int32 LobbyGameValue = 1;
}

using YSGameInstance::NetworkConfigSection;
using YSGameInstance::LastJoinAddressKey;

void UYSGameInstance::Init()
{
	Super::Init();

	// manette : dernier périphérique utilisé (focus des menus)
	YSGamepad::Register();

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UYSGameInstance::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UYSGameInstance::HandleTravelFailure);
	}

	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		CreateSessionHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UYSGameInstance::HandleCreateSessionComplete));
		FindSessionsHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UYSGameInstance::HandleFindSessionsComplete));
		JoinSessionHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UYSGameInstance::HandleJoinSessionComplete));
		// invitation acceptée dans l'overlay Steam (jeu lancé) ou avant le lancement (+connect_lobby : Steam la garde jusqu'ici)
		InviteAcceptedHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UYSGameInstance::HandleSessionUserInviteAccepted));
	}

	const IOnlineSubsystem* Online = IOnlineSubsystem::Get();
	UE_LOG(LogTemp, Log, TEXT("[YSSteam] Sous-système en ligne : %s, Steam %s%s"),
		Online ? *Online->GetSubsystemName().ToString() : TEXT("aucun"),
		IsSteamAvailable() ? TEXT("connecté : ") : TEXT("indisponible (mode IP seulement)"),
		IsSteamAvailable() ? *GetSteamPersonaName() : TEXT(""));
}

void UYSGameInstance::Shutdown()
{
	YSGamepad::Unregister();

	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}

	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
		Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
	}

	Super::Shutdown();
}

void UYSGameInstance::HostGame()
{
	if (HostMap.IsNull())
	{
		PendingError = NSLOCTEXT("YS", "NoHostMap", "Aucune map d'hôte configurée (HostMap).");
		return;
	}

	// partie par IP : pas de lobby Steam, driver IP classique (port 7777)
	DestroyOnlineSession();
	UseSteamNetDriver(false);
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, HostMap, true, TEXT("listen"));
}

bool UYSGameInstance::JoinGame(const FString& Address)
{
	const FString Trimmed = Address.TrimStartAndEnd();
	APlayerController* PlayerController = GetFirstLocalPlayerController();
	if (Trimmed.IsEmpty() || !PlayerController)
	{
		return false;
	}

	GConfig->SetString(NetworkConfigSection, LastJoinAddressKey, *Trimmed, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);

	DestroyOnlineSession();
	UseSteamNetDriver(false);

	// le monde actuel (menu) reste affiché jusqu'à la connexion ; en cas d'échec, HandleNetworkFailure garde le message
	PlayerController->ClientTravel(Trimmed, TRAVEL_Absolute);
	return true;
}

void UYSGameInstance::UseSteamNetDriver(bool bSteam)
{
	if (!GEngine)
	{
		return;
	}

	// lu à la création du driver (ouverture en ?listen, ClientTravel) ; SteamSockets indisponible = repli sur IpNetDriver
	for (FNetDriverDefinition& Definition : GEngine->NetDriverDefinitions)
	{
		if (Definition.DefName == NAME_GameNetDriver)
		{
			Definition.DriverClassName = bSteam ? YSGameInstance::SteamNetDriverClass : YSGameInstance::IpNetDriverClass;
			Definition.DriverClassNameFallback = YSGameInstance::IpNetDriverClass;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[YSSteam] Driver réseau du jeu : %s"), bSteam ? TEXT("SteamSockets") : TEXT("IpNetDriver"));
}

IOnlineSessionPtr UYSGameInstance::GetSessionInterface() const
{
	const IOnlineSubsystem* Online = IOnlineSubsystem::Get();
	return Online ? Online->GetSessionInterface() : nullptr;
}

FUniqueNetIdPtr UYSGameInstance::GetLocalUserId() const
{
	const IOnlineSubsystem* Online = IOnlineSubsystem::Get();
	const IOnlineIdentityPtr Identity = Online ? Online->GetIdentityInterface() : nullptr;
	return Identity ? Identity->GetUniquePlayerId(0) : nullptr;
}

bool UYSGameInstance::IsSteamAvailable() const
{
	// sans Steam (pas lancé, éditeur), le moteur se rabat sur le sous-système NULL
	const IOnlineSubsystem* Online = IOnlineSubsystem::Get();
	if (!Online || Online->GetSubsystemName() != STEAM_SUBSYSTEM)
	{
		return false;
	}

	const IOnlineIdentityPtr Identity = Online->GetIdentityInterface();
	return Identity && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn;
}

FString UYSGameInstance::GetSteamPersonaName() const
{
	if (!IsSteamAvailable())
	{
		return FString();
	}

	return IOnlineSubsystem::Get()->GetIdentityInterface()->GetPlayerNickname(0);
}

void UYSGameInstance::HostOnlineGame()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!IsSteamAvailable() || !Sessions)
	{
		ReportOnlineError(NSLOCTEXT("YS", "SteamUnavailable", "Steam n'est pas lancé : lance Steam puis le jeu, ou utilise le mode IP."));
		return;
	}
	if (HostMap.IsNull())
	{
		ReportOnlineError(NSLOCTEXT("YS", "NoHostMap", "Aucune map d'hôte configurée (HostMap)."));
		return;
	}

	// ancien lobby encore ouvert (partie précédente) : le quitter d'abord
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		Sessions->DestroySession(NAME_GameSession, FOnDestroySessionCompleteDelegate::CreateWeakLambda(this, [this](FName, bool)
		{
			CreateOnlineSession();
		}));
		return;
	}

	CreateOnlineSession();
}

void UYSGameInstance::CreateOnlineSession()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	const FUniqueNetIdPtr UserId = GetLocalUserId();
	if (!Sessions || !UserId.IsValid())
	{
		ReportOnlineError(NSLOCTEXT("YS", "SteamNoUser", "Joueur Steam introuvable : Steam est-il connecté ?"));
		return;
	}

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = MaxOnlinePlayers;
	Settings.NumPrivateConnections = 0;
	Settings.bIsLANMatch = false;
	Settings.bIsDedicated = false;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = true;
	// lobby Steam : présence et lobby vont ensemble (exigé par OnlineSubsystemSteam)
	Settings.bUsesPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bAllowJoinViaPresenceFriendsOnly = false;
	Settings.Set(YSGameInstance::LobbyGameKey, YSGameInstance::LobbyGameValue, EOnlineDataAdvertisementType::ViaOnlineService);

	OnOnlineStatus.Broadcast(NSLOCTEXT("YS", "SteamCreating", "Création du lobby Steam…"), false);
	UE_LOG(LogTemp, Log, TEXT("[YSSteam] Création du lobby (%d joueurs max)"), MaxOnlinePlayers);
	if (!Sessions->CreateSession(*UserId, NAME_GameSession, Settings))
	{
		ReportOnlineError(NSLOCTEXT("YS", "SteamCreateFailed", "Impossible de créer le lobby Steam."));
	}
}

void UYSGameInstance::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName != NAME_GameSession)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[YSSteam] Lobby créé : %s"), bWasSuccessful ? TEXT("oui") : TEXT("ÉCHEC"));
	if (!bWasSuccessful)
	{
		ReportOnlineError(NSLOCTEXT("YS", "SteamCreateFailed", "Impossible de créer le lobby Steam."));
		return;
	}

	// connexion des amis par le réseau Steam (SteamID de l'hôte), pas par son IP
	UseSteamNetDriver(true);
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, HostMap, true, TEXT("listen"));
}

void UYSGameInstance::FindOnlineGames()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	const FUniqueNetIdPtr UserId = GetLocalUserId();
	if (!IsSteamAvailable() || !Sessions || !UserId.IsValid())
	{
		ReportOnlineError(NSLOCTEXT("YS", "SteamUnavailable", "Steam n'est pas lancé : lance Steam puis le jeu, ou utilise le mode IP."));
		OnOnlineGamesFound.Broadcast(false);
		return;
	}
	if (bSearching)
	{
		return;
	}

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->bIsLanQuery = false;
	SessionSearch->MaxSearchResults = MaxSearchResults;
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SessionSearch->QuerySettings.Set(YSGameInstance::LobbyGameKey, YSGameInstance::LobbyGameValue, EOnlineComparisonOp::Equals);

	bSearching = true;
	FoundGames.Reset();
	UE_LOG(LogTemp, Log, TEXT("[YSSteam] Recherche des parties…"));
	if (!Sessions->FindSessions(*UserId, SessionSearch.ToSharedRef()))
	{
		bSearching = false;
		OnOnlineGamesFound.Broadcast(false);
	}
}

void UYSGameInstance::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (!bSearching)
	{
		return;
	}
	bSearching = false;

	FoundGames.Reset();
	if (bWasSuccessful && SessionSearch.IsValid())
	{
		for (const FOnlineSessionSearchResult& Result : SessionSearch->SearchResults)
		{
			FYSOnlineGame& Game = FoundGames.AddDefaulted_GetRef();
			Game.HostName = Result.Session.OwningUserName;
			Game.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
			Game.Players = FMath::Max(0, Game.MaxPlayers - Result.Session.NumOpenPublicConnections);
			Game.PingMs = Result.PingInMs;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[YSSteam] Recherche terminée : %s, %d partie(s)"), bWasSuccessful ? TEXT("ok") : TEXT("ÉCHEC"), FoundGames.Num());
	for (int32 Index = 0; Index < FoundGames.Num(); ++Index)
	{
		UE_LOG(LogTemp, Log, TEXT("[YSSteam]   %d : %s (%d / %d), %d ms"), Index, *FoundGames[Index].HostName, FoundGames[Index].Players, FoundGames[Index].MaxPlayers, FoundGames[Index].PingMs);
	}
	OnOnlineGamesFound.Broadcast(bWasSuccessful);

	const int32 JoinIndex = DebugJoinIndex;
	DebugJoinIndex = -1;
	if (FoundGames.IsValidIndex(JoinIndex))
	{
		JoinOnlineGame(JoinIndex);
	}
}

void UYSGameInstance::JoinOnlineGame(int32 Index)
{
	if (!SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(Index))
	{
		ReportOnlineError(NSLOCTEXT("YS", "SteamGameGone", "Cette partie n'existe plus : relance la recherche."));
		return;
	}

	JoinSessionResult(SessionSearch->SearchResults[Index]);
}

void UYSGameInstance::JoinSessionResult(const FOnlineSessionSearchResult& Result)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	const FUniqueNetIdPtr UserId = GetLocalUserId();
	if (!Sessions || !UserId.IsValid())
	{
		ReportOnlineError(NSLOCTEXT("YS", "SteamNoUser", "Joueur Steam introuvable : Steam est-il connecté ?"));
		return;
	}

	// déjà dans un lobby (partie en cours, invitation reçue en jeu) : le quitter avant de rejoindre l'autre
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		const FOnlineSessionSearchResult ResultCopy = Result;
		Sessions->DestroySession(NAME_GameSession, FOnDestroySessionCompleteDelegate::CreateWeakLambda(this, [this, ResultCopy](FName, bool)
		{
			JoinSessionResult(ResultCopy);
		}));
		return;
	}

	OnOnlineStatus.Broadcast(FText::Format(NSLOCTEXT("YS", "SteamJoining", "Connexion à la partie de {0}…"), FText::FromString(Result.Session.OwningUserName)), false);
	UE_LOG(LogTemp, Log, TEXT("[YSSteam] Rejoint le lobby de %s"), *Result.Session.OwningUserName);
	if (!Sessions->JoinSession(*UserId, NAME_GameSession, Result))
	{
		ReportOnlineError(NSLOCTEXT("YS", "SteamJoinFailed", "Impossible de rejoindre le lobby Steam."));
	}
}

void UYSGameInstance::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionName != NAME_GameSession)
	{
		return;
	}

	IOnlineSessionPtr Sessions = GetSessionInterface();
	FString ConnectString;
	const bool bJoined = Result == EOnJoinSessionCompleteResult::Success || Result == EOnJoinSessionCompleteResult::AlreadyInSession;
	if (!bJoined || !Sessions || !Sessions->GetResolvedConnectString(NAME_GameSession, ConnectString))
	{
		UE_LOG(LogTemp, Warning, TEXT("[YSSteam] Échec pour rejoindre le lobby : %s"), LexToString(Result));
		DestroyOnlineSession();
		ReportOnlineError(Result == EOnJoinSessionCompleteResult::SessionIsFull
			? NSLOCTEXT("YS", "SteamFull", "La partie est pleine.")
			: NSLOCTEXT("YS", "SteamJoinFailed", "Impossible de rejoindre le lobby Steam."));
		return;
	}

	APlayerController* PlayerController = GetFirstLocalPlayerController();
	if (!PlayerController)
	{
		DestroyOnlineSession();
		return;
	}

	// adresse « steam.<SteamID de l'hôte>:7777 » : connexion par le réseau de relais Steam
	UE_LOG(LogTemp, Log, TEXT("[YSSteam] Lobby rejoint, connexion à %s"), *ConnectString);
	UseSteamNetDriver(true);
	PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
}

void UYSGameInstance::HandleSessionUserInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	UE_LOG(LogTemp, Log, TEXT("[YSSteam] Invitation acceptée : %s"), bWasSuccessful && InviteResult.IsValid() ? TEXT("ok") : TEXT("invalide"));
	if (!bWasSuccessful || !InviteResult.IsValid())
	{
		ReportOnlineError(NSLOCTEXT("YS", "SteamInviteInvalid", "Invitation Steam invalide (la partie est peut-être terminée)."));
		return;
	}

	JoinSessionResult(InviteResult);
}

bool UYSGameInstance::ShowInviteFriendsUI()
{
	const IOnlineSubsystem* Online = IOnlineSubsystem::Get();
	const IOnlineExternalUIPtr ExternalUI = Online ? Online->GetExternalUIInterface() : nullptr;
	if (!HasOnlineSession() || !ExternalUI)
	{
		return false;
	}

	return ExternalUI->ShowInviteUI(0, NAME_GameSession);
}

bool UYSGameInstance::HasOnlineSession() const
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	return IsSteamAvailable() && Sessions && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
}

void UYSGameInstance::DestroyOnlineSession()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions && Sessions->GetNamedSession(NAME_GameSession))
	{
		UE_LOG(LogTemp, Log, TEXT("[YSSteam] Lobby quitté"));
		Sessions->DestroySession(NAME_GameSession);
	}
}

void UYSGameInstance::YSSteamHost()
{
	HostOnlineGame();
}

void UYSGameInstance::YSSteamFind(int32 JoinIndex)
{
	DebugJoinIndex = JoinIndex;
	FindOnlineGames();
}

void UYSGameInstance::ReportOnlineError(const FText& Error)
{
	UE_LOG(LogTemp, Warning, TEXT("[YSSteam] %s"), *Error.ToString());
	PendingError = Error;
	OnOnlineStatus.Broadcast(Error, true);
}

void UYSGameInstance::ReturnToMenu()
{
	DestroyOnlineSession();

	if (!MenuMap.IsNull())
	{
		// ferme la connexion (client) ou le serveur (hôte : les clients reviennent à leur menu)
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, MenuMap);
	}
}

FString UYSGameInstance::GetLastJoinAddress() const
{
	FString Address;
	GConfig->GetString(NetworkConfigSection, LastJoinAddressKey, Address, GGameUserSettingsIni);
	return Address;
}

TArray<FString> UYSGameInstance::GetLocalIPv4Addresses()
{
	TArray<FString> Result;

	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	TArray<TSharedPtr<FInternetAddr>> Addresses;
	if (Sockets && Sockets->GetLocalAdapterAddresses(Addresses))
	{
		for (const TSharedPtr<FInternetAddr>& Address : Addresses)
		{
			if (Address.IsValid() && Address->GetProtocolType() == FNetworkProtocolTypes::IPv4)
			{
				const FString Text = Address->ToString(false);
				if (!Text.StartsWith(TEXT("127.")) && !Text.StartsWith(TEXT("169.254.")))
				{
					Result.AddUnique(Text);
				}
			}
		}
	}

	return Result;
}

FText UYSGameInstance::ConsumePendingError()
{
	FText Error = PendingError;
	PendingError = FText::GetEmpty();
	return Error;
}

void UYSGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// plusieurs GameInstance en PIE multi-joueurs : ne garder que les erreurs de notre monde
	if (World && World->GetGameInstance() != this)
	{
		return;
	}

	switch (FailureType)
	{
	case ENetworkFailure::ConnectionTimeout:
	case ENetworkFailure::PendingConnectionFailure:
		PendingError = NSLOCTEXT("YS", "JoinTimeout", "Impossible de joindre l'hôte (IP, pare-feu, ou partie pas encore créée ?).");
		break;
	case ENetworkFailure::ConnectionLost:
	case ENetworkFailure::FailureReceived:
		PendingError = NSLOCTEXT("YS", "ConnectionLost", "Connexion avec l'hôte perdue.");
		break;
	case ENetworkFailure::OutdatedClient:
	case ENetworkFailure::OutdatedServer:
		PendingError = NSLOCTEXT("YS", "VersionMismatch", "Version du jeu différente de celle de l'hôte : utilisez la même build.");
		break;
	case ENetworkFailure::NetDriverListenFailure:
		PendingError = NSLOCTEXT("YS", "ListenFailure", "Impossible d'héberger : le port est peut-être déjà utilisé.");
		break;
	default:
		PendingError = FText::Format(NSLOCTEXT("YS", "NetworkError", "Erreur réseau : {0}"), FText::FromString(ErrorString));
		break;
	}

	UE_LOG(LogTemp, Warning, TEXT("[YSGameInstance] Network failure %s : %s"), ENetworkFailure::ToString(FailureType), *ErrorString);
}

void UYSGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (World && World->GetGameInstance() != this)
	{
		return;
	}

	PendingError = FText::Format(NSLOCTEXT("YS", "TravelError", "Échec du chargement de la partie : {0}"), FText::FromString(ErrorString));
	UE_LOG(LogTemp, Warning, TEXT("[YSGameInstance] Travel failure %s : %s"), ETravelFailure::ToString(FailureType), *ErrorString);
}
