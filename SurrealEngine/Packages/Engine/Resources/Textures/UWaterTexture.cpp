
#include "Precomp.h"
#include "UWaterTexture.h"

// The original's constructor and PostLoad: masks, the water at rest (128),
// and the table that halves the sums.
void UWaterTexture::Prepare()
{
	FireEngine::InitTables();
	UMask() = USize() - 1;
	VMask() = VSize() - 1;
	UnrealMipmap& mipmap = UsedMipmaps.front();
	Fields.resize((size_t)mipmap.Width * mipmap.Height / 2);
	memset(Fields.data(), 0x80, Fields.size());
	FireEngine::BuildWaterTable(WaterTable().data());
}

void UWaterTexture::StepWater()
{
	UnrealMipmap& mipmap = UsedMipmaps.front();
	FireEngine::Water water;
	water.Bits = mipmap.Data.data();
	water.Fields = Fields.data();
	water.USize = mipmap.Width;
	water.VSize = mipmap.Height;
	water.UBits = mipmap.UBits;
	water.UMask = mipmap.Width - 1;
	water.VMask = mipmap.Height - 1;
	static_assert(sizeof(ADrop) == sizeof(FireEngine::Drop), "a drop is 8 bytes");
	water.Drops = reinterpret_cast<FireEngine::Drop*>(Drops().data());
	water.NumDrops = clamp(NumDrops(), 0, 256);
	water.GlobalPhase = GlobalPhase();
	water.Parity = WaterParity();

	FireEngine::RedrawDrops(water);
	FireEngine::WaterPass(water, RenderTable().data(), WaterTable().data());

	GlobalPhase() = water.GlobalPhase;
	WaterParity() = water.Parity;
}

void UWaterTexture::UpdateFrame()
{
	UnrealMipmap& mipmap = UsedMipmaps.front();
	if (mipmap.Width < 8 || mipmap.Height < 8)
		return;
	StepWater();
	TextureModified = true;
}
