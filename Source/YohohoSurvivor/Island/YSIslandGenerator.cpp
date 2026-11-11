#include "Island/YSIslandGenerator.h"
#include "Math/RandomStream.h"

namespace YSIslandGen
{
	constexpr float PlayerRadius = 42.0f;
	constexpr float SeaLevel = -30.0f;
	constexpr float GridStep = 20.0f;
	/** Plans essayés à partir de la graine demandée */
	constexpr int32 PlanAttempts = 60;
	/** Un PJ qui entre de plus de ça dans un massif : trou à boucher (en dessous : petit renfoncement) */
	constexpr float LeakDepth = 110.0f;
	constexpr int32 PlugPasses = 40;
	constexpr float PlugSpacing = 140.0f;
	constexpr float FillerStep = 250.0f;

	float Smoothstep(float T)
	{
		T = FMath::Clamp(T, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	float PointSegmentDistance(const FVector2f& P, const FVector2f& A, const FVector2f& B)
	{
		const FVector2f AB = B - A;
		const float LengthSq = AB.SizeSquared();
		const float T = LengthSq <= 0.0f ? 0.0f : FMath::Clamp(FVector2f::DotProduct(P - A, AB) / LengthSq, 0.0f, 1.0f);
		return (P - A - AB * T).Size();
	}

	float SegmentDistance(const FVector2f& A, const FVector2f& B, const FVector2f& C, const FVector2f& D)
	{
		float Best = TNumericLimits<float>::Max();
		for (int32 K = 0; K <= 20; ++K)
		{
			const float T = K / 20.0f;
			Best = FMath::Min(Best, PointSegmentDistance(FMath::Lerp(A, B, T), C, D));
			Best = FMath::Min(Best, PointSegmentDistance(FMath::Lerp(C, D, T), A, B));
		}
		return Best;
	}

	float MassifGap(const FYSIslandMassif& M1, const FYSIslandMassif& M2)
	{
		return SegmentDistance(M1.A, M1.B, M2.A, M2.B) - M1.Radius - M2.Radius;
	}

	float CoreRadius(const FYSIslandMassif& Massif, const FYSIslandGenerationRules& Rules)
	{
		return Massif.Radius - FMath::Min(Rules.CoreRingWidth, Massif.Radius * Rules.CoreRingRatio);
	}

	const FYSIslandPropDef& PropDef(const FYSIslandGenerationRules& Rules, EYSIslandPropType Type)
	{
		switch (Type)
		{
		case EYSIslandPropType::Bush:		return Rules.BushProp;
		case EYSIslandPropType::Rock:		return Rules.RockProp;
		case EYSIslandPropType::SmallHouse:	return Rules.SmallHouseProp;
		case EYSIslandPropType::BigHouse:	return Rules.BigHouseProp;
		default:							return Rules.TreeProp;
		}
	}

	bool IsHouse(EYSIslandPropType Type)
	{
		return Type == EYSIslandPropType::SmallHouse || Type == EYSIslandPropType::BigHouse;
	}

	/** Angle (radians, -PI..PI) d'un angle quelconque */
	float NormalizeAngle(float Theta)
	{
		return FMath::Atan2(FMath::Sin(Theta), FMath::Cos(Theta));
	}

	/** Prop du plan avec ses rayons pour les calculs */
	struct FPlacedProp
	{
		FYSIslandPropSpawn Spawn;
		float Radius = 0.0f;
		float Collision = 0.0f;
	};

	/** Grille de contrôle sur tout le carré de l'île */
	struct FGrid
	{
		float Half = 0.0f;
		int32 N = 0;

		explicit FGrid(float InHalf) : Half(InHalf), N(FMath::CeilToInt32(InHalf * 2.0f / GridStep) + 1) {}

		int32 Total() const { return N * N; }
		FVector2f World(int32 I, int32 J) const { return FVector2f(-Half + I * GridStep, -Half + J * GridStep); }
		int32 IndexOf(const FVector2f& P) const
		{
			const int32 I = FMath::Clamp(FMath::FloorToInt32((P.X + Half) / GridStep), 0, N - 1);
			const int32 J = FMath::Clamp(FMath::FloorToInt32((P.Y + Half) / GridStep), 0, N - 1);
			return J * N + I;
		}
	};

	void ForEachCellAround(const FGrid& Grid, const FVector2f& Min, const FVector2f& Max, TFunctionRef<void(int32, const FVector2f&)> Visit)
	{
		const int32 I0 = FMath::Clamp(FMath::FloorToInt32((Min.X + Grid.Half) / GridStep), 0, Grid.N - 1);
		const int32 J0 = FMath::Clamp(FMath::FloorToInt32((Min.Y + Grid.Half) / GridStep), 0, Grid.N - 1);
		const int32 I1 = FMath::Clamp(FMath::CeilToInt32((Max.X + Grid.Half) / GridStep), 0, Grid.N - 1);
		const int32 J1 = FMath::Clamp(FMath::CeilToInt32((Max.Y + Grid.Half) / GridStep), 0, Grid.N - 1);
		for (int32 J = J0; J <= J1; ++J)
		{
			for (int32 I = I0; I <= I1; ++I)
			{
				Visit(J * Grid.N + I, Grid.World(I, J));
			}
		}
	}

	/** Distance approchée (cm) à la case marquée la plus proche : deux passes de chanfrein */
	void DistanceField(const FGrid& Grid, const TArray<uint8>& Marked, TArray<float>& Out)
	{
		const int32 N = Grid.N;
		const float Orth = GridStep;
		const float Diag = GridStep * 1.414f;
		Out.SetNumUninitialized(Grid.Total());
		for (int32 K = 0; K < Grid.Total(); ++K)
		{
			Out[K] = Marked[K] ? 0.0f : 1e9f;
		}
		for (int32 J = 0; J < N; ++J)
		{
			for (int32 I = 0; I < N; ++I)
			{
				const int32 K = J * N + I;
				float V = Out[K];
				if (V == 0.0f)
				{
					continue;
				}
				if (I > 0) { V = FMath::Min(V, Out[K - 1] + Orth); }
				if (J > 0)
				{
					V = FMath::Min(V, Out[K - N] + Orth);
					if (I > 0) { V = FMath::Min(V, Out[K - N - 1] + Diag); }
					if (I < N - 1) { V = FMath::Min(V, Out[K - N + 1] + Diag); }
				}
				Out[K] = V;
			}
		}
		for (int32 J = N - 1; J >= 0; --J)
		{
			for (int32 I = N - 1; I >= 0; --I)
			{
				const int32 K = J * N + I;
				float V = Out[K];
				if (V == 0.0f)
				{
					continue;
				}
				if (I < N - 1) { V = FMath::Min(V, Out[K + 1] + Orth); }
				if (J < N - 1)
				{
					V = FMath::Min(V, Out[K + N] + Orth);
					if (I < N - 1) { V = FMath::Min(V, Out[K + N + 1] + Diag); }
					if (I > 0) { V = FMath::Min(V, Out[K + N - 1] + Diag); }
				}
				Out[K] = V;
			}
		}
	}

	void StampDiscs(const FGrid& Grid, const TArray<FPlacedProp>& Props, float Extra, TArray<uint8>& InOut)
	{
		for (const FPlacedProp& Prop : Props)
		{
			const float R = Prop.Collision + Extra;
			const FVector2f Center = Prop.Spawn.Location;
			const FVector2f Reach(R + GridStep, R + GridStep);
			ForEachCellAround(Grid, Center - Reach, Center + Reach, [&](int32 K, const FVector2f& P)
			{
				if ((P - Center).SizeSquared() <= R * R)
				{
					InOut[K] = 1;
				}
			});
		}
	}

	/** Cases interdites à pied : pente > 44°, sous la mer */
	void TerrainMask(const FYSIslandTerrain& Terrain, const FGrid& Grid, TArray<uint8>& Out)
	{
		const float Q = Terrain.GetRules().QuadSize;
		const int32 Size = FMath::CeilToInt32(Grid.Half * 2.0f / Q) + 1;
		TArray<float> Heights;
		Heights.SetNumUninitialized(Size * Size);
		for (int32 J = 0; J < Size; ++J)
		{
			for (int32 I = 0; I < Size; ++I)
			{
				Heights[J * Size + I] = Terrain.Height(-Grid.Half + I * Q, -Grid.Half + J * Q);
			}
		}
		const float Limit = Q * FMath::Tan(FMath::DegreesToRadians(44.0f));
		TArray<uint8> Blocked;
		Blocked.SetNumZeroed(Size * Size);
		for (int32 J = 0; J < Size; ++J)
		{
			for (int32 I = 0; I < Size; ++I)
			{
				const float H = Heights[J * Size + I];
				bool bBlocked = H < SeaLevel;
				if (!bBlocked)
				{
					const int32 NI[4] = { I + 1, I - 1, I, I };
					const int32 NJ[4] = { J, J, J + 1, J - 1 };
					for (int32 D = 0; D < 4 && !bBlocked; ++D)
					{
						bBlocked = NI[D] >= 0 && NI[D] < Size && NJ[D] >= 0 && NJ[D] < Size && FMath::Abs(Heights[NJ[D] * Size + NI[D]] - H) > Limit;
					}
				}
				Blocked[J * Size + I] = bBlocked ? 1 : 0;
			}
		}
		Out.SetNumUninitialized(Grid.Total());
		const float Ratio = GridStep / Q;
		for (int32 J = 0; J < Grid.N; ++J)
		{
			const int32 TJ = FMath::Min(Size - 1, FMath::RoundToInt32(J * Ratio));
			for (int32 I = 0; I < Grid.N; ++I)
			{
				Out[J * Grid.N + I] = Blocked[TJ * Size + FMath::Min(Size - 1, FMath::RoundToInt32(I * Ratio))];
			}
		}
	}

	bool Flood(const FGrid& Grid, const TArray<uint8>& Blocked, const FVector2f& Start, TArray<uint8>& Seen)
	{
		const int32 N = Grid.N;
		const int32 S = Grid.IndexOf(Start);
		Seen.SetNumZeroed(Grid.Total());
		FMemory::Memzero(Seen.GetData(), Seen.Num());
		if (Blocked[S])
		{
			return false;
		}
		Seen[S] = 1;
		TArray<int32> Stack;
		Stack.Reserve(4096);
		Stack.Add(S);
		const int32 Total = Grid.Total();
		while (Stack.Num() > 0)
		{
			const int32 K = Stack.Pop(EAllowShrinking::No);
			const int32 Next[4] = { K + 1, K - 1, K + N, K - N };
			for (int32 D = 0; D < 4; ++D)
			{
				const int32 NK = Next[D];
				if (NK >= 0 && NK < Total && !Seen[NK] && !Blocked[NK])
				{
					Seen[NK] = 1;
					Stack.Add(NK);
				}
			}
		}
		return true;
	}

	/** Ceinture côté falaise : tronçons de capsules qui suivent la côte, séparés par les ouvertures pour sauter à l'eau */
	void PlaceBelt(const FYSIslandTerrain& Terrain, FRandomStream& Rng, TArray<FYSIslandMassif>& OutPieces, TArray<float>& OutSpots)
	{
		const FYSIslandGenerationRules& Rules = Terrain.GetRules();
		const float PlayerWidth = PlayerRadius * 2.0f;
		const float Low = Rules.WaterAccessPlayers.X * PlayerWidth + 20.0f;
		const float High = Rules.WaterAccessPlayers.Y * PlayerWidth;
		// côté ouest de la plage (Y < 0), en tournant par le nord
		const float EndAngle = FMath::DegreesToRadians(Rules.BeachAngleEnd);
		const float Start = PI + EndAngle;
		const float Arc = 2.0f * PI - 2.0f * EndAngle;
		const int32 Count = Rng.RandRange(Rules.WaterAccessCount.X, Rules.WaterAccessCount.Y);

		TArray<float> Bounds;
		Bounds.Add(Start);
		for (int32 K = 0; K < Count; ++K)
		{
			// ouvertures réparties régulièrement le long de la falaise, un peu décalées
			const float Spot = Start + Arc * (K + 0.5f + Rng.FRandRange(-0.15f, 0.15f)) / Count;
			OutSpots.Add(Spot);
			const float Half = Rng.FRandRange(Low, High) / 2.0f / Terrain.CoastRadius(Spot);
			Bounds.Add(Spot - Half);
			Bounds.Add(Spot + Half);
		}
		Bounds.Add(Start + Arc);

		int32 Belt = 0;
		for (int32 Index = 0; Index + 1 < Bounds.Num(); Index += 2, ++Belt)
		{
			const float Begin = Bounds[Index];
			const float End = Bounds[Index + 1];
			// épaisseur qui varie doucement le long du tronçon
			float Thick = Rng.FRandRange(Rules.BeltThickness.X, Rules.BeltThickness.Y);
			const int32 Steps = FMath::Max(2, FMath::FloorToInt32((End - Begin) * Terrain.CoastRadius(Begin) / Rules.BeltStep));
			TArray<float> Thicknesses;
			for (int32 K = 0; K <= Steps; ++K)
			{
				Thick = FMath::Clamp(Thick + Rng.FRandRange(-150.0f, 150.0f), static_cast<float>(Rules.BeltThickness.X), static_cast<float>(Rules.BeltThickness.Y));
				Thicknesses.Add(Thick);
			}
			// les bouts arrondis ne doivent pas mordre sur l'ouverture
			const float FirstMargin = Thicknesses[0] / 2.0f / Terrain.CoastRadius(Begin);
			const float LastMargin = Thicknesses.Last() / 2.0f / Terrain.CoastRadius(End);
			FVector2f Previous = FVector2f::ZeroVector;
			float PreviousRadius = 0.0f;
			for (int32 K = 0; K <= Steps; ++K)
			{
				const float Angle = Begin + FirstMargin + (End - Begin - FirstMargin - LastMargin) * K / Steps;
				const float R = Terrain.CoastRadius(Angle) - (Thicknesses[K] / 2.0f - Rules.BeltOverhang);
				const FVector2f Point(R * FMath::Cos(Angle), R * FMath::Sin(Angle));
				const float Radius = Thicknesses[K] / 2.0f;
				if (K > 0)
				{
					FYSIslandMassif& Piece = OutPieces.AddDefaulted_GetRef();
					Piece.A = Previous;
					Piece.B = Point;
					Piece.Radius = FMath::Max(Radius, PreviousRadius);
					Piece.Belt = Belt;
				}
				Previous = Point;
				PreviousRadius = Radius;
			}
		}
	}

	bool MassifFitsAwayFromCoast(const FYSIslandTerrain& Terrain, const FVector2f& Center, const FVector2f& HalfAxis, float Radius)
	{
		for (int32 K = -4; K <= 4; ++K)
		{
			const FVector2f P = Center + HalfAxis * (K / 4.0f);
			if (Terrain.JungleDepth(P.X, P.Y) < Radius)
			{
				return false;
			}
		}
		return true;
	}

	/** Tant qu'un massif peut encore tenir à un couloir minimum de tout le reste : on le pose au milieu du plus grand vide */
	void FillLargeVoids(const FYSIslandTerrain& Terrain, FRandomStream& Rng, const TArray<FYSIslandMassif>& Belt, TArray<FYSIslandMassif>& Massifs, float Low)
	{
		const FYSIslandGenerationRules& Rules = Terrain.GetRules();
		const float Smallest = Rules.FillerMassifRadius;
		const float Largest = Rules.MassifRadius.Y;
		TArray<FVector2f> Spots;
		for (float X = -Rules.Radius; X <= Rules.Radius; X += FillerStep)
		{
			for (float Y = -Rules.Radius; Y <= Rules.Radius; Y += FillerStep)
			{
				if (Terrain.JungleDepth(X, Y) >= Smallest)
				{
					Spots.Emplace(X, Y);
				}
			}
		}

		while (true)
		{
			float BestRoom = 0.0f;
			FVector2f BestSpot = FVector2f::ZeroVector;
			bool bFound = false;
			for (const FVector2f& Spot : Spots)
			{
				float Room = 1e9f;
				for (const FYSIslandMassif& M : Belt) { Room = FMath::Min(Room, YSIslandGenerator::MassifDistance(M, Spot)); }
				for (const FYSIslandMassif& M : Massifs) { Room = FMath::Min(Room, YSIslandGenerator::MassifDistance(M, Spot)); }
				if (Room > BestRoom)
				{
					BestRoom = Room;
					BestSpot = Spot;
					bFound = true;
				}
			}
			if (!bFound)
			{
				return;
			}
			const float Radius = FMath::Min3(Largest, BestRoom - Low, Terrain.JungleDepth(BestSpot.X, BestSpot.Y));
			if (Radius < Smallest)
			{
				return;
			}

			// le plus long possible dans une direction au hasard ; un massif rond si rien d'autre ne passe
			FYSIslandMassif Placed;
			float PlacedLength = -1.0f;
			for (int32 Attempt = 0; Attempt < 40; ++Attempt)
			{
				const float Angle = Rng.FRandRange(0.0f, PI);
				const float Length = Rng.FRandRange(100.0f, Rules.MassifLength.Y);
				const FVector2f HalfAxis(FMath::Cos(Angle) * Length / 2.0f, FMath::Sin(Angle) * Length / 2.0f);
				if (Length <= PlacedLength || !MassifFitsAwayFromCoast(Terrain, BestSpot, HalfAxis, Radius))
				{
					continue;
				}
				FYSIslandMassif Candidate;
				Candidate.A = BestSpot - HalfAxis;
				Candidate.B = BestSpot + HalfAxis;
				Candidate.Radius = Radius;
				bool bFits = true;
				for (const FYSIslandMassif& M : Belt) { bFits = bFits && MassifGap(Candidate, M) >= Low; }
				for (const FYSIslandMassif& M : Massifs) { bFits = bFits && MassifGap(Candidate, M) >= Low; }
				if (bFits)
				{
					Placed = Candidate;
					PlacedLength = Length;
				}
			}
			if (PlacedLength < 0.0f)
			{
				Placed.A = BestSpot - FVector2f(50.0f, 0.0f);
				Placed.B = BestSpot + FVector2f(50.0f, 0.0f);
				Placed.Radius = FMath::Max(Smallest, Radius - 60.0f);
			}
			Massifs.Add(Placed);
		}
	}

	/** Massifs intérieurs : plusieurs essais complets, on garde celui qui couvre le plus de surface, puis on remplit les grands vides */
	void PlaceInteriorMassifs(const FYSIslandTerrain& Terrain, FRandomStream& Rng, const TArray<FYSIslandMassif>& Belt, TArray<FYSIslandMassif>& OutMassifs)
	{
		const FYSIslandGenerationRules& Rules = Terrain.GetRules();
		const float Low = Rules.CorridorPlayersMin * PlayerRadius * 2.0f + 20.0f;
		const float Reach = Rules.Radius;
		float BestArea = -1.0f;
		for (int32 Layout = 0; Layout < Rules.MassifLayouts; ++Layout)
		{
			TArray<FYSIslandMassif> Massifs;
			float Area = 0.0f;
			int32 Fails = 0;
			while (Fails < Rules.MassifAttempts)
			{
				++Fails;
				const FVector2f Center(Rng.FRandRange(-Reach, Reach), Rng.FRandRange(-Reach, Reach));
				const float Angle = Rng.FRandRange(0.0f, PI);
				const float Length = Rng.FRandRange(Rules.MassifLength.X, Rules.MassifLength.Y);
				const float Radius = Rng.FRandRange(Rules.MassifRadius.X, Rules.MassifRadius.Y);
				const FVector2f HalfAxis(FMath::Cos(Angle) * Length / 2.0f, FMath::Sin(Angle) * Length / 2.0f);
				if (!MassifFitsAwayFromCoast(Terrain, Center, HalfAxis, Radius))
				{
					continue;
				}
				FYSIslandMassif Candidate;
				Candidate.A = Center - HalfAxis;
				Candidate.B = Center + HalfAxis;
				Candidate.Radius = Radius;
				bool bFits = true;
				for (const FYSIslandMassif& M : Massifs) { bFits = bFits && MassifGap(Candidate, M) >= Low; }
				for (const FYSIslandMassif& M : Belt) { bFits = bFits && MassifGap(Candidate, M) >= Low; }
				if (!bFits)
				{
					continue;
				}
				Massifs.Add(Candidate);
				Area += Length * 2.0f * Radius + PI * Radius * Radius;
				Fails = 0;
			}
			if (Area > BestArea)
			{
				OutMassifs = MoveTemp(Massifs);
				BestArea = Area;
			}
		}
		FillLargeVoids(Terrain, Rng, Belt, OutMassifs, Low);
	}

	/** Recherche rapide des props voisins */
	struct FPropIndex
	{
		static constexpr float Cell = 300.0f;
		TMap<FIntPoint, TArray<FPlacedProp>> Cells;

		static FIntPoint KeyOf(const FVector2f& P) { return FIntPoint(FMath::FloorToInt32(P.X / Cell), FMath::FloorToInt32(P.Y / Cell)); }

		void Add(const FPlacedProp& Prop) { Cells.FindOrAdd(KeyOf(Prop.Spawn.Location)).Add(Prop); }

		bool Fits(const FVector2f& P, float Radius, float Factor, float Gap) const
		{
			const FIntPoint Key = KeyOf(P);
			for (int32 I = Key.X - 1; I <= Key.X + 1; ++I)
			{
				for (int32 J = Key.Y - 1; J <= Key.Y + 1; ++J)
				{
					if (const TArray<FPlacedProp>* Found = Cells.Find(FIntPoint(I, J)))
					{
						for (const FPlacedProp& Other : *Found)
						{
							const float Limit = (Radius + Other.Radius) * Factor + Gap;
							if ((Other.Spawn.Location - P).SizeSquared() < Limit * Limit)
							{
								return false;
							}
						}
					}
				}
			}
			return true;
		}
	};

	FPlacedProp MakeProp(const FYSIslandGenerationRules& Rules, EYSIslandPropType Type, EYSIslandPropRole Role, float Scale, const FVector2f& Location, FRandomStream& Rng)
	{
		const FYSIslandPropDef& Def = PropDef(Rules, Type);
		FPlacedProp Prop;
		Prop.Spawn.Location = Location;
		// maisons : alignées sur la direction du milieu de l'île (rues à peu près droites), un des 4 côtés au hasard
		Prop.Spawn.Yaw = IsHouse(Type) ? FMath::RadiansToDegrees(FMath::Atan2(Location.Y, Location.X)) + 90.0f * Rng.RandRange(0, 3) : Rng.FRandRange(0.0f, 360.0f);
		Prop.Spawn.Scale = Scale;
		Prop.Spawn.Type = Type;
		Prop.Spawn.Role = Role;
		Prop.Radius = Def.Radius * Scale;
		Prop.Collision = Def.CollisionRadius * Scale;
		return Prop;
	}

	/** Bateau de départ échoué sur la plage, au sud : proue vers l'île ; joueurs entre lui et la jungle */
	void PlaceBoatAndPlayers(const FYSIslandTerrain& Terrain, FRandomStream& Rng, FYSIslandPlan& OutPlan)
	{
		const float BoatAngle = PI + FMath::DegreesToRadians(Rng.FRandRange(-15.0f, 15.0f));
		const float BoatRadius = Terrain.CoastRadius(BoatAngle) - Terrain.BeachDepth(NormalizeAngle(BoatAngle)) * 0.3f;
		OutPlan.BoatLocation = FVector2f(BoatRadius * FMath::Cos(BoatAngle), BoatRadius * FMath::Sin(BoatAngle));
		OutPlan.BoatYaw = FMath::RadiansToDegrees(BoatAngle) + 180.0f + Rng.FRandRange(-25.0f, 25.0f);
		for (int32 K = 0; K < 4; ++K)
		{
			// assez loin de la chaloupe pour ne pas atterrir dessus
			OutPlan.PlayerSpots.Add(OutPlan.BoatLocation + FVector2f(900.0f + (K / 2) * 250.0f, -350.0f + (K % 2) * 700.0f));
		}
	}

	/** Bord de l'eau (hauteur Z) en partant de X vers le sud (-X), à Y fixe */
	float FindShoreX(const FYSIslandTerrain& Terrain, float X, float Y, float Z)
	{
		const float Limit = -2.0f * Terrain.GetHalfSize();
		while (X > Limit && Terrain.Height(X, Y) > Z)
		{
			X -= 25.0f;
		}
		return X;
	}

	/**
	 *  Deux pontons parallèles sur la plage, de part et d'autre de l'axe d'arrivée de la chaloupe (Y du bateau), du bord de l'eau vers le large ;
	 *  la place du bateau est entre eux
	 */
	void BuildBeachPiers(const FYSIslandTerrain& Terrain, FYSIslandPlan& OutPlan)
	{
		const FYSIslandGenerationRules& Rules = Terrain.GetRules();
		if (!Rules.bBeachPiers)
		{
			return;
		}
		const float Deck = SeaLevel + Rules.PierDeckHeight;
		const float AxisY = OutPlan.BoatLocation.Y;
		// vrai bord de l'eau sur l'axe : longueur des pontons et place du bateau comptées depuis là (les planches commencent plus haut sur la plage)
		const float AxisShore = FindShoreX(Terrain, OutPlan.BoatLocation.X, AxisY, SeaLevel);

		// ponton tourné de 90° : quai parallèle à la plage en travers de l'axe de la chaloupe, deux passerelles à ses bouts, sloop côté large
		if (Rules.bBeachQuay)
		{
			const float QuayInner = AxisShore - Rules.BeachQuayDistance;
			const float QuayOuter = QuayInner - Rules.BeachQuayWidth;
			const float QuayX = (QuayInner + QuayOuter) * 0.5f;
			const float HalfLength = Rules.BeachPierLength * 0.5f;
			FYSIslandPierSegment& Quay = OutPlan.PierSegments.AddDefaulted_GetRef();
			Quay.A = FVector3f(QuayX, AxisY - HalfLength, Deck);
			Quay.B = FVector3f(QuayX, AxisY + HalfLength, Deck);
			Quay.bPosts = true;
			Quay.Width = Rules.BeachQuayWidth;
			for (const float Side : { -1.0f, 1.0f })
			{
				const float Y = AxisY + Side * (HalfLength - Rules.BeachPierWidth * 0.5f);
				const float StartX = FindShoreX(Terrain, OutPlan.BoatLocation.X, Y, Deck) + Rules.BeachPierLandLength;
				FYSIslandPierSegment& Walkway = OutPlan.PierSegments.AddDefaulted_GetRef();
				Walkway.A = FVector3f(StartX, Y, Deck);
				Walkway.B = FVector3f(QuayX, Y, Deck);
				Walkway.bPosts = true;
				Walkway.Width = Rules.BeachPierWidth;
			}
			OutPlan.bHasBerth = true;
			OutPlan.bBerthAlongQuay = true;
			OutPlan.BerthLocation = FVector2f(QuayOuter - Rules.BeachPierGap * 0.5f, AxisY);
			return;
		}
		const float EndX = AxisShore - Rules.BeachPierLength;
		const float Offset = Rules.BeachPierGap * 0.5f + Rules.BeachPierWidth * 0.5f;
		for (const float Side : { -1.0f, 1.0f })
		{
			const float Y = AxisY + Side * Offset;
			const float StartX = FindShoreX(Terrain, OutPlan.BoatLocation.X, Y, Deck) + Rules.BeachPierLandLength;
			FYSIslandPierSegment& Segment = OutPlan.PierSegments.AddDefaulted_GetRef();
			Segment.A = FVector3f(StartX, Y, Deck);
			Segment.B = FVector3f(EndX, Y, Deck);
			Segment.bPosts = true;
			Segment.Width = Rules.BeachPierWidth;
		}
		OutPlan.bHasBerth = true;
		OutPlan.BerthLocation = FVector2f(AxisShore - (Rules.BerthDistanceFromShore > 0.0f ? Rules.BerthDistanceFromShore : Rules.BeachPierLength * 0.5f), AxisY);
	}

	/** Point près des pontons de la plage (props de plage, palmiers du désert) */
	bool IsNearBeachPiers(const FYSIslandTerrain& Terrain, const FYSIslandPlan& Plan, const FVector2f& P, float Margin)
	{
		const FYSIslandGenerationRules& Rules = Terrain.GetRules();
		if (!Plan.bHasBerth)
		{
			return false;
		}
		const float HalfSpan = FMath::Max(Rules.BeachPierGap * 0.5f + Rules.BeachPierWidth, Rules.bBeachQuay ? Rules.BeachPierLength * 0.5f : 0.0f) + Margin;
		return FMath::Abs(P.Y - Plan.BerthLocation.Y) <= HalfSpan && P.X <= Plan.BoatLocation.X + Margin;
	}

	/**
	 *  Pontons de Port Town : à chaque ouverture de la ceinture côté ville, une rampe descend du haut de la falaise jusqu'à un ponton qui longe
	 *  la côte au large et relie les rampes ; au bout de chaque quai et régulièrement le long du ponton, une pente plonge dans l'eau pour
	 *  remonter à la nage
	 */
	void BuildPiers(const FYSIslandTerrain& Terrain, const TArray<float>& WaterSpots, TArray<FYSIslandPierSegment>& Out)
	{
		const FYSIslandGenerationRules& Rules = Terrain.GetRules();
		const float Deck = SeaLevel + Rules.PierDeckHeight;
		const float WaterEnd = SeaLevel - Rules.PierWaterRampDepth;
		const float TownLimit = FMath::DegreesToRadians(Rules.TownHalfAngle + 10.0f);

		// ouvertures côté ville (autour du nord, +X), triées d'ouest en est
		TArray<float> Spots;
		for (const float Spot : WaterSpots)
		{
			if (FMath::Abs(NormalizeAngle(Spot)) <= TownLimit)
			{
				Spots.Add(NormalizeAngle(Spot));
			}
		}
		if (Spots.Num() == 0 && WaterSpots.Num() > 0)
		{
			float Best = WaterSpots[0];
			for (const float Spot : WaterSpots)
			{
				Best = FMath::Abs(NormalizeAngle(Spot)) < FMath::Abs(NormalizeAngle(Best)) ? Spot : Best;
			}
			Spots.Add(NormalizeAngle(Best));
		}
		Spots.Sort();

		const auto Radial = [](float Theta, float R) { return FVector2f(FMath::Cos(Theta), FMath::Sin(Theta)) * R; };
		const auto AddSegment = [&Out](const FVector2f& A, float ZA, const FVector2f& B, float ZB, bool bPosts, bool bOverlap, float Width = 0.0f)
		{
			FYSIslandPierSegment& Segment = Out.AddDefaulted_GetRef();
			Segment.A = FVector3f(A.X, A.Y, ZA);
			Segment.B = FVector3f(B.X, B.Y, ZB);
			Segment.bPosts = bPosts;
			Segment.bOverlapEnds = bOverlap;
			Segment.Width = Width;
		};
		// ponton de la côte : collé au pied de la falaise (Pier Coast Offset = écart), son milieu à cette distance du centre
		const auto WalkRadius = [&Terrain, &Rules](float Theta) { return Terrain.CoastRadius(Theta) + Rules.PierCoastOffset + Rules.PierWidth * 0.5f; };
		const auto RampRadius = [&Terrain, &Rules](float Theta) { return Terrain.CoastRadius(Theta) + Rules.PierCoastOffset + Rules.PierRampWidth * 0.5f; };

		// ponton de la côte de From à To (angles), avec des accès depuis l'eau tous les Pier Water Access Spacing
		const auto AddWalkway = [&](float From, float To)
		{
			const float Length = FMath::Abs(To - From) * WalkRadius((From + To) * 0.5f);
			const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Length / Rules.PierStep));
			float SinceAccess = 0.0f;
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				const float T0 = From + (To - From) * Step / Steps;
				const float T1 = From + (To - From) * (Step + 1) / Steps;
				const FVector2f P0 = Radial(T0, WalkRadius(T0));
				const FVector2f P1 = Radial(T1, WalkRadius(T1));
				AddSegment(P0, Deck, P1, Deck, true, true);
				SinceAccess += (P1 - P0).Size();
				if (SinceAccess >= Rules.PierWaterAccessSpacing && Step + 1 < Steps)
				{
					SinceAccess = 0.0f;
					AddSegment(P1, Deck, P1 + Radial(T1, 1.0f) * (Rules.PierWidth * 0.5f + Rules.PierWaterRampLength), WaterEnd, false, false);
				}
			}
		};

