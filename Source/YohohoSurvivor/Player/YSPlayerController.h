#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "YSPlayerController.generated.h"

class UYSAbilityIconWidget;
class UYSCharacterSelectWidget;
class UYSDeathScreenWidget;
class ACameraActor;

/**
 *  Contrôleur joueur : affiche un curseur de visée, oriente le pawn vers le point visé au sol
 *  et affiche l'icône de recharge de l'action de la main principale (droite, sinon gauche).
 *  Tant que le joueur n'a pas de personnage, affiche le menu de sélection du personnage.
 *  La visée est calculée localement puis envoyée au serveur avec chaque mouvement
 *  (rotation de contrôle transmise par le CharacterMovementComponent).
 */
UCLASS()
class AYSPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Point visé au sol par la souris (valide uniquement sur le client local) */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Aim")
	FVector AimLocation = FVector::ZeroVector;

	/** Widget de l'icône de capacité (en bas au centre de l'écran) */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UYSAbilityIconWidget> AbilityIconClass;

	/** Distance entre l'icône et le bas de l'écran */
	UPROPERTY(EditAnywhere, Category="UI")
	float AbilityIconBottomMargin = 40.0f;

private:

	UPROPERTY(Transient)
	TObjectPtr<UYSAbilityIconWidget> AbilityIcon;

	/** Menu de sélection du personnage, affiché tant que le joueur n'a pas de personnage */
	UPROPERTY(Transient)
	TObjectPtr<UYSCharacterSelectWidget> CharacterSelectMenu;

	/** Écran « You're dead », affiché quand le PlayerState est mort */
	UPROPERTY(Transient)
	TObjectPtr<UYSDeathScreenWidget> DeathScreen;

	/** Caméra fixe laissée à l'endroit de la mort (locale, non répliquée) */
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> DeathCamera;

	/** Choix local (affiché immédiatement, confirmé par le serveur via le PlayerState) */
	int32 LocalSelectedCharacter = INDEX_NONE;

	/** Marche de debug en cours (YSDebugWalk) */
	FVector DebugWalkDirection = FVector::ZeroVector;
	float DebugWalkRemaining = 0.0f;

public:

	AYSPlayerController();

	virtual void BeginPlay() override;

	virtual void SetupInputComponent() override;

	virtual void PlayerTick(float DeltaTime) override;

	/** Quitte la partie et revient au menu principal (touche Échap). Hôte : les autres joueurs reviennent aussi au menu. */
	UFUNCTION(Exec)
	void YSLeaveGame();

	FVector GetAimLocation() const { return AimLocation; }

	/**
	 *  Debug (console « ² ») : inflige Amount dégâts à tous les NME après Delay secondes.
	 *  bScreenshot = 1 prend une capture (avec UI) juste après.  Ex : YSDamageFoes 3 0
	 */
	UFUNCTION(Exec)
	void YSDamageFoes(float Amount = 3.0f, float Delay = 0.0f, bool bScreenshot = false);

	/**
	 *  Debug (console « ² ») : fait marcher le pawn dans la direction (X = nord, Y = est) pendant Duration secondes,
	 *  en prenant Shots captures (avec UI) réparties sur la marche.  Ex : YSDebugWalk 1 -1 2 4  (nord-ouest)
	 */
	UFUNCTION(Exec)
	void YSDebugWalk(float X = 1.0f, float Y = -1.0f, float Duration = 2.0f, int32 Shots = 0);

	/** Debug (console) : choisit un personnage comme un clic dans le menu.  Ex : YSSelectCharacter 1 */
	UFUNCTION(Exec)
	void YSSelectCharacter(int32 CharacterIndex);

	/** Debug (console) : inflige Amount dégâts à son propre personnage.  Ex : YSDamageSelf 25 */
	UFUNCTION(Exec)
	void YSDamageSelf(float Amount = 5.0f);

	/** Envoyé par le serveur juste avant la destruction du personnage mort : la caméra reste sur place */
	UFUNCTION(Client, Reliable)
	void ClientPlayerDied(FVector CameraLocation, FRotator CameraRotation, float CameraFOV);

protected:

	/** Envoie le choix de personnage au serveur */
	UFUNCTION(Server, Reliable)
	void ServerSelectCharacter(int32 CharacterIndex);

	/** Met à jour le menu de sélection (boutons, illumination, état, visibilité) */
	void UpdateCharacterSelectMenu();

	UFUNCTION(Server, Reliable)
	void ServerDamageFoes(float Amount);

	UFUNCTION(Server, Reliable)
	void ServerDamageSelf(float Amount);

	/** Applique les dégâts de debug (serveur) */
	void DamageAllFoes(float Amount);


	/** Projette la souris sur le plan horizontal du pawn et tourne la rotation de contrôle vers ce point */
	void UpdateAim();
};
