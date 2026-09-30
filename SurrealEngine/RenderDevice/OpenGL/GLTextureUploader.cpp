
#include "Precomp.h"
#include "GLTextureUploader.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"
#include "RenderDevice/RenderDevice.h"
#include <map>

#ifdef USE_SSE2
#include <immintrin.h>
#endif

#ifdef USE_NEON
#include <arm_neon.h>
#endif

// Capability switches for ES drivers without the extensions (the PowerVR
// GE8300 has none): default on, as desktop GL has both.
static bool S3TCSupported = true;
static bool RGBA32FLinearSupported = true;

void GLTextureUploader::SetS3TCSupported(bool supported)
{
	S3TCSupported = supported;
}

void GLTextureUploader::SetRGBA32FLinearSupported(bool supported)
{
	RGBA32FLinearSupported = supported;
}

GLTextureUploader* GLTextureUploader::GetUploader(TextureFormat format)
{
	static std::map<TextureFormat, std::unique_ptr<GLTextureUploader>> Uploaders;
	if (Uploaders.empty())
	{
		Uploaders[TextureFormat::P8].reset(new GLTextureUploader_P8());
		Uploaders[TextureFormat::BGRA8_LM].reset(new GLTextureUploader_BGRA8_LM());
		Uploaders[TextureFormat::R5G6B5].reset(new GLTextureUploader_Simple(GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 2));
		Uploaders[TextureFormat::BC1].reset(new GLTextureUploader_4x4Block(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 0, 0, 8)); // To do: GL_COMPRESSED_RGB_S3TC_DXT1_EXT and also needs different upload
		Uploaders[TextureFormat::RGB8].reset(new GLTextureUploader_RGB8());
		Uploaders[TextureFormat::BGRA8].reset(new GLTextureUploader_Simple(GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, 4));
		Uploaders[TextureFormat::RGBA32_F].reset(new GLTextureUploader_Simple(GL_RGBA32F, GL_RGBA, GL_FLOAT, 16));
	}

	auto it = Uploaders.find(format);
	if (it != Uploaders.end())
	{
		// A driver without the extension samples an unsupported format as
		// garbage, and one that cannot linearly filter (RGBA32F on the GE8300)
		// speckles every lightmap -- the same substitutions the Vulkan device
		// makes for that GPU: decode to RGBA8 on the CPU.
		static std::map<TextureFormat, std::unique_ptr<GLTextureUploader>> Decoders;
		if (format == TextureFormat::BC1 && !S3TCSupported)
		{
			if (!Decoders[TextureFormat::BC1])
				Decoders[TextureFormat::BC1].reset(new GLTextureUploader_BC1_Decode());
			return Decoders[TextureFormat::BC1].get();
		}
		if (format == TextureFormat::RGBA32_F && !RGBA32FLinearSupported)
		{
			if (!Decoders[TextureFormat::RGBA32_F])
				Decoders[TextureFormat::RGBA32_F].reset(new GLTextureUploader_RGBA32F_Decode());
			return Decoders[TextureFormat::RGBA32_F].get();
		}
		return it->second.get();
	}
	else
		return nullptr;
}

/////////////////////////////////////////////////////////////////////////////

int GLTextureUploader_P8::GetUploadSize(int x, int y, int w, int h)
{
	return w * h * 4;
}

