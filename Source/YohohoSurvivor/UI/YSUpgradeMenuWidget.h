#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Upgrades/YSUpgrade.h"
#include "YSUpgradeMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UHorizontalBox;
class UVerticalBox;
class UYSUpgradeMenuWidget;

DECLARE_DELEGATE_OneParam(FYSOnUpgradeCardChosen, int32 /*OfferIndex*/);

/** Relie le clic d'une carte (sans paramètre) à son index ; INDEX_NONE = bouton « Relance », ValidateButtonIndex = « Valider » */
UCLASS()
class UYSUpgradeMenuButtonProxy : public UObject
{
	GENERATED_BODY()

public:

	int32 Index = INDEX_NONE;

	TWeakObjectPtr<UYSUpgradeMenuWidget> Menu;

	UFUNCTION()
	void HandleClicked();
};

/**
 *  Menu plein écran de montée de niveau : titre en haut, une grande carte cliquable par proposition (côte à côte ; un clic la
 *  sélectionne), « Joueurs prêts X / N » puis en bas au centre les boutons « Valider » (envoie la carte sélectionnée ; on peut
 *  en valider une autre tant que tous n'ont pas validé) et « Relance » (nouveau tirage pour ce joueur seulement, autant de fois
 *  qu'il veut ; caché pour un choix de classe).
 *  Carte, de haut en bas : nom, rareté, icône (carré de la couleur de la rareté + texture de l'upgrade), nombre de bonus,
 *  description, espace, puis par valeur modifiée son nom et une ligne ancienne valeur rayée / nouvelle plus grosse.
 *  Arbre construit en C++ ; placements réglables (catégorie Layout) dans un Widget Blueprint enfant
 *  (WBP_UpgradeMenu, Class Defaults) choisi par AYSPlayerController::UpgradeMenuClass.
 */
