#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/YSMatchStats.h"
#include "YSScoreboardWidget.generated.h"

class AYSPlayerState;
class UButton;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UTexture2D;

/**
 *  Écran des scores de fin de partie (victoire ou game over), plein écran : résultat et durée de la partie en haut au milieu,
 *  upgrades du joueur à gauche (classe, upgrade d'arme, golden, puis les shards de la plus haute à la plus basse, dans une liste
 *  qui défile), statistiques à droite (valeur du joueur et part de l'équipe), bouton « Retour menu » en bas.
 *  Arbre construit en C++ ; réglages (Layout) dans WBP_Scoreboard (Class Defaults) choisi par AYSPlayerController::ScoreboardClass.
 */
UCLASS()
class UYSScoreboardWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	// --- Haut : résultat et durée ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Title")
	FText VictoryTitle = NSLOCTEXT("YS", "ScoreVictory", "BRAVO");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Title")
	FText GameOverTitle = NSLOCTEXT("YS", "ScoreGameOver", "GAME OVER");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Title")
	FLinearColor VictoryColor = FLinearColor(1.0f, 0.8f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Title")
	FLinearColor GameOverColor = FLinearColor(1.0f, 0.15f, 0.1f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Title", meta=(ClampMin="1"))
	int32 TitleFontSize = 48;

	/** Durée de la partie ({0} = mm:ss ou h:mm:ss) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Title")
	FText DurationFormat = NSLOCTEXT("YS", "ScoreDuration", "Durée de la partie : {0}");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Title", meta=(ClampMin="1"))
	int32 DurationFontSize = 28;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Title")
	FLinearColor DurationColor = FLinearColor::White;

	// --- Fond et marges ---

	/** Voile sur tout l'écran */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor BackgroundColor = FLinearColor(0.02f, 0.03f, 0.06f, 0.94f);

	/** Marges de l'écran (gauche, haut, droite, bas) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FMargin ScreenPadding = FMargin(60.0f, 30.0f, 60.0f, 30.0f);

	/** Espace entre la durée et les deux colonnes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	float ColumnsTopSpacing = 24.0f;

	/** Fond des deux colonnes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor PanelColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FMargin PanelPadding = FMargin(20.0f, 16.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FLinearColor HeaderColor = FLinearColor(1.0f, 0.85f, 0.4f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout", meta=(ClampMin="1"))
	int32 HeaderFontSize = 24;

	// --- Gauche : upgrades ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Upgrades")
	FText UpgradesHeader = NSLOCTEXT("YS", "ScoreUpgradesHeader", "Upgrades");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Upgrades", meta=(ClampMin="100"))
	float UpgradesWidth = 620.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Upgrades", meta=(ClampMin="8"))
	float IconSize = 48.0f;

	/** Espace entre deux lignes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Upgrades")
	float RowSpacing = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Upgrades", meta=(ClampMin="1"))
	int32 UpgradeFontSize = 22;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Upgrades", meta=(ClampMin="1"))
	int32 ShardFontSize = 20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Upgrades")
	FLinearColor UpgradeColor = FLinearColor::White;

	/** Carré derrière l'icône (aussi seul, sans icône) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Upgrades")
	FLinearColor IconBackgroundColor = FLinearColor(0.15f, 0.15f, 0.2f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Upgrades")
	FLinearColor GoldenColor = FLinearColor(1.0f, 0.78f, 0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Upgrades")
	FLinearColor EmptyColor = FLinearColor(0.55f, 0.55f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Upgrades")
	FText NoClassText = NSLOCTEXT("YS", "ScoreNoClass", "Pas d'upgrade de classe");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Upgrades")
	FText NoWeaponText = NSLOCTEXT("YS", "ScoreNoWeapon", "Pas d'upgrade d'arme");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Upgrades")
	FText NoGoldenText = NSLOCTEXT("YS", "ScoreNoGolden", "Pas de golden");

	/** Shard : {0} = nom, {1} = niveau, {2} = niveau max */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Upgrades")
	FText ShardFormat = NSLOCTEXT("YS", "ScoreShard", "{0}   {1} / {2}");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Upgrades")
	FLinearColor ShardColor = FLinearColor(0.75f, 0.9f, 1.0f);

	/** Barre de défilement toujours affichée */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Upgrades")
	bool bAlwaysShowScrollbar = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Upgrades")
	FVector2D ScrollbarThickness = FVector2D(12.0f, 12.0f);

	// --- Droite : statistiques ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FText StatsHeader = NSLOCTEXT("YS", "ScoreStatsHeader", "Statistiques");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats", meta=(ClampMin="100"))
	float StatsWidth = 620.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats", meta=(ClampMin="1"))
	int32 StatLabelFontSize = 18;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats", meta=(ClampMin="1"))
	int32 StatValueFontSize = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FLinearColor StatLabelColor = FLinearColor(0.7f, 0.7f, 0.7f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FLinearColor StatValueColor = FLinearColor::White;

	/** Espace entre deux statistiques */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	float StatSpacing = 14.0f;

	/** Valeur et part de l'équipe : {0} = valeur du joueur, {1} = pourcentage de l'équipe */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FText TeamShareFormat = NSLOCTEXT("YS", "ScoreTeamShare", "{0}   /   {1} % de l'équipe");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FText DamageDealtLabel = NSLOCTEXT("YS", "ScoreDamageDealt", "Dégâts infligés");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FText DamageDealtUnit = NSLOCTEXT("YS", "ScoreDamageDealtUnit", "{0} dommages");

	/** Plus gros coup : valeur seule (pas de part de l'équipe, demande de Paul) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FText BiggestHitLabel = NSLOCTEXT("YS", "ScoreBiggestHit", "Plus gros coup");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FText BiggestHitUnit = NSLOCTEXT("YS", "ScoreBiggestHitUnit", "{0} dommages");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FText KillsLabel = NSLOCTEXT("YS", "ScoreKills", "NME tués");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FText KillsUnit = NSLOCTEXT("YS", "ScoreKillsUnit", "{0} NME");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FText DamageTakenLabel = NSLOCTEXT("YS", "ScoreDamageTaken", "Dégâts subis");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FText DamageTakenUnit = NSLOCTEXT("YS", "ScoreDamageTakenUnit", "{0} dommages");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FText ArmorLabel = NSLOCTEXT("YS", "ScoreArmor", "PV réduits par l'armure");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FText ArmorUnit = NSLOCTEXT("YS", "ScoreArmorUnit", "{0} PV");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FText RevivesLabel = NSLOCTEXT("YS", "ScoreRevives", "Réanimations");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FText RevivesUnit = NSLOCTEXT("YS", "ScoreRevivesUnit", "{0} revive");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FText ItemsLabel = NSLOCTEXT("YS", "ScoreItems", "Objets déposés");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FText ItemsUnit = NSLOCTEXT("YS", "ScoreItemsUnit", "{0} objets");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FText DeathsLabel = NSLOCTEXT("YS", "ScoreDeaths", "Morts");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Stats")
	FText DeathsUnit = NSLOCTEXT("YS", "ScoreDeathsUnit", "{0} morts");

	// --- Bas : bouton ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Button")
	FText ReturnText = NSLOCTEXT("YS", "ScoreReturn", "Retour menu");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Button")
	FVector2D ReturnButtonSize = FVector2D(300.0f, 64.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Button")
	FLinearColor ReturnButtonColor = FLinearColor(1.0f, 0.55f, 0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Button", meta=(ClampMin="1"))
	int32 ReturnFontSize = 28;

	/** Espace au-dessus du bouton */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Button")
	float ReturnTopSpacing = 20.0f;

	/** Clic sur « Retour menu » */
	FSimpleDelegate OnReturnClicked;

	virtual bool Initialize() override;

	/** Remplit l'écran : résultat, durée (s), upgrades du joueur local, ses statistiques et celles de toute l'équipe (Me compris) */
	void SetResult(bool bVictory, float DurationSeconds, const FYSScoreUpgrades& Upgrades, const FYSMatchStats& MyStats, const TArray<FYSMatchStats>& TeamStats);

	/** Bouton « Retour menu » (focus manette) */
	UButton* GetReturnButton() const { return ReturnButton; }

private:

	void BuildWidgetTree();

	UFUNCTION()
	void HandleReturnClicked();

	UTextBlock* MakeText(const FText& Text, int32 FontSize, const FLinearColor& Color, FName Name = NAME_None);

	void AddUpgradeRow(UTexture2D* Icon, const FText& Name, const FLinearColor& Color, bool bShowIcon);

	void AddStat(const FText& Label, const FText& Value);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DurationText;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> UpgradesList;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> StatsList;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ReturnButton;
};