void GLTextureUploader_P8::UploadRect(void* d, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked)
{
	int pitch = mip->Width;
	uint8_t* src = mip->Data.data() + x + y * pitch;
	TextureColor* Ptr = (TextureColor*)d;
	if (masked)
	{
		TextureColor translucent(0, 0, 0, 0);
		for (int i = 0; i < h; i++)
		{
			for (int j = 0; j < w; j++)
			{
				int idx = src[j];
				*Ptr++ = (idx != 0) ? palette[idx] : translucent;
			}
			src += pitch;
		}
	}
	else
	{
		for (int i = 0; i < h; i++)
		{
			for (int j = 0; j < w; j++)
			{
				int idx = src[j];
				*Ptr++ = palette[idx];
			}
			src += pitch;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

int GLTextureUploader_RGB8::GetUploadSize(int x, int y, int w, int h)
{
	return w * h * 4;
}

void GLTextureUploader_RGB8::UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked)
{
	int pitch = mip->Width * 3;
	uint8_t* src = ((uint8_t*)mip->Data.data()) + x + y * pitch;
	auto Ptr = (TextureColor*)dst;
	for (int i = 0; i < h; i++)
	{
		int k = 0;
		for (int j = 0; j < w; j++)
		{
			Ptr->R = src[k++];
			Ptr->G = src[k++];
			Ptr->B = src[k++];
			Ptr->A = 255;
			Ptr++;
		}
		src += pitch;
	}
}

/////////////////////////////////////////////////////////////////////////////

int GLTextureUploader_BGRA8_LM::GetUploadSize(int x, int y, int w, int h)
{
	return w * h * 4;
}

void GLTextureUploader_BGRA8_LM::UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked)
{
#ifdef USE_SSE2
	int pitch = mip->Width;
	TextureColor* src = ((TextureColor*)mip->Data.data()) + x + y * pitch;
	auto Ptr = (TextureColor*)dst;
	if (w % 4 == 0)
	{
		for (int i = 0; i < h; i++)
		{
			for (int j = 0; j < w; j += 4)
			{
				__m128i p = _mm_loadu_si128((const __m128i*)(src + j));
				__m128i p_hi = _mm_unpackhi_epi8(p, _mm_setzero_si128());
				__m128i p_lo = _mm_unpacklo_epi8(p, _mm_setzero_si128());
				p_hi = _mm_shufflehi_epi16(p_hi, _MM_SHUFFLE(3, 0, 1, 2));
				p_hi = _mm_shufflelo_epi16(p_hi, _MM_SHUFFLE(3, 0, 1, 2));
				p_hi = _mm_slli_epi16(p_hi, 1);
				p_lo = _mm_shufflehi_epi16(p_lo, _MM_SHUFFLE(3, 0, 1, 2));
				p_lo = _mm_shufflelo_epi16(p_lo, _MM_SHUFFLE(3, 0, 1, 2));
				p_lo = _mm_slli_epi16(p_lo, 1);
				p = _mm_packus_epi16(p_lo, p_hi);
				_mm_storeu_si128((__m128i*)(Ptr + j), p);
			}
			Ptr += w;
			src += pitch;
		}
	}
	else
	{
		for (int i = 0; i < h; i++)
		{
			for (int j = 0; j < w; j++)
			{
				TextureColor Src = src[j];
				Ptr->R = Src.B << 1;
				Ptr->G = Src.G << 1;
				Ptr->B = Src.R << 1;
				Ptr->A = Src.A << 1;
				Ptr++;
			}
			src += pitch;
		}
	}
#else
	int pitch = mip->Width;
	TextureColor* src = ((TextureColor*)mip->Data.data()) + x + y * pitch;
	auto Ptr = (TextureColor*)dst;
	for (int i = 0; i < h; i++)
	{
		for (int j = 0; j < w; j++)
		{
			TextureColor Src = src[j];
			Ptr->R = Src.B << 1;
			Ptr->G = Src.G << 1;
			Ptr->B = Src.R << 1;
			Ptr->A = Src.A << 1;
			Ptr++;
		}
		src += pitch;
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////

int GLTextureUploader_RGB10A2::GetUploadSize(int x, int y, int w, int h)
{
	return w * h * 8;
}

void GLTextureUploader_RGB10A2::UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked)
{
	int pitch = mip->Width;
	uint32_t* src = ((uint32_t*)mip->Data.data()) + x + y * pitch;
	uint16_t* Ptr = (uint16_t*)dst;
	for (int i = 0; i < h; i++)
	{
		for (int j = 0; j < w; j++)
		{
			uint32_t c = *Ptr;
			uint32_t r = (c >> 22) & 0x3ff;
			uint32_t g = (c >> 12) & 0x3ff;
			uint32_t b = (c >> 2) & 0x3ff;
			uint32_t a = c & 0x3;

			r = r * 0xffff / 0x3ff;
			g = g * 0xffff / 0x3ff;
			b = b * 0xffff / 0x3ff;
			a = a * 0xffff / 0x3;

			*(Ptr++) = r;
			*(Ptr++) = g;
			*(Ptr++) = b;
			*(Ptr++) = a;
		}
		src += pitch;
	}
}

/////////////////////////////////////////////////////////////////////////////

int GLTextureUploader_RGB10A2_UI::GetUploadSize(int x, int y, int w, int h)
{
	return w * h * 8;
}

void GLTextureUploader_RGB10A2_UI::UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked)
{
	int pitch = mip->Width;
	uint32_t* src = ((uint32_t*)mip->Data.data()) + x + y * pitch;
	uint16_t* Ptr = (uint16_t*)dst;
	for (int i = 0; i < h; i++)
	{
		for (int j = 0; j < w; j++)
		{
			uint32_t c = *Ptr;
			uint32_t r = (c >> 22) & 0x3ff;
			uint32_t g = (c >> 12) & 0x3ff;
			uint32_t b = (c >> 2) & 0x3ff;
			uint32_t a = c & 0x3;

			*(Ptr++) = r;
			*(Ptr++) = g;
			*(Ptr++) = b;
			*(Ptr++) = a;
		}
		src += pitch;
	}
}

/////////////////////////////////////////////////////////////////////////////

int GLTextureUploader_RGB10A2_LM::GetUploadSize(int x, int y, int w, int h)
{
	return w * h * 8;
}

void GLTextureUploader_RGB10A2_LM::UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked)
{
	int pitch = mip->Width;
	uint32_t* src = ((uint32_t*)mip->Data.data()) + x + y * pitch;
	uint16_t* Ptr = (uint16_t*)dst;
	for (int i = 0; i < h; i++)
	{
		for (int j = 0; j < w; j++)
		{
			uint32_t c = *Ptr;
			uint32_t r = (c >> 22) & 0x3ff;
			uint32_t g = (c >> 12) & 0x3ff;
			uint32_t b = (c >> 2) & 0x3ff;
			uint32_t a = c & 0x3;

			r = (r << 1) * 0xffff / 0xff;
			g = (g << 1) * 0xffff / 0xff;
			b = (b << 1) * 0xffff / 0xff;
			a = (a << 1) * 0xffff / 0x3;

			*(Ptr++) = r;
			*(Ptr++) = g;
			*(Ptr++) = b;
			*(Ptr++) = a;
		}
		src += pitch;
	}
}

