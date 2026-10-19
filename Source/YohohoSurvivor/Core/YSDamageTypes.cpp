#include "Core/YSDamageTypes.h"
#include "Engine/DamageEvents.h"

bool UYSCriticalDamageType::IsCritical(const FDamageEvent& DamageEvent)
{
	return DamageEvent.DamageTypeClass && DamageEvent.DamageTypeClass->IsChildOf(UYSCriticalDamageType::StaticClass());
}
