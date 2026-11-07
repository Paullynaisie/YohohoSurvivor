#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "YSIslandData.generated.h"

class AActor;
class AYSFoeCharacter;
class AYSInteractiveObject;
class AYSStartBoat;
class UMaterialInterface;

/** Type de génération (biome) d'une île (règles dans FYSIslandGenerationRules) */
UENUM(BlueprintType)
enum class EYSIslandGeneration : uint8
{
	/** Plage au sud, falaises ailleurs, jungle en massifs */
	Tropical,
	/** Plage tout autour, palmiers seuls ou en trio répartis régulièrement */
	Desert,
	/** Tropical + ville côté falaise à l'opposé de la plage (maisons à la place de la jungle) et réseau de pontons de la mer à la falaise */
	PortTown
};

/** Où apparaît une vague de NME */
UENUM(BlueprintType)
enum class EYSIslandSpawnShape : uint8
{
	/** Chacun hors écran : autour d'un joueur, jamais à moins de Off Screen Radius de tous les joueurs */
	OffScreen,
	/** Circle spawn : réparti sur le tour de l'île, sauf la plage */
	Circle,
	/** Au milieu de l'île (élite) */
	Center
};

/** Un type de prop de la génération */
USTRUCT(BlueprintType)
struct FYSIslandPropDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prop")
	TSubclassOf<AActor> Class;

	/** Place occupée à l'échelle 1 (jamais hors d'un massif) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prop", meta=(ClampMin="1", Units="Centimeters"))
	float Radius = 100.0f;

	/** Rayon du collider à l'échelle 1 : doit correspondre au Blueprint du prop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prop", meta=(ClampMin="1", Units="Centimeters"))
	float CollisionRadius = 100.0f;

	/** Part de ce prop dans les zones de bord (buissons / rochers) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prop", meta=(ClampMin="0"))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prop", meta=(ClampMin="0.1"))
	float ScaleMin = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prop", meta=(ClampMin="0.1"))
	float ScaleMax = 1.15f;

	/** Enfoncé dans le sol */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prop", meta=(Units="Centimeters"))
	float Sink = 10.0f;
};

/** Un type de trio posé au milieu des grands espaces vides */
USTRUCT(BlueprintType)
struct FYSIslandTrioDef
{
	GENERATED_BODY()

	/** Chance relative d'être tiré */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trio", meta=(ClampMin="0"))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trio", meta=(ClampMin="0.1"))
	float Scale = 0.85f;

	/** Distance entre le milieu du trio et chaque prop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trio", meta=(ClampMin="0", Units="Centimeters"))
	float Circumradius = 95.0f;
};

/**
 *  Règles de génération d'une île (« Génération d'île de type Tropical »). Repères : +X = nord, +Y = est.
 *  La jungle = massifs allongés remplis de props ; une ceinture de massifs longe les falaises avec quelques ouvertures pour sauter à l'eau ;
 *  les couloirs sont l'espace entre les massifs ; la plage au sud reste dégagée. Bord de chaque massif : rangées de buissons et de rochers ; centre : des arbres.
 */
USTRUCT(BlueprintType)
struct FYSIslandGenerationRules
{
	GENERATED_BODY()

