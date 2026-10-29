#include "Abilities/YSDragonBreathAbility.h"

UYSDragonBreathAbility::UYSDragonBreathAbility()
{
	DisplayName = NSLOCTEXT("YS", "DragonBreath", "Dragon breath");
	Damage = 35.0f;
	Cooldown = 10.0f;
	bSpreadFire = true;
	ShotgunAmmoPerShot = 7;
	ShotgunSpread = 30.0f;
	// pas la dégressivité des plombs du Shotgun (réglable)
	SpreadDamageFalloff = 0.0f;
	// recul de 0.25 s, puis l'attaque auto est rechargée
	RecoilKickDuration = 0.05f;
	RecoilRecoverDuration = 0.2f;
	bResetAutoAttackOnUse = true;
	AutoAttackResetDelay = 0.25f;
	IconTint = FLinearColor(1.0f, 0.35f, 0.05f);
}
