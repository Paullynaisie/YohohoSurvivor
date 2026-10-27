#include "Ship/YSLongboatLauncher.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Player/YSPlayerCharacter.h"
#include "Ship/YSLongboat.h"
#include "Ship/YSPirateShip.h"
#include "Ship/YSShipHelm.h"

AYSLongboatLauncher::AYSLongboatLauncher()
{
	DisplayName = NSLOCTEXT("YS", "LongboatLauncherName", "Chalouperie");
	MaxHealth = 1000.0f;
	LongboatClass = TSoftClassPtr<AYSLongboat>(FSoftObjectPath(TEXT("/Game/Placeable/Ship/BP_Longboat.BP_Longboat_C")));
}

void AYSLongboatLauncher::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYSLongboatLauncher, ActiveLongboat);
}

bool AYSLongboatLauncher::CanInteract(const AYSPlayerCharacter* Character) const
{
	return !IsValid(ActiveLongboat) && Super::CanInteract(Character);
}

void AYSLongboatLauncher::Interact(AYSPlayerCharacter* Character)
{
	AYSPirateShip* Ship = GetShip();
	if (!HasAuthority() || !CanInteract(Character) || !Ship)
	{
		return;
	}

	// sur l'eau (hauteur de la ligne de flottaison du bateau), derrière la barre
	const AActor* Helm = Ship->GetHelm();
	const FVector Back = -Ship->GetActorForwardVector().GetSafeNormal2D();
	FVector Location = (Helm ? Helm->GetActorLocation() : GetActorLocation()) + Back * SpawnDistanceBehindHelm;
	Location.Z = Ship->GetActorLocation().Z;
	const float Yaw = Ship->GetActorRotation().Yaw + (bSpawnFacingAway ? 180.0f : 0.0f);

	UClass* SpawnClass = LongboatClass.LoadSynchronous();
	if (!SpawnClass)
	{
		SpawnClass = AYSLongboat::StaticClass();
		UE_LOG(LogTemp, Warning, TEXT("[YSLongboat] %s : Longboat Class introuvable (%s), classe C++ sans visuel"), *GetName(), *LongboatClass.ToString());
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AYSLongboat* Longboat = GetWorld()->SpawnActor<AYSLongboat>(SpawnClass, Location, FRotator(0.0f, Yaw, 0.0f), Params);
	if (!Longboat)
	{
		return;
	}
	Longboat->SetMotherShip(Ship);
	ActiveLongboat = Longboat;
	ForceNetUpdate();
	SetPromptVisible(false);
	UE_LOG(LogTemp, Log, TEXT("[YSLongboat] %s : %s lance %s en %s"), *GetName(), *Character->GetName(), *Longboat->GetName(), *Location.ToCompactString());

	// le joueur est à la barre de la chaloupe (téléporté dessus)
	if (AYSShipHelm* LongboatHelm = Longboat->GetHelm())
	{
		Character->EnterStation(LongboatHelm);
	}
}