/////////////////////////////////////////////////////////////////////////////

int GLTextureUploader_Simple::GetUploadSize(int x, int y, int w, int h)
{
	return w * h * BytesPerPixel;
}

void GLTextureUploader_Simple::UploadRect(void* d, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked)
{
	int pitch = mip->Width * BytesPerPixel;
	int size = w * BytesPerPixel;
	uint8_t* src = mip->Data.data() + x * BytesPerPixel + y * pitch;
	uint8_t* dst = (uint8_t*)d;
	for (int i = 0; i < h; i++)
	{
		memcpy(dst, src, size);
		dst += size;
		src += pitch;
	}
}

/////////////////////////////////////////////////////////////////////////////

int GLTextureUploader_4x4Block::GetUploadSize(int x, int y, int w, int h)
{
	int x0 = x / 4;
	int y0 = y / 4;
	int x1 = (x + w + 3) / 4;
	int y1 = (y + h + 3) / 4;
	return (x1 - x0) * (y1 - y0) * BytesPerBlock;
}

void GLTextureUploader_4x4Block::UploadRect(void* d, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked)
{
	int x0 = x / 4;
	int y0 = y / 4;
	int x1 = (x + w + 3) / 4;
	int y1 = (y + h + 3) / 4;

	int pitch = (mip->Width + 3) / 4 * BytesPerBlock;
	int size = (x1 - x0) * BytesPerBlock;
	uint8_t* src = mip->Data.data() + x0 * BytesPerBlock + y0 * pitch;
	uint8_t* dst = (uint8_t*)d;
	for (int i = y0; i < y1; i++)
	{
		memcpy(dst, src, size);
		dst += size;
		src += pitch;
	}
}


/////////////////////////////////////////////////////////////////////////////

