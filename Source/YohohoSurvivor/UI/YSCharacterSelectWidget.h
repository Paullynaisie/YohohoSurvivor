#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSCharacterSelectWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UYSCharacterSelectWidget;

DECLARE_DELEGATE_OneParam(FYSOnCharacterChosen, int32 /*CharacterIndex*/);

/** Relie le clic d'un bouton (sans paramètre) à l'index du personnage correspondant */
UCLASS()
class UYSCharacterSelectButtonProxy : public UObject
{
	GENERATED_BODY()

public:

	int32 Index = INDEX_NONE;

	TWeakObjectPtr<UYSCharacterSelectWidget> Menu;

	UFUNCTION()
	void HandleClicked();
};

/**
 *  Menu plein écran de sélection du personnage : un bouton par personnage, le bouton choisi est illuminé,
 *  et une ligne d'état (combien de joueurs ont choisi). Construit en C++ (pas de Widget Blueprint nécessaire) ;
 *  piloté par AYSPlayerController.
 */
UCLASS()
class UYSCharacterSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Couleur du bouton choisi */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Select")
	FLinearColor SelectedColor = FLinearColor(1.0f, 0.7f, 0.15f);

	/** Couleur des autres boutons */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Select")
	FLinearColor NormalColor = FLinearColor(0.2f, 0.2f, 0.22f);

	/** Voile derrière le menu */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Select")
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);

	/** Appelé quand le joueur clique sur un personnage */
	FYSOnCharacterChosen OnCharacterChosen;

	virtual bool Initialize() override;

	/** Crée un bouton par nom (une seule fois) */
	void SetCharacters(const TArray<FText>& Names);

	bool HasCharacters() const { return Buttons.Num() > 0; }

	/** Illumine le bouton choisi (INDEX_NONE = aucun) */
	void SetSelectedIndex(int32 Index);

	void SetStatus(const FText& Status);

	/** Clic reçu d'un bouton (via son proxy) */
	void NotifyButtonClicked(int32 Index);

private:

	void BuildWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ButtonList;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> Buttons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UYSCharacterSelectButtonProxy>> ButtonProxies;
};
