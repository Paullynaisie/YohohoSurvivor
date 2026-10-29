#pragma once

#include "CoreMinimal.h"

class UUserWidget;
class UWidget;

/**
 *  Manette : dernier périphérique utilisé (manette ou clavier / souris), suivi pour toute l'application par un pré-processeur
 *  d'entrées Slate (voit aussi les entrées pendant la pause et dans les menus). Les menus s'en servent pour donner le focus
 *  à un bouton (navigation au stick gauche, A = valider) seulement quand on joue à la manette.
 */
namespace YSGamepad
{
	/** Enregistre le suivi (UYSGameInstance::Init) */
	void Register();

	/** Retire le suivi (UYSGameInstance::Shutdown) */
	void Unregister();

	/** Dernière entrée = manette (bouton, gâchette ou stick au-delà de la zone morte) */
	bool IsUsingGamepad();

	/**
	 *  À appeler dans le NativeTick d'un menu visible : à la manette, donne le focus à Preferred (sinon au premier bouton visible et
	 *  actif du menu) si rien du menu ne l'a, et grossit le bouton qui a le focus de FocusScale (1 = rien) pour le voir
	 */
	void UpdateMenuFocus(UUserWidget* Menu, UWidget* Preferred, float FocusScale);

	/** Rend le focus au jeu (menu caché alors qu'un de ses boutons l'avait : A doit de nouveau interagir) */
	void ReturnFocusToGame();
}
