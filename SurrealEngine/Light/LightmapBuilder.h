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
	void Setup(UModel* model, const Coords& mapCoords, int lightMap);
	void SetAmbientLight(UZoneInfo* zoneActor);
	void AddStaticLights(UModel* model, int lightMap);
	void AddAnimatedLights(UModel* model, int lightMap, const Array<int>& lightIndices);
	void AddDynamicLights(UModel* model, int lightMap, const Array<UActor*>& lights);

	// Whether the light's brightness or shape changes over time: any type
	// past steady, and the animating effects (a searchlight only with a
	// period). A still light belongs in a surface's kept static map; an
	// animating one is added over it each frame
	// (dx-reverse-info/render-dll.md, light maps).
	static bool LightAnimates(UActor* light);

	void LoadStaticLight(const Array<vec3>& staticLightColors);
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

	void CalcWorldLocations(Coords MapCoords, const LightMapIndex& lmindex);

	// Fills spans with the texels a light can reach; the others get nothing from it.
	void FindLitSpans(UActor* light);

	void AddLightContribution(UActor* light);
	static void AddLightContribution(const vec3& lightcolor, const float* src, float* dest, int size);
	void AddLightContributionDX(UActor* light);

	int width = 0;
	int height = 0;
	Array<vec3> lightcolors;

	Array<vec3> points;
	vec3 normal;
	vec3 base;

	Shadowmap Shadow;
	LightEffect Effect;
	Array<float> illuminationmap;
	Array<LightmapSpan> spans;
};