		const float RampTan = FMath::Tan(FMath::DegreesToRadians(Rules.PierRampMaxSlope));
		for (int32 Index = 0; Index < Spots.Num(); ++Index)
		{
			const float Theta = Spots[Index];
			const float Coast = Terrain.CoastRadius(Theta);
			// rampe : du haut de la falaise à l'ouverture, elle longe la falaise en descendant (côté de la rampe voisine) et se pose sur le ponton
			const float TopZ = Terrain.Height(Radial(Theta, Coast - 40.0f).X, Radial(Theta, Coast - 40.0f).Y) + 2.0f;
			const float RampLength = FMath::Max(TopZ - Deck, 0.0f) / FMath::Max(RampTan, 0.05f);
			const float Side = Index + 1 < Spots.Num() || Spots.Num() == 1 ? 1.0f : -1.0f;
			const float RampEnd = Theta + Side * RampLength / FMath::Max(RampRadius(Theta), 100.0f);
			const int32 RampSteps = FMath::Max(1, FMath::CeilToInt32(RampLength / Rules.PierStep));
			for (int32 Step = 0; Step < RampSteps; ++Step)
			{
				const float A0 = float(Step) / RampSteps;
				const float A1 = float(Step + 1) / RampSteps;
				const float T0 = FMath::Lerp(Theta, RampEnd, A0);
				const float T1 = FMath::Lerp(Theta, RampEnd, A1);
				AddSegment(Radial(T0, RampRadius(T0)), FMath::Lerp(TopZ, Deck, A0), Radial(T1, RampRadius(T1)), FMath::Lerp(TopZ, Deck, A1), false, true, Rules.PierRampWidth);
			}
			// ponton sous la rampe (une seule ouverture : il n'y a pas de ponton entre deux rampes)
			if (Spots.Num() == 1)
			{
				AddWalkway(FMath::Min(Theta, RampEnd), FMath::Max(Theta, RampEnd) + Rules.PierWidth / FMath::Max(WalkRadius(Theta), 100.0f));
			}
			// quai vers le large depuis le bord extérieur du ponton, puis pente dans l'eau
			const float Outer = Coast + Rules.PierCoastOffset + Rules.PierWidth;
			const FVector2f DockStart = Radial(Theta, Outer - Rules.PierWidth * 0.5f);
			const FVector2f DockEnd = Radial(Theta, Outer + Rules.PierDockLength);
			AddSegment(DockStart, Deck, DockEnd, Deck, true, true);
			AddSegment(DockEnd, Deck, Radial(Theta, Outer + Rules.PierDockLength + Rules.PierWaterRampLength), WaterEnd, false, false);
		}

