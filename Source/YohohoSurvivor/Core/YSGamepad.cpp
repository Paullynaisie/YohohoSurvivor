#include "Core/YSGamepad.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

namespace YSGamepad
{
	/** Zone morte des sticks et gâchettes pour passer en mode manette */
	constexpr float AnalogThreshold = 0.3f;

	bool bUsingGamepad = false;

	class FYSGamepadInputProcessor : public IInputProcessor
	{
	public:

		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
		{
			bUsingGamepad = InKeyEvent.GetKey().IsGamepadKey();
			return false;
		}

		virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override
		{
			if (InAnalogInputEvent.GetKey().IsGamepadKey() && FMath::Abs(InAnalogInputEvent.GetAnalogValue()) > AnalogThreshold)
			{
				bUsingGamepad = true;
			}
			return false;
		}

		// le déplacement de la souris ne compte pas : la visée au stick droit déplace elle-même le curseur
		virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
		{
			bUsingGamepad = false;
			return false;
		}

		virtual const TCHAR* GetDebugName() const override { return TEXT("YSGamepad"); }
	};

	TSharedPtr<FYSGamepadInputProcessor> Processor;
}

void YSGamepad::Register()
{
	if (!Processor && FSlateApplication::IsInitialized())
	{
		Processor = MakeShared<FYSGamepadInputProcessor>();
		FSlateApplication::Get().RegisterInputPreProcessor(Processor);
	}
}

void YSGamepad::Unregister()
{
	if (Processor && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
	}
	Processor.Reset();
}

bool YSGamepad::IsUsingGamepad()
{
	return bUsingGamepad;
}

void YSGamepad::UpdateMenuFocus(UUserWidget* Menu, UWidget* Preferred, float FocusScale)
{
	if (!Menu || !Menu->WidgetTree)
	{
		return;
	}

	TArray<UButton*> Buttons;
	Menu->WidgetTree->ForEachWidget([&Buttons](UWidget* Widget)
	{
		if (UButton* Button = Cast<UButton>(Widget))
		{
			Buttons.Add(Button);
		}
	});

	if (bUsingGamepad && !Menu->HasFocusedDescendants())
	{
		UWidget* Target = Preferred && Preferred->IsVisible() && Preferred->GetIsEnabled() ? Preferred : nullptr;
		for (int32 Index = 0; !Target && Index < Buttons.Num(); ++Index)
		{
			// bouton affiché (lui et ses parents) et actif
			if (Buttons[Index]->GetIsEnabled() && Buttons[Index]->GetCachedWidget() && Buttons[Index]->GetCachedGeometry().GetLocalSize().X > 0.0f)
			{
				Target = Buttons[Index];
			}
		}
		if (Target)
		{
			Target->SetKeyboardFocus();
		}
	}

	// bouton qui a le focus grossi (à la manette seulement : à la souris, le survol suffit)
	for (UButton* Button : Buttons)
	{
		const float Scale = bUsingGamepad && Button->HasKeyboardFocus() ? FocusScale : 1.0f;
		if (!FMath::IsNearlyEqual(Button->GetRenderTransform().Scale.X, Scale))
		{
			Button->SetRenderScale(FVector2D(Scale));
		}
	}
}

void YSGamepad::ReturnFocusToGame()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}
