#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

/**
 *  Panneau Details des actions d'arme (UYSAbilityComponent et sous-classes) : cache les propriétés héritées que la classe
 *  n'utilise pas (UYSAbilityComponent::GetUnusedProperties), ex. réglages de tir de la Roll and Bomb, Windup / Swing Duration
 *  du Dashing Slash. Les valeurs restent dans les assets, elles ne sont juste plus affichées.
 */
class FYSAbilityDetails : public IDetailCustomization
{
public:

	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

#endif