		// ponton de la côte entre deux rampes voisines, avec des accès depuis l'eau
		for (int32 Index = 0; Index + 1 < Spots.Num(); ++Index)
		{
			AddWalkway(Spots[Index], Spots[Index + 1]);
		}
	}

	/** Désert : plage tout autour, palmiers seuls ou en trio sur une grille régulière (un peu décalée) */
	void BuildDesertPlan(const FYSIslandGenerationRules& Rules, int32 Seed, FYSIslandPlan& OutPlan, FString& OutInfo)
	{
		const FYSIslandTerrain Terrain(Rules, Seed, EYSIslandGeneration::Desert);
		FRandomStream Rng(Seed * 31 + 1);
		OutPlan = FYSIslandPlan();
		OutPlan.Seed = Seed;
		OutPlan.Biome = EYSIslandGeneration::Desert;
		PlaceBoatAndPlayers(Terrain, Rng, OutPlan);
		BuildBeachPiers(Terrain, OutPlan);

		const float Spacing = Rules.DesertPalmSpacing;
		const float Jitter = Spacing * Rules.DesertPalmJitter;
		const float Reach = Terrain.GetHalfSize();
		const FYSIslandPropDef& Palm = Rules.TreeProp;
		int32 Solos = 0;
		int32 Trios = 0;
		for (float X = -Reach; X <= Reach; X += Spacing)
		{
			for (float Y = -Reach; Y <= Reach; Y += Spacing)
			{
				const FVector2f P(X + Rng.FRandRange(-Jitter, Jitter), Y + Rng.FRandRange(-Jitter, Jitter));
				const bool bTrio = Rng.FRand() < Rules.DesertTrioChance;
				bool bFree = Terrain.Inland(P.X, P.Y) >= Rules.DesertShoreMargin && (P - OutPlan.BoatLocation).Size() >= 1200.0f && !IsNearBeachPiers(Terrain, OutPlan, P, 300.0f);
				for (const FVector2f& Spot : OutPlan.PlayerSpots) { bFree = bFree && (P - Spot).Size() >= 700.0f; }
				if (!bFree)
				{
					continue;
				}
				if (bTrio)
				{
					const float Start = Rng.FRandRange(0.0f, 2.0f * PI);
					for (int32 M = 0; M < 3; ++M)
					{
						const float Angle = Start + M * 2.0f * PI / 3.0f;
						OutPlan.Props.Add(MakeProp(Rules, EYSIslandPropType::Tree, EYSIslandPropRole::Trio, Rules.TreeTrio.Scale,
							P + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Rules.TreeTrio.Circumradius, Rng).Spawn);
					}
					++Trios;
				}
				else
				{
					OutPlan.Props.Add(MakeProp(Rules, EYSIslandPropType::Tree, EYSIslandPropRole::Beach, Rng.FRandRange(Palm.ScaleMin, Palm.ScaleMax), P, Rng).Spawn);
					++Solos;
				}
			}
		}

		// élite : point le plus dégagé vers le milieu
		float BestScore = -1e9f;
		for (float X = -Rules.Radius * 0.5f; X <= Rules.Radius * 0.5f; X += 200.0f)
		{
			for (float Y = -Rules.Radius * 0.5f; Y <= Rules.Radius * 0.5f; Y += 200.0f)
			{
				const FVector2f P(X, Y);
				float Clear = 1e9f;
				for (const FYSIslandPropSpawn& Prop : OutPlan.Props) { Clear = FMath::Min(Clear, (Prop.Location - P).Size()); }
				const float Score = Clear - P.Size() * 0.25f;
				if (Score > BestScore)
				{
					BestScore = Score;
					OutPlan.CenterSpot = P;
				}
			}
		}
		OutInfo = FString::Printf(TEXT("désert : %d palmiers seuls, %d trios (%d props)"), Solos, Trios, OutPlan.Props.Num());
	}

	/** Un essai de plan (Tropical, Port Town) à une graine : vide OutError si toutes les règles sont respectées */
	void BuildPlan(const FYSIslandGenerationRules& Rules, EYSIslandGeneration Biome, int32 Seed, FYSIslandPlan& OutPlan, FString& OutError, FString& OutInfo)
	{
		const FYSIslandTerrain Terrain(Rules, Seed, Biome);
		FRandomStream Rng(Seed * 31 + 1);
		const FGrid Grid(Terrain.GetHalfSize());
		const int32 N = Grid.N;
		const float PlayerWidth = PlayerRadius * 2.0f;
		const float MinWidth = Rules.CorridorPlayersMin * PlayerWidth;

		OutPlan = FYSIslandPlan();
		OutPlan.Seed = Seed;
		OutPlan.Biome = Biome;

		TArray<FYSIslandMassif> Belt;
		TArray<FYSIslandMassif> Interior;
		PlaceBelt(Terrain, Rng, Belt, OutPlan.WaterSpots);
		PlaceInteriorMassifs(Terrain, Rng, Belt, Interior);
		OutPlan.Massifs = Belt;
		OutPlan.Massifs.Append(Interior);

		// Port Town : les massifs côté falaise, à l'opposé de la plage, deviennent la ville
		int32 TownMassifs = 0;
		for (FYSIslandMassif& M : OutPlan.Massifs)
		{
			const FVector2f Middle = (M.A + M.B) * 0.5f;
			M.bTown = Terrain.IsInTownSector(Middle.X, Middle.Y);
			TownMassifs += M.bTown ? 1 : 0;
		}

		PlaceBoatAndPlayers(Terrain, Rng, OutPlan);
		BuildBeachPiers(Terrain, OutPlan);

		TArray<FPlacedProp> Props;

		// props isolés sur la plage : pas près des joueurs ni du bateau
		const auto PlaceBeachProps = [&](EYSIslandPropType Type, int32 Count, TArray<FVector2f>& Placed)
		{
			const FYSIslandPropDef& Def = PropDef(Rules, Type);
			int32 Done = 0;
			for (int32 Tries = 0; Done < Count && Tries < 3000; ++Tries)
			{
				const float Theta = FMath::Atan2(0.0f, -1.0f) + FMath::DegreesToRadians(Rng.FRandRange(-Rules.BeachAngleFull, Rules.BeachAngleFull));
				const float Normalized = FMath::Atan2(FMath::Sin(Theta), FMath::Cos(Theta));
				const float R = Terrain.CoastRadius(Normalized) - Rng.FRandRange(0.25f, 0.65f) * Terrain.BeachDepth(Normalized);
				const FVector2f P(R * FMath::Cos(Normalized), R * FMath::Sin(Normalized));
				bool bFree = Terrain.Height(P.X, P.Y) >= SeaLevel + 40.0f && (P - OutPlan.BoatLocation).Size() >= 1200.0f && !IsNearBeachPiers(Terrain, OutPlan, P, 300.0f);
				for (const FVector2f& Spot : OutPlan.PlayerSpots) { bFree = bFree && (P - Spot).Size() >= 700.0f; }
				for (const FVector2f& Other : Placed) { bFree = bFree && (P - Other).Size() >= Rules.BeachPropSpacing; }
				if (bFree)
				{
					Props.Add(MakeProp(Rules, Type, EYSIslandPropRole::Beach, Rng.FRandRange(Def.ScaleMin, Def.ScaleMax), P, Rng));
					Placed.Add(P);
					++Done;
				}
			}
		};
		TArray<FVector2f> BeachPlaced;
		PlaceBeachProps(EYSIslandPropType::Tree, Rules.BeachTreeCount, BeachPlaced);
		PlaceBeachProps(EYSIslandPropType::Rock, Rules.BeachRockCount, BeachPlaced);

		// massifs : 0 hors massif, sinon distance à leur bord ; centre réservé aux arbres
		TArray<uint8> Carved;
		Carved.Init(1, Grid.Total());
		TArray<uint8> Core;
		Core.SetNumZeroed(Grid.Total());
		TArray<uint8> Town;
		Town.SetNumZeroed(Grid.Total());
		for (const FYSIslandMassif& M : OutPlan.Massifs)
		{
			const FVector2f Min(FMath::Min(M.A.X, M.B.X) - M.Radius, FMath::Min(M.A.Y, M.B.Y) - M.Radius);
			const FVector2f Max(FMath::Max(M.A.X, M.B.X) + M.Radius, FMath::Max(M.A.Y, M.B.Y) + M.Radius);
			const float CoreR = CoreRadius(M, Rules);
			ForEachCellAround(Grid, Min, Max, [&](int32 K, const FVector2f& P)
			{
				const float D = PointSegmentDistance(P, M.A, M.B);
				if (D <= M.Radius) { Carved[K] = 0; Town[K] = Town[K] || M.bTown; }
				if (D <= CoreR) { Core[K] = 1; }
			});
		}
		TArray<float> Distance;
		DistanceField(Grid, Carved, Distance);

		// zones de même type sur le bord (bosquets, éboulis), réparties selon les poids
		TArray<TPair<FVector2f, EYSIslandPropType>> Patches;
		const float TotalWeight = FMath::Max(Rules.BushProp.Weight + Rules.RockProp.Weight, 0.01f);
		const int32 BushPatches = FMath::RoundToInt32(Rules.PatchCount * Rules.BushProp.Weight / TotalWeight);
		for (int32 K = 0; K < Rules.PatchCount; ++K)
		{
			const FVector2f P(Rng.FRandRange(-Grid.Half, Grid.Half), Rng.FRandRange(-Grid.Half, Grid.Half));
			Patches.Emplace(P, K < BushPatches ? EYSIslandPropType::Bush : EYSIslandPropType::Rock);
		}
		const auto PatchType = [&Patches](const FVector2f& P)
		{
			float Best = TNumericLimits<float>::Max();
			EYSIslandPropType Type = EYSIslandPropType::Bush;
			for (const TPair<FVector2f, EYSIslandPropType>& Patch : Patches)
			{
				const float D = (Patch.Key - P).SizeSquared();
				if (D < Best)
				{
					Best = D;
					Type = Patch.Value;
				}
			}
			return Type;
		};

		TArray<int32> Candidates;
		for (int32 J = 0; J < N; J += 2)
		{
			for (int32 I = 0; I < N; I += 2)
			{
				if (Distance[J * N + I] >= 70.0f)
				{
					Candidates.Add(J * N + I);
				}
			}
		}
		for (int32 K = Candidates.Num() - 1; K > 0; --K)
		{
			Candidates.Swap(K, Rng.RandRange(0, K));
		}

		FPropIndex Index;
		for (const FPlacedProp& Prop : Props)
		{
			Index.Add(Prop);
		}
		// ville : ce qui remplace les buissons / rochers / arbres dans ses massifs
		enum class ETownPick : uint8 { Houses, BigHouses, Palms };
		const auto HouseMix = [&]()
		{
			const float Total = FMath::Max(Rules.SmallHouseProp.Weight + Rules.BigHouseProp.Weight, 0.01f);
			return Rng.FRandRange(0.0f, Total) < Rules.BigHouseProp.Weight ? EYSIslandPropType::BigHouse : EYSIslandPropType::SmallHouse;
		};
		const auto TryPlace = [&](int32 K, float Band, float Factor, float Gap, EYSIslandPropRole Role, float Offset, ETownPick TownPick)
		{
			const FVector2f P = Grid.World(K % N, K / N);
			EYSIslandPropType Type = Core[K] ? EYSIslandPropType::Tree : PatchType(P);
			if (Town[K])
			{
				Type = TownPick == ETownPick::Houses ? HouseMix() : TownPick == ETownPick::BigHouses ? EYSIslandPropType::BigHouse : EYSIslandPropType::Tree;
			}
			if (IsHouse(Type))
			{
				// jamais l'une dans l'autre, quelle que soit leur orientation (Radius = demi-diagonale)
				Factor = 1.0f;
				Gap = Rules.HouseGap;
			}
			const FYSIslandPropDef& Def = PropDef(Rules, Type);
			const float Scale = Rng.FRandRange(Def.ScaleMin, Def.ScaleMax);
			const float Radius = Def.Radius * Scale;
			const float D = Distance[K];
			if (D < Radius + Offset || (Band >= 0.0f && D > Radius + Offset + Band) || Terrain.Inland(P.X, P.Y) < 40.0f)
			{
				return;
			}
			if (Index.Fits(P, Radius, Factor, Gap))
			{
				const FPlacedProp Prop = MakeProp(Rules, Type, Role, Scale, P, Rng);
				Props.Add(Prop);
				Index.Add(Prop);
			}
		};

		// un arbre au milieu de chaque massif (il y en a toujours un), puis les rangées du bord depuis l'extérieur, puis les arbres du centre
		for (const FYSIslandMassif& M : OutPlan.Massifs)
		{
			const FVector2f Middle = (M.A + M.B) * 0.5f;
			// ville : une grande maison au milieu si elle tient, sinon un palmier
			if (M.bTown)
			{
				const float HouseScale = Rules.BigHouseProp.ScaleMin;
				if (Terrain.Inland(Middle.X, Middle.Y) >= 40.0f && YSIslandGenerator::MassifDistance(M, Middle) <= -Rules.BigHouseProp.Radius * HouseScale
					&& Index.Fits(Middle, Rules.BigHouseProp.Radius * HouseScale, 1.0f, Rules.HouseGap))
				{
					const FPlacedProp Prop = MakeProp(Rules, EYSIslandPropType::BigHouse, EYSIslandPropRole::Core, HouseScale, Middle, Rng);
					Props.Add(Prop);
					Index.Add(Prop);
					continue;
				}
			}
			const float Scale = Rules.TreeProp.ScaleMin;
			if (Terrain.Inland(Middle.X, Middle.Y) >= 40.0f && Index.Fits(Middle, Rules.TreeProp.Radius * Scale, Rules.LiningOverlap, 0.0f))
			{
				const FPlacedProp Prop = MakeProp(Rules, EYSIslandPropType::Tree, EYSIslandPropRole::Core, Scale, Middle, Rng);
				Props.Add(Prop);
				Index.Add(Prop);
			}
		}
		for (int32 Row = 0; Row < Rules.RingRowCount; ++Row)
		{
			for (const int32 K : Candidates)
			{
				if (!Core[K])
				{
					TryPlace(K, 50.0f, Rules.LiningOverlap, 0.0f, EYSIslandPropRole::Lining, Row * Rules.RingRowWidth, ETownPick::Houses);
				}
			}
		}
		for (const int32 K : Candidates)
		{
			if (Core[K])
			{
				TryPlace(K, -1.0f, 1.0f, Rules.InteriorGap, EYSIslandPropRole::Core, 0.0f, ETownPick::BigHouses);
			}
		}
		// intérieur : arbres hors de la ville ; dans la ville, des maisons (demande de Paul : presque que des maisons)
		for (const int32 K : Candidates)
		{
			if (!Core[K])
			{
				TryPlace(K, -1.0f, 1.0f, Rules.InteriorGap, EYSIslandPropRole::Interior, 0.0f, ETownPick::Houses);
			}
		}
		// ville : petites maisons entre les grandes, puis de rares palmiers dans les places qui restent
		for (const int32 K : Candidates)
		{
			if (Core[K] && Town[K])
			{
				TryPlace(K, -1.0f, 1.0f, Rules.InteriorGap, EYSIslandPropRole::Core, 0.0f, ETownPick::Houses);
			}
		}
		for (const int32 K : Candidates)
		{
			if (Town[K] && Rng.FRand() < Rules.TownTreeChance)
			{
				TryPlace(K, -1.0f, 1.0f, Rules.InteriorGap, Core[K] ? EYSIslandPropRole::Core : EYSIslandPropRole::Interior, 0.0f, ETownPick::Palms);
			}
		}

		// trous : un PJ qui entre dans un massif reçoit un prop à cet endroit
		const FVector2f BeachStart = OutPlan.PlayerSpots[0];
		TArray<uint8> TerrainBlocked;
		TerrainMask(Terrain, Grid, TerrainBlocked);
		TArray<uint8> Blocked;
		TArray<uint8> Reached;
		int32 Plugged = 0;
		for (int32 Pass = 0; Pass < PlugPasses; ++Pass)
		{
			Blocked = TerrainBlocked;
			StampDiscs(Grid, Props, PlayerRadius, Blocked);
			Flood(Grid, Blocked, BeachStart, Reached);
			TArray<int32> Leaks;
			for (int32 K = 0; K < Grid.Total(); ++K)
			{
				if (Reached[K] && Distance[K] > LeakDepth)
				{
					Leaks.Add(K);
				}
			}
			if (Leaks.Num() == 0)
			{
				break;
			}
			Leaks.Sort([&Distance](int32 A, int32 B) { return Distance[A] > Distance[B]; });
			TArray<FVector2f> Placed;
			for (const int32 K : Leaks)
			{
				const FVector2f P = Grid.World(K % N, K / N);
				bool bFree = true;
				for (const FVector2f& Other : Placed) { bFree = bFree && (Other - P).SizeSquared() >= PlugSpacing * PlugSpacing; }
				if (bFree)
				{
					// ville : une petite maison (demande de Paul : presque pas d'arbres), réduite si elle ne tient pas en entier, quitte à
					// toucher sa voisine (c'est un bouchon : il ferme le passage)
					EYSIslandPropType Type = Core[K] || Town[K] ? EYSIslandPropType::Tree : EYSIslandPropType::Bush;
					float Scale = PropDef(Rules, Type).ScaleMin;
					if (Town[K])
					{
						Type = EYSIslandPropType::SmallHouse;
						Scale = Rules.SmallHouseProp.ScaleMin;
						if (!Index.Fits(P, Rules.SmallHouseProp.Radius * Scale, 1.0f, Rules.HouseGap))
						{
							Scale *= Rules.TownPlugHouseScale;
						}
					}
					const FPlacedProp Prop = MakeProp(Rules, Type, EYSIslandPropRole::Lining, Scale, P, Rng);
					Props.Add(Prop);
					Index.Add(Prop);
					Placed.Add(P);
					++Plugged;
				}
			}
		}

		// vérifications (sans les trios ni les props de plage, qui ne comptent pas dans la largeur)
		Blocked = TerrainBlocked;
		StampDiscs(Grid, Props, PlayerRadius, Blocked);
		Flood(Grid, Blocked, BeachStart, Reached);

		// objectif « le plus au nord » : case atteinte à pied depuis la plage la plus au nord (+X), pas trop près du bord de la falaise ;
		// à égalité, la plus proche de l'axe nord-sud
		{
			int32 Best = INDEX_NONE;
			for (int32 K = 0; K < Grid.Total(); ++K)
			{
				const FVector2f P = Grid.World(K % N, K / N);
				if (!Reached[K] || Terrain.Inland(P.X, P.Y) < Rules.NorthSpotShoreInset)
				{
					continue;
				}
				if (Best == INDEX_NONE)
				{
					Best = K;
					continue;
				}
				const FVector2f B = Grid.World(Best % N, Best / N);
				if (P.X > B.X + 1.0f || (FMath::IsNearlyEqual(P.X, B.X, 1.0f) && FMath::Abs(P.Y) < FMath::Abs(B.Y)))
				{
					Best = K;
				}
			}
			OutPlan.bHasNorthSpot = Best != INDEX_NONE;
			OutPlan.NorthSpot = Best != INDEX_NONE ? Grid.World(Best % N, Best / N) : FVector2f::ZeroVector;
		}

		int32 Inside = 0;
		for (int32 K = 0; K < Grid.Total(); ++K)
		{
			Inside += Reached[K] && Distance[K] > LeakDepth ? 1 : 0;
		}

		// côté falaise, le bord n'est atteignable qu'aux ouvertures : autant de zones atteintes que d'ouvertures prévues
		const float Limit = FMath::DegreesToRadians(Rules.BeachAngleEnd + 5.0f);
		int32 Runs = 0;
		bool bPrevious = false;
		constexpr int32 Samples = 1440;
		for (int32 K = 0; K < Samples; ++K)
		{
			const float Theta = PI + Limit + (2.0f * PI - 2.0f * Limit) * K / Samples;
			const float R = Terrain.CoastRadius(Theta) - 110.0f;
			const bool bOpen = Reached[Grid.IndexOf(FVector2f(R * FMath::Cos(Theta), R * FMath::Sin(Theta)))] != 0;
			Runs += bOpen && !bPrevious ? 1 : 0;
			bPrevious = bOpen;
		}

		// plage : le bord de l'eau est dégagé sur toute sa longueur
		int32 BlockedShore = 0;
		for (int32 K = 0; K < 180; ++K)
		{
			const float Theta = PI + FMath::DegreesToRadians(-Rules.BeachAngleFull + 2.0f * Rules.BeachAngleFull * K / 179.0f);
			const float Coast = Terrain.CoastRadius(Theta);
			bool bFound = false;
			for (int32 M = 1; M < 8 && !bFound; ++M)
			{
				const float R = Coast - M * 40.0f;
				bFound = Reached[Grid.IndexOf(FVector2f(R * FMath::Cos(Theta), R * FMath::Sin(Theta)))] != 0;
			}
			BlockedShore += bFound ? 0 : 1;
		}

		// largeur : un disque de la largeur minimale fait le tour de chaque massif, depuis la plage
		TArray<uint8> Wide = TerrainBlocked;
		TArray<FPlacedProp> WallProps = Props.FilterByPredicate([](const FPlacedProp& P) { return P.Spawn.Role != EYSIslandPropRole::Beach; });
		StampDiscs(Grid, WallProps, MinWidth / 2.0f, Wide);
		TArray<uint8> WideSeen;
		Flood(Grid, Wide, BeachStart, WideSeen);
		int32 Cut = 0;
		for (const FYSIslandMassif& M : OutPlan.Massifs)
		{
			const float R = M.Radius + MinWidth / 2.0f + 40.0f;
			const FVector2f Axis = (M.B - M.A).GetSafeNormal();
			const float Base = FMath::Atan2(Axis.Y, Axis.X);
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const FVector2f& Origin = Side == 0 ? M.B : M.A;
				for (int32 K = 0; K < 12; ++K)
				{
					const float Angle = Base + Side * PI - PI / 2.0f + PI * K / 11.0f;
					const FVector2f Q = Origin + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * R;
					const int32 QK = Grid.IndexOf(Q);
					if (Terrain.Inland(Q.X, Q.Y) > 300.0f && Distance[QK] <= 0.0f && !Wide[QK] && !WideSeen[QK])
					{
						++Cut;
					}
				}
			}
		}

		// point le plus dégagé du plateau (élite), avant les trios
		TArray<uint8> Solid = TerrainBlocked;
		StampDiscs(Grid, Props, 0.0f, Solid);
		TArray<float> Clearance;
		DistanceField(Grid, Solid, Clearance);
		float BestClearance = -1.0f;
		float Widest = 0.0f;
		TArray<int32> TrioCandidates;
		const float TrioLimit = Rules.TrioEmptyPlayers * PlayerRadius;
		for (int32 K = 0; K < Grid.Total(); ++K)
		{
			const FVector2f P = Grid.World(K % N, K / N);
			const float Theta = FMath::Atan2(P.Y, P.X);
			const float S = Terrain.Inland(P.X, P.Y);
			if (S <= 0.0f || (Terrain.BeachWeight(Theta) > 0.0f && S < Terrain.BeachDepth(Theta)))
			{
				continue;
			}
			Widest = FMath::Max(Widest, Clearance[K]);
			if (Clearance[K] > TrioLimit)
			{
				TrioCandidates.Add(K);
			}
			// vers le milieu de l'île : sans ça, l'élite peut apparaître le long d'une falaise
			const float Score = Clearance[K] - P.Size() * 0.25f;
			if (Score > BestClearance)
			{
				BestClearance = Score;
				OutPlan.CenterSpot = P;
			}
		}

		// trios (arbres ou buissons au hasard) au milieu des espaces vides trop larges
		int32 TreeTrios = 0;
		int32 BushTrios = 0;
		while (TrioCandidates.Num() > 0)
		{
			int32 Best = TrioCandidates[0];
			for (const int32 K : TrioCandidates)
			{
				if (Clearance[K] > Clearance[Best]) { Best = K; }
			}
			const FVector2f Center = Grid.World(Best % N, Best / N);
			// ville : que des palmiers
			const bool bTrees = Rng.FRandRange(0.0f, Rules.TreeTrio.Weight + Rules.BushTrio.Weight) < Rules.TreeTrio.Weight || Terrain.IsInTownSector(Center.X, Center.Y);
			const FYSIslandTrioDef& Trio = bTrees ? Rules.TreeTrio : Rules.BushTrio;
			const EYSIslandPropType Type = bTrees ? EYSIslandPropType::Tree : EYSIslandPropType::Bush;
			const float Reach = Trio.Circumradius + PropDef(Rules, Type).CollisionRadius * Trio.Scale;
			const float Start = Rng.FRandRange(0.0f, 2.0f * PI);
			for (int32 M = 0; M < 3; ++M)
			{
				const float Angle = Start + M * 2.0f * PI / 3.0f;
				Props.Add(MakeProp(Rules, Type, EYSIslandPropRole::Trio, Trio.Scale, Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Trio.Circumradius, Rng));
			}
			(bTrees ? TreeTrios : BushTrios)++;
			// le trio devient un obstacle pour les espaces voisins
			TArray<int32> Keep;
			for (const int32 K : TrioCandidates)
			{
				const float D = (Grid.World(K % N, K / N) - Center).Size() - Reach;
				Clearance[K] = FMath::Min(Clearance[K], D);
				if (Clearance[K] > TrioLimit)
				{
					Keep.Add(K);
				}
			}
			TrioCandidates = MoveTemp(Keep);
		}

		OutPlan.Props.Reserve(Props.Num());
		int32 Trees = 0;
		int32 Bushes = 0;
		int32 Rocks = 0;
		int32 Houses = 0;
		for (const FPlacedProp& Prop : Props)
		{
			OutPlan.Props.Add(Prop.Spawn);
			Trees += Prop.Spawn.Type == EYSIslandPropType::Tree ? 1 : 0;
			Bushes += Prop.Spawn.Type == EYSIslandPropType::Bush ? 1 : 0;
			Rocks += Prop.Spawn.Type == EYSIslandPropType::Rock ? 1 : 0;
			Houses += IsHouse(Prop.Spawn.Type) ? 1 : 0;
		}

		// Port Town : pontons depuis les ouvertures côté ville
		if (Biome == EYSIslandGeneration::PortTown && Rules.bPortTownPiers)
		{
			BuildPiers(Terrain, OutPlan.WaterSpots, OutPlan.PierSegments);
		}

		OutInfo = FString::Printf(TEXT("%d massifs intérieurs + ceinture (%d morceaux, %d de la ville), %d accès à l'eau, %d props (%d arbres, %d buissons, %d rochers, %d maisons, %d bouchés), trios %d arbres / %d buissons, %d morceaux de ponton, plus grand vide %.0f PJ"),
			Interior.Num(), Belt.Num(), TownMassifs, Runs, Props.Num(), Trees, Bushes, Rocks, Houses, Plugged, TreeTrios, BushTrios, OutPlan.PierSegments.Num(), Widest * 2.0f / PlayerWidth);

		OutError.Reset();
		if (Interior.Num() < Rules.MassifCountMin) { OutError += FString::Printf(TEXT("%d massifs intérieurs ; "), Interior.Num()); }
		if (Inside > 0) { OutError += FString::Printf(TEXT("un PJ entre dans un massif (%d cases) ; "), Inside); }
		if (Runs != OutPlan.WaterSpots.Num()) { OutError += FString::Printf(TEXT("%d accès à l'eau au lieu de %d ; "), Runs, OutPlan.WaterSpots.Num()); }
		if (BlockedShore > 0) { OutError += FString::Printf(TEXT("bord de plage bouché (%d) ; "), BlockedShore); }
		if (Cut > 0) { OutError += FString::Printf(TEXT("tour de massif coupé (%d) ; "), Cut); }
		if (Props.Num() > 20000) { OutError += FString::Printf(TEXT("trop de props (%d) ; "), Props.Num()); }
	}
}

