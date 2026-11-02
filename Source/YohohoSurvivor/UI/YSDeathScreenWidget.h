#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSDeathScreenWidget.generated.h"

/**
 *  Écran plein écran affiché quand le personnage du joueur est mort (AYSPlayerState::IsDead).
 *  Arbre construit en C++ ; réglages (Layout) dans WBP_DeathScreen (Class Defaults) choisi par AYSPlayerController::DeathScreenClass, qui l'affiche / le cache.
 */
UCLASS()
class UYSDeathScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Texte principal */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death Screen")
	FText Title = NSLOCTEXT("YS", "DeathTitle", "You're dead");

	/** Ligne sous le titre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death Screen")
	FText Hint = NSLOCTEXT("YS", "DeathHint", "Échap : retour au menu");

	/** Couleur du titre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death Screen")
	FLinearColor TitleColor = FLinearColor(1.0f, 0.15f, 0.1f);

	/** Voile par-dessus le jeu (la partie continue derrière) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death Screen")
	FLinearColor BackgroundColor = FLinearColor(0.08f, 0.0f, 0.0f, 0.6f);

	/** Spectateur : ligne sous le message ({0} = joueur regardé) ; le titre et le voile sont cachés pour bien voir le jeu */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Spectate")
	FText SpectateFormat = NSLOCTEXT("YS", "DeathSpectate", "Spectateur : {0}   (Q / D : changer de joueur)");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Spectate")
	FLinearColor SpectateColor = FLinearColor(1.0f, 0.85f, 0.4f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Spectate", meta=(ClampMin="1"))
	int32 SpectateFontSize = 18;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Spectate", meta=(ClampMin="1"))
	int32 SpectateHintFontSize = 28;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	int32 TitleFontSize = 72;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	int32 HintFontSize = 20;

	/** Bouton de réanimation (plume de phoenix), sous la ligne du spectateur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Revive")
	FText ReviveText = NSLOCTEXT("YS", "DeathRevive", "Revive");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Revive")
	FVector2D ReviveButtonSize = FVector2D(260.0f, 64.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Revive")
	FLinearColor ReviveButtonColor = FLinearColor(1.0f, 0.55f, 0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Revive", meta=(ClampMin="1"))
	int32 ReviveFontSize = 28;

	/** Espace entre la ligne du spectateur et le bouton */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Revive")
	float ReviveTopSpacing = 16.0f;

	/** Clic sur le bouton de réanimation */
	FSimpleDelegate OnReviveClicked;

	virtual bool Initialize() override;

	/** Mort pendant le ragdoll (titre + voile) ou spectateur (message au milieu + joueur regardé) */
	void SetSpectating(bool bSpectating, const FText& TargetName);

	/** Bouton de réanimation affiché (spectateur, plume en stock) */
	void SetReviveAvailable(bool bAvailable);

private:

	void BuildWidgetTree();

	UFUNCTION()
	void HandleReviveClicked();

	UPROPERTY(Transient)
	TObjectPtr<class UWidget> ReviveBox;

	UPROPERTY(Transient)
	TObjectPtr<class UImage> BackgroundImage;

	UPROPERTY(Transient)
	TObjectPtr<class UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<class UTextBlock> HintText;

	UPROPERTY(Transient)
	TObjectPtr<class UTextBlock> SpectateText;

	bool bShownSpectating = false;
	FText ShownTargetName;
};
