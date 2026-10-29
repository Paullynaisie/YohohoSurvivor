#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSEliteHealthWidget.generated.h"

class UProgressBar;
class USizeBox;
class UVerticalBox;
class UTextBlock;

/**
 *  Barre de vie de l'élite dans le HUD (haut droite, symétrique des PV du joueur) : nom + barre « X / Y ».
 *  Affichée par AYSPlayerController quand l'élite est hors de l'écran (sinon sa barre est au-dessus de sa tête).
 *  Sous les PV : bouclier de boss et rage (Boss armor). Arbre construit en C++ ; réglages dans WBP_EliteHealth (Layout).
 */
UCLASS()
class UYSEliteHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FVector2D BarSize = FVector2D(320.0f, 26.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FLinearColor BarColor = FLinearColor(0.6f, 0.05f, 0.75f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FLinearColor BarBackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);

	/** Bouclier de boss : barre sous les PV (cachée sans bouclier) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Boss")
	FVector2D ShieldBarSize = FVector2D(320.0f, 18.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Boss")
	FLinearColor ShieldBarColor = FLinearColor(0.2f, 0.75f, 1.0f);

	/** Barre de rage (Boss armor) sous les PV, cachée sous bouclier */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Boss")
	FVector2D RageBarSize = FVector2D(320.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Boss")
	FLinearColor RageBarColor = FLinearColor(1.0f, 0.35f, 0.02f);

	/** Espace entre les barres (pixels) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Boss")
	float BossBarSpacing = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Boss")
	int32 BossBarFontSize = 11;

	virtual bool Initialize() override;

	void SetElite(const FText& Name, float Health, float MaxHealth);

	/** Boss armor : bouclier (0 = caché) et rage (MaxRage 0 = pas de Boss armor : cachée) */
	void SetBossState(float Shield, float MaxShield, float Rage, float MaxRage);

private:

	void BuildWidgetTree();

	/** Barre supplémentaire (bouclier, rage) avec son texte */
	UProgressBar* AddBossBar(UVerticalBox* Column, const FVector2D& Size, const FLinearColor& Color, USizeBox*& OutBox, UTextBlock*& OutLabel);

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> ShieldBox;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ShieldBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ShieldLabel;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> RageBox;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> RageBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RageLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameLabel;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ValueLabel;
};
