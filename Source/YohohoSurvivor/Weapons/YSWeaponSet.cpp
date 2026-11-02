#include "Weapons/YSWeaponSet.h"
#include "Player/YSPlayerCharacter.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
void UYSWeaponSet::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// le Data Asset ne reconstruit pas les personnages : ceux de l'éditeur (Blueprint ouvert, map) sont remis en pose à la main
	for (TObjectIterator<AYSPlayerCharacter> It; It; ++It)
	{
		const UWorld* World = It->IsTemplate() ? nullptr : It->GetWorld();
		if (World && !World->IsGameWorld() && It->GetWeaponSet() == this)
		{
			It->RefreshShowcasePose();
		}
	}
}
#endif
