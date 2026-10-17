#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSPlayerHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UHorizontalBox;
class UVerticalBox;
class UTexture2D;
class UYSPlayerIconWidget;

/** Infos d'un joueur pour le HUD (icône, pseudo, PV) */
struct FYSHUDPlayerInfo
{
	FText Name;
	FLinearColor Color = FLinearColor::White;
	UTexture2D* Face = nullptr;
	float Health = 0.0f;
	float MaxHealth = 0.0f;
	bool bAlive = false;
};

/**
 *  HUD du joueur (haut gauche) : icône (couleur + visage), pseudo, barre de PV, niveau de l'équipe et barre d'XP ;
 *  en dessous, les autres joueurs de la partie (icône, pseudo, PV).
 *  Construit en C++ (pas de Widget Blueprint nécessaire) ; rafraîchi par AYSPlayerController.
 */
UCLASS()
class UYSPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Taille de la barre de PV */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FVector2D HealthBarSize = FVector2D(320.0f, 26.0f);

	/** Taille de la barre d'XP */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FVector2D ExperienceBarSize = FVector2D(320.0f, 16.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FLinearColor HealthColor = FLinearColor(0.85f, 0.05f, 0.05f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FLinearColor ExperienceColor = FLinearColor(0.1f, 0.45f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FLinearColor BarBackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);

	/** Icône du joueur (rond de couleur + visage) à gauche des barres : diamètre en part de la largeur de l'écran */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD|Icons", meta=(ClampMin="0.01", ClampMax="0.3"))
	float SelfIconScreenRatio = 0.05f;

	/** Espace entre l'icône du joueur et ses barres */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD|Icons")
	float SelfIconSpacing = 12.0f;

	/** Autres joueurs (sous les infos du joueur) : espace avant la liste, entre deux joueurs, taille de l'icône et de la barre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD|Allies")
	float AlliesTopSpacing = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD|Allies")
	float AllySpacing = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD|Allies")
	float AllyIconSize = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD|Allies")
	FVector2D AllyBarSize = FVector2D(200.0f, 14.0f);

	virtual bool Initialize() override;

	/** Icône du joueur (taille = SelfIconScreenRatio × largeur de l'écran en unités Slate) */
	void SetSelfIcon(const FLinearColor& Color, UTexture2D* Face, float ViewportWidth);

	/** Autres joueurs de la partie (lignes recréées si leur nombre change) */
	void SetAllies(const TArray<FYSHUDPlayerInfo>& Allies);

	/** PV du personnage (bHasCharacter = false : barre de PV cachée) */
	void SetHealth(bool bHasCharacter, float Health, float MaxHealth);

	/** Niveau et XP de l'équipe */
	void SetLevel(int32 Level, float Experience, float ExperienceToNextLevel);

	/** Pseudo du joueur (au-dessus de la barre de PV) */
	void SetPlayerName(const FText& Name);

private:

	void BuildWidgetTree();

	/** Crée une barre (fond + remplissage + texte centré) ; renvoie le conteneur */
	UWidget* CreateBar(const FName& Name, const FVector2D& Size, const FLinearColor& FillColor, int32 FontSize, UProgressBar*& OutBar, UTextBlock*& OutLabel);

	UPROPERTY(Transient)
	TObjectPtr<UWidget> HealthGroup;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameLabel;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LevelLabel;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ExperienceBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ExperienceLabel;

	UPROPERTY(Transient)
	TObjectPtr<UYSPlayerIconWidget> SelfIcon;

	/** Liste des autres joueurs et ses lignes (icône, pseudo, barre) */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> AlliesList;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UYSPlayerIconWidget>> AllyIcons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> AllyNames;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> AllyBars;
};
