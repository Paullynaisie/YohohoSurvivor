#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Upgrades/YSUpgrade.h"
#include "YSPlayerController.generated.h"

class UYSAbilityIconWidget;
class UYSStaminaBarWidget;
class UYSBloodlustStacksWidget;
class UYSCharacterSelectWidget;
class UYSUpgradeMenuWidget;
class UYSPlayerHUDWidget;
class UYSEliteHealthWidget;
class AYSFoeCharacter;
class UYSDayBannerWidget;
class UYSOffscreenIndicatorsWidget;
class UYSObjectiveWidget;
class UYSShipBoardingWidget;
class ASceneCapture2D;
class AYSPlayerCharacter;
class UTextureRenderTarget2D;
class APointLight;
class UYSDeathScreenWidget;
class ACameraActor;
class UYSWeaponSet;

/** Aperçu d'un personnage : personnage local (non répliqué, non contrôlé) filmé par une capture */
USTRUCT()
struct FYSCharacterPreviewScene
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<AYSPlayerCharacter> Character;

	UPROPERTY(Transient)
	TObjectPtr<ASceneCapture2D> Capture;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> Target;

	/** Lumière de l'aperçu (rien d'autre ne l'éclaire là-haut sans le ciel) */
	UPROPERTY(Transient)
	TObjectPtr<APointLight> Light;
};

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

	/** Écran de mort et de spectateur (réglages : WBP_DeathScreen) */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UYSDeathScreenWidget> DeathScreenClass;

	/** Widget de l'icône de capacité (en bas au centre de l'écran) */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UYSAbilityIconWidget> AbilityIconClass;

	/** Distance entre l'icône et le bas de l'écran */
	UPROPERTY(EditAnywhere, Category="UI")
	float AbilityIconBottomMargin = 40.0f;

	/** Widget de l'icône de l'attaque auto (à gauche de celle de l'Ability, même défilement de recharge) */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UYSAbilityIconWidget> AutoAttackIconClass;

	/** Espace entre l'icône de l'attaque auto et celle de l'Ability */
	UPROPERTY(EditAnywhere, Category="UI")
	float AutoAttackIconSpacing = 16.0f;

	/** Widget de l'icône de la MoveAction (à droite de celle de l'Ability) */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UYSAbilityIconWidget> MoveActionIconClass;

	/** Espace entre l'icône de l'Ability et celle de la MoveAction */
	UPROPERTY(EditAnywhere, Category="UI")
	float MoveActionIconSpacing = 16.0f;

	/** Barre de stamina (sous les icônes) : Widget Blueprint enfant de YSStaminaBarWidget (WBP_StaminaBar, réglages Layout) ; vide = classe C++ */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UYSStaminaBarWidget> StaminaBarClass;

	/** Manette : zone morte du stick droit (visée) */
	UPROPERTY(EditAnywhere, Category="Gamepad", meta=(ClampMin="0", ClampMax="1"))
	float GamepadAimDeadZone = 0.25f;

	/** Manette : distance du point visé (curseur) au personnage, stick juste sorti de la zone morte */
	UPROPERTY(EditAnywhere, Category="Gamepad", meta=(ClampMin="0", Units="Centimeters"))
	float GamepadAimMinDistance = 300.0f;

	/** Manette : distance du point visé au personnage, stick poussé à fond (ex. lancer de bombe vers le curseur) */
	UPROPERTY(EditAnywhere, Category="Gamepad", meta=(ClampMin="0", Units="Centimeters"))
	float GamepadAimMaxDistance = 1000.0f;

	/** Distance entre les icônes des joueurs hors écran et le bord de l'écran */
	UPROPERTY(EditAnywhere, Category="UI")
	float OffscreenIndicatorMargin = 12.0f;

	/** Écran de sélection : Widget Blueprint enfant de YSCharacterSelectWidget (placements dans ses Class Defaults, catégorie Layout) */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UYSCharacterSelectWidget> CharacterSelectMenuClass;

	/** Menu de montée de niveau : Widget Blueprint enfant de YSUpgradeMenuWidget (placements dans ses Class Defaults, catégorie Layout) */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UYSUpgradeMenuWidget> UpgradeMenuClass;

	/** Jauge d'objectif (doublons, haut droite) : Widget Blueprint enfant de YSObjectiveWidget (placement et réglages dans ses Class Defaults, catégorie Layout) */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UYSObjectiveWidget> ObjectiveWidgetClass;

	/** Barre de l'élite (haut droite : PV, bouclier de boss, rage) : Widget Blueprint enfant de YSEliteHealthWidget (réglages Layout) ; vide = classe C++ */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UYSEliteHealthWidget> EliteHealthWidgetClass;

	/** Fin de jeu temporaire (« X/Y sur le bateau », « BRAVO ») : Widget Blueprint enfant de YSShipBoardingWidget (réglages dans ses Class Defaults, catégorie Layout) */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UYSShipBoardingWidget> ShipBoardingWidgetClass;

	/** BL stacks du Bloodlust (à droite de l'icône d'Ability ; Widget Blueprint réglable WBP_BloodlustStacks) */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UYSBloodlustStacksWidget> BloodlustWidgetClass;

	/** Aperçu d'un personnage sans set d'arme (sinon : Showcase du set) : distance et hauteur de la caméra */
	UPROPERTY(EditAnywhere, Category="UI|Preview", meta=(ClampMin="50", Units="cm"))
	float PreviewCameraDistance = 450.0f;

	UPROPERTY(EditAnywhere, Category="UI|Preview", meta=(Units="cm"))
	float PreviewCameraHeight = 40.0f;

	/** Champ de vision des aperçus */
	UPROPERTY(EditAnywhere, Category="UI|Preview", meta=(ClampMin="5", ClampMax="170", Units="deg"))
	float PreviewFOV = 40.0f;

	/** Passage de la vue du GameBoat à la caméra du joueur quand tout le monde est prêt */
	UPROPERTY(EditAnywhere, Category="UI|Boat Intro", meta=(ClampMin="0", Units="s"))
	float BoatIntroCameraBlendTime = 1.5f;

	/** Intensité de la lumière des aperçus */
	UPROPERTY(EditAnywhere, Category="UI|Preview")
	float PreviewLightIntensity = 20000.0f;