FYSIslandTerrain::FYSIslandTerrain(const FYSIslandGenerationRules& InRules, int32 Seed, EYSIslandGeneration InBiome)
	: Rules(InRules)
	, Biome(InBiome)
{
	FRandomStream Rng(Seed);
	for (float& Phase : CoastPhases) { Phase = Rng.FRandRange(0.0f, 2.0f * PI); }
	for (float& Phase : NoisePhases) { Phase = Rng.FRandRange(0.0f, 2.0f * PI); }
	const float MaxCoast = Rules.Radius + static_cast<float>(FMath::Abs(Rules.CoastNoise.X) + FMath::Abs(Rules.CoastNoise.Y) + FMath::Abs(Rules.CoastNoise.Z));
	const float Margin = FMath::Max(Rules.ShoreSlopeWidth, Rules.CliffWidth) + Rules.QuadSize * 2.0f;
	HalfSize = FMath::CeilToFloat((MaxCoast + Margin) / Rules.QuadSize) * Rules.QuadSize;
}

float FYSIslandTerrain::CoastRadius(float Theta) const
{
	return Rules.Radius + static_cast<float>(Rules.CoastNoise.X) * FMath::Sin(3.0f * Theta + CoastPhases[0])
		+ static_cast<float>(Rules.CoastNoise.Y) * FMath::Sin(5.0f * Theta + CoastPhases[1])
		+ static_cast<float>(Rules.CoastNoise.Z) * FMath::Sin(9.0f * Theta + CoastPhases[2]);
}