	/** Rayon moyen de l'île */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="1 Shape", meta=(ClampMin="1000", Units="Centimeters"))
	float Radius = 5000.0f;

	/** Irrégularité de la côte (amplitude de 3 ondulations) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="1 Shape", meta=(Units="Centimeters"))
	FVector CoastNoise = FVector(270.0f, 155.0f, 85.0f);

	/** Taille d'une maille du terrain */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="1 Shape", meta=(ClampMin="20", Units="Centimeters"))
	float QuadSize = 50.0f;

	/** Matériau du sol (M_IslandGround_Cliff : sable, jungle selon l'altitude, roche sur les pentes) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="1 Shape")
	TObjectPtr<UMaterialInterface> GroundMaterial;

	/** Hauteur du plateau de jungle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="2 Relief", meta=(Units="Centimeters"))
	float PlateauHeight = 350.0f;

	/** Ondulation du plateau (amplitude de 2 ondulations) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="2 Relief", meta=(Units="Centimeters"))
	FVector2D PlateauNoise = FVector2D(30.0, 25.0);

	/** Largeur de la falaise, du haut jusqu'au fond */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="2 Relief", meta=(ClampMin="10", Units="Centimeters"))
	float CliffWidth = 150.0f;

	/** Fond de la mer (sous la deadzone) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="2 Relief", meta=(Units="Centimeters"))
	float SeaFloor = -650.0f;

	/** Hauteur du sol au bord de l'eau, sur la plage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3 Beach", meta=(Units="Centimeters"))
	float ShoreHeight = -22.0f;

	/** Largeur de la pente sous l'eau, devant la plage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3 Beach", meta=(ClampMin="10", Units="Centimeters"))
	float ShoreSlopeWidth = 1300.0f;

	/** Angle depuis le sud : plage complète jusque-là... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3 Beach", meta=(ClampMin="0", ClampMax="180", Units="Degrees"))
	float BeachAngleFull = 45.0f;

	/** ... falaise à partir de celui-là */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3 Beach", meta=(ClampMin="0", ClampMax="180", Units="Degrees"))
	float BeachAngleEnd = 65.0f;

	/** Profondeur de la plage au sud */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3 Beach", meta=(ClampMin="0", Units="Centimeters"))
	float BeachDepth = 2000.0f;

	/** Profondeur aux extrémités du croissant */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3 Beach", meta=(ClampMin="0", Units="Centimeters"))
	float BeachDepthMin = 400.0f;

	/** Props isolés sur la plage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3 Beach", meta=(ClampMin="0"))
	int32 BeachTreeCount = 7;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3 Beach", meta=(ClampMin="0"))
	int32 BeachRockCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="3 Beach", meta=(ClampMin="0", Units="Centimeters"))
	float BeachPropSpacing = 800.0f;

	/** Largeur minimale d'un couloir, en joueurs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="4 Corridors", meta=(ClampMin="1"))
	float CorridorPlayersMin = 15.0f;

	/** Massif intérieur et falaise (devant une ouverture), le long du rayon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="4 Corridors", meta=(ClampMin="0", Units="Centimeters"))
	float RimWidth = 1500.0f;

	/** Épaisseur de la ceinture de massifs côté falaise */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="5 Cliff Belt", meta=(ClampMin="100", Units="Centimeters"))
	FVector2D BeltThickness = FVector2D(1200.0, 1900.0);

	/** Distance entre deux points de la ceinture */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="5 Cliff Belt", meta=(ClampMin="100", Units="Centimeters"))
	float BeltStep = 500.0f;

	/** La ceinture dépasse de la falaise : aucun passage derrière les props */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="5 Cliff Belt", meta=(ClampMin="0", Units="Centimeters"))
	float BeltOverhang = 100.0f;

	/** Nombre d'ouvertures pour sauter à l'eau côté falaise (min, max) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="5 Cliff Belt")
	FIntPoint WaterAccessCount = FIntPoint(3, 4);

	/** Largeur de chaque ouverture, en joueurs (min, max) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="5 Cliff Belt")
	FVector2D WaterAccessPlayers = FVector2D(15.0, 20.0);

	/** Longueur de l'axe d'un massif intérieur (min, max) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="6 Massifs", meta=(Units="Centimeters"))
	FVector2D MassifLength = FVector2D(800.0, 3500.0);

	/** Rayon d'un massif intérieur (min, max) ; le minimum doit laisser la place aux rangées de bord et aux arbres */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="6 Massifs", meta=(Units="Centimeters"))
	FVector2D MassifRadius = FVector2D(550.0, 1000.0);

	/** Essais de pose sans succès avant de considérer l'île pleine */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="6 Massifs", meta=(ClampMin="1"))
	int32 MassifAttempts = 300;

	/** Essais complets : on garde celui qui couvre le plus de surface */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="6 Massifs", meta=(ClampMin="1"))
	int32 MassifLayouts = 40;

	/** Moins de massifs intérieurs que ça : plan refusé */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="6 Massifs", meta=(ClampMin="0"))
	int32 MassifCountMin = 2;

	/** Plus petit massif posé pour remplir un grand vide */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="6 Massifs", meta=(Units="Centimeters"))
	float FillerMassifRadius = 550.0f;

	/** Rangées de buissons et de rochers sur le bord de chaque massif */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="7 Massif Filling", meta=(ClampMin="1"))
	int32 RingRowCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="7 Massif Filling", meta=(ClampMin="1", Units="Centimeters"))
	float RingRowWidth = 230.0f;

	/** Épaisseur du bord ; au-delà, que des arbres jusqu'au centre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="7 Massif Filling", meta=(ClampMin="0", Units="Centimeters"))
	float CoreRingWidth = 500.0f;

	/** Le bord ne prend jamais plus que cette part du rayon : il reste toujours un centre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="7 Massif Filling", meta=(ClampMin="0", ClampMax="1"))
	float CoreRingRatio = 0.95f;

	/** Zones de même type sur le bord (bosquets, éboulis) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="7 Massif Filling", meta=(ClampMin="1"))
	int32 PatchCount = 30;

	/** Rangées du bord : props serrés (distance entre centres en part de la somme des rayons) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="7 Massif Filling", meta=(ClampMin="0.1", ClampMax="1"))
	float LiningOverlap = 0.8f;

	/** Centre des massifs : petit espace entre les props */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="7 Massif Filling", meta=(ClampMin="0", Units="Centimeters"))
	float InteriorGap = 30.0f;

	/** Arbre (centre des massifs, plage, trios) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="8 Props")
	FYSIslandPropDef TreeProp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="8 Props")
	FYSIslandPropDef BushProp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="8 Props")
	FYSIslandPropDef RockProp;

	/** Espace vide plus large que ça, en joueurs : un trio en son milieu (pas sur la plage) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="9 Trios", meta=(ClampMin="1"))
	float TrioEmptyPlayers = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="9 Trios")
	FYSIslandTrioDef TreeTrio;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="9 Trios")
	FYSIslandTrioDef BushTrio;

	/** Désert : distance entre deux palmiers (ou trios) de la grille régulière */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 Desert", meta=(ClampMin="200", Units="Centimeters"))
	float DesertPalmSpacing = 1600.0f;

	/** Décalage au hasard de chaque point de la grille, en part de l'espacement */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 Desert", meta=(ClampMin="0", ClampMax="0.5"))
	float DesertPalmJitter = 0.3f;

	/** Chance qu'un point de la grille soit un trio plutôt qu'un palmier seul */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 Desert", meta=(ClampMin="0", ClampMax="1"))
	float DesertTrioChance = 0.4f;

	/** Pas de palmier à moins de cette distance du bord de l'eau */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 Desert", meta=(ClampMin="0", Units="Centimeters"))
	float DesertShoreMargin = 400.0f;

	/** Port Town : la ville occupe les massifs dont le milieu est à moins de cet angle du nord (à l'opposé de la plage) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="11 Port Town", meta=(ClampMin="0", ClampMax="180", Units="Degrees"))
	float TownHalfAngle = 65.0f;

	/** ... et à plus de cette part du rayon du milieu de l'île (pas jusqu'au centre) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="11 Port Town", meta=(ClampMin="0", ClampMax="1"))
	float TownInnerRatio = 0.3f;

	/** Petite maison (~5 PJ de côté) : Radius = demi-diagonale (place), Collision Radius = demi-côté (contrôles de passage) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="11 Port Town")
	FYSIslandPropDef SmallHouseProp;

	/** Grande maison (~10 PJ de côté) ; Weight = part des grandes maisons dans les rangées du bord (avec celui des petites) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="11 Port Town")
	FYSIslandPropDef BigHouseProp;

	/** Espace minimal entre deux maisons */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="11 Port Town", meta=(ClampMin="0", Units="Centimeters"))
	float HouseGap = 40.0f;

	/** Massifs de la ville : presque que des maisons ; chance qu'une place libre entre elles reçoive quand même un palmier */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="11 Port Town", meta=(ClampMin="0", ClampMax="1"))
	float TownTreeChance = 0.05f;

	/** Bouchons de la ville (trous du bord des massifs) : petites maisons ; réduites à cette échelle quand une maison entière ne tient pas */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="11 Port Town", meta=(ClampMin="0.2", ClampMax="1"))
	float TownPlugHouseScale = 0.6f;

	/** Pontons : largeur du ponton de la côte et des quais (15 PJ) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="12 Piers", meta=(ClampMin="50", Units="Centimeters"))
	float PierWidth = 1260.0f;

	/** Rampes qui descendent du haut de la falaise : largeur ; elles longent la falaise, posées sur le ponton */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="12 Piers", meta=(ClampMin="50", Units="Centimeters"))
	float PierRampWidth = 420.0f;

	/** Pente max des rampes de la falaise (leur longueur en dépend) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="12 Piers", meta=(ClampMin="5", ClampMax="40", Units="Degrees"))
	float PierRampMaxSlope = 25.0f;

	/** Dessus des pontons au-dessus de la mer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="12 Piers", meta=(Units="Centimeters"))
	float PierDeckHeight = 25.0f;

	/** Épaisseur des planches */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="12 Piers", meta=(ClampMin="5", Units="Centimeters"))
	float PierThickness = 30.0f;

	/** Écart entre le pied de la falaise et le bord intérieur du ponton de la côte (0 = collé à la falaise) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="12 Piers", meta=(ClampMin="0", Units="Centimeters"))
	float PierCoastOffset = 0.0f;

	/** Quai au pied de chaque rampe, vers le large */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="12 Piers", meta=(ClampMin="0", Units="Centimeters"))
	float PierDockLength = 500.0f;

	/** Accès depuis l'eau : pente qui descend du ponton jusqu'à cette profondeur sous la mer, sur cette longueur */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="12 Piers", meta=(ClampMin="0", Units="Centimeters"))
	float PierWaterRampDepth = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="12 Piers", meta=(ClampMin="100", Units="Centimeters"))
	float PierWaterRampLength = 800.0f;

	/** Accès depuis l'eau aussi tous les ... le long du ponton de la côte */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="12 Piers", meta=(ClampMin="500", Units="Centimeters"))
	float PierWaterAccessSpacing = 1500.0f;

	/** Longueur d'un morceau du ponton de la côte (il suit la courbe) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="12 Piers", meta=(ClampMin="100", Units="Centimeters"))
	float PierStep = 400.0f;

	/** Poteaux sous les pontons, tous les ... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="12 Piers", meta=(ClampMin="100", Units="Centimeters"))
	float PierPostSpacing = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="12 Piers")
	TObjectPtr<UMaterialInterface> PierMaterial;
};

/** Un objectif : un objet à trouver sur l'île et à ramener dans la zone du bateau de départ */
USTRUCT(BlueprintType)
struct FYSIslandObjectiveDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective")
	FText DisplayName;

	/** Objet posé sur l'île (une carte au trésor génère son chemin) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective")
	TSubclassOf<AYSInteractiveObject> ItemClass;

	/** Objet à ramener, s'il est différent de celui posé (carte au trésor : le coffre qu'elle fait apparaître) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective")
	TSubclassOf<AYSInteractiveObject> DeliveryClass;

	/** Distance au bateau de départ (min, max) où l'objet apparaît */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective", meta=(Units="Centimeters"))
	FVector2D DistanceFromBoat = FVector2D(3000.0, 9000.0);

	/** Jour de l'île où l'objet apparaît (1 = dès le début) ; tant qu'il n'est pas apparu, l'objectif compte comme ouvert */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective", meta=(ClampMin="1"))
	int32 SpawnDay = 1;

	/** Apparaît seulement dans la ville (Port Town) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective")
	bool bInTown = false;
};

/** Un type de NME d'une vague */
USTRUCT(BlueprintType)
struct FYSIslandFoeEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe")
	TSubclassOf<AYSFoeCharacter> FoeClass;

	/** Non vide : chaque NME est tiré au hasard dans cette liste (Foe Class est ignoré) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe")
	TArray<TSubclassOf<AYSFoeCharacter>> RandomFoeClasses;

	/** Nombre pour 1 joueur (x Spawn Multiplier du nombre de joueurs si Scale With Players) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Foe", meta=(ClampMin="0"))
	int32 Count = 1;
};

/** Une vague de NME d'un cycle, comptée depuis le début de la phase (jour, nuit, coulaison) */
USTRUCT(BlueprintType)
struct FYSIslandSpawnEvent
{
	GENERATED_BODY()

