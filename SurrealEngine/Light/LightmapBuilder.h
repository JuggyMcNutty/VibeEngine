#pragma once

#include "Math/vec.h"
#include "LightEffect.h"
#include "Shadowmap.h"

class BspSurface;
class LightMapIndex;
class UModel;
class UZoneInfo;
class Coords;
struct Poly;

class LightmapBuilder
{
public:
	// The map's size and plane, and the line each row of its texels lies
	// on. Where the texels themselves are comes from CalcWorldLocations, for
	// the part of the map built.
	void Setup(UModel* model, const Coords& mapCoords, int lightMap);
	void CalcWorldLocations(const LightmapRect& rect);

	void SetAmbientLight(UZoneInfo* zoneActor);
	void AddStaticLights(UModel* model, int lightMap);

	// The lights added over the static map each time it is built, in order:
	// the surface's animating lights, each through its shadow bits, then the
	// moving lights over it. FindAddedLights finds the texels each reaches,
	// and their rectangle, before any is built; AddLights adds them.
	void FindAddedLights(UModel* model, int lightMap, const Array<int>& animatedIndices, const Array<UActor*>& dynamicLights);
	const LightmapRect& AddedLightsRect() const { return addedRect; }
	void AddLights(UModel* model, int lightMap);

	// Whether the light's brightness or shape changes over time: any type
	// past steady, and the animating effects (a searchlight only with a
	// period). A still light belongs in a surface's kept static map; an
	// animating one is added over it each frame
	// (dx-reverse-info/render-dll.md, light maps).
	static bool LightAnimates(UActor* light);

	void LoadStaticLight(const Array<vec3>& staticLightColors, const LightmapRect& rect);
	void SaveStaticLight(Array<vec3>& staticLightColors);

	int Width() const { return width; }
	int Height() const { return height; }
	const vec3* Pixels() const { return lightcolors.data(); }

	static vec3 GetLightColor(UActor* light);

	// URender::GlobalLighting (dx-reverse-info/render-dll.md, lighting): the
	// brightness given, shaped by the light's type at this moment and kept
	// to 0-1, and the light's colour, FGetHSV's at full value -- a palette
	// light's its palette's. Deus Ex's light maps and ambient sounds use it.
	static float GlobalLighting(UActor* light, float brightness, vec3* color);

	// Deus Ex's light colour at this moment, as the original's light record
	// holds it for its maps and meshes alike: GlobalLighting's colour times
	// its brightness and the level's Brightness
	static vec3 GetLightColorDX(UActor* light);

private:
	const vec3* WorldLocations() const { return points.data(); }
	const vec3& WorldNormal() const { return normal; }

	// A row of texels lies on the line from p0 to p1, texel i at
	// (i + 0.5 - x0) / (x1 - x0) of the way; first and last are its ends.
	struct RowLine
	{
		vec3 p0, p1;
		float x0, x1;
		vec3 first, last;
	};
	static vec3 TexelLocation(const RowLine& line, int i)
	{
		float t = (i + 0.5f - line.x0) / (line.x1 - line.x0);
		return mix(line.p0, line.p1, t);
	}
	void CalcRowLines(Coords MapCoords, const LightMapIndex& lmindex);

	// Adds to spans the texels a light can reach, a row at a time; the
	// others get nothing from it.
	void FindLitSpans(UActor* light, Array<LightmapSpan>& spans);

	void AddLightContribution(UActor* light, const LightmapSpan* spans, size_t count);
	static void AddLightContribution(const vec3& lightcolor, const float* src, float* dest, int size);
	void AddLightContributionDX(UActor* light, const LightmapSpan* spans, size_t count);

	int width = 0;
	int height = 0;
	Array<vec3> lightcolors;

	Array<RowLine> rows;
	Array<vec3> points;
	vec3 normal;
	vec3 base;

	Shadowmap Shadow;
	LightEffect Effect;
	Array<float> illuminationmap;
	Array<LightmapSpan> spans;

	// The lights FindAddedLights found: each its light, its index in the
	// surface's list (-1 for a moving light: no shadow bits), and its run
	// of addedSpans and their rectangle
	struct AddedLight
	{
		UActor* Light;
		int LightIndex;
		size_t FirstSpan;
		size_t NumSpans;
		LightmapRect Rect;
	};
	Array<AddedLight> addedLights;
	Array<LightmapSpan> addedSpans;
	LightmapRect addedRect;
};