private:

	UPROPERTY(Transient)
	TObjectPtr<UYSAbilityIconWidget> AbilityIcon;

	UPROPERTY(Transient)
	TObjectPtr<UYSAbilityIconWidget> AutoAttackIcon;

	UPROPERTY(Transient)
	TObjectPtr<UYSAbilityIconWidget> MoveActionIcon;

	UPROPERTY(Transient)
	TObjectPtr<UYSStaminaBarWidget> StaminaBar;

	UPROPERTY(Transient)
	TObjectPtr<UYSBloodlustStacksWidget> BloodlustStacks;

	/** Menu de sélection du personnage, affiché tant que le joueur n'a pas de personnage */
	UPROPERTY(Transient)
	TObjectPtr<UYSCharacterSelectWidget> CharacterSelectMenu;

	/** HUD haut gauche : PV, niveau et XP */
	UPROPERTY(Transient)
	TObjectPtr<UYSPlayerHUDWidget> PlayerHUD;

	/** Menu de choix d'upgrade (cartes + relance), affiché pendant une montée de niveau */
	UPROPERTY(Transient)
	TObjectPtr<UYSUpgradeMenuWidget> UpgradeMenu;

	/** Barre de vie de l'élite dans le HUD (haut droite), quand il est hors écran */
	UPROPERTY(Transient)
	TObjectPtr<UYSEliteHealthWidget> EliteHealth;

	/** Barres des élites / boss en plus (plusieurs en vie : empilées au-dessus de EliteHealth) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UYSEliteHealthWidget>> ExtraEliteHealths;

	/** Élites affichés dans le HUD, dans l'ordre d'arrivée (le premier en bas) */
	TArray<TWeakObjectPtr<AYSFoeCharacter>> HudElites;

	/** Barre d'index Index de la pile (0 = EliteHealth), créée au besoin */
	UYSEliteHealthWidget* GetEliteHealthBar(int32 Index);

	/** Icônes des autres joueurs hors écran (au bord, dans leur direction) */
	UPROPERTY(Transient)
	TObjectPtr<UYSOffscreenIndicatorsWidget> OffscreenIndicators;

	/** Jauge d'objectif commune (doublons) */
	UPROPERTY(Transient)
	TObjectPtr<UYSObjectiveWidget> Objective;

	UPROPERTY(Transient)
	TObjectPtr<UYSShipBoardingWidget> ShipBoarding;

	/** Victoire vue par cette machine : secondes avant le retour au menu (-1 = pas de victoire, -2 = déjà parti) */
	float VictoryReturnTimer = -1.0f;

	/** Annonce « Jour X » */
	UPROPERTY(Transient)
	TObjectPtr<UYSDayBannerWidget> DayBanner;

	/** Aperçu du personnage à l'écran de sélection */
	UPROPERTY(Transient)
	FYSCharacterPreviewScene SelectPreview;

	/** Aperçus du choix de classe : une par proposition, le personnage avec son nouveau set */
	UPROPERTY(Transient)
	TArray<FYSCharacterPreviewScene> ClassPreviews;

	/** FlowMod : personnages locaux sur les places de la chaloupe, un par joueur qui a choisi et n'a pas encore son personnage */
	UPROPERTY(Transient)
	TMap<TObjectPtr<APlayerState>, TObjectPtr<AYSPlayerCharacter>> BoatSeatPreviews;

	/** La vue est sur le GameBoat (écran de sélection du FlowMod) */
	bool bBoatIntroView = false;

	/**
	 *  Le joueur local a déjà eu son personnage : ni menu de sélection ni vue du GameBoat ensuite (chez un client, le PlayerState
	 *  ressuscité arrive avant le nouveau personnage : le menu et la vue du bateau revenaient le temps d'une image)
	 */
	bool bHadPlayerCharacter = false;

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

	/** Caméra fixe laissée à l'endroit de la mort (locale, non répliquée) : vue de secours quand plus personne n'est vivant */
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> DeathCamera;

	/** Spectateur (mort, corps disparu) : joueur vivant dont on regarde la caméra, ou son propre cadavre (AYSPlayerCorpse) */
	TWeakObjectPtr<AActor> SpectateTarget;

	/** Regardables : joueurs vivants (pas les minions) dans l'ordre des PlayerState, puis son propre cadavre */
	TArray<AActor*> GetSpectateTargets() const;

	/** Vue du spectateur et écran de mort (local, chaque image) */
	void UpdateSpectating();

	/** Q / A : joueur précédent, D : suivant (seulement en spectateur) */
	void SpectateStep(int32 Step);

