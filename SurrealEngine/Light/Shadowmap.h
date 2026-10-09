#pragma once

class UModel;
class BspSurface;
struct LightmapRect;

class Shadowmap
{
public:
	// A light's shadows over a lightmap from its bits, in the rectangle only:
	// a texel is what lies around it, so a part comes out as it would whole
	void Load(UModel* model, int lightMap, int lightindex, const LightmapRect& rect);

	// No shadows, for a light without bits, in the rectangle only
	void Clear(UModel* model, int lightMap, const LightmapRect& rect);

	int Width() const { return width; }
	int Height() const { return height; }
	const float* Pixels() const { return pixels.data(); }

private:
	void Resize(UModel* model, int lightMap);
	void LoadDX(const uint8_t* bits, int pitch, const LightmapRect& rect);

	int width = 0;
	int height = 0;
	Array<float> pixels;
	Array<float> tempbuf;
};
