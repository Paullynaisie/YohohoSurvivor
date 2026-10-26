#include "EditorTools/YSAbilityDetails.h"

#if WITH_EDITOR

#include "Abilities/YSAbilityComponent.h"
#include "DetailLayoutBuilder.h"

TSharedRef<IDetailCustomization> FYSAbilityDetails::MakeInstance()
{
	return MakeShared<FYSAbilityDetails>();
}

void FYSAbilityDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	// plusieurs actions sélectionnées : on ne cache que ce qu'aucune n'utilise
	TArray<FName> Hidden;
	bool bFirst = true;
	for (const TWeakObjectPtr<UObject>& Object : Objects)
	{
		const UYSAbilityComponent* Action = Cast<UYSAbilityComponent>(Object.Get());
		if (!Action)
		{
			return;
		}
		TArray<FName> Unused;
		Action->GetUnusedProperties(Unused);
		if (bFirst)
		{
			Hidden = MoveTemp(Unused);
			bFirst = false;
		}
		else
		{
			Hidden.RemoveAll([&Unused](const FName& Name) { return !Unused.Contains(Name); });
		}
	}
	if (bFirst)
	{
		return;
	}

	const UClass* Class = Objects[0]->GetClass();
	for (const FName& Name : Hidden)
	{
		// la propriété est demandée à la classe qui la déclare (propriété héritée)
		const FProperty* Property = FindFProperty<FProperty>(Class, Name);
		if (!Property)
		{
			UE_LOG(LogTemp, Warning, TEXT("[YSAbilityDetails] %s : propriété %s introuvable"), *Class->GetName(), *Name.ToString());
			continue;
		}
		const TSharedRef<IPropertyHandle> Handle = DetailBuilder.GetProperty(Name, Property->GetOwnerClass());
		if (Handle->IsValidHandle())
		{
			DetailBuilder.HideProperty(Handle);
		}
	}
}

#endif