float FYSIslandTerrain::BeachWeight(float Theta) const
{
	// désert : plage tout autour
	if (Biome == EYSIslandGeneration::Desert)
	{
		return 1.0f;
	}
	const float Angle = FMath::RadiansToDegrees(FMath::Abs(FMath::Abs(Theta) - PI));
	return 1.0f - YSIslandGen::Smoothstep((Angle - Rules.BeachAngleFull) / FMath::Max(Rules.BeachAngleEnd - Rules.BeachAngleFull, 0.01f));
}

bool FYSIslandTerrain::IsInTownSector(float X, float Y) const
{
	return Biome == EYSIslandGeneration::PortTown && FMath::RadiansToDegrees(FMath::Abs(FMath::Atan2(Y, X))) <= Rules.TownHalfAngle
		&& FMath::Sqrt(X * X + Y * Y) >= Rules.Radius * Rules.TownInnerRatio;
}

float FYSIslandTerrain::BeachDepth(float Theta) const
{
	if (Biome == EYSIslandGeneration::Desert)
	{
		return Rules.BeachDepth;
	}
	const float Angle = FMath::RadiansToDegrees(FMath::Abs(FMath::Abs(Theta) - PI));
	const float T = FMath::Min(1.0f, Angle / FMath::Max(Rules.BeachAngleEnd, 0.01f));
	return Rules.BeachDepthMin + (Rules.BeachDepth - Rules.BeachDepthMin) * FMath::Square(FMath::Cos(T * PI / 2.0f));
}

