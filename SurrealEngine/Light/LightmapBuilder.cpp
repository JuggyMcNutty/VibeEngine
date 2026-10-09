
#include "Precomp.h"
#include "LightmapBuilder.h"
#include "Engine.h"
#include "Packages/Engine/USurrealClient.h"
#include "Packages/Engine/Resources/UPalette.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Actors/Info/UZoneInfo.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Core/UClass.h"
#include "RenderDevice/RenderDevice.h"
#include "Math/hsb.h"
#include "Utils/Random.h"
#include <cstring>

#ifdef USE_SSE2
#include <immintrin.h>
#endif

void LightmapBuilder::Setup(UModel* model, const Coords& mapCoords, int lightMap)
{
	const LightMapIndex& lmindex = model->LightMap[lightMap];

	width = lmindex.UClamp;
	height = lmindex.VClamp;
	normal = normalize(mapCoords.ZAxis);
	base = mapCoords.Origin;

	// Stop allocations over time by building up a reserve

	size_t size = (size_t)width * height;
	if (points.size() < size)
		points.resize(size);
	if (lightcolors.size() < size)
		lightcolors.resize(size);
	if (illuminationmap.size() < size)
		illuminationmap.resize(size);
	if (rows.size() < (size_t)height)
		rows.resize(height);

	CalcRowLines(mapCoords, lmindex);
}

void LightmapBuilder::SetAmbientLight(UZoneInfo* zoneActor)
{
	if (engine->LaunchInfo.IsDeusEx())
	{
		// Deus Ex's maps are the original's bytes, 127 at most, a byte worth
		// 2/255 on the screen as D3DDrv shows it (dx-reverse-info/render-dll.md,
		// light maps): the zone's ambient light starts them, FGetHSV's
		// colour times 64
		vec3 ambient = FGetHSV(zoneActor->AmbientHue(), zoneActor->AmbientSaturation(), zoneActor->AmbientBrightness());
		vec3 bytes(std::floor(ambient.r * 64.0f), std::floor(ambient.g * 64.0f), std::floor(ambient.b * 64.0f));
		std::fill(lightcolors.begin(), lightcolors.begin() + (size_t)width * height, bytes);
		return;
	}

	// Initialize lightmap with the ambient color

	vec3 ambientColor = hsbtorgb(zoneActor->AmbientHue(), zoneActor->AmbientSaturation(), zoneActor->AmbientBrightness()); // To do: is this the correct scale?
	// To do: is there more ambient light than just from the zone?

	std::fill(lightcolors.begin(), lightcolors.begin() + (size_t)width * height, ambientColor);

	// To do: how does polyflags affect the lightmap (if at all)?

	//bool isSpecialLit = (surface.PolyFlags & PF_SpecialLit) == PF_SpecialLit;
	//bool isTranslucent = (surface.PolyFlags & PF_Translucent) == PF_Translucent;
}

void LightmapBuilder::LoadStaticLight(const Array<vec3>& staticLightColors, const LightmapRect& rect)
{
	for (int y = rect.y0; y < rect.y1; y++)
	{
		size_t offset = (size_t)y * width + rect.x0;
		std::memcpy(lightcolors.data() + offset, staticLightColors.data() + offset, (rect.x1 - rect.x0) * sizeof(vec3));
	}
}

void LightmapBuilder::SaveStaticLight(Array<vec3>& staticLightColors)
{
	staticLightColors.resize(width * height);
	std::memcpy(staticLightColors.data(), lightcolors.data(), width * height * sizeof(vec3));
}

void LightmapBuilder::AddStaticLights(UModel* model, int lightMap)
{
	const LightMapIndex& lmindex = model->LightMap[lightMap];
	if (lmindex.LightActors >= 0)
	{
		UActor** lightlist = &model->Lights[lmindex.LightActors];
		for (int lightindex = 0; lightlist[lightindex] != nullptr; lightindex++)
		{
			UActor* light = lightlist[lightindex];
			if (light->LightType() != LT_None && light->LightBrightness() > 0)
			{
				// An animating light is added over the kept static map
				// each frame instead (AddLights), unless the
				// client's NoDynamicLights stills it into the map here
				if (LightAnimates(light) && !engine->client->NoDynamicLights)
					continue;
				spans.clear();
				FindLitSpans(light, spans);
				if (spans.empty())
					continue;
				Shadow.Load(model, lightMap, lightindex, LightmapRect::Of(spans.data(), spans.size()));
				Effect.Run(light, width, spans.data(), spans.size(), WorldLocations(), base, WorldNormal(), Shadow.Pixels(), illuminationmap.data());
				AddLightContribution(light, spans.data(), spans.size());
			}
		}
	}
}

