#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "YSMainMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UVerticalBox;
class UYSGameInstance;
class UYSMainMenuButtonProxy;

/**
 *  Menu principal plein écran (map MainMenu), deux colonnes :
 *   - Steam (si Steam est lancé) : Héberger (lobby Steam, les amis rejoignent par invitation), Chercher les parties
 *     (liste des lobbies de ce jeu, un bouton par partie) ;
 *   - IP (réseau local / VPN) : Héberger en IP, IP locales de ce PC + « Copier mon IP », champ IP + Rejoindre.
 *  Quitter et la dernière erreur réseau en dessous. Construit en C++ (pas de Widget Blueprint) ; actions déléguées à UYSGameInstance.
 */
UCLASS()
class UYSMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Couleur des boutons */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Main Menu")
	FLinearColor ButtonColor = FLinearColor(0.2f, 0.2f, 0.22f);

	/** Couleur des boutons Héberger */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Main Menu")
	FLinearColor HostButtonColor = FLinearColor(1.0f, 0.7f, 0.15f);

	/** Couleur des boutons Steam (hors Héberger) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Main Menu")
	FLinearColor SteamButtonColor = FLinearColor(0.1f, 0.25f, 0.45f);

	/** Fond du menu */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Main Menu")
	FLinearColor BackgroundColor = FLinearColor(0.03f, 0.05f, 0.08f, 1.0f);

	/** Largeur d'une colonne (et de ses boutons) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	float ColumnWidth = 380.0f;

	/** Espace entre les colonnes Steam et IP */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	float ColumnSpacing = 80.0f;

	/** Hauteur des boutons */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	float ButtonHeight = 56.0f;

	/** Hauteur des boutons de la liste des parties Steam */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	float GameButtonHeight = 44.0f;

	virtual bool Initialize() override;

	/** Clic sur une partie de la liste Steam (via son proxy) */
	void NotifyGameClicked(int32 Index);

protected:

	virtual void NativeConstruct() override;

	virtual void NativeDestruct() override;

private:

	void BuildWidgetTree();

	UButton* AddButton(UVerticalBox* Column, const FText& Label, const FLinearColor& Color, FName Name, float Height = 0.0f, int32 FontSize = 22);

	UTextBlock* AddText(UVerticalBox* Column, const FText& Text, int32 FontSize, const FMargin& TextPadding);

	void SetStatus(const FText& Status, bool bError = false);

	/** Colonne Steam active ou grisée selon que Steam est lancé */
	void RefreshSteamState();

	/** Reconstruit la liste des parties Steam trouvées */
	void RefreshGameList();

	UYSGameInstance* GetYSGameInstance() const;

	void HandleOnlineStatus(const FText& Status, bool bError);

	void HandleOnlineGamesFound(bool bSuccess);

	UFUNCTION()
	void HandleHostClicked();

	UFUNCTION()
	void HandleSteamHostClicked();

	UFUNCTION()
	void HandleSteamSearchClicked();

	UFUNCTION()
	void HandleJoinClicked();

	UFUNCTION()
	void HandleQuitClicked();

	/** Met l'IP de ce PC dans le presse-papier (à envoyer aux autres joueurs) */
	UFUNCTION()
	void HandleCopyAddressClicked();

	UFUNCTION()
	void HandleAddressCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> AddressBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LocalAddressText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SteamStateText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SteamHostButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SteamSearchButton;

	/** Liste des parties Steam trouvées (un bouton par partie) */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> GameList;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UYSMainMenuButtonProxy>> GameProxies;

	FDelegateHandle OnlineStatusHandle;
	FDelegateHandle GamesFoundHandle;
};

/** Relie le clic d'un bouton de la liste des parties à son index (OnClicked n'a pas de paramètre) */
UCLASS()
class UYSMainMenuButtonProxy : public UObject
{
	GENERATED_BODY()

public:

	int32 Index = 0;

	TWeakObjectPtr<UYSMainMenuWidget> Menu;

	UFUNCTION()
	void HandleClicked()
	{
		if (Menu.IsValid())
		{
			Menu->NotifyGameClicked(Index);
		}
	}
};
