
#include "Precomp.h"
#include "UFractalTexture.h"

void UFractalTexture::Load(ObjectStream* stream)
{
	UTexture::Load(stream);

	// A package keeps no pixels for a fractal texture, only its size: they
	// start as zeros (the original's UTexture::Serialize).
	int width = UsedMipmaps.empty() ? USize() : UsedMipmaps.front().Width;
	int height = UsedMipmaps.empty() ? VSize() : UsedMipmaps.front().Height;
	UsedFormat = TextureFormat::P8;
	UsedMipmaps.resize(1);
	UnrealMipmap& mipmap = UsedMipmaps.front();
	mipmap.Width = width;
	mipmap.Height = height;
	mipmap.Data.resize((size_t)width * height);
	memset(mipmap.Data.data(), 0, (size_t)width * height);
}

void UFractalTexture::Save(PackageStreamWriter* stream)
{
	UTexture::Save(stream);
}