	/** Secondes après le début de la phase */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0", Units="s"))
	float Time = 0.0f;

	/** Seulement le jour (ou la nuit) N de l'île (0 = tous les jours) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0"))
	int32 IslandDay = 0;

	/** Avec Island Day : aussi tous les Island Day Period jours après (ex. Island Day 1, période 2 = jours 1, 3, 5...) ; 0 = seulement Island Day */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0"))
	int32 IslandDayPeriod = 0;

	/** Plus après ce jour de l'île (0 = pas de limite) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0"))
	int32 IslandDayMax = 0;

	/** La vague part le jour (ou la nuit) Day de l'île */
	bool IsForIslandDay(int32 Day) const
	{
		if (IslandDayMax > 0 && Day > IslandDayMax)
		{
			return false;
		}
		if (IslandDay <= 0)
		{
			return true;
		}
		return IslandDayPeriod > 0 ? Day >= IslandDay && (Day - IslandDay) % IslandDayPeriod == 0 : Day == IslandDay;
	}

	/** Nombre de fois que la vague part (0 = jusqu'à la fin de la phase) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0"))
	int32 RepeatCount = 1;

	/** Secondes entre deux répétitions */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0", Units="s"))
	float RepeatInterval = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn")
	EYSIslandSpawnShape Shape = EYSIslandSpawnShape::OffScreen;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn")
	TArray<FYSIslandFoeEntry> Foes;