float FYSIslandTerrain::Inland(float X, float Y) const
{
	return CoastRadius(FMath::Atan2(Y, X)) - FMath::Sqrt(X * X + Y * Y);
}

float FYSIslandTerrain::JungleDepth(float X, float Y) const
{
	const float Theta = FMath::Atan2(Y, X);
	return CoastRadius(Theta) - FMath::Sqrt(X * X + Y * Y) - FMath::Max(Rules.RimWidth, BeachDepth(Theta));
}

float FYSIslandTerrain::LandHeight(float X, float Y) const
{
	return Rules.PlateauHeight + static_cast<float>(Rules.PlateauNoise.X) * FMath::Sin(X / 900.0f + NoisePhases[0]) * FMath::Cos(Y / 1100.0f + NoisePhases[1])
		+ static_cast<float>(Rules.PlateauNoise.Y) * FMath::Sin((X + Y) / 1900.0f + NoisePhases[2]);
}

float FYSIslandTerrain::Height(float X, float Y) const
{
	const float Theta = FMath::Atan2(Y, X);
	const float S = CoastRadius(Theta) - FMath::Sqrt(X * X + Y * Y);
	const float Land = LandHeight(X, Y);
	const float Cliff = S >= 0.0f ? Land : Land + (Rules.SeaFloor - Land) * FMath::Min(1.0f, -S / Rules.CliffWidth);
	const float Weight = BeachWeight(Theta);
	if (Weight <= 0.0f)
	{
		return Cliff;
	}
	const float Depth = BeachDepth(Theta);
	float Beach;
	if (S >= Depth)
	{
		Beach = Land;
	}
	else if (S >= 0.0f)
	{
		Beach = Rules.ShoreHeight + (Land - Rules.ShoreHeight) * YSIslandGen::Smoothstep(S / FMath::Max(Depth, 1.0f));
	}
	else
	{
		Beach = Rules.ShoreHeight + (Rules.SeaFloor - Rules.ShoreHeight) * YSIslandGen::Smoothstep(-S / Rules.ShoreSlopeWidth);
	}
	return Cliff + (Beach - Cliff) * Weight;
}

