
#include "Precomp.h"
#include "Shadowmap.h"
#include "LightEffect.h"
#include "Math/vec.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Engine.h"

void Shadowmap::Resize(UModel* model, int lightMap)
{
	const LightMapIndex& lmindex = model->LightMap[lightMap];
	width = lmindex.UClamp;
	height = lmindex.VClamp;
	size_t size = (size_t)width * height;
	if (pixels.size() < size)
		pixels.resize(size);
	if (tempbuf.size() < size)
		tempbuf.resize(size);
}

void Shadowmap::Clear(UModel* model, int lightMap, const LightmapRect& rect)
{
	Resize(model, lightMap);
	// Deus Ex's shadows are the original's bytes: a light without shadow
	// bits, a moving one, is 127 all over, half a lit texel's 254
	// (dx-reverse-info/render-dll.md, light maps)
	float lit = engine->LaunchInfo.IsDeusEx() ? 127.0f : 1.0f;
	for (int y = rect.y0; y < rect.y1; y++)
	{
		float* dest = pixels.data() + y * width;
		for (int x = rect.x0; x < rect.x1; x++)
			dest[x] = lit;
	}
}

void Shadowmap::LoadDX(const uint8_t* bits, int pitch, const LightmapRect& rect)
{
	// The original's ShadowFromBits: a texel is the bits around it through
	// the kernel 24 40 24 / 40 64 40 / 24 40 24, a row at a time, each row
	// 255 x its weights / 320 rounded down -- 254 where all are lit. A row
	// takes its first bit again to the left of the map and its last byte's
	// last bit to the right (the bits past the width are the stored
	// padding); the first and last rows stand for the rows beyond them,
	// but a map of one row gets nothing from below.
	auto bit = [&](int x, int y) -> int
	{
		x = clamp(x, 0, pitch * 8 - 1);
		return (bits[y * pitch + (x >> 3)] >> (x & 7)) & 1;
	};
	auto row = [&](int x, int y, int side, int middle) -> int
	{
		return 255 * (side * (bit(x - 1, y) + bit(x + 1, y)) + middle * bit(x, y)) / 320;
	};
	for (int y = rect.y0; y < rect.y1; y++)
	{
		float* dest = pixels.data() + y * width;
		for (int x = rect.x0; x < rect.x1; x++)
		{
			int value = row(x, y, 40, 64) + row(x, std::max(y - 1, 0), 24, 40);
			if (height > 1)
				value += row(x, std::min(y + 1, height - 1), 24, 40);
			dest[x] = (float)value;
		}
	}
}

void Shadowmap::Load(UModel* model, int lightMap, int lightindex, const LightmapRect& rect)
{
	Resize(model, lightMap);
	const LightMapIndex& lmindex = model->LightMap[lightMap];
	int pitch = (width + 7) / 8;

	// Convert bits to floats that are easier to work with

	const uint8_t* bits = model->LightBits.data() + lmindex.DataOffset + lightindex * pitch * height;
	if (engine->LaunchInfo.IsDeusEx())
	{
		LoadDX(bits, pitch, rect);
		return;
	}

	// The bits of the rectangle and a texel around it, which the blur reads
	int bx0 = std::max(rect.x0 - 1, 0), bx1 = std::min(rect.x1 + 1, width);
	int by0 = std::max(rect.y0 - 1, 0), by1 = std::min(rect.y1 + 1, height);
	for (int y = by0; y < by1; y++)
	{
		const uint8_t* line = bits + y * pitch;
		float* dest = &tempbuf[y * width];
		for (int x = bx0; x < bx1; x++)
		{
			bool shadowtest = (line[x >> 3] & (1 << (x & 7))) != 0;
			dest[x] = (float)shadowtest;
		}
	}

	// Apply 3x3 gaussian blur
	static const float weights[9] = { 0.125f, 0.25f, 0.125f, 0.25f, 0.50f, 0.25f, 0.125f, 0.25f, 0.125f };
	for (int y = rect.y0; y < rect.y1; y++)
	{
		float* dest = pixels.data() + y * width;
		const float* src = tempbuf.data() + y * width;
		for (int x = rect.x0; x < rect.x1; x++)
		{
			float value = 0.0f;
			for (int yy = -1; yy <= 1; yy++)
			{
				int yyy = clamp(y + yy, 0, height - 1) - y;
				for (int xx = -1; xx <= 1; xx++)
				{
					int xxx = clamp(x + xx, 0, width - 1);
					value += src[yyy * width + xxx] * weights[4 + xx + yy * 3];
				}
			}

			dest[x] = value;
		}
	}
}