void LightmapBuilder::FindAddedLights(UModel* model, int lightMap, const Array<int>& animatedIndices, const Array<UActor*>& dynamicLights)
{
	addedLights.clear();
	addedSpans.clear();
	addedRect = {};
	auto add = [&](UActor* light, int lightIndex)
	{
		if (light->LightType() == LT_None || light->LightBrightness() == 0)
			return;
		size_t first = addedSpans.size();
		FindLitSpans(light, addedSpans);
		size_t count = addedSpans.size() - first;
		if (count == 0)
			return;
		LightmapRect rect = LightmapRect::Of(addedSpans.data() + first, count);
		addedLights.push_back({ light, lightIndex, first, count, rect });
		addedRect.Add(rect);
	};

	const LightMapIndex& lmindex = model->LightMap[lightMap];
	if (lmindex.LightActors >= 0)
	{
		UActor** lightlist = &model->Lights[lmindex.LightActors];
		for (int lightindex : animatedIndices)
			add(lightlist[lightindex], lightindex);
	}
	for (UActor* light : dynamicLights)
		add(light, -1);
}

void LightmapBuilder::AddLights(UModel* model, int lightMap)
{
	// An animating light goes through its shadow bits, a moving one has none
	for (const AddedLight& added : addedLights)
	{
		const LightmapSpan* lit = addedSpans.data() + added.FirstSpan;
		if (added.LightIndex >= 0)
			Shadow.Load(model, lightMap, added.LightIndex, added.Rect);
		else
			Shadow.Clear(model, lightMap, added.Rect);
		Effect.Run(added.Light, width, lit, added.NumSpans, WorldLocations(), base, WorldNormal(), Shadow.Pixels(), illuminationmap.data());
		AddLightContribution(added.Light, lit, added.NumSpans);
	}
}

bool LightmapBuilder::LightAnimates(UActor* light)
{
	uint8_t lightType = light->LightType();
	if (lightType > LT_Steady && lightType != LT_BackdropLight)
		return true;

	switch (light->LightEffect())
	{
	case LE_Searchlight:
		return light->LightPeriod() != 0;
	case LE_TorchWaver:
	case LE_FireWaver:
	case LE_WateryShimmer:
	case LE_SlowWave:
	case LE_FastWave:
	case LE_Shock:
	case LE_Disco:
	case LE_Interference:
	case LE_Rotor:
		return true;
	default:
		return false;
	}
}

void LightmapBuilder::FindLitSpans(UActor* light, Array<LightmapSpan>& spans)
{
	// A cylinder light reaches any height, so its reach is not a sphere
	if (width < 2 || light->LightEffect() == LE_Cylinder)
	{
		for (int y = 0; y < height; y++)
			spans.push_back({ y, 0, width });
		return;
	}

	// Each row of texel positions is a line, p(x) = first + x * D, D the step
	// from one texel to the next (the texels are placed between the row's
	// ends, so D is taken over the whole row, not from two neighbours).
	// The texels within the radius are where |p(x) - light|^2 < radius^2; one
	// texel of margin on each side covers the rounding in the positions.
	vec3 lightLocation = light->Location();
	float radius = light->WorldLightRadius();
	float radiusSquared = radius * radius;
	for (int y = 0; y < height; y++)
	{
		const RowLine& line = rows[y];
		vec3 P = line.first - lightLocation;
		vec3 D = (line.last - line.first) * (1.0f / (width - 1));
		float dd = dot(D, D), pd = dot(P, D), pp = dot(P, P);
		if (!(dd > 0.0f))
		{
			if (pp < radiusSquared)
				spans.push_back({ y, 0, width });
			continue;
		}
		float disc = pd * pd - dd * (pp - radiusSquared);
		if (!(disc > 0.0f))
			continue;
		float s = std::sqrt(disc);
		float x0 = std::floor((-pd - s) / dd) - 1.0f;
		float x1 = std::ceil((-pd + s) / dd) + 2.0f;
		int ix0 = (int)std::clamp(x0, 0.0f, (float)width);
		int ix1 = (int)std::clamp(x1, 0.0f, (float)width);
		if (ix0 < ix1)
			spans.push_back({ y, ix0, ix1 });
	}
}

