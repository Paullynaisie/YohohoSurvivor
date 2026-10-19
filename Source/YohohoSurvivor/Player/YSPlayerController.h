#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Upgrades/YSUpgrade.h"
#include "YSPlayerController.generated.h"

class UYSAbilityIconWidget;
class UYSBloodlustStacksWidget;
class UYSCharacterSelectWidget;
class UYSPlayerHUDWidget;
class UYSEliteHealthWidget;
class UYSDayBannerWidget;
class UYSOffscreenIndicatorsWidget;
class UYSObjectiveWidget;
class ASceneCapture2D;
class AYSPlayerCharacter;
class UTextureRenderTarget2D;
class APointLight;
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

	/** Distance entre les icônes des joueurs hors écran et le bord de l'écran */
	UPROPERTY(EditAnywhere, Category="UI")
	float OffscreenIndicatorMargin = 12.0f;

	/** Écran de sélection : Widget Blueprint enfant de YSCharacterSelectWidget (placements dans ses Class Defaults, catégorie Layout) */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UYSCharacterSelectWidget> CharacterSelectMenuClass;

	/** Jauge d'objectif (doublons, haut droite) : Widget Blueprint enfant de YSObjectiveWidget (placement et réglages dans ses Class Defaults, catégorie Layout) */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UYSObjectiveWidget> ObjectiveWidgetClass;

	/** BL stacks du Bloodlust (à droite de l'icône d'Ability ; Widget Blueprint réglable WBP_BloodlustStacks) */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UYSBloodlustStacksWidget> BloodlustWidgetClass;

	/** Aperçu du personnage : distance et hauteur de la caméra, champ de vision */
	UPROPERTY(EditAnywhere, Category="UI|Preview", meta=(ClampMin="50", Units="cm"))
	float PreviewCameraDistance = 450.0f;

	UPROPERTY(EditAnywhere, Category="UI|Preview", meta=(Units="cm"))
	float PreviewCameraHeight = 40.0f;

	UPROPERTY(EditAnywhere, Category="UI|Preview", meta=(ClampMin="5", ClampMax="170", Units="deg"))
	float PreviewFOV = 40.0f;

	/** Intensité de la lumière de l'aperçu du personnage (écran de sélection) */
	UPROPERTY(EditAnywhere, Category="UI|Preview")
	float PreviewLightIntensity = 20000.0f;

