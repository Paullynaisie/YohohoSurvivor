#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSPlayerHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 *  HUD du joueur (haut gauche) : barre de PV, niveau de l'équipe et barre d'XP.
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

	virtual bool Initialize() override;

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
};
