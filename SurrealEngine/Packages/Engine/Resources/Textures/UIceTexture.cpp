
#include "Precomp.h"
#include "UIceTexture.h"
#include "Engine.h"
#include "Render/RenderSubsystem.h"

// The original's PostLoad: both textures used when they have this one's
// size; otherwise the glass dropped and the source scaled up into this
// texture's own pixels, which then stay as they are.
void UIceTexture::Prepare()
{
	FireEngine::InitTables();
	UMask() = USize() - 1;
	VMask() = VSize() - 1;

	UnrealMipmap& mipmap = UsedMipmaps.front();
	UTexture* source = SourceTexture();
	if (source && (source->UsedMipmaps.empty() || mipmap.Width < 8 || mipmap.Height < 8))
	{
		SourceTexture() = nullptr;
		GlassTexture() = nullptr;
		source = nullptr;
	}
	if (source)
	{
		const UnrealMipmap& sourceMip = source->UsedMipmaps.front();
		bool sourceFits = sourceMip.Width == mipmap.Width && sourceMip.Height == mipmap.Height;
		UTexture* glass = GlassTexture();
		bool glassFits = glass && !glass->UsedMipmaps.empty() && glass->UsedMipmaps.front().Width == mipmap.Width && glass->UsedMipmaps.front().Height == mipmap.Height;
		if (glass && !glassFits)
			GlassTexture() = nullptr;
		if (!sourceFits || !glassFits)
		{
			int ushift = mipmap.UBits - sourceMip.UBits;
			int vshift = mipmap.VBits - sourceMip.VBits;
			if (ushift < 0 || vshift < 0)
			{
				SourceTexture() = nullptr;
			}
			else
			{
				for (int y = 0; y < mipmap.Height; y++)
					for (int x = 0; x < mipmap.Width; x++)
						mipmap.Data[(size_t)y * mipmap.Width + x] = sourceMip.Data[((size_t)(y >> vshift) << sourceMip.UBits) + (x >> ushift)];
				LocalCopy = true;
				TextureModified = true;
			}
		}
	}

	source = SourceTexture();
	if (source && source != OldSourceTex())
	{
		Palette() = source->Palette();
		ForceRefresh() = 1;
	}
	OldSourceTex() = source;
	if (GlassTexture() != OldGlassTex())
		ForceRefresh() = 1;
	OldGlassTex() = GlassTexture();
}

// TIME_RealTimeScroll pans by the time between the frames that draw it;
// TIME_FrameRateSync by 1/120 s a step, paced as any texture's.
void UIceTexture::Update(float elapsed)
{
	if (TimeMethod() == 0)
	{
		UFractalTexture::Update(elapsed);
		return;
	}
	if (!Prepared)
	{
		Prepared = true;
		Prepare();
	}
	RenderIce(elapsed);
}

void UIceTexture::UpdateFrame()
{
	RenderIce(0.0083333338f);
}

void UIceTexture::RenderIce(float deltaTime)
{
	UTexture* glass = GlassTexture();
	UTexture* source = SourceTexture();
	if (!glass || !source)
		return;
	if (glass != this)
		engine->render->UpdateTexture(glass);
	if (source != this)
		engine->render->UpdateTexture(source);

	FireEngine::Ice ice;
	ice.PanningStyle = PanningStyle();
	ice.HorizPanSpeed = HorizPanSpeed();
	ice.VertPanSpeed = VertPanSpeed();
	ice.Frequency = Frequency();
	ice.Amplitude = Amplitude();
	ice.MasterCount = MasterCount();
	ice.UDisplace = UDisplace();
	ice.VDisplace = VDisplace();
	ice.UPosition = UPosition();
	ice.VPosition = VPosition();
	FireEngine::MoveIce(ice, deltaTime);
	MasterCount() = ice.MasterCount;
	UDisplace() = ice.UDisplace;
	VDisplace() = ice.VDisplace;
	UPosition() = ice.UPosition;
	VPosition() = ice.VPosition;

	int upos = (int)UPosition();
	int vpos = (int)VPosition();
	if (upos == OldUDisplace() && vpos == OldVDisplace() && ForceRefresh() == 0)
		return;
	OldUDisplace() = upos;
	OldVDisplace() = vpos;
	ForceRefresh() = 0;
	if (LocalCopy)
		return;

	UnrealMipmap& mipmap = UsedMipmaps.front();
	FireEngine::BlitIce(mipmap.Data.data(), glass->UsedMipmaps.front().Data.data(), source->UsedMipmaps.front().Data.data(),
		mipmap.Width, mipmap.Height, mipmap.UBits, mipmap.Width - 1, mipmap.Height - 1, upos, vpos, MoveIce());
	TextureModified = true;
}
