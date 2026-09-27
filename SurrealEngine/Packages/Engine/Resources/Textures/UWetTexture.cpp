
#include "Precomp.h"
#include "UWetTexture.h"
#include "Engine.h"
#include "Render/RenderSubsystem.h"

// The original's PostLoad: the source used as it is when it has this
// texture's size, scaled up when smaller, dropped when larger; its palette
// made this texture's; the shifts for WaveAmp.
void UWetTexture::Prepare()
{
	UWaterTexture::Prepare();

	UTexture* source = SourceTexture();
	if (!source)
		return;
	source->LoadNow();
	if (source->UsedMipmaps.empty())
	{
		SourceTexture() = nullptr;
		return;
	}

	LocalSource.clear();
	UnrealMipmap& mipmap = UsedMipmaps.front();
	const UnrealMipmap& sourceMip = source->UsedMipmaps.front();
	int ushift = mipmap.UBits - sourceMip.UBits;
	int vshift = mipmap.VBits - sourceMip.VBits;
	if (ushift < 0 || vshift < 0)
	{
		SourceTexture() = nullptr;
		return;
	}
	if (ushift != 0 || vshift != 0)
	{
		LocalSource.resize((size_t)mipmap.Width * mipmap.Height);
		for (int y = 0; y < mipmap.Height; y++)
			for (int x = 0; x < mipmap.Width; x++)
				LocalSource[(size_t)y * mipmap.Width + x] = sourceMip.Data[((size_t)(y >> vshift) << sourceMip.UBits) + (x >> ushift)];
	}

	if (source != OldSourceTex())
		Palette() = source->Palette();
	OldSourceTex() = source;

	if (WaveAmp() != OldWaveAmp())
	{
		FireEngine::BuildRefractionTable(RenderTable().data(), WaveAmp());
		OldWaveAmp() = WaveAmp();
	}
}

// The original's ConstantTimeTick: the source brought up to date, the
// drops, the water, and the source's pixels shifted by it.
void UWetTexture::UpdateFrame()
{
	UTexture* source = SourceTexture();
	UnrealMipmap& mipmap = UsedMipmaps.front();
	if (!source || mipmap.Width < 8 || mipmap.Height < 8)
		return;

	if (source != this)
		engine->render->UpdateTexture(source);

	StepWater();

	const uint8_t* sourceBits = LocalSource.empty() ? source->UsedMipmaps.front().Data.data() : LocalSource.data();
	FireEngine::ApplyWet(mipmap.Data.data(), sourceBits, mipmap.Width, mipmap.Height, mipmap.UBits, mipmap.Width - 1);
	TextureModified = true;
}