void LightmapBuilder::AddLightContribution(UActor* light, const LightmapSpan* spans, size_t count)
{
	if (engine->LaunchInfo.IsDeusEx())
	{
		AddLightContributionDX(light, spans, count);
		return;
	}

	vec3 lightcolor = GetLightColor(light);
	for (size_t i = 0; i < count; i++)
	{
		const LightmapSpan& span = spans[i];
		int offset = span.y * width + span.x0;
		AddLightContribution(lightcolor, illuminationmap.data() + offset, (float*)(lightcolors.data() + offset), span.x1 - span.x0);
	}
}

void LightmapBuilder::AddLightContribution(const vec3& lightcolor, const float* src, float* dest, int size)
{
	float lightcolorR = lightcolor.r;
	float lightcolorG = lightcolor.g;
	float lightcolorB = lightcolor.b;

#ifdef USE_SSE2
	__m128 mmlightcolor0 = _mm_setr_ps(lightcolorR, lightcolorG, lightcolorB, lightcolorR);
	__m128 mmlightcolor1 = _mm_setr_ps(lightcolorG, lightcolorB, lightcolorR, lightcolorG);
	__m128 mmlightcolor2 = _mm_setr_ps(lightcolorB, lightcolorR, lightcolorG, lightcolorB);
	int sse_size = size / 4 * 4;
	for (size_t i = 0; i < sse_size; i += 4)
	{
		// To do: memory align buffers
		__m128 s = _mm_loadu_ps(src);
		__m128 s0 = _mm_shuffle_ps(s, s, _MM_SHUFFLE(1, 0, 0, 0));
		__m128 s1 = _mm_shuffle_ps(s, s, _MM_SHUFFLE(2, 2, 1, 1));
		__m128 s2 = _mm_shuffle_ps(s, s, _MM_SHUFFLE(3, 3, 3, 2));
		__m128 one = _mm_set_ps1(1.0f);
		_mm_storeu_ps(dest, _mm_min_ps(_mm_add_ps(_mm_loadu_ps(dest), _mm_mul_ps(s0, mmlightcolor0)), one));
		_mm_storeu_ps(dest + 4, _mm_min_ps(_mm_add_ps(_mm_loadu_ps(dest + 4), _mm_mul_ps(s1, mmlightcolor1)), one));
		_mm_storeu_ps(dest + 8, _mm_min_ps(_mm_add_ps(_mm_loadu_ps(dest + 8), _mm_mul_ps(s2, mmlightcolor2)), one));
		src += 4;
		dest += 3 * 4;
	}
#else
	int sse_size = 0;
#endif
	for (size_t i = sse_size; i < size; i++)
	{
		float s = *src;
		float r = s * lightcolorR;
		float g = s * lightcolorG;
		float b = s * lightcolorB;
		r = r <= 1.0f ? r : 1.0f;
		g = g <= 1.0f ? g : 1.0f;
		b = b <= 1.0f ? b : 1.0f;
		dest[0] += r;
		dest[1] += g;
		dest[2] += b;
		src++;
		dest += 3;
	}
}