UCLASS(meta=(PrioritizeCategories="Layout"))
class UYSUpgradeMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	// ---- couleurs ----

	/** Voile derrière le menu */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Colors")
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);

	/** Fond des cartes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Colors")
	FLinearColor CardColor = FLinearColor(0.2f, 0.2f, 0.22f);

	/** Fond de la carte sélectionnée (cliquée, pas encore validée) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Colors")
	FLinearColor SelectedCardColor = FLinearColor(1.0f, 0.7f, 0.15f);

	/** Fond de la carte validée (choix envoyé, encore modifiable tant que tous n'ont pas validé) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Colors")
	FLinearColor ValidatedCardColor = FLinearColor(0.2f, 0.75f, 0.3f);

	/** Ancienne valeur (rayée) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Colors")
	FLinearColor OldValueColor = FLinearColor(0.65f, 0.65f, 0.65f);

	/** Nom de la valeur modifiée et nouvelle valeur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Colors")
	FLinearColor ValueColor = FLinearColor::White;

	/** Nombre de bonus (« Bonus 1 / 10 ») */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Colors")
	FLinearColor StackColor = FLinearColor(0.7f, 0.7f, 0.7f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Colors")
	FLinearColor DescriptionColor = FLinearColor(0.9f, 0.9f, 0.9f);

	// ---- titre ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Title")
	int32 TitleFontSize = 36;

	/** Marges autour du titre (haut = distance au bord de l'écran) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Title")
	FMargin TitlePadding = FMargin(0.0f, 32.0f, 0.0f, 24.0f);

	// ---- cartes ----

	/** Taille d'une carte */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Cards")
	FVector2D CardSize = FVector2D(440.0f, 640.0f);

	/** Espace horizontal entre deux cartes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Cards")
	float CardSpacing = 60.0f;

	/** Marges intérieures d'une carte */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Cards")
	FMargin CardPadding = FMargin(20.0f, 24.0f);

	/** Espace entre deux éléments de la carte (nom, rareté, icône, bonus, description) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Cards")
	float CardElementSpacing = 12.0f;

	/** Espace entre la description et les valeurs modifiées */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Cards")
	float StatsSpacing = 28.0f;

	/** Espace entre deux valeurs modifiées */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Cards")
	float StatLineSpacing = 14.0f;

	/** Nom de la valeur modifiée (« Dégât »), au-dessus de la ligne ancienne / nouvelle valeur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Cards")
	int32 StatLabelFontSize = 16;

	/** Espace entre le nom de la valeur et la ligne ancienne / nouvelle valeur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Cards")
	float StatLabelSpacing = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Cards")
	int32 NameFontSize = 28;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Cards")
	int32 RarityFontSize = 20;

	/** Côté du carré de l'icône */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Cards")
	float IconSize = 128.0f;

	/** Bord de couleur de rareté visible autour de la texture de l'icône */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Cards")
	float IconFramePadding = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Cards")
	int32 StackFontSize = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Cards")
	int32 DescriptionFontSize = 18;

	/** Ancienne valeur (rayée, à gauche) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Cards")
	int32 OldValueFontSize = 18;

	/** Nouvelle valeur (gras, plus grosse) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Cards")
	int32 NewValueFontSize = 24;

	/** Espace entre l'ancienne et la nouvelle valeur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Cards")
	float StatValueSpacing = 14.0f;

	// ---- bas de l'écran : joueurs prêts, relance ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Bottom")
	int32 StatusFontSize = 18;

	/** Marges autour de « Joueurs prêts X / N » */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Bottom")
	FMargin StatusPadding = FMargin(0.0f, 24.0f, 0.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Bottom")
	FVector2D RerollButtonSize = FVector2D(260.0f, 64.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Bottom")
	int32 RerollFontSize = 28;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Bottom")
	FLinearColor RerollButtonColor = FLinearColor(0.2f, 0.2f, 0.22f);

	/** Marges autour des boutons Valider / Relance (bas = distance au bord de l'écran) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Bottom")
	FMargin RerollPadding = FMargin(0.0f, 0.0f, 0.0f, 40.0f);

	/** Bouton « Valider » (à gauche de « Relance », les deux centrés ensemble) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Bottom")
	FVector2D ValidateButtonSize = FVector2D(260.0f, 64.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Bottom")
	int32 ValidateFontSize = 28;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Bottom")
	FLinearColor ValidateButtonColor = FLinearColor(0.2f, 0.75f, 0.3f);

	/** Couleur de « Valider » sans carte sélectionnée, ou quand la carte sélectionnée est déjà validée */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Bottom")
	FLinearColor ValidateDisabledColor = FLinearColor(0.2f, 0.2f, 0.22f);

	/** Espace entre « Valider » et « Relance » */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Bottom")
	float BottomButtonSpacing = 40.0f;

	/** Appelé quand le joueur valide une carte (index de la proposition) */
	FYSOnUpgradeCardChosen OnOfferChosen;

	/** Appelé quand le joueur clique sur « Relance » */
	FSimpleDelegate OnRerollClicked;

	virtual bool Initialize() override;

	void SetTitle(const FText& Title);

	/** Remplace les cartes */
	void SetCards(const TArray<FYSUpgradeCard>& Cards);

	/** Carte sélectionnée sur cette machine (cliquée, pas encore validée ; INDEX_NONE = aucune) */
	void SetSelectedIndex(int32 Index);

	/** Carte validée (choix reçu du serveur ; INDEX_NONE = aucune) */
	void SetValidatedIndex(int32 Index);

	void SetStatus(const FText& Status);

	/** Bouton « Relance » visible (pas pour un choix de classe) ; caché, « Valider » reste seul au centre */
	void SetRerollVisible(bool bVisible);

	/** Clic reçu d'un bouton (via son proxy) */
	void NotifyButtonClicked(int32 Index);

	/** Index du proxy du bouton « Valider » (INDEX_NONE = « Relance », 0..N-1 = cartes) */
	static constexpr int32 ValidateButtonIndex = -2;

private:

	void BuildWidgetTree();

	/** Contenu d'une carte */
	UWidget* MakeCardContent(const FYSUpgradeCard& Card);

	UTextBlock* MakeText(const FText& Text, int32 FontSize, const FLinearColor& Color, bool bBold, bool bWrap) const;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> CardRow;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> RerollRow;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> CardButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UYSUpgradeMenuButtonProxy>> ButtonProxies;

	UPROPERTY(Transient)
	TObjectPtr<UYSUpgradeMenuButtonProxy> RerollProxy;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ValidateButton;

	UPROPERTY(Transient)
	TObjectPtr<UYSUpgradeMenuButtonProxy> ValidateProxy;

	int32 SelectedIndex = INDEX_NONE;
	int32 ValidatedIndex = INDEX_NONE;

	/** Couleurs des cartes et du bouton « Valider » selon la sélection et la validation */
	void RefreshSelection();
};
