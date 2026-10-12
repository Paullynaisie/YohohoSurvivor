#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSAbilityIconWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;
class UYSAbilityComponent;

/**
 *  Icône carrée d'une capacité avec son temps de recharge :
 *  un voile sombre qui se vide de haut en bas et les secondes restantes.
 *  Construite entièrement en C++ (pas de Widget Blueprint nécessaire) ;
 *  mise à jour par AYSPlayerController via Refresh().
 */
UCLASS()
class UYSAbilityIconWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Taille du carré en pixels */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability Icon")
	float IconSize = 96.0f;

	/** Couleur du voile de recharge */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability Icon")
	FLinearColor CooldownColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.75f);

	/** Taille du texte des secondes restantes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ability Icon")
	int32 FontSize = 28;

	/** Met à jour l'icône et la recharge à partir de la capacité (nullptr = caché) */
	void Refresh(const UYSAbilityComponent* Ability);

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;

private:

	void BuildWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UImage> IconImage;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> CooldownBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CooldownText;
};
