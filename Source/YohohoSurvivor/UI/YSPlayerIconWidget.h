#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSPlayerIconWidget.generated.h"

class UImage;
class USizeBox;
class UTexture2D;

/**
 *  Icône d'un joueur : un rond de sa couleur avec la texture de son visage par-dessus.
 *  Utilisée dans le HUD (joueur, autres joueurs) et par les indicateurs des joueurs hors écran.
 *  Construit en C++ (pas de Widget Blueprint).
 */
UCLASS()
class UYSPlayerIconWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Part du diamètre occupée par le visage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Icon", meta=(ClampMin="0.1", ClampMax="1"))
	float FaceScale = 0.8f;

	virtual bool Initialize() override;

	/** Diamètre (unités Slate) */
	void SetIconSize(float Size);

	/** Couleur du rond et visage (nullptr = pas de visage) */
	void SetIcon(const FLinearColor& Color, UTexture2D* Face);

private:

	void BuildWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> SizeBox;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> FaceBox;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Circle;

	UPROPERTY(Transient)
	TObjectPtr<UImage> FaceImage;

	float CurrentSize = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CurrentFace;
};
