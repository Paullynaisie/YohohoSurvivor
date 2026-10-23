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

	/** 0 = choix principal (personnage / upgrade), 1 = couleur, 2 = visage, 3 = « Prêt », 4 = main (0 droitier, 1 gaucher) */
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
 *  Arbre construit en C++ ; placements réglables (catégorie Layout) dans un Widget Blueprint enfant
 *  (WBP_CharacterSelect, Class Defaults) choisi par AYSPlayerController::CharacterSelectMenuClass.
 */
UCLASS(meta=(PrioritizeCategories="Layout"))
class UYSCharacterSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	// ---- couleurs ----

	/** Couleur du bouton choisi (et du bouton « Prêt » quand le joueur est prêt) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Colors")
	FLinearColor SelectedColor = FLinearColor(1.0f, 0.7f, 0.15f);

	/** Couleur des autres boutons */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Colors")
	FLinearColor NormalColor = FLinearColor(0.2f, 0.2f, 0.22f);

	/** Voile derrière le menu */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Colors")
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);

	/** Cadre de la couleur / du visage choisi */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Colors")
	FLinearColor SwatchSelectedFrameColor = FLinearColor::White;

	/** Assombrissement des couleurs prises par les autres joueurs (multiplicateur) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Colors", meta=(ClampMin="0", ClampMax="1"))
	float TakenColorDim = 0.25f;

	// ---- pseudo (en haut au centre) ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Nickname")
	float NicknameWidth = 340.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Nickname")
	int32 NicknameFontSize = 22;

	/** Marges autour de la zone de pseudo (haut = distance au bord de l'écran) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Nickname")
	FMargin NicknamePadding = FMargin(0.0f, 32.0f, 0.0f, 0.0f);

	// ---- 3 colonnes ----

	/** Largeur relative des colonnes gauche (choix) / centre (personnalisation) / droite (aperçu) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Columns", meta=(ClampMin="0.1"))
	float LeftColumnWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Columns", meta=(ClampMin="0.1"))
	float CenterColumnWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Columns", meta=(ClampMin="0.1"))
	float RightColumnWeight = 1.0f;

	/** Marges de chaque colonne (décalage du contenu dans sa colonne) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Columns")
	FMargin LeftColumnPadding;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Columns")
	FMargin CenterColumnPadding;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Columns")
	FMargin RightColumnPadding;

	/** Alignement vertical du contenu des colonnes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Columns")
	TEnumAsByte<EVerticalAlignment> ColumnVerticalAlignment = VAlign_Center;

	// ---- colonne gauche : choix du personnage ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Choices")
	int32 TitleFontSize = 36;

	/** Espace entre le titre et les boutons */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Choices")
	float TitleSpacing = 24.0f;

	/** Taille des boutons et de leur texte (à régler avant SetCharacters) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Choices")
	FVector2D ButtonSize = FVector2D(380.0f, 64.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Choices")
	int32 ButtonFontSize = 24;

	/** Espace vertical entre deux boutons */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Choices")
	float ButtonSpacing = 16.0f;

	/** Opacité du bouton d'un personnage unique déjà pris par un autre joueur (grisé, non cliquable) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Choices", meta=(ClampMin="0", ClampMax="1"))
	float TakenCharacterOpacity = 0.35f;

	/** « Joueurs prêts X / N » */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Choices")
	int32 StatusFontSize = 18;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Choices")
	float StatusSpacing = 24.0f;

	// ---- colonne centre : couleur et visage ----

	/** « Choisis ta couleur » / « Choisis ton visage » */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Customization")
	int32 SectionTitleFontSize = 24;

	/** Espace entre un titre et ses boutons */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Customization")
	float SectionTitleSpacing = 12.0f;

	/** Espace entre les couleurs et le titre des visages */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Customization")
	float SectionSpacing = 40.0f;

	/** Côté des boutons carrés de couleur / visage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Customization")
	float SwatchSize = 64.0f;

	/** Espace horizontal entre deux boutons carrés */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Customization")
	float SwatchSpacing = 12.0f;

	/** Épaisseur du cadre de sélection */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Customization")
	float SwatchFrameThickness = 4.0f;

	/** Nombre max de boutons carrés par ligne (couleurs, visages) : au-delà, retour à la ligne sous les premiers */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Customization", meta=(ClampMin="1"))
	int32 SwatchesPerRow = 8;

	/** Espace vertical entre deux lignes de boutons carrés */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Customization")
	float SwatchRowSpacing = 12.0f;

	/** « Choisis ta main » : boutons Droitier / Gaucher */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Customization")
	FVector2D HandednessButtonSize = FVector2D(150.0f, 48.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Customization")
	int32 HandednessFontSize = 20;

	/** Espace entre les deux boutons */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Customization")
	float HandednessSpacing = 16.0f;

	// ---- colonne droite : aperçu ----

	/** Taille de l'aperçu du personnage (l'image capturée a la même proportion : AYSPlayerController) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Preview")
	FVector2D PreviewSize = FVector2D(360.0f, 480.0f);

	// ---- bouton « Prêt » (en bas au centre) ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Ready")
	FVector2D ReadyButtonSize = FVector2D(260.0f, 64.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Ready")
	int32 ReadyFontSize = 28;

	/** Marges autour du bouton (bas = distance au bord de l'écran) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Ready")
	FMargin ReadyPadding = FMargin(0.0f, 0.0f, 0.0f, 40.0f);

	/** Appelé quand le joueur clique sur un personnage (ou une upgrade) */
	FYSOnCharacterChosen OnCharacterChosen;

	/** Appelés quand le joueur choisit une couleur / un visage (index) */
	FYSOnCharacterChosen OnColorChosen;
	FYSOnCharacterChosen OnFaceChosen;

	/** Appelé quand le joueur choisit sa main (0 = droitier, 1 = gaucher) */
	FYSOnCharacterChosen OnHandednessChosen;

	/** Appelé quand le joueur clique sur « Prêt » */
	FSimpleDelegate OnReadyClicked;

	/** Appelé quand le joueur valide son pseudo (Entrée ou perte du focus) */
	FYSOnNicknameCommitted OnNicknameCommitted;

	virtual bool Initialize() override;

	/** Clic dans le champ du pseudo : l'ancien pseudo est effacé pour taper directement le nouveau */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

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

	/** Illumine Droitier ou Gaucher */
	void SetSelectedHandedness(bool bLeftHanded);

	/** Pseudo affiché (ignoré pendant que le joueur l'édite) */
	void SetNickname(const FText& Nickname);

	/** Bouton « Prêt » illuminé quand le joueur est prêt */
	void SetReady(bool bIsReady);

	/** Couleurs prises par d'autres joueurs : grisées et non cliquables */
	void SetTakenColors(const TArray<bool>& Taken);

	/** Personnages uniques pris par d'autres joueurs : grisés et non cliquables */
	void SetTakenCharacters(const TArray<bool>& Taken);

	/** Image de l'aperçu du personnage (render target) */
	void SetPreviewTexture(UTexture* Texture);

	/** Clic reçu d'un bouton (via son proxy) */
	void NotifyButtonClicked(int32 Group, int32 Index);

private:

	void BuildWidgetTree();

	/** Ajoute un bouton (contenu = Content) relié à son index */
	void AddChoiceButton(UWidget* Content);

	/** Bouton carré encadré (couleur ou visage) ajouté à Row ; renvoie le cadre */
	/** Ajoute un bouton carré dans la grille (lignes de SwatchesPerRow, la dernière alignée à gauche) */
	UBorder* AddSwatchButton(UVerticalBox* Grid, int32 Group, int32 Index, const FLinearColor& ButtonColor, UTexture2D* Icon, UButton*& OutButton);

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

	/** Le champ du pseudo avait le focus à la dernière frame (clic dessus = pseudo effacé) */
	bool bNicknameHadFocus = false;

	/** Pseudo avant le clic : remis si on valide le champ vide */
	FText NicknameBeforeEdit;

	/** Colonnes centre (personnalisation) et droite (aperçu) */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> CustomizationColumn;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> PreviewColumn;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PreviewImage;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ColorRow;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> FaceRow;

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

	/** Boutons Droitier / Gaucher */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> HandednessButtons;
};
