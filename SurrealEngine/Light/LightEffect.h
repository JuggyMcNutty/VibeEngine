#pragma once

#include <cmath>
#include "Math/vec.h"
#include "Packages/Engine/Actors/UActor.h"

class UActor;

// A run of lightmap texels: [x0, x1) of row y.
struct LightmapSpan
{
	int y, x0, x1;
};

// A rectangle of lightmap texels: [x0, x1) of the rows [y0, y1).
struct LightmapRect
{
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;

	bool Empty() const { return x0 >= x1 || y0 >= y1; }

	// The rectangle that holds both
	void Add(const LightmapRect& r)
	{
		if (r.Empty())
			return;
		if (Empty())
		{
			*this = r;
			return;
		}
		x0 = std::min(x0, r.x0);
		y0 = std::min(y0, r.y0);
		x1 = std::max(x1, r.x1);
		y1 = std::max(y1, r.y1);
	}

	// The spans' rectangle; the spans go down the rows
	static LightmapRect Of(const LightmapSpan* spans, size_t count)
	{
		LightmapRect r;
		if (count == 0)
			return r;
		r.x0 = spans[0].x0;
		r.x1 = spans[0].x1;
		r.y0 = spans[0].y;
		r.y1 = spans[count - 1].y + 1;
		for (size_t i = 1; i < count; i++)
		{
			r.x0 = std::min(r.x0, spans[i].x0);
			r.x1 = std::max(r.x1, spans[i].x1);
		}
		return r;
	}
};

struct LightEffectArgs
{
	UActor* light;
	int size;
	const vec3* locations;
	float* result;
	vec3 LightLocation;
	float radius;
	float invRadius;
	float invRadiusSquared;
	vec3 N;
	const float* shadowmap;
	bool smoothFalloff; // Deus Ex's: the original's 1 + 2v^3 - 3v^2 (CalcLightDistanceFalloff)
};

class LightEffect
{
public:
	// Writes result[] in the spans only; the texels outside them are left as they were.
	void Run(UActor* light, int width, const LightmapSpan* spans, size_t count, const vec3* locations, vec3 base, vec3 normal, const float* shadowmap, float* result);

private:
	void NoneEffect(LightEffectArgs* args);
	void NonIncidenceEffect(LightEffectArgs* args);
	void CylinderEffect(LightEffectArgs* args);
	void SlowWaveEffect(LightEffectArgs* args);
	void FastWaveEffect(LightEffectArgs* args);
	void ShellEffect(LightEffectArgs* args);
	void SpotlightEffect(LightEffectArgs* args);
	void SearchlightEffect(LightEffectArgs* args);
	void OmniBumpMapEffect(LightEffectArgs* args);

	typedef void (LightEffect::* EffectFunc)(LightEffectArgs* args);
	static EffectFunc Effects[LE_Unused + 1];

	enum { SinTableSize = 1024, FalloffTableSize = 1024 };

	static void InitTables();
	static float Sin(float v);
	static float Cos(float v);
	static float LightDistanceFalloff(float distsqr);
	static float CalcLightDistanceFalloff(float distsqr);
	static bool TablesInitialized;
	static float SinTable[SinTableSize], CosTable[SinTableSize], FalloffTable[FalloffTableSize];
};
