#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSDeathScreenWidget.generated.h"

/**
 *  Écran plein écran affiché quand le personnage du joueur est mort (AYSPlayerState::IsDead).
 *  Construit en C++ (pas de Widget Blueprint) ; affiché / caché par AYSPlayerController.
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

	virtual bool Initialize() override;

	/** Mort pendant le ragdoll (titre + voile) ou spectateur (message au milieu + joueur regardé) */
	void SetSpectating(bool bSpectating, const FText& TargetName);

private:

	void BuildWidgetTree();

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
