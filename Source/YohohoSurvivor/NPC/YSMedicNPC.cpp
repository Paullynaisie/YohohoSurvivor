#include "NPC/YSMedicNPC.h"
#include "Core/YSGameMode.h"
#include "Engine/World.h"
#include "Interaction/YSPlayerCorpse.h"

AYSMedicNPC::AYSMedicNPC()
{
	BrookerName = NSLOCTEXT("YS", "MedicName", "Medic");
	IndicatorColor = FLinearColor(0.15f, 0.85f, 0.25f);
	ZoneColor = FLinearColor(0.15f, 0.85f, 0.25f);
}

bool AYSMedicNPC::AcceptsObject(const AYSInteractiveObject* Object) const
{
	return Object && Object->IsA<AYSPlayerCorpse>();
}

void AYSMedicNPC::ReceiveObject(AYSInteractiveObject* Object)
{
	AYSGameMode* GameMode = GetWorld()->GetAuthGameMode<AYSGameMode>();
	AYSPlayerCorpse* Corpse = Cast<AYSPlayerCorpse>(Object);
	if (!GameMode || !Corpse)
	{
		return;
	}

	// là où le cadavre a été posé ; dans l'eau ou sans sol : à côté du Medic, du côté du cadavre
	FVector Location = AYSInteractiveObject::FindGround(GetWorld(), Corpse->GetActorLocation(), Corpse);
	if (Location.Equals(Corpse->GetActorLocation()) || !AYSInteractiveObject::IsSafeDropLocation(GetWorld(), Location))
	{
		const FVector Side = (Corpse->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		Location = GetActorLocation() + (Side.IsNearlyZero() ? GetActorForwardVector() : Side) * ReviveDistance;
		Location = AYSInteractiveObject::FindGround(GetWorld(), Location, this);
	}

	UE_LOG(LogTemp, Log, TEXT("[YSMedic] %s reçoit %s"), *GetName(), *Corpse->GetName());
	GameMode->RevivePlayer(Corpse, Location, ReviveHealthRatio);
}