	/** Nombre x Spawn Multiplier du nombre de joueurs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn")
	bool bScaleWithPlayers = true;

	/** Règles d'overtime : niveau imposé = jour en cours + Level Offset */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Level")
	bool bOvertimeLevel = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Level", meta=(EditCondition="bOvertimeLevel"))
	int32 LevelOffset = 3;

	/** Niveaux de plus à chaque répétition */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Level", meta=(ClampMin="0"))
	int32 LevelStepPerRepeat = 0;
};

/**
 *  Une île (DA_I_...) : tout ce qui la définit. À copier pour créer une nouvelle île.
 *  Plusieurs îles peuvent être actives en même temps, chacune dans sa phase : le jour global passe au suivant dès qu'une nuit se termine quelque part.
 */
UCLASS(BlueprintType, meta=(PrioritizeCategories="Generation Objectives Time DayCycle NightCycle SinkRoutine Start"))
class UYSIslandData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Nom de l'île (logs) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Generation")
	FText IslandName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Generation")
	EYSIslandGeneration GenerationType = EYSIslandGeneration::Tropical;

	/** Non vide : le biome est tiré au hasard dans cette liste à chaque génération (Generation Type est ignoré) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Generation")
	TArray<EYSIslandGeneration> RandomGenerationTypes;

	/** L'île est tournée au hasard autour de son centre : la plage n'est pas forcément au sud (jamais pour l'île de départ) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Generation")
	bool bRandomRotation = false;

	/** Graine fixe de la génération (0 = une île différente à chaque partie) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Generation")
	int32 FixedSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Generation")
	FYSIslandGenerationRules Generation;

	/** Tous accomplis : l'île passe au jour suivant et lance sa routine de coulaison */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objectives")
	TArray<FYSIslandObjectiveDef> MainObjectives;

	/** Sans effet sur la coulaison */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objectives")
	TArray<FYSIslandObjectiveDef> SecondaryObjectives;

	/** Temps pour finir les objectifs : quand le jour global atteint jour d'activation + Max Days - 1, l'île lance sa routine de coulaison */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Time", meta=(ClampMin="1", EditCondition="!bInfiniteDays"))
	int32 MaxDays = 2;

	/** Jours infinis : l'île ne coule jamais (ni Max Days, ni objectifs principaux accomplis), ses jours et ses nuits s'enchaînent (île finale) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Time")
	bool bInfiniteDays = false;

	/** Secondes entre le début du jour de l'île et la tombée de la nuit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Time", meta=(ClampMin="1", Units="s"))
	float DayDuration = 90.0f;

	/** Vagues du jour : comptées depuis l'activation de l'île */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DayCycle")
	TArray<FYSIslandSpawnEvent> DayEvents;

	/** Vagues de la nuit : comptées depuis la tombée de la nuit. La nuit se termine à la mort de l'élite */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NightCycle")
	TArray<FYSIslandSpawnEvent> NightEvents;

	/** Vagues de la routine de coulaison : comptées depuis son début */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SinkRoutine")
	TArray<FYSIslandSpawnEvent> SinkEvents;

	/** Secondes entre le début de la routine et le moment où l'île coule */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SinkRoutine", meta=(ClampMin="0", Units="s"))
	float SinkDelay = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SinkRoutine", meta=(ClampMin="0.1", Units="s"))
	float SinkDuration = 30.0f;

	/** Descente totale : doit passer sous la deadzone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SinkRoutine", meta=(ClampMin="0", Units="Centimeters"))
	float SinkDepth = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SinkRoutine", meta=(ClampMin="0.1"))
	float SinkEaseExponent = 1.5f;

	/** Bateau de départ échoué sur la plage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Start")
	TSubclassOf<AYSStartBoat> StartBoatClass;

	/** Zone autour du bateau de départ qui valide la pose des objectifs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Start", meta=(ClampMin="0", Units="Centimeters"))
	float BoatZoneRadius = 1500.0f;

	/** Vagues « hors écran » : jamais à moins de cette distance de tous les joueurs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Start", meta=(ClampMin="0", Units="Centimeters"))
	float OffScreenRadius = 1500.0f;

	/** ... et à moins de cette largeur en plus d'un des joueurs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Start", meta=(ClampMin="0", Units="Centimeters"))
	float OffScreenBand = 1000.0f;
};