int GLTextureUploader_2DBlock::GetUploadSize(int x, int y, int w, int h)
{
	int x0 = x / BlockX;
	int y0 = y / BlockY;
	int x1 = (x + w + BlockX - 1) / BlockX;
	int y1 = (y + h + BlockY - 1) / BlockY;
	return (x1 - x0) * (y1 - y0) * BytesPerBlock;
}

void GLTextureUploader_2DBlock::UploadRect(void* d, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked)
{
	int x0 = x / BlockX;
	int y0 = y / BlockY;
	int x1 = (x + w + BlockX - 1) / BlockX;
	int y1 = (y + h + BlockY - 1) / BlockY;

	int pitch = (mip->Width + BlockX - 1) / BlockX * BytesPerBlock;
	int size = (x1 - x0) * BytesPerBlock;
	uint8_t* src = mip->Data.data() + x0 * BytesPerBlock + y0 * pitch;
	uint8_t* dst = (uint8_t*)d;
	for (int i = y0; i < y1; i++)
	{
		memcpy(dst, src, size);
		dst += size;
		src += pitch;
	}
}

/////////////////////////////////////////////////////////////////////////////
// CPU decoders for drivers without the extension (the Vulkan device's
// decoders, the same algorithms, for the GL device's format table)

static void DecodeColorBlock(const uint8_t* src, uint8_t* dst, int dstWidth, bool oneBitAlpha)
{
	uint16_t c0 = src[0] | (src[1] << 8);
	uint16_t c1 = src[2] | (src[3] << 8);
	uint32_t idx = src[4] | (src[5] << 8) | (src[6] << 16) | ((uint32_t)src[7] << 24);

	uint8_t r[4], g[4], b[4], a[4];
	r[0] = (uint8_t)(((c0 >> 11) << 3) | ((c0 >> 11) >> 2));
	g[0] = (uint8_t)((((c0 >> 5) & 63) << 2) | (((c0 >> 5) & 63) >> 4));
	b[0] = (uint8_t)(((c0 & 31) << 3) | ((c0 & 31) >> 2));
	r[1] = (uint8_t)(((c1 >> 11) << 3) | ((c1 >> 11) >> 2));
	g[1] = (uint8_t)((((c1 >> 5) & 63) << 2) | (((c1 >> 5) & 63) >> 4));
	b[1] = (uint8_t)(((c1 & 31) << 3) | ((c1 & 31) >> 2));

	if (oneBitAlpha && c0 <= c1)
	{
		// DXTC1 transparency: index 3 is a fully transparent texel.
		r[2] = (uint8_t)((r[0] + r[1]) / 2);
		g[2] = (uint8_t)((g[0] + g[1]) / 2);
		b[2] = (uint8_t)((b[0] + b[1]) / 2);
		a[2] = 255;
		r[3] = g[3] = b[3] = 0;
		a[3] = 0;
	}
	else
	{
		r[2] = (uint8_t)((2 * r[0] + r[1]) / 3);
		g[2] = (uint8_t)((2 * g[0] + g[1]) / 3);
		b[2] = (uint8_t)((2 * b[0] + b[1]) / 3);
		r[3] = (uint8_t)((r[0] + 2 * r[1]) / 3);
		g[3] = (uint8_t)((g[0] + 2 * g[1]) / 3);
		b[3] = (uint8_t)((b[0] + 2 * b[1]) / 3);
		a[2] = a[3] = 255;
	}
	a[0] = a[1] = 255;

	for (int i = 0; i < 16; i++)
	{
		int t = (idx >> (2 * i)) & 3;
		dst[i * 4 + 0] = r[t];
		dst[i * 4 + 1] = g[t];
		dst[i * 4 + 2] = b[t];
		dst[i * 4 + 3] = a[t];
	}
}

