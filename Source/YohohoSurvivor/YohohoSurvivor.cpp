// Copyright Epic Games, Inc. All Rights Reserved.

#include "YohohoSurvivor.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Abilities/YSAbilityComponent.h"
#include "EditorTools/YSAbilityDetails.h"
#include "PropertyEditorModule.h"
#endif

/** Module du jeu : en éditeur, enregistre les customisations du panneau Details */
class FYohohoSurvivorModule : public FDefaultGameModuleImpl
{
public:

	virtual void StartupModule() override
	{
#if WITH_EDITOR
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		// actions d'arme : propriétés héritées inutilisées cachées (s'applique aussi aux sous-classes)
		PropertyEditor.RegisterCustomClassLayout(UYSAbilityComponent::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FYSAbilityDetails::MakeInstance));
#endif
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
		if (FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
		{
			PropertyEditor->UnregisterCustomClassLayout(UYSAbilityComponent::StaticClass()->GetFName());
		}
#endif
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE( FYohohoSurvivorModule, YohohoSurvivor, "YohohoSurvivor" );

DEFINE_LOG_CATEGORY(LogYohohoSurvivor)
