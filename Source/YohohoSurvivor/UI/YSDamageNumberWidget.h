#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "YSDamageNumberWidget.generated.h"

class UTextBlock;

/**
 *  Texte d'un chiffre de dégâts (contour noir pour rester lisible sur tous les fonds).
 *  Affiché par AYSDamageNumber. Construit en C++ (pas de Widget Blueprint nécessaire).
 */
UCLASS()
class UYSDamageNumberWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	virtual bool Initialize() override;

	void SetDamage(const FText& Text, const FLinearColor& Color, int32 FontSize);

private:

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;
};