void LightmapBuilder::AddLightContributionDX(UActor* light, const LightmapSpan* spans, size_t count)
{
	// The original's merge (dx-reverse-info/render-dll.md, light maps): a
	// texel's illumination i, its shadow byte (254 lit, a light without
	// shadow bits 127) times the effect's shape, rounded, goes through the
	// light's table -- i x its colour in 65536ths, at most 127 a channel --
	// and is added to the map, each channel held to 127. The colour is
	// GlobalLighting's times its brightness and the level's Brightness. A
	// torch or fire waver or a watery shimmer dims each texel by up to 5%,
	// 20% or 40% (MergeLight, 0x10b03040): i x (1 − amount + amount x a
	// draw) − 0.5, cut down, the draws from the frame's random tables
	// (LightSystem::TickRandoms), the shimmer's its own, taken in turn over
	// the texels of the light's rectangle on the map, row by row from 0.
	vec3 scale = GetLightColorDX(light) * 65536.0f;
	int scaleR = std::max((int)std::floor(scale.r), 0);
	int scaleG = std::max((int)std::floor(scale.g), 0);
	int scaleB = std::max((int)std::floor(scale.b), 0);

	float waver = 0.0f;
	const float* randoms = engine->Level->Light.Randoms;
	switch (light->LightEffect())
	{
	case LE_TorchWaver: waver = 0.05f; break;
	case LE_FireWaver: waver = 0.2f; break;
	case LE_WateryShimmer: waver = 0.4f; randoms = engine->Level->Light.ShimmerRandoms; break;
	}

	LightmapRect rect = LightmapRect::Of(spans, count);
	int rectWidth = rect.x1 - rect.x0;

	for (size_t s = 0; s < count; s++)
	{
		const LightmapSpan& span = spans[s];
		const float* src = illuminationmap.data() + span.y * width + span.x0;
		vec3* dest = lightcolors.data() + span.y * width + span.x0;
		int index = (span.y - rect.y0) * rectWidth + (span.x0 - rect.x0);
		for (int x = span.x0; x < span.x1; x++, src++, dest++, index++)
		{
			int i = std::clamp((int)(*src + 0.5f), 0, 255);
			if (waver != 0.0f)
				i = std::max((int)(i * (1.0f - waver + waver * randoms[index & 255]) - 0.5f), 0);
			dest->r = std::min(dest->r + (float)std::min((i * scaleR) >> 16, 127), 127.0f);
			dest->g = std::min(dest->g + (float)std::min((i * scaleG) >> 16, 127), 127.0f);
			dest->b = std::min(dest->b + (float)std::min((i * scaleB) >> 16, 127), 127.0f);
		}
	}
}

vec3 LightmapBuilder::GetLightColorDX(UActor* light)
{
	vec3 color;
	float brightness = GlobalLighting(light, light->LightBrightness() * (1.0f / 255.0f), &color);
	return color * (brightness * light->Level()->Brightness());
}

float LightmapBuilder::GlobalLighting(UActor* light, float brightness, vec3* color)
{
	if (color)
		*color = FGetHSV(light->LightHue(), light->LightSaturation(), 255);

	double time = light->Level()->TimeSeconds();
	switch (light->LightType())
	{
	case LT_None:
		brightness = 0.0f;
		break;
	case LT_Pulse:
	case LT_SubtlePulse:
	{
		// 35 turns a second over the period, from the phase's 256ths of a turn
		double turns = time * 35.0 / std::max((int)light->LightPeriod(), 1) + light->LightPhase() * (1.0 / 256.0);
		float wave = (float)std::sin((turns - std::floor(turns)) * (2.0 * 3.14159265358979));
		brightness *= light->LightType() == LT_Pulse ? 0.6f + 0.39f * wave : 0.9f + 0.09f * wave;
		break;
	}
	case LT_Blink:
		// The original's blink goes by the lowest bit of its turn count, so
		// by the frame: the fork's own even blink instead
		if (std::fmod(light->LightPhase() * (1.0 / 255.0) + time * 40.0 / std::max((int)light->LightPeriod(), 1), 2.0) >= 1.0)
			brightness = 0.0f;
		break;
	case LT_Strobe:
		// The original's strobe turns over each frame: the fork's own 5 Hz
		if (std::fmod(time * 10.0, 2.0) >= 1.0)
			brightness = 0.0f;
		break;
	case LT_Flicker:
		// A random draw, at most 25 times a second (LightSystem::BeginFrame):
		// out below one half, else that much of the brightness
		brightness = light->Light.FlickerValue >= 0.5f ? brightness * light->Light.FlickerValue : 0.0f;
		break;
	case LT_TexturePaletteOnce:
	case LT_TexturePaletteLoop:
	{
		// The skin's palette gives the colour, its direction, and the
		// brightness, (2 red + 3 green + blue) / 1536 x 2.8 of it: through the
		// palette over the light's life once, or at 35 turns a second over the
		// period
		UTexture* skin = light->Skin();
		UPalette* palette = skin ? skin->Palette() : nullptr;
		if (!palette || palette->Colors.empty())
			break;
		int index;
		if (light->LightType() == LT_TexturePaletteOnce)
		{
			float lifeSpan = light->Class->GetDefaultObject<UActor>()->LifeSpan();
			float t = lifeSpan != 0.0f ? std::clamp(1.0f - light->LifeSpan() / lifeSpan, 0.0f, 1.0f) : 0.0f;
			index = (int)std::floor(t * 255.0f);
		}
		else
		{
			double turns = time * 35.0 / std::max((int)light->LightPeriod(), 1) + light->LightPhase();
			index = (uint8_t)(int64_t)(turns * 256.0) % 255;
		}
		uint32_t c = palette->Colors[std::min(index, (int)palette->Colors.size() - 1)];
		vec3 rgb((float)(c & 0xff), (float)((c >> 8) & 0xff), (float)((c >> 16) & 0xff));
		if (color)
		{
			float len2 = dot(rgb, rgb);
			*color = len2 > 0.0f ? rgb / std::sqrt(len2) : vec3(0.0f);
		}
		brightness *= (2.0f * rgb.r + 3.0f * rgb.g + rgb.b) * (2.8f / 1536.0f);
		break;
	}
	}
	return std::clamp(brightness, 0.0f, 1.0f);
}

