#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/YSCustomization.h"
#include "YSGameMode.generated.h"

class AYSPlayerCharacter;
class UYSEnemySpawnRule;
class AYSFoeCharacter;
class UTexture2D;

/**
 *  GameMode du jeu (serveur uniquement), avec sélection du personnage :
 *  les joueurs arrivent sans pawn et choisissent un personnage dans le menu (AYSPlayerController).
 *  Quand tous les joueurs connectés ont choisi, la partie démarre et chacun apparaît avec son personnage.
 *  Un joueur qui arrive en cours de partie apparaît dès qu'il a choisi.
 *  XP commune à l'équipe (AddTeamExperience) : à chaque niveau, partie en pause et choix d'une upgrade par joueur.
 *  Classe de base de BP_TestGameMode.
 */
UCLASS(meta=(PrioritizeCategories="Spawn DayNight Level"))
class AYSGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:

	/**
	 *  Règles de spawn des NME, actives dès le démarrage de la partie (choisir le type dans la liste, régler ses paramètres).
	 *  Plusieurs règles peuvent tourner ensemble ; pour d'autres réglages, faire un BP enfant de ce GameMode.
	 */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="Spawn")
	TArray<TObjectPtr<UYSEnemySpawnRule>> SpawnRules;

	/** Personnages proposés dans le menu de sélection (dans l'ordre des boutons) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Select")
	TArray<TSubclassOf<AYSPlayerCharacter>> SelectableCharacters;

	/** Couleurs proposées à l'écran de sélection (matériau appliqué au « Head Feature » du personnage) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Select")
	TArray<FYSCustomizationColor> CustomizationColors;

	/** Visages proposés à l'écran de sélection (texture du decal de visage) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Select")
	TArray<TObjectPtr<UTexture2D>> CustomizationFaces;

	/** Longueur max d'un pseudo */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Select", meta=(ClampMin="1"))
	int32 MaxNicknameLength = 16;

	/** XP nécessaire pour passer du niveau 0 au niveau 1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level", meta=(ClampMin="1"))
	float LevelUpBaseExperience = 5.0f;

	/** XP supplémentaire nécessaire par niveau déjà atteint (XP requise = Base + ParNiveau × niveau) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level", meta=(ClampMin="0"))
	float LevelUpExperiencePerLevel = 5.0f;

	/** Nombre d'upgrades proposées à chaque montée de niveau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level", meta=(ClampMin="1"))
	int32 UpgradeChoiceCount = 3;

	/** Niveau d'équipe où chaque joueur choisit sa classe (upgrades de classe de son personnage, sans hasard) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Level", meta=(ClampMin="1"))
	int32 ClassUpgradeLevel = 5;

	/** Durée du jour ; ensuite la nuit commence et dure tant que l'élite est en vie */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DayNight", meta=(ClampMin="1", Units="s"))
	float DayDuration = 30.0f;

public:

	AYSGameMode();

	virtual void InitGameState() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	virtual void Logout(AController* Exiting) override;

	/** Choix d'un joueur (reçu par RPC du PlayerController) */
	void PlayerSelectedCharacter(APlayerController* Player, int32 CharacterIndex);

	/** Bouton « Prêt » d'un joueur (reçu par RPC) : la partie démarre quand tous les joueurs sont prêts */
	void PlayerSetReady(APlayerController* Player, bool bReady);

	/** Couleur déjà prise par un autre joueur (les couleurs sont uniques) */
	bool IsColorTaken(int32 ColorIndex, const APlayerState* Except) const;

	/** Personnalisation d'un joueur (reçue par RPC ; index invalides ignorés) */
	void PlayerSetCustomization(APlayerController* Player, int32 ColorIndex, int32 FaceIndex);

	/** Pseudo d'un joueur (reçu par RPC ; vide = inchangé, tronqué à MaxNicknameLength) */
	void PlayerSetNickname(APlayerController* Player, const FString& Nickname);

	/** XP gagnée par l'équipe (NME tué, debug) : gère les montées de niveau et la sélection d'upgrade */
	void AddTeamExperience(float Amount);

	/** Choix d'upgrade d'un joueur (reçu par RPC du PlayerController ; index dans ses propositions) */
	void PlayerChoseUpgrade(APlayerController* Player, int32 OfferIndex);

	/** Fin du jour : la nuit commence (élite et vagues de nuit via les règles de spawn) */
	void StartNight();

	/** Mort de l'élite : la nuit se termine, jour suivant */
	void OnEliteKilled(AYSFoeCharacter* Elite);

protected:

	/** XP nécessaire pour quitter le niveau donné */
	float GetExperienceForLevel(int32 Level) const { return LevelUpBaseExperience + LevelUpExperiencePerLevel * Level; }

	/** Met la partie en pause et propose des upgrades à chaque joueur vivant */
	void StartUpgradeSelection();

	/** Termine la sélection si tout le monde a choisi (puis enchaîne une autre si plusieurs niveaux ont été gagnés) */
	void TryFinishUpgradeSelection();

	/** Montées de niveau pas encore traitées par une sélection d'upgrade */
	int32 PendingLevelUps = 0;

	/** Prochain numéro de joueur (nom par défaut « Player N », couleur par défaut) */
	int32 NextPlayerNumber = 1;

	/** Démarre la partie si tous les joueurs connectés ont choisi */
	void TryStartMatch();

	/** Le joueur a-t-il déjà son personnage ? */
	static bool HasCharacter(const AController* Controller);
};
