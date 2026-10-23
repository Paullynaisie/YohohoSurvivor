#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSHealthBarWidget.generated.h"

class UImage;
class UProgressBar;
class USizeBox;
class UTextBlock;

/**
 *  Barre de vie au-dessus d'un NME : remplissage proportionnel aux PV,
 *  fond pour la partie vide et texte « X / Y » par-dessus. Cachée quand les PV sont au maximum.
 *  Au-dessus de la barre, un point critique quand la prochaine balle d'attaque auto du joueur local sur ce NME sera critique (Atk critique).
 *  Arbre construit en C++ ; un Widget Blueprint enfant (WBP_FoeHealthBar) sert aux réglages (Class Defaults, catégorie Layout).
 */
UCLASS()
class UYSHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Taille de la barre en pixels */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FVector2D BarSize = FVector2D(120.0f, 16.0f);

	/** Couleur de la partie remplie */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor FillColor = FLinearColor(0.85f, 0.05f, 0.05f);

	/** Couleur de la partie vide */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor BackgroundColor = FLinearColor::Black;

	/** Taille du texte « X / Y » */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	int32 FontSize = 10;

	/** Diamètre du point critique (pixels) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Critical")
	float CriticalDotSize = 12.0f;

	/** Couleur du point critique */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Critical")
	FLinearColor CriticalDotColor = FLinearColor(1.0f, 0.75f, 0.05f);

	/** Couleur et épaisseur du contour du point */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Critical")
	FLinearColor CriticalDotOutlineColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Critical", meta=(ClampMin="0"))
	float CriticalDotOutlineWidth = 1.5f;

	/** Espace entre le point et la barre (pixels) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Critical")
	float CriticalDotSpacing = 3.0f;

	/** Décalage horizontal du point depuis le centre de la barre (pixels) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Critical")
	float CriticalDotOffsetX = 0.0f;

	virtual bool Initialize() override;

	/** Met à jour la barre ; cachée si Current >= Max (ou PV pas encore connus) */
	void SetHealth(float Current, float Max);

	/** Affiche le point critique (prochaine balle du joueur local critique) */
	void SetCriticalReady(bool bReady);

private:

	void BuildWidgetTree();

	/** Visibilité de la barre, du point et du widget */
	void RefreshVisibility();

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> BarBox;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;

	UPROPERTY(Transient)
	TObjectPtr<UImage> CriticalDot;

	bool bBarVisible = false;
	bool bCriticalReady = false;
};
