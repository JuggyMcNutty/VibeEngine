#pragma once

#include "UWaterTexture.h"

// Water lit as a bumpy surface: each slope's colour from BumpMapLight, with a
// highlight towards BumpMapAngle.
class UWaveTexture : public UWaterTexture
{
public:
	using UWaterTexture::UWaterTexture;

	void Prepare() override;

	uint8_t& BumpMapAngle() { return Value<uint8_t>(PropOffsets_WaveTexture.BumpMapAngle); }
	uint8_t& BumpMapLight() { return Value<uint8_t>(PropOffsets_WaveTexture.BumpMapLight); }
	uint8_t& PhongRange() { return Value<uint8_t>(PropOffsets_WaveTexture.PhongRange); }
	uint8_t& PhongSize() { return Value<uint8_t>(PropOffsets_WaveTexture.PhongSize); }
};
