#pragma once

#include "NativeGameplayTags.h"

/**
 *  Gameplay Tags natifs du jeu (visibles dans les sélecteurs de tags de l'éditeur).
 *  Les acteurs exposent leurs tags via IGameplayTagAssetInterface ;
 *  les projectiles s'en servent pour décider qui ils touchent.
 */
namespace YSTags
{
	/** Camp des joueurs */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Team_Player);

	/** Camp des ennemis */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Team_Foe);
}
