#pragma once

#include "UWaterTexture.h"

// Water as a lens over SourceTexture: each pixel shifted along its row by
// the water's slope there, in the source's colours.
class UWetTexture : public UWaterTexture
{
public:
	using UWaterTexture::UWaterTexture;

	void Prepare() override;
	void UpdateFrame() override;

	int& LocalSourceBitmap() { return Value<int>(PropOffsets_WetTexture.LocalSourceBitmap); }
	UTexture*& OldSourceTex() { return Value<UTexture*>(PropOffsets_WetTexture.OldSourceTex); }
	UTexture*& SourceTexture() { return Value<UTexture*>(PropOffsets_WetTexture.SourceTexture); }

private:
	// A smaller source scaled up to this texture's size.
	Array<uint8_t> LocalSource;
};
