#include "Core/YSGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "IPAddress.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "SocketSubsystem.h"

namespace
{
	const TCHAR* NetworkConfigSection = TEXT("YohohoSurvivor.Network");
	const TCHAR* LastJoinAddressKey = TEXT("LastJoinAddress");
}

void UYSGameInstance::Init()
{
	Super::Init();

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UYSGameInstance::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UYSGameInstance::HandleTravelFailure);
	}
}

void UYSGameInstance::Shutdown()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
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

	// le monde actuel (menu) reste affiché jusqu'à la connexion ; en cas d'échec, HandleNetworkFailure garde le message
	PlayerController->ClientTravel(Trimmed, TRAVEL_Absolute);
	return true;
}

void UYSGameInstance::ReturnToMenu()
{
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