private:

	UPROPERTY(Transient)
	TObjectPtr<UYSAbilityIconWidget> AbilityIcon;

	UPROPERTY(Transient)
	TObjectPtr<UYSBloodlustStacksWidget> BloodlustStacks;

	/** Menu de sélection du personnage, affiché tant que le joueur n'a pas de personnage */
	UPROPERTY(Transient)
	TObjectPtr<UYSCharacterSelectWidget> CharacterSelectMenu;

	/** HUD haut gauche : PV, niveau et XP */
	UPROPERTY(Transient)
	TObjectPtr<UYSPlayerHUDWidget> PlayerHUD;

	/** Menu de choix d'upgrade (même widget que la sélection de personnage), affiché pendant une montée de niveau */
	UPROPERTY(Transient)
	TObjectPtr<UYSCharacterSelectWidget> UpgradeMenu;

	/** Barre de vie de l'élite dans le HUD (haut droite), quand il est hors écran */
	UPROPERTY(Transient)
	TObjectPtr<UYSEliteHealthWidget> EliteHealth;

	/** Icônes des autres joueurs hors écran (au bord, dans leur direction) */
	UPROPERTY(Transient)
	TObjectPtr<UYSOffscreenIndicatorsWidget> OffscreenIndicators;

	/** Jauge d'objectif commune (doublons) */
	UPROPERTY(Transient)
	TObjectPtr<UYSObjectiveWidget> Objective;

	/** Annonce « Jour X » */
	UPROPERTY(Transient)
	TObjectPtr<UYSDayBannerWidget> DayBanner;

	/** Aperçu du personnage à l'écran de sélection : personnage local (non répliqué, non contrôlé) filmé par une capture */
	UPROPERTY(Transient)
	TObjectPtr<AYSPlayerCharacter> PreviewCharacter;

	UPROPERTY(Transient)
	TObjectPtr<ASceneCapture2D> PreviewCapture;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PreviewTarget;

	/** Lumière de l'aperçu (rien d'autre ne l'éclaire là-haut sans le ciel) */
	UPROPERTY(Transient)
	TObjectPtr<APointLight> PreviewLight;

	/** Dernier jour vu (pour annoncer les nouveaux) et temps d'affichage restant de l'annonce */
	int32 LastSeenDay = INDEX_NONE;
	float DayBannerRemaining = 0.0f;

	/** Dernier état de pause vu (log [YSPause]) */
	bool bWasPaused = false;

	/** Propositions actuellement affichées dans UpgradeMenu (pour reconstruire les boutons quand elles changent) */
	TArray<FYSUpgradeOffer> DisplayedUpgradeOffers;

	/** Nombre total de bonus du personnage quand les boutons ont été construits */
	int32 DisplayedUpgradeStacks = INDEX_NONE;

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

	/** Tourne aussi pendant la pause (contrairement à PlayerTick) : mise à jour de l'UI */
	virtual void Tick(float DeltaSeconds) override;

	/** Debug (console) : donne de l'XP à l'équipe.  Ex : YSAddXP 10 */
	UFUNCTION(Exec)
	void YSAddXP(float Amount = 10.0f);

	/** Debug (console) : choisit une upgrade comme un clic dans le menu (index dans les propositions).  Ex : YSChooseUpgrade 0 */
	UFUNCTION(Exec)
	void YSChooseUpgrade(int32 OfferIndex);

	/** Debug (console) : donne directement une upgrade (index dans l'Upgrade Pool, variante, rareté 1 Bronze / 2 Silver / 3 Gold).  Ex : YSGiveUpgrade 4 0 3 */
	UFUNCTION(Exec)
	void YSGiveUpgrade(int32 PoolIndex, int32 Variant = 0, int32 Rarity = 1);

	/** Debug (console) : déclenche l'action de clic de l'arme d'une main (0 droite, 1 gauche) vers le NME le plus proche.  Ex : YSFireAtFoe 0 */
	UFUNCTION(Exec)
	void YSFireAtFoe(int32 Hand = 0);

	/** Debug (console) : comme un clic gauche (akimbo compris) */
	UFUNCTION(Exec)
	void YSFire();

	/** Debug : comme la touche E (prendre / reposer un objet) */
	UFUNCTION(Exec)
	void YSInteract();

	/** Debug (hôte) : fait apparaître un coffre (BP_InteractiveObject_Chest) devant le personnage */
	UFUNCTION(Exec)
	void YSSpawnChest(float Distance = 150.0f);

	/** Debug (hôte) : téléporte le personnage à côté du Brooker le plus proche */
	UFUNCTION(Exec)
	void YSGoToBrooker(float Distance = 300.0f);

	/** Debug (console, hôte) : met l'arme BP de /Game/Placeable/Weapons dans une main (0 droite, 1 gauche).  Ex : YSEquipWeapon 0 BattlePetoir (préfixe BP_ facultatif) */
	UFUNCTION(Exec)
	void YSEquipWeapon(int32 Hand, const FString& WeaponName);

	/** Debug (console) : personnalisation comme à l'écran de sélection.  Ex : YSSetColor 2 / YSSetFace 3 / YSSetName Paul */
	UFUNCTION(Exec)
	void YSSetColor(int32 ColorIndex);

	UFUNCTION(Exec)
	void YSSetFace(int32 FaceIndex);

	/** Debug (console) : 0 = droitier, 1 = gaucher.  Ex : YSSetLeftHanded 1 */
	UFUNCTION(Exec)
	void YSSetLeftHanded(int32 bLeftHanded);

	UFUNCTION(Exec)
	void YSSetName(const FString& Nickname);

	/** Debug (console) : termine le jour en cours (la nuit et son élite commencent) */
	UFUNCTION(Exec)
	void YSStartNight();

	/** Debug (console) : tire Draws fois 3 propositions et logue la fréquence de chaque proposition par position (vérifie les poids) */
	UFUNCTION(Exec)
	void YSUpgradeOdds(int32 Draws = 10000);

	/**
	 *  Debug (console) : lance une commande après Delay secondes de temps réel (fonctionne aussi en pause).
	 *  Les « _ » de Command sont remplacés par des espaces.  Ex : YSAfter 2 shot_showui
	 */
	UFUNCTION(Exec)
	void YSAfter(float Delay, const FString& Command);

	/** Quitte la partie et revient au menu principal (touche Échap). Hôte : les autres joueurs reviennent aussi au menu. */
	UFUNCTION(Exec)
	void YSLeaveGame();

	/** Triche (flèche droite, builds non Shipping) : 10 bonus Atk speed Silver (CD reduction de l'Auto atk), sans dépasser le maximum */
	UFUNCTION(Exec)
	void YSCheatAtkSpeed();

	UFUNCTION(Server, Reliable)
	void ServerCheatAtkSpeed();

	/** Debug : fixe les BL stacks du Bloodlust (si débloqué) et relance leur délai.  Ex : YSSetBloodlustStacks 5 */
	UFUNCTION(Exec)
	void YSSetBloodlustStacks(int32 Stacks);

	UFUNCTION(Server, Reliable)
	void ServerSetBloodlustStacks(int32 Stacks);

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

	/** Debug (console) : clique sur « Prêt » (la partie démarre quand tout le monde est prêt) */
	UFUNCTION(Exec)
	void YSReady();

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

	/** Crée / met à jour / détruit l'aperçu du personnage (bShow : menu de sélection affiché) */
	void UpdateCharacterPreview(bool bShow);

	/** Choix de couleur / visage / pseudo à l'écran de sélection */
	void ChooseColor(int32 ColorIndex);
	void ChooseFace(int32 FaceIndex);
	void ChooseHandedness(int32 Handedness);

	UFUNCTION(Server, Reliable)
	void ServerSetLeftHanded(bool bLeftHanded);
	void CommitNickname(const FString& Nickname);
	void ToggleReady();

	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bReady);

	UFUNCTION(Server, Reliable)
	void ServerSetCustomization(int32 ColorIndex, int32 FaceIndex);

	UFUNCTION(Server, Reliable)
	void ServerSetNickname(const FString& Nickname);

	/** Met à jour le HUD (PV, niveau, XP) */
	void UpdatePlayerHUD();

	/** Met à jour le menu de choix d'upgrade */
	void UpdateUpgradeMenu();

	/** Barre de l'élite hors écran et annonce des jours */
	void UpdateDayNightUI(float DeltaSeconds);

	UFUNCTION(Server, Reliable)
	void ServerStartNight();

	/** Clic sur une upgrade (index dans les propositions) */
	void ChooseUpgrade(int32 OfferIndex);

	UFUNCTION(Server, Reliable)
	void ServerChooseUpgrade(int32 OfferIndex);

	UFUNCTION(Server, Reliable)
	void ServerAddXP(float Amount);

	UFUNCTION(Server, Reliable)
	void ServerGiveUpgrade(int32 PoolIndex, int32 Variant, int32 Rarity);

	UFUNCTION(Server, Reliable)
	void ServerDamageFoes(float Amount);

	UFUNCTION(Server, Reliable)
	void ServerDamageSelf(float Amount);

	/** Applique les dégâts de debug (serveur) */
	void DamageAllFoes(float Amount);


	/** Projette la souris sur le plan horizontal du pawn et tourne la rotation de contrôle vers ce point */
	void UpdateAim();
};