vec3 LightmapBuilder::GetLightColor(UActor* light)
{
	constexpr float phaseScale = (1.0f / 255.0f);
	constexpr float periodSpeed = 40.0f;
	constexpr float turnsToRadians = 2.0f * 3.14159265359f;
	constexpr float strobeSpeed = 10.0f;
	switch (light->LightType())
	{
	default:
	case LT_Steady:
	case LT_BackdropLight:
		return hsbtorgb(light->LightHue(), light->LightSaturation(), light->LightBrightness());
	case LT_Pulse:
	{
		float pulseTurns = light->LightPhase() * phaseScale + light->Level()->TimeSeconds() * periodSpeed / std::max(light->LightPeriod(), (uint8_t)1);
		float pulse = std::sin(pulseTurns * turnsToRadians);
		float brightness = light->LightBrightness() * (0.65f + 0.35f * pulse);
		return hsbtorgb(light->LightHue(), light->LightSaturation(), (uint8_t)std::clamp(brightness, 0.0f, 255.0f));
	}
	case LT_SubtlePulse:
	{
		float pulseTurns = light->LightPhase() * phaseScale + light->Level()->TimeSeconds() * periodSpeed / std::max(light->LightPeriod(), (uint8_t)1);
		float pulse = std::sin(pulseTurns * turnsToRadians);
		float brightness = light->LightBrightness() * (0.8f + 0.2f * pulse);
		return hsbtorgb(light->LightHue(), light->LightSaturation(), (uint8_t)std::clamp(brightness, 0.0f, 255.0f));
	}
	case LT_Blink:
		if (std::fmod(light->LightPhase() * phaseScale + light->Level()->TimeSeconds() * periodSpeed / std::max(light->LightPeriod(), (uint8_t)1), 2.0f) < 1.0f)
			return hsbtorgb(light->LightHue(), light->LightSaturation(), light->LightBrightness());
		else
			return vec3(0.0f);
	case LT_Strobe:
		if (std::fmod(light->Level()->TimeSeconds() * strobeSpeed, 2.0f) < 1.0f)
			return hsbtorgb(light->LightHue(), light->LightSaturation(), light->LightBrightness());
		else
			return vec3(0.0f);
	case LT_Flicker:
		if (light->Light.FlickerRandom)
			return hsbtorgb(light->LightHue(), light->LightSaturation(), light->LightBrightness());
		else
			return vec3(0.0f);
	case LT_TexturePaletteOnce:
	{
		if (light->LifeSpan() <= 0.0f || !light->Skin() || !light->Skin()->Palette())
			return vec3(0.0f);

		float t = light->LifeSpan() / light->Class->GetDefaultObject<UActor>()->LifeSpan();
		UPalette* palette = light->Skin()->Palette();
		if (palette->Colors.empty())
			return vec3(0.0f);
		uint32_t color = palette->Colors[(int)(t * palette->Colors.size())];
		return vec3(
			((color >> 16) & 0xff) * (1.0f / 255.0f),
			((color >> 8) & 0xff) * (1.0f / 255.0f),
			(color & 0xff) * (1.0f / 255.0f));
	}
	case LT_TexturePaletteLoop:
	{
		if (light->LifeSpan() <= 0.0f || !light->Skin() || !light->Skin()->Palette())
			return vec3(0.0f);

		float t = light->LightPhase() * phaseScale + light->Level()->TimeSeconds() * periodSpeed / std::max(light->LightPeriod(), (uint8_t)1);
		t -= std::floor(t);

		UPalette* palette = light->Skin()->Palette();
		if (palette->Colors.empty())
			return vec3(0.0f);
		uint32_t color = palette->Colors[(int)(t * palette->Colors.size())];
		return vec3(
			((color >> 16) & 0xff) * (1.0f / 255.0f),
			((color >> 8) & 0xff) * (1.0f / 255.0f),
			(color & 0xff) * (1.0f / 255.0f));
	}
	}
}