public:

	/** Mort et plus de personnage : regarde la caméra d'un autre joueur */
	bool IsSpectating() const;

private:

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

	/** Log de possession (toutes machines : serveur à la possession, client à sa prise en compte) */
	virtual void OnPossess(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* P) override;

	/** Tourne aussi pendant la pause (contrairement à PlayerTick) : mise à jour de l'UI */
	virtual void Tick(float DeltaSeconds) override;

	/** Debug (console) : donne de l'XP à l'équipe.  Ex : YSAddXP 10 */
	UFUNCTION(Exec)
	void YSAddXP(float Amount = 10.0f);

	/** Debug (console) : choisit une upgrade comme un clic dans le menu (index dans les propositions).  Ex : YSChooseUpgrade 0 */
	UFUNCTION(Exec)
	void YSChooseUpgrade(int32 OfferIndex);

	/** Debug (console) : relance le tirage d'upgrades comme le bouton « Relance » */
	UFUNCTION(Exec)
	void YSRerollUpgrades();

	/** Debug (console) : donne directement une upgrade (index dans l'Upgrade Pool, variante, rareté 1 Bronze / 2 Silver / 3 Gold).  Ex : YSGiveUpgrade 4 0 3 */
	UFUNCTION(Exec)
	void YSGiveUpgrade(int32 PoolIndex, int32 Variant = 0, int32 Rarity = 1);

	/** Debug (console) : ajoute des niveaux à une shard (nom d'asset, avec ou sans DA_Up_Shard_).  Ex : YSGiveShard AtkSpeed 5 */
	UFUNCTION(Exec)
	void YSGiveShard(const FString& ShardName, int32 Levels = 1);

	/** Debug (console) : log des niveaux de shards du personnage et des chances de rareté (log [YSShard]) */
	UFUNCTION(Exec)
	void YSShards();

	/** Debug (console) : déclenche l'action de clic de l'arme d'une main (0 droite, 1 gauche) vers le NME le plus proche.  Ex : YSFireAtFoe 0 */
	UFUNCTION(Exec)
	void YSFireAtFoe(int32 Hand = 0);

	/** Debug (console) : déclenche l'action de clic de l'arme d'une main (0 droite, 1 gauche) vers un point du monde (X, Y).  Ex : YSFireAt 0 -3000 200 */
	UFUNCTION(Exec)
	void YSFireAt(int32 Hand, float X, float Y);

	/** Debug (console, hôte) : point d'impact que choisirait la BombThrower tenue pour un curseur en (X, Y), sans lancer (log [YSBombThrower]) */
	UFUNCTION(Exec)
	void YSBombLanding(float X, float Y);

	/** Debug (console) : comme un clic droit = Ability (akimbo compris) */
	UFUNCTION(Exec)
	void YSFire();

	/** Debug (console) : comme la touche Espace = MoveAction */
	UFUNCTION(Exec)
	void YSMoveAction();

	/** Debug (console) : comme le bouton Revive du spectateur */
	UFUNCTION(Exec)
	void YSPhoenixRevive();

	/** Debug (console, hôte) : ajoute (ou retire) des plumes de phoenix au stock */
	UFUNCTION(Exec)
	void YSAddFeathers(int32 Amount);

	/** Debug (console) : fixe la stamina du personnage (serveur).  Ex : YSSetStamina 40 */
	UFUNCTION(Exec)
	void YSSetStamina(float Value);

	UFUNCTION(Server, Reliable)
	void ServerSetStamina(float Value);

	/** Debug (console) : clic gauche considéré comme maintenu (1) ou non (0) : attaque auto.  Ex : YSHoldAutoAttack 1 */
	UFUNCTION(Exec)
	void YSHoldAutoAttack(int32 bHeld = 1);

	/** Debug (console) : Camera Lock (1 = caméra centrée sur le PJ, 0 = décalée vers le curseur) */
	UFUNCTION(Exec)
	void YSCameraLock(int32 bLocked = 1);

	/** Camera Lock (réglage local du joueur, décoché par défaut ; case de l'écran de sélection, touche TAB en jeu) : caméra centrée sur le PJ */
	bool IsCameraLocked() const { return bCameraLock; }
	void SetCameraLock(bool bLocked);
	void ToggleCameraLock() { SetCameraLock(!bCameraLock); }

	/** Debug : comme la touche E (prendre / reposer un objet) */
	UFUNCTION(Exec)
	void YSInteract();

	/** Debug : prend la barre du bateau pilotable le plus proche (Skip : le suivant par distance), téléporté derrière la roue.  Ex : YSTakeHelm 1 */
	UFUNCTION(Exec)
	void YSTakeHelm(int32 Skip = 0);

	/** Debug : commandes de la barre tenues comme des touches (Z / S = 1 / -1, D / Q = 1 / -1 ; 0 0 = relâchées).  Ex : YSHelmInput 1 0 */
	UFUNCTION(Exec)
	void YSHelmInput(float Throttle = 0.0f, float Steer = 0.0f);

	/** Debug (hôte) : mode playtest (spline) de tous les bateaux pilotables, comme la case à cocher.  Ex : YSShipPlaytest 1 */
	UFUNCTION(Exec)
	void YSShipPlaytest(int32 bEnabled = 1);

	/** Debug (hôte) : NME Jolly Ship sur l'eau.  Ex : YSSpawnJollyShip 5 */
	UFUNCTION(Exec)
	void YSSpawnJollyShip(int32 Count = 1);

	/** Debug (hôte) : dégâts à la coque de tous les Jolly Ships (pas à leur équipage).  Ex : YSDamageJollyHull 1000 */
	UFUNCTION(Exec)
	void YSDamageJollyHull(float Damage = 1000.0f);

	/** Debug (hôte) : NME Galion sur l'eau.  Ex : YSSpawnGalion */
	UFUNCTION(Exec)
	void YSSpawnGalion();

	/** Debug (hôte) : le Galion le plus proche, placé à Distance devant la proue du bateau du joueur, en travers (test des collisions) */
	UFUNCTION(Exec)
	void YSPlaceGalionAhead(float Distance = 500.0f);

	/** Debug (hôte) : téléporte le joueur sur le pont du Galion le plus proche (0) ou sur l'accès de son flanc (1) */
	UFUNCTION(Exec)
	void YSGoToGalion(int32 bAtAccess = 0);

	/** Debug (hôte) : dégâts à un module (Kind : Cannon, Longboat) du bateau le plus proche du camp demandé (0 joueurs, 1 NME), index dans ce type */
	UFUNCTION(Exec)
	void YSDamageModule(const FString& Kind, float Damage, int32 bFoeShip = 0, int32 Index = 0);

	/** Debug (hôte) : dégâts au core du bateau le plus proche du camp demandé (0 joueurs, 1 NME) */
	UFUNCTION(Exec)
	void YSDamageCore(float Damage, int32 bFoeShip = 0);

	/** Debug : le joueur s'installe pour réparer le module à 0 PV le plus proche (comme E, sans la portée) */
	UFUNCTION(Exec)
	void YSRepairModule();

	/** Debug (hôte) : tue les matelots du Galion qui ne sont pas à un poste */
	UFUNCTION(Exec)
	void YSKillFreeCrew();

	/** Debug (hôte) : état de tous les bateaux dans le log (PV, modules, équipage) */
	UFUNCTION(Exec)
	void YSShipStatus();

	/** Debug : utilise le poste de bateau libre le plus proche de ce type (Helm, Nest, Cannon, Longboat = chalouperie, Seat = passager de la chaloupe).  Ex : YSUseStation Cannon */
	UFUNCTION(Exec)
	void YSUseStation(const FString& Kind, int32 Skip = 0);

	/** Debug : action au poste vers un point du monde (0 = clic droit : tir de canon, 1 = clic gauche : ping du nid d'pie).  Ex : YSStationAction 0 2000 500 */
	UFUNCTION(Exec)
	void YSStationAction(int32 Action, float X, float Y, float Z = 0.0f);

	/** Debug : quitte le poste (comme E) */
	UFUNCTION(Exec)
	void YSLeaveStation();

	/** Debug (hôte) : fait apparaître un coffre (BP_InteractiveObject_Chest) devant le personnage */
	UFUNCTION(Exec)
	void YSSpawnChest(float Distance = 150.0f);

	/** Debug (hôte ou client) : téléporte le personnage à côté du Brooker le plus proche */
	UFUNCTION(Exec)
	void YSGoToBrooker(float Distance = 300.0f);

	/** Debug (hôte ou client) : téléporte le personnage à côté du Medic le plus proche */
	UFUNCTION(Exec)
	void YSGoToMedic(float Distance = 300.0f);

	/** Téléporte le personnage à Distance du Brooker (ou du Medic) le plus proche */
	void GoToNPC(float Distance, bool bMedic);

	/** Debug (hôte ou client) : téléporte le personnage sur le cadavre de joueur le plus proche (E le prend alors à coup sûr) */
	UFUNCTION(Exec)
	void YSGoToCorpse();

	/** Debug (hôte ou client) : téléporte le personnage à Distance de l'accès du bateau le plus proche vers le large et Side sur le côté (dans l'eau à côté du ponton), tourné vers le bateau */
	UFUNCTION(Exec)
	void YSGoToBoarding(float Distance = 300.0f, float Side = 0.0f);

	/** Debug d'un client : YSGoToBrooker (0) / YSGoToMedic (1) / YSGoToCorpse (2) / YSGoToBoarding (3) faits par le serveur */
	UFUNCTION(Server, Reliable)
	void ServerGoToDebug(uint8 Kind, float Distance, float Side = 0.0f);

	/** Debug (hôte) : carte au trésor devant le personnage (Distance > 0) ou au hasard dans la zone du GameMode (0) */
	UFUNCTION(Exec)
	void YSSpawnTreasureMap(float Distance = 150.0f);

	/** Debug (hôte) : fait apparaître le NME /Game/Placeable/Character/(sous-dossiers compris)/<Name> devant le personnage. Ex : YSSpawnFoe FoeCharacter_Skull_Bomb 1500 */
	UFUNCTION(Exec)
	void YSSpawnFoe(const FString& Name, float Distance = 1500.0f);

	/** Debug (hôte) : le personnage qui porte une carte au trésor est téléporté au point suivant de son chemin
	 *  (Skip : points sautés sans y passer, comme un virage coupé) */
	UFUNCTION(Exec)
	void YSTreasureNextPoint(int32 Skip = 0);

	/** Debug (console, hôte) : met l'arme BP de /Game/Placeable/Weapons (sous-dossiers compris) dans une main (0 droite, 1 gauche).  Ex : YSEquipWeapon 0 BattlePetoir (préfixe BP_ facultatif) */
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

	/** Debug (console, hôte) : ajoute de la rage aux élites (Boss armor) ; pleine = bouclier de boss (ex. YSBossRage 650) */
	UFUNCTION(Exec)
	void YSBossRage(float Amount);

	/** Debug (console, hôte) : le bateau apparaît comme si l'objectif de doublons était atteint */
	UFUNCTION(Exec)
	void YSShipArrive();

	/** Debug (console, hôte) : l'île passe en overtime maintenant (circle spawns d'overtime, l'île coulera à la tombée de la nuit) */
	UFUNCTION(Exec)
	void YSStartOvertime();

	/** Debug (console, hôte) : l'île coule maintenant */
	UFUNCTION(Exec)
	void YSSinkIsland();

	/** Debug : état de l'île dans le log */
	UFUNCTION(Exec)
	void YSIslandStatus();

	/** Debug (hôte) : avance le temps de la phase de l'île */
	UFUNCTION(Exec)
	void YSIslandSkip(float Seconds);

	/** Debug (hôte) : téléporté sur l'objet de cet objectif (0, 1... : principaux puis secondaires) et le prend dans ses mains */
	UFUNCTION(Exec)
	void YSIslandCarry(int32 ObjectiveIndex);

	/** Debug (hôte) : tue les élites vivants */
	UFUNCTION(Exec)
	void YSIslandKillElite();

	/** Debug (hôte) : téléporté à côté du bateau de départ de l'île la plus proche, sinon au core du bateau des joueurs le plus proche */
	UFUNCTION(Exec)
	void YSIslandGoToBoat();

	/** Debug (hôte, FlowMod) : les îles suivantes arrivent maintenant (remontée des îles prégénérées, batailles navales) */
	UFUNCTION(Exec)
	void YSIslandSpawnNext();

	/** Debug (hôte, FlowMod) : l'île finale arrive tout de suite (logue si les îles suivantes et les batailles sont faites) */
	UFUNCTION(Exec)
	void YSFinalIsland();

	/** Debug (hôte, FlowMod) : réveille toutes les batailles navales */
	UFUNCTION(Exec)
	void YSNavalBattleActivate();

	/** Debug (hôte, FlowMod) : bateau des joueurs posé sur l'eau à Distance (cm, 0 = 90 m) de la bataille navale Index, le joueur sur son pont */
	UFUNCTION(Exec)
	void YSShipToNavalBattle(int32 Index, float Distance);

	/** Debug (hôte) : l'île la plus proche commence à couler tout de suite (sans attendre Sink Delay) */
	UFUNCTION(Exec)
	void YSIslandSinkNow();

	/** Debug (FlowMod, hôte ou client) : téléporté sur l'île Index (0 = départ, puis les îles suivantes dans l'ordre de Next Islands) */
	UFUNCTION(Exec)
	void YSIslandGoTo(int32 Index);

	/** Debug (hôte, FlowMod) : téléporté au milieu du morceau de ponton Segment (0 = rampe de la 1re ouverture) de l'île Index */
	UFUNCTION(Exec)
	void YSIslandGoToPier(int32 Index, int32 Segment);

	/** Debug (hôte, FlowMod) : « go naviguer » (vagues en mer) */
	UFUNCTION(Exec)
	void YSNavigate();

	/** Debug (console) : longueur du bras de la caméra (ex. 15000 pour voir une île entière) ; 0 = celle du Blueprint */
	UFUNCTION(Exec)
	void YSCameraDistance(float Length);

	/** Debug (console) : téléporte son personnage (ex. sur le pont du bateau de GameMap : YSTeleport -6050 550 300) */
	UFUNCTION(Exec)
	void YSTeleport(float X, float Y, float Z);

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

	/** Partie Steam : ouvre la fenêtre d'invitation d'amis de l'overlay Steam (aussi le bouton du menu de sélection) */
	UFUNCTION(Exec)
	void YSInviteFriends();

	/** Triche (flèche haut, builds non Shipping) : +1 niveau d'équipe, donc un choix d'upgrade */
	UFUNCTION(Exec)
	void YSCheatLevelUp();

	UFUNCTION(Server, Reliable)
	void ServerCheatLevelUp();

	/** Triche (flèche droite, builds non Shipping) : 10 bonus Atk speed Silver (CD reduction de l'Auto atk), sans dépasser le maximum */
	UFUNCTION(Exec)
	void YSCheatAtkSpeed();

	UFUNCTION(Server, Reliable)
	void ServerCheatAtkSpeed();

	/** Map de test : touche du pavé numérique -> NME de cette touche devant le personnage (si le GameMode l'autorise).  Ex : YSNumpadSpawn 2 */
	UFUNCTION(Exec)
	void YSNumpadSpawn(int32 Key);

	UFUNCTION(Server, Reliable)
	void ServerNumpadSpawn(int32 Key);

	/** Map de test : Ctrl gauche + touche du pavé numérique -> circle spawn du NME de cette touche sur le contour de la plage.  Ex : YSNumpadCircle 1 */
	UFUNCTION(Exec)
	void YSNumpadCircle(int32 Key);

	UFUNCTION(Server, Reliable)
	void ServerNumpadCircle(int32 Key);

	/** Debug : fixe les BL stacks du Bloodlust (si débloqué) et relance leur délai.  Ex : YSSetBloodlustStacks 5 */
	UFUNCTION(Exec)
	void YSSetBloodlustStacks(int32 Stacks);

	UFUNCTION(Server, Reliable)
	void ServerSetBloodlustStacks(int32 Stacks);

	/** Debug : fixe les stacks de rage de la BloodRage (si débloquée) et relance leur délai.  Ex : YSSetBloodRageStacks 5 */
	UFUNCTION(Exec)
	void YSSetBloodRageStacks(int32 Stacks);

	UFUNCTION(Server, Reliable)
	void ServerSetBloodRageStacks(int32 Stacks);

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

	/** Debug (console) : comme les boutons « Prêt » et « Annuler » */
	UFUNCTION(Exec)
	void YSToggleReady() { ToggleReady(); }

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

	/** Crée ou met à jour un aperçu (WeaponSet vide = celui de la classe) ; changement de classe, de set ou de main = nouveau personnage */
	void UpdatePreviewScene(FYSCharacterPreviewScene& Scene, TSubclassOf<AYSPlayerCharacter> PreviewClass, UYSWeaponSet* PreviewWeaponSet, const FVector& Location, bool bPreviewLeftHanded, const FVector2D& Size);

	void DestroyPreviewScene(FYSCharacterPreviewScene& Scene);

	/** FlowMod : vue sur le GameBoat tant que le joueur n'a pas son personnage, puis passage en douceur à sa caméra */
	void UpdateBoatIntroView();

	/** FlowMod : personnages visibles sur les places de la chaloupe (mêmes personnages sur toutes les machines, chacun les crée localement) */
	void UpdateBoatSeatPreviews();

	/** Aperçus du choix de classe (bShow faux = détruits) */
	void UpdateClassPreviews(bool bShow, const AYSPlayerCharacter* PlayerCharacter);

	/** Choix de couleur / visage / pseudo à l'écran de sélection */
	void ChooseColor(int32 ColorIndex);
	void ChooseFace(int32 FaceIndex);
	void ChooseHandedness(int32 Handedness);

	/** Camera Lock (local, non répliqué) */
	bool bCameraLock = false;

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

	/** Bouton Revive : une plume de phoenix pour ressusciter ce joueur */
	UFUNCTION(Server, Reliable)
	void ServerPhoenixRevive();

	/** Debug : 0 = bateau, 1 = overtime, 2 = l'île coule */
	UFUNCTION(Server, Reliable)
	void ServerLoopDebug(int32 Action);

	UFUNCTION(Server, Reliable)
	void ServerTeleport(FVector Location);

	/** « X/Y sur le bateau », « BRAVO » puis retour au menu */
	void UpdateShipBoardingUI(float DeltaSeconds);

	/** Clic sur une upgrade (index dans les propositions) */
	void ChooseUpgrade(int32 OfferIndex);

	UFUNCTION(Server, Reliable)
	void ServerChooseUpgrade(int32 OfferIndex);

	/** Bouton « Relance » : nouveau tirage de propositions pour ce joueur */
	void RerollUpgrades();

	UFUNCTION(Server, Reliable)
	void ServerRerollUpgrades();

	UFUNCTION(Server, Reliable)
	void ServerAddXP(float Amount);

	UFUNCTION(Server, Reliable)
	void ServerGiveUpgrade(int32 PoolIndex, int32 Variant, int32 Rarity);

	UFUNCTION(Server, Reliable)
	void ServerGiveShard(const FString& ShardName, int32 Levels);

	UFUNCTION(Server, Reliable)
	void ServerDamageFoes(float Amount);

	UFUNCTION(Server, Reliable)
	void ServerDamageSelf(float Amount);

	/** Applique les dégâts de debug (serveur) */
	void DamageAllFoes(float Amount);


	/** Projette la souris sur le plan horizontal du pawn et tourne la rotation de contrôle vers ce point */
	void UpdateAim();

	/**
	 *  Stick droit : place le curseur de la souris sur le point visé (direction du stick depuis le personnage, distance selon
	 *  l'inclinaison), donc tout ce qui suit le curseur (regard, lancers, caméra, postes du bateau) marche à la manette.
	 *  Stick relâché : le point visé reste au même endroit par rapport au personnage ; la souris bougée reprend la main.
	 */
	void UpdateGamepadAim(const APawn* ControlledPawn);

	/** Visée à la manette en cours (le curseur est placé par le stick droit) */
	bool bGamepadAiming = false;

	/** Point visé à la manette, par rapport au personnage (à plat) */
	FVector GamepadAimOffset = FVector::ZeroVector;

	/** Positions du curseur placées aux deux dernières images (la souris réelle est rendue avec une image de retard) */
	FVector2D GamepadCursorPositions[2] = { FVector2D(-1.0f), FVector2D(-1.0f) };

	/** Un menu (sélection, upgrades) gardait le focus : rendu au jeu quand il se cache (A doit de nouveau interagir) */
	void UpdateMenuFocus();
};
