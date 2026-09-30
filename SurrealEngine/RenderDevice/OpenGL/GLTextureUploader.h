#pragma once

#include "GLHandles.h"

struct TextureInfo;
class UnrealMipmap;
struct TextureColor;
enum class TextureFormat : uint32_t;

class GLTextureUploader
{
public:
	GLTextureUploader(GLint internalformat, GLenum format, GLenum type) : Internalformat(internalformat), Format(format), Type(type) { }
	virtual ~GLTextureUploader() = default;

	virtual int GetUploadSize(int x, int y, int w, int h) = 0;
	virtual void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) = 0;

	GLint GetInternalformat() const { return Internalformat; }
	GLenum GetFormat() const { return Format; }
	GLenum GetType() const { return Type; }

	static GLTextureUploader* GetUploader(TextureFormat format);

	// ES-driver capability switches, set by the device at init from the
	// context's extension list: without EXT_texture_compression_s3tc (the
	// PowerVR GE8300 has none) BC1 textures decode to RGBA8 on the CPU, and
	// without OES_texture_float_linear RGBA32F is sampleable but not
	// filterable (every lightmap and fog map would come up speckled), so it
	// comes down to RGBA8 as well. Desktop GL has both; they default on.
	static void SetS3TCSupported(bool supported);
	static void SetRGBA32FLinearSupported(bool supported);

private:
	GLint Internalformat;
	GLenum Format;
	GLenum Type;
};

class GLTextureUploader_P8 : public GLTextureUploader
{
public:
	GLTextureUploader_P8() : GLTextureUploader(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) override;
};

class GLTextureUploader_RGB8 : public GLTextureUploader
{
public:
	GLTextureUploader_RGB8() : GLTextureUploader(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) override;
};

class GLTextureUploader_BGRA8_LM : public GLTextureUploader
{
public:
	GLTextureUploader_BGRA8_LM() : GLTextureUploader(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) override;
};

class GLTextureUploader_RGB10A2 : public GLTextureUploader
{
public:
	GLTextureUploader_RGB10A2() : GLTextureUploader(GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) override;
};

class GLTextureUploader_RGB10A2_UI : public GLTextureUploader
{
public:
	GLTextureUploader_RGB10A2_UI() : GLTextureUploader(GL_RGBA16UI, GL_RGBA, GL_UNSIGNED_SHORT) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) override;
};

class GLTextureUploader_RGB10A2_LM : public GLTextureUploader
{
public:
	GLTextureUploader_RGB10A2_LM() : GLTextureUploader(GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) override;
};

class GLTextureUploader_Simple : public GLTextureUploader
{
public:
	GLTextureUploader_Simple(GLint internalformat, GLenum format, GLenum type, int bytesPerPixel) : GLTextureUploader(internalformat, format, type), BytesPerPixel(bytesPerPixel) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) override;

private:
	int BytesPerPixel;
};

class GLTextureUploader_4x4Block : public GLTextureUploader
{
public:
	GLTextureUploader_4x4Block(GLint internalformat, GLenum format, GLenum type, int bytesPerBlock) : GLTextureUploader(internalformat, format, type), BytesPerBlock(bytesPerBlock) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) override;

private:
	int BytesPerBlock;
};

class GLTextureUploader_BC1_Decode : public GLTextureUploader
{
public:
	GLTextureUploader_BC1_Decode() : GLTextureUploader(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) override;
};

class GLTextureUploader_RGBA32F_Decode : public GLTextureUploader
{
public:
	GLTextureUploader_RGBA32F_Decode() : GLTextureUploader(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) override;
};

class GLTextureUploader_2DBlock : public GLTextureUploader
{
public:
	GLTextureUploader_2DBlock(GLint internalformat, GLenum format, GLenum type, int blockX, int blockY, int bytesPerBlock) : GLTextureUploader(internalformat, format, type), BlockX(blockX), BlockY(blockY), BytesPerBlock(bytesPerBlock) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) override;

private:
	int BlockX;
	int BlockY;
	int BytesPerBlock;
};