void LightmapBuilder::CalcRowLines(Coords MapCoords, const LightMapIndex& lmindex)
{
	// Allow optimizer to move them into registers
	int width = this->width;
	int height = this->height;

	float UDot = dot(MapCoords.XAxis, MapCoords.Origin);
	float VDot = dot(MapCoords.YAxis, MapCoords.Origin);
	float LMUPan = UDot + lmindex.PanX - 0.5f * lmindex.UScale;
	float LMVPan = VDot + lmindex.PanY - 0.5f * lmindex.VScale;
	float LMUMult = 1.0f / lmindex.UScale;
	float LMVMult = 1.0f / lmindex.VScale;

	vec3 p[3] =
	{
		MapCoords.Origin,
		MapCoords.Origin + MapCoords.XAxis,
		MapCoords.Origin + MapCoords.YAxis
	};

	vec2 uv[3];
	for (int j = 0; j < 3; j++)
	{
		uv[j] =
		{
			(dot(MapCoords.XAxis, p[j]) - LMUPan) * LMUMult,
			(dot(MapCoords.YAxis, p[j]) - LMVPan) * LMVMult
		};
	}

	float leftDX = uv[2].x - uv[0].x;
	float leftDY = uv[2].y - uv[0].y;
	float leftStep = leftDX / leftDY;
	float rightDX = uv[2].x - uv[1].x;
	float rightDY = uv[2].y - uv[1].y;
	float rightStep = rightDX / rightDY;

	for (int y = 0; y < height; y++)
	{
		float x0 = uv[0].x + leftStep * (y + 0.5f - uv[0].y) + 0.5f;
		float x1 = uv[1].x + rightStep * (y + 0.5f - uv[1].y) + 0.5f;
		float t0 = (y + 0.5f - uv[0].y) / leftDY;
		float t1 = (y + 0.5f - uv[1].y) / rightDY;
		vec3 p0 = mix(p[0], p[2], t0);
		vec3 p1 = mix(p[1], p[2], t1);
		if (x1 < x0)
		{
			std::swap(x0, x1);
			std::swap(p0, p1);
		}

		RowLine& line = rows[y];
		line.p0 = p0;
		line.p1 = p1;
		line.x0 = x0;
		line.x1 = x1;
		line.first = TexelLocation(line, 0);
		line.last = TexelLocation(line, width - 1);
	}
}

void LightmapBuilder::CalcWorldLocations(const LightmapRect& rect)
{
	for (int y = rect.y0; y < rect.y1; y++)
	{
		const RowLine& line = rows[y];
		vec3* dest = &points[y * width];
		for (int i = rect.x0; i < rect.x1; i++)
			dest[i] = TexelLocation(line, i);
	}
}
