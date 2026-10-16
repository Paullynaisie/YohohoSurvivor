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

	virtual bool Initialize() override;

private:

	void BuildWidgetTree();
};
