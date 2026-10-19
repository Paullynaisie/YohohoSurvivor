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

/**
 *  Menu principal plein écran (map MainMenu) : Héberger (listen server), champ IP + Rejoindre, Quitter.
 *  Affiche les IP locales de ce PC (à donner aux autres joueurs) et la dernière erreur réseau.
 *  Construit en C++ (pas de Widget Blueprint) ; actions déléguées à UYSGameInstance.
 */
UCLASS()
class UYSMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Couleur des boutons */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Main Menu")
	FLinearColor ButtonColor = FLinearColor(0.2f, 0.2f, 0.22f);

	/** Couleur du bouton Héberger */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Main Menu")
	FLinearColor HostButtonColor = FLinearColor(1.0f, 0.7f, 0.15f);

	/** Fond du menu */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Main Menu")
	FLinearColor BackgroundColor = FLinearColor(0.03f, 0.05f, 0.08f, 1.0f);

	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

private:

	void BuildWidgetTree();

	UButton* AddButton(UVerticalBox* Column, const FText& Label, const FLinearColor& Color, FName Name);

	void SetStatus(const FText& Status, bool bError = false);

	UYSGameInstance* GetYSGameInstance() const;

	UFUNCTION()
	void HandleHostClicked();

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
};
