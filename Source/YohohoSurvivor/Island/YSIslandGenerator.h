#pragma once

#include "CoreMinimal.h"
#include "Island/YSIslandData.h"
#include "YSIslandGenerator.generated.h"

/** Type d'un prop du plan */
UENUM()
enum class EYSIslandPropType : uint8
{
	Tree,
	Bush,
	Rock
};

/** Rôle d'un prop du plan */
UENUM()
enum class EYSIslandPropRole : uint8
{
	Lining,
	Interior,
	Core,
	Beach,
	Trio
};

/** Un massif : capsule (axe A-B, rayon). Belt >= 0 : morceau de ce tronçon de la ceinture côté falaise */
USTRUCT()
struct FYSIslandMassif
{
	GENERATED_BODY()

	UPROPERTY()
	FVector2f A = FVector2f::ZeroVector;

	UPROPERTY()
	FVector2f B = FVector2f::ZeroVector;

	UPROPERTY()
	float Radius = 0.0f;

	UPROPERTY()
	int32 Belt = INDEX_NONE;
};

USTRUCT()
struct FYSIslandPropSpawn
{
	GENERATED_BODY()

	UPROPERTY()
	FVector2f Location = FVector2f::ZeroVector;

	UPROPERTY()
	float Yaw = 0.0f;

	UPROPERTY()
	float Scale = 1.0f;

	UPROPERTY()
	EYSIslandPropType Type = EYSIslandPropType::Tree;

	UPROPERTY()
	EYSIslandPropRole Role = EYSIslandPropRole::Lining;
};

/** Résultat de la génération : calculé par le serveur, répliqué aux clients (qui en déduisent le terrain et posent les props) */
USTRUCT()
struct FYSIslandPlan
{
	GENERATED_BODY()

	/** Graine du plan retenu (0 = pas encore de plan) */
	UPROPERTY()
	int32 Seed = 0;

	UPROPERTY()
	TArray<FYSIslandMassif> Massifs;

	UPROPERTY()
	TArray<FYSIslandPropSpawn> Props;

	/** Angle (radians) de chaque ouverture vers l'eau côté falaise */
	UPROPERTY()
	TArray<float> WaterSpots;

	/** Bateau de départ échoué sur la plage */
	UPROPERTY()
	FVector2f BoatLocation = FVector2f::ZeroVector;

	UPROPERTY()
	float BoatYaw = 0.0f;

	/** Où apparaissent les joueurs */
	UPROPERTY()
	TArray<FVector2f> PlayerSpots;

	/** Point le plus dégagé du plateau (élite) */
	UPROPERTY()
	FVector2f CenterSpot = FVector2f::ZeroVector;
};

/** Forme et relief de l'île : ne dépendent que des règles et de la graine (mêmes sur toutes les machines) */
class FYSIslandTerrain
{
public:

	FYSIslandTerrain() = default;
	FYSIslandTerrain(const FYSIslandGenerationRules& InRules, int32 Seed);

	float CoastRadius(float Theta) const;

	/** 1 sur la plage, 0 sur les falaises */
	float BeachWeight(float Theta) const;

	float BeachDepth(float Theta) const;

	/** Distance à la côte le long du rayon (> 0 : sur l'île) */
	float Inland(float X, float Y) const;

	/** Distance à la zone laissée libre le long de la côte (plage, bord de falaise) ; > 0 : un massif peut être là */
	float JungleDepth(float X, float Y) const;

	float Height(float X, float Y) const;

	/** Demi-côté du carré qui contient l'île et ses pentes sous l'eau */
	float GetHalfSize() const { return HalfSize; }

	const FYSIslandGenerationRules& GetRules() const { return Rules; }

private:

	float LandHeight(float X, float Y) const;

	FYSIslandGenerationRules Rules;
	float CoastPhases[3] = { 0.0f, 0.0f, 0.0f };
	float NoisePhases[3] = { 0.0f, 0.0f, 0.0f };
	float HalfSize = 0.0f;
};

/** Génération d'une île : massifs, props, emplacements. Pur calcul (ni monde ni acteur) */
namespace YSIslandGenerator
{
	/** Premier plan qui respecte toutes les règles à partir de cette graine (la suivante sinon) ; si aucun, le dernier essayé (signalé dans le rapport) */
	bool GeneratePlan(const FYSIslandGenerationRules& Rules, int32 Seed, FYSIslandPlan& OutPlan, FString& OutReport);

	/** Distance à l'axe d'un massif moins son rayon : négative à l'intérieur */
	float MassifDistance(const FYSIslandMassif& Massif, const FVector2f& Point);
}
