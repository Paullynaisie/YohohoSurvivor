#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "YSFoeCharacter.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class UYSFaceDecalComponent;
class AYSInteractiveObject;

/**
 *  Ennemi de base : attiré en ligne droite vers le joueur le plus proche, sans pathfinding,
 *  et toujours tourné vers lui. Meurt (détruit) quand ses points de vie tombent à 0 et donne alors de l'XP à l'équipe.
 *  Niveau (Level) : PV, dégâts de contact et XP = valeur de base + bonus par niveau (catégorie Foe|Level).
 *  DA « primitives » comme le joueur : cylindre (BodyMesh, aussi sa hurtbox) + visage en decal, sans inclinaison (perf).
 *  Contact : sa Hitbox inflige ContactDamage à la hurtbox (corps) des joueurs, puis toutes les ContactDamageInterval secondes.
 *  La logique tourne uniquement sur le serveur ; position, rotation et vie sont répliquées aux clients.
 *  Classe de base du Blueprint FoeCharacter_Skull : les réglages se font dans le Blueprint.
 */
UCLASS(abstract, meta=(PrioritizeCategories="Foe"))
class AYSFoeCharacter : public ACharacter, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

private:

	/** Corps (cylindre par défaut) ; c'est aussi la hurtbox de l'ennemi */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** Visage projeté sur l'avant du corps */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UYSFaceDecalComponent> FaceDecal;

	/** Zone de contact qui blesse les joueurs (un peu plus large que le corps : les capsules empêchent les corps de se toucher) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCapsuleComponent> Hitbox;

	/** Barre de vie au-dessus de la tête (cachée tant que les PV sont au max) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UWidgetComponent> HealthBar;

protected:

	/** Nom affiché (barre de vie de l'élite dans le HUD) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe")
	FText FoeName;

	/** Le niveau suit le numéro du jour en cours au moment de l'apparition (+ LevelFromDayOffset : jour 1 = niveau 0) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level")
	bool bLevelFromDay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level", meta=(EditCondition="bLevelFromDay"))
	int32 LevelFromDayOffset = -1;

	/** Élite : apparaît la nuit, sa mort fait passer au jour suivant ; barre de vie dans le HUD (haut droite) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Elite")
	bool bIsElite = false;

	/** Multiplicateurs (élites) : PV, dégâts de contact, taille (appliquée au BeginPlay sur toutes les machines) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Elite", meta=(ClampMin="0"))
	float HealthMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Elite", meta=(ClampMin="0"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Elite", meta=(ClampMin="0.1"))
	float SizeMultiplier = 1.0f;

	/** Objet laissé au sol à sa mort (ex. coffre de l'élite ; vide = rien) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Loot")
	TSubclassOf<AYSInteractiveObject> DeathLootClass;

	/** Niveau du NME : augmente ses PV, ses dégâts et l'XP qu'il donne */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Foe|Level", meta=(ClampMin="0"))
	int32 Level = 0;

	/** PV au niveau 0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level", meta=(ClampMin="1"))
	float BaseHealth = 100.0f;

	/** PV ajoutés par niveau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level", meta=(ClampMin="0"))
	float HealthPerLevel = 10.0f;

	/** Dégâts de contact au niveau 0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level", meta=(ClampMin="0"))
	float BaseContactDamage = 5.0f;

	/** Dégâts de contact ajoutés par niveau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level", meta=(ClampMin="0"))
	float ContactDamagePerLevel = 3.0f;

	/** XP donnée à l'équipe à sa mort, au niveau 0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level", meta=(ClampMin="0"))
	float BaseExperience = 1.0f;

	/** XP ajoutée par niveau (XP donnée = Base + ParNiveau × niveau) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Level", meta=(ClampMin="0"))
	float ExperiencePerLevel = 1.0f;

	/** PV max, calculés au spawn depuis le niveau */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Foe|Health")
	float MaxHealth = 100.0f;

	/** Points de vie actuels (gérés par le serveur) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Foe|Health")
	float Health = 0.0f;

	/** Dégâts infligés à un joueur au contact, calculés au spawn depuis le niveau */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Foe|Attack")
	float ContactDamage = 5.0f;

	/** Tant que le contact dure, dégâts répétés toutes les N secondes (par joueur touché) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foe|Attack", meta=(ClampMin="0.05", Units="s"))
	float ContactDamageInterval = 1.0f;

	/** Cibles blessées au contact : acteurs portant un de ces tags */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foe|Attack")
	FGameplayTagContainer ContactHitTags;

	/** Tags portés par l'ennemi (camp, etc.), lus par les projectiles */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Tags")
	FGameplayTagContainer GameplayTags;

	/** Vitesse de déplacement maximale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Movement", meta=(ClampMin="0", Units="CentimetersPerSecond"))
	float MoveSpeed = 400.0f;

	/** Vitesse à laquelle l'ennemi se tourne vers sa cible (0 = instantané) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe|Movement", meta=(ClampMin="0"))
	float TurnSpeed = 10.0f;

	/** Joueur actuellement ciblé (renseigné sur le serveur uniquement) */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Foe")
	TObjectPtr<APawn> Target;

public:

	AYSFoeCharacter();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/**
	 *  Pushback (serveur) : poussé dans la direction du coup (FPointDamageEvent::ShotDirection), avec une force selon la part
	 *  de sa vie retirée (réglages du personnage qui frappe, catégorie 3C|Pushback ; AYSPlayerCharacter::ComputePushbackForce)
	 */
	void ApplyPushback(float Damage, const FDamageEvent& DamageEvent, const AController* EventInstigator);

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override { TagContainer.AppendTags(GameplayTags); }

	UFUNCTION(BlueprintPure, Category="Foe|Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category="Foe|Level")
	int32 GetFoeLevel() const { return Level; }

	bool IsElite() const { return bIsElite; }

	float GetMaxHealth() const { return MaxHealth; }

	float GetSizeMultiplier() const { return SizeMultiplier; }

	/** Nom affiché (repli sur le nom de la classe) */
	FText GetFoeName() const { return FoeName.IsEmpty() ? FText::FromString(GetClass()->GetName().Replace(TEXT("_C"), TEXT(""))) : FoeName; }

	/** XP donnée à l'équipe quand ce NME meurt */
	UFUNCTION(BlueprintPure, Category="Foe|Level")
	float GetExperienceReward() const { return BaseExperience + ExperiencePerLevel * Level; }

	/** Hurtbox : le corps */
	UPrimitiveComponent* GetHurtbox() const;

protected:

	/** Mort de l'ennemi (serveur) */
	virtual void Die();

	/** Deadzone : tombé sous le KillZ de la map (World Settings) -> mort (serveur ; rien chez les clients) */
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

	UFUNCTION()
	void OnRep_Health();

	/** Rafraîchit la barre de vie avec Health / MaxHealth */
	void UpdateHealthBar();

	/** Retourne le pawn joueur le plus proche (sur le plan horizontal), ou nullptr */
	APawn* FindNearestPlayer() const;

	/** Blesse les joueurs dont la hurtbox touche la Hitbox (serveur) */
	void ApplyContactDamage();

private:

	/** Prochain instant (temps du monde) où chaque joueur touché peut reprendre des dégâts de contact */
	TMap<TWeakObjectPtr<AActor>, double> NextContactDamageTime;
};
