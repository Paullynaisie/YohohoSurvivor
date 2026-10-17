#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "YSCharacterSelectWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UHorizontalBox;
class UBorder;
class UImage;
class UEditableTextBox;
class UTexture;
class UTexture2D;
class UYSCharacterSelectWidget;

DECLARE_DELEGATE_OneParam(FYSOnCharacterChosen, int32 /*CharacterIndex*/);
DECLARE_DELEGATE_OneParam(FYSOnNicknameCommitted, const FString& /*Nickname*/);

/** Une ligne de texte d'un bouton de choix (upgrades : nom, valeur rayée, nouvelle valeur en gras…) */
USTRUCT(BlueprintType)
struct FYSChoiceLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Choice")
	FText Text;

	UPROPERTY(BlueprintReadWrite, Category="Choice")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(BlueprintReadWrite, Category="Choice")
	int32 FontSize = 16;

	UPROPERTY(BlueprintReadWrite, Category="Choice")
	bool bBold = false;

	UPROPERTY(BlueprintReadWrite, Category="Choice")
	bool bStrikethrough = false;
};

/** Relie le clic d'un bouton (sans paramètre) à son groupe et à son index */
UCLASS()
class UYSCharacterSelectButtonProxy : public UObject
{
	GENERATED_BODY()

public:

	/** 0 = choix principal (personnage / upgrade), 1 = couleur, 2 = visage, 3 = « Prêt » */
	int32 Group = 0;

	int32 Index = INDEX_NONE;

	TWeakObjectPtr<UYSCharacterSelectWidget> Menu;

	UFUNCTION()
	void HandleClicked();
};

/**
 *  Menu plein écran de sélection du personnage, en 3 colonnes de même largeur, avec le pseudo en haut au centre :
 *  - gauche : un bouton par personnage (le choisi est illuminé) et l'état (combien de joueurs ont choisi) ;
 *  - centre : personnalisation (4 couleurs, 4 visages) ;
 *  - droite : aperçu du personnage (image d'une capture, fournie par AYSPlayerController).
 *  Réutilisé pour le choix d'upgrade : SetCustomizationVisible(false) ne garde que la colonne des choix, centrée.
 *  Construit en C++ (pas de Widget Blueprint nécessaire) ; piloté par AYSPlayerController.
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

	/** Taille des boutons et de leur texte (à régler avant SetCharacters) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Select")
	FVector2D ButtonSize = FVector2D(380.0f, 64.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Select")
	int32 ButtonFontSize = 24;

	/** Côté des boutons carrés de couleur / visage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Select")
	float SwatchSize = 64.0f;

	/** Taille de l'aperçu du personnage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Select")
	FVector2D PreviewSize = FVector2D(360.0f, 480.0f);

	/** Appelé quand le joueur clique sur un personnage (ou une upgrade) */
	FYSOnCharacterChosen OnCharacterChosen;

	/** Appelés quand le joueur choisit une couleur / un visage (index) */
	FYSOnCharacterChosen OnColorChosen;
	FYSOnCharacterChosen OnFaceChosen;

	/** Appelé quand le joueur clique sur « Prêt » */
	FSimpleDelegate OnReadyClicked;

	/** Appelé quand le joueur valide son pseudo (Entrée ou perte du focus) */
	FYSOnNicknameCommitted OnNicknameCommitted;

	virtual bool Initialize() override;

	/** Crée un bouton par nom (une seule fois) */
	void SetCharacters(const TArray<FText>& Names);

	/** Crée un bouton par choix, chacun avec plusieurs lignes de texte (une seule fois, voir ClearChoices) */
	void SetChoices(const TArray<TArray<FYSChoiceLine>>& Choices);

	bool HasCharacters() const { return Buttons.Num() > 0; }

	/** Supprime les boutons (pour proposer de nouveaux choix) */
	void ClearChoices();

	/** Titre affiché au-dessus des boutons */
	void SetTitle(const FText& Title);

	/** Illumine le bouton choisi (INDEX_NONE = aucun) */
	void SetSelectedIndex(int32 Index);

	void SetStatus(const FText& Status);

	/** Affiche le pseudo, la personnalisation et l'aperçu (faux pour le menu d'upgrade) */
	void SetCustomizationVisible(bool bVisible);

	/** Crée les boutons de couleur et de visage (une seule fois) */
	void SetCustomizationOptions(const TArray<FLinearColor>& Colors, const TArray<UTexture2D*>& Faces);

	bool HasCustomizationOptions() const { return ColorButtons.Num() > 0; }

	/** Encadre la couleur / le visage choisi */
	void SetSelectedColor(int32 Index);
	void SetSelectedFace(int32 Index);

	/** Pseudo affiché (ignoré pendant que le joueur l'édite) */
	void SetNickname(const FText& Nickname);

	/** Bouton « Prêt » illuminé quand le joueur est prêt */
	void SetReady(bool bIsReady);

	/** Couleurs prises par d'autres joueurs : grisées et non cliquables */
	void SetTakenColors(const TArray<bool>& Taken);

	/** Image de l'aperçu du personnage (render target) */
	void SetPreviewTexture(UTexture* Texture);

	/** Clic reçu d'un bouton (via son proxy) */
	void NotifyButtonClicked(int32 Group, int32 Index);

private:

	void BuildWidgetTree();

	/** Ajoute un bouton (contenu = Content) relié à son index */
	void AddChoiceButton(UWidget* Content);

	/** Bouton carré encadré (couleur ou visage) ajouté à Row ; renvoie le cadre */
	UBorder* AddSwatchButton(UHorizontalBox* Row, int32 Group, int32 Index, const FLinearColor& ButtonColor, UTexture2D* Icon, UButton*& OutButton);

	UFUNCTION()
	void HandleNicknameCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ButtonList;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> Buttons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UYSCharacterSelectButtonProxy>> ButtonProxies;

	/** Pseudo (en haut au centre) */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> NicknameRow;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> NicknameBox;

	/** Colonnes centre (personnalisation) et droite (aperçu) */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> CustomizationColumn;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> PreviewColumn;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PreviewImage;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> ColorRow;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> FaceRow;

	/** Cadres des boutons de couleur / visage (sélection) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> ColorButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> FaceButtons;

	/** Boutons de couleur (pour griser les couleurs prises) et leur couleur */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> ColorSwatches;

	TArray<FLinearColor> ColorValues;

	/** Bouton « Prêt » (en bas au centre) */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> ReadyRow;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ReadyButton;
};