template<int DstBytes>
static void UploadDecoded(void* d, UnrealMipmap* mip, int x, int y, int w, int h, int blockBytes, int blockX, int blockY, void (*blockToTexels)(const uint8_t*, uint8_t*))
{
	int bx0 = x / blockX;
	int by0 = y / blockY;
	int bx1 = (x + w + blockX - 1) / blockX;
	int by1 = (y + h + blockY - 1) / blockY;
	int blockCols = (mip->Width + blockX - 1) / blockX;
	int pitch = blockCols * blockBytes;
	uint8_t* dst = (uint8_t*)d;
	for (int by = by0; by < by1; by++)
	{
		for (int bx = bx0; bx < bx1; bx++)
		{
			const uint8_t* block = mip->Data.data() + ((size_t)by * blockCols + bx) * blockBytes;
			uint8_t texels[16 * 4]; // up to 4x4 RGBA8
			blockToTexels(block, texels);
			for (int ty = 0; ty < blockY; ty++)
			{
				int gy = by * blockY + ty - y;
				if (gy < 0 || gy >= h) continue;
				for (int tx = 0; tx < blockX; tx++)
				{
					int gx = bx * blockX + tx - x;
					if (gx < 0 || gx >= w) continue;
					memcpy(dst + ((size_t)gy * w + gx) * DstBytes, texels + ((size_t)ty * blockX + tx) * DstBytes, DstBytes);
				}
			}
		}
	}
}

int GLTextureUploader_BC1_Decode::GetUploadSize(int x, int y, int w, int h)
{
	return w * h * 4;
}

static void BC1BlockToTexels(const uint8_t* block, uint8_t* texels)
{
	DecodeColorBlock(block, texels, 4, true);
}

void GLTextureUploader_BC1_Decode::UploadRect(void* d, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked)
{
	UploadDecoded<4>(d, mip, x, y, w, h, 8, 4, 4, BC1BlockToTexels);
}

int GLTextureUploader_RGBA32F_Decode::GetUploadSize(int x, int y, int w, int h)
{
	return w * h * 4;
}

void GLTextureUploader_RGBA32F_Decode::UploadRect(void* d, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked)
{
	int pitch = mip->Width * 16;
	float* src = (float*)(mip->Data.data() + (size_t)x * 16 + (size_t)y * pitch);
	uint8_t* dst = (uint8_t*)d;
#ifdef USE_NEON
	// Every lightmap rebuilt goes through here, several a frame on the
	// handheld. The same clamp, scale and truncation, four channels at once
	// (the Vulkan device's decoder, checked on the device against the loop
	// below for every float from 0 to 1 and for negatives, overflows,
	// infinities and NaNs -- all the same).
	const float32x4_t zero = vdupq_n_f32(0.0f), one = vdupq_n_f32(1.0f), scale = vdupq_n_f32(255.0f), half = vdupq_n_f32(0.5f);
	for (int i = 0; i < h; i++)
	{
		for (int j = 0; j < w; j++)
		{
			float32x4_t c = vld1q_f32(src + (size_t)j * 4);
			uint32x4_t lt = vcltq_f32(c, zero), gt = vcgtq_f32(c, one);
			c = vbslq_f32(lt, zero, vbslq_f32(gt, one, c));
			uint32x4_t u = vcvtq_u32_f32(vaddq_f32(vmulq_f32(c, scale), half));
			uint16x4_t u16 = vmovn_u32(u);
			uint8x8_t u8 = vmovn_u16(vcombine_u16(u16, u16));
			vst1_lane_u32((uint32_t*)(dst + (size_t)j * 4), vreinterpret_u32_u8(u8), 0);
		}
		dst += (size_t)w * 4;
		src = (float*)((uint8_t*)src + pitch);
	}
#else
	for (int i = 0; i < h; i++)
	{
		for (int j = 0; j < w; j++)
		{
			float r = std::clamp(src[(size_t)j * 4 + 0], 0.0f, 1.0f);
			float g = std::clamp(src[(size_t)j * 4 + 1], 0.0f, 1.0f);
			float b = std::clamp(src[(size_t)j * 4 + 2], 0.0f, 1.0f);
			float a = std::clamp(src[(size_t)j * 4 + 3], 0.0f, 1.0f);
			dst[(size_t)j * 4 + 0] = (uint8_t)(r * 255.0f + 0.5f);
			dst[(size_t)j * 4 + 1] = (uint8_t)(g * 255.0f + 0.5f);
			dst[(size_t)j * 4 + 2] = (uint8_t)(b * 255.0f + 0.5f);
			dst[(size_t)j * 4 + 3] = (uint8_t)(a * 255.0f + 0.5f);
		}
		dst += (size_t)w * 4;
		src = (float*)((uint8_t*)src + pitch);
	}
#endif
}