float YSIslandGenerator::MassifDistance(const FYSIslandMassif& Massif, const FVector2f& Point)
{
	return YSIslandGen::PointSegmentDistance(Point, Massif.A, Massif.B) - Massif.Radius;
}

const FYSIslandPropDef& YSIslandGenerator::GetPropDef(const FYSIslandGenerationRules& Rules, EYSIslandPropType Type)
{
	return YSIslandGen::PropDef(Rules, Type);
}

bool YSIslandGenerator::GeneratePlan(const FYSIslandGenerationRules& Rules, EYSIslandGeneration Biome, int32 Seed, FYSIslandPlan& OutPlan, FString& OutReport)
{
	const double StartTime = FPlatformTime::Seconds();
	// désert : pas de massif, rien à vérifier
	if (Biome == EYSIslandGeneration::Desert)
	{
		FString Info;
		YSIslandGen::BuildDesertPlan(Rules, Seed, OutPlan, Info);
		OutReport = FString::Printf(TEXT("graine %d (%.0f ms) : %s"), Seed, (FPlatformTime::Seconds() - StartTime) * 1000.0, *Info);
		return true;
	}
	FString LastError;
	FYSIslandPlan BestPlan;
	for (int32 Attempt = 0; Attempt < YSIslandGen::PlanAttempts; ++Attempt)
	{
		FString Error;
		FString Info;
		YSIslandGen::BuildPlan(Rules, Biome, Seed + Attempt, OutPlan, Error, Info);
		if (Error.IsEmpty())
		{
			OutReport = FString::Printf(TEXT("graine %d (demandée %d, %d essai(s), %.0f ms) : %s"), OutPlan.Seed, Seed, Attempt + 1, (FPlatformTime::Seconds() - StartTime) * 1000.0, *Info);
			return true;
		}
		LastError = Error;
		BestPlan = OutPlan;
		UE_LOG(LogTemp, Verbose, TEXT("[YSIsland] Graine %d refusée : %s"), Seed + Attempt, *Error);
	}
	// une île imparfaite vaut mieux qu'une partie sans île
	OutPlan = BestPlan;
	OutReport = FString::Printf(TEXT("ATTENTION aucun plan parfait en %d essais, dernier gardé (graine %d) ; erreurs : %s"), YSIslandGen::PlanAttempts, OutPlan.Seed, *LastError);
	return true;
}
