
#include "Precomp.h"
#include "GLRenderDevice.h"
#include "GLCachedTexture.h"
#include "GLFileResource.h"
#include "Utils/Logger.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include <surrealwidgets/core/widget.h>
#include <cmath>

static Widget* InitGLWidget = nullptr;
extern "C"
{
	void* IntGetProcAddress(const GLubyte* name)
	{
		return reinterpret_cast<void*>(InitGLWidget->GetGLProcAddress((const char*)name));
	}
}

GLRenderDevice::GLRenderDevice(Widget* InViewport)
{
	Viewport = InViewport;
	if (!Init(Viewport->GetNativePixelWidth(), Viewport->GetNativePixelHeight(), false))
		Exception::Throw("Could not init GLRenderDevice");
}

GLRenderDevice::~GLRenderDevice()
{
	Exit();
}

int GLRenderDevice::GetSettingsMultisample()
{
	switch (AntialiasMode)
	{
	default:
	case 0: return 0;
	case 1: return 2;
	case 2: return 4;
	}
}

bool GLRenderDevice::Init(int NewX, int NewY, bool Fullscreen)
{
	ActiveHdr = Hdr;
	BufferCount = UseVSync ? 2 : 3;

	try
	{
		Viewport->CreateGLContext();
		Viewport->MakeGLContextCurrent();

		InitGLWidget = Viewport;
		int result = ogl_LoadFunctions();
		InitGLWidget = nullptr;
		if (result == ogl_LOAD_FAILED)
			throw std::runtime_error("ogl_LoadFunctions failed");

		// An ES context (RenderAPI::GLES) reports its version as
		// "OpenGL ES 3.2 <driver>"; the desktop one as "4.6 (Core Profile) ...".
		const GLubyte* version = glGetString(GL_VERSION);
		IsGLES = version && strncmp((const char*)version, "OpenGL ES", 9) == 0;
		if (IsGLES)
			LogMessage(std::string("GLDrv: OpenGL ES context, ") + (const char*)version);

		// The ES path can sample and linearly filter everything the game
		// uploads only where the driver has the extensions for it: the GE8300
		// has no S3TC at all (BC1 textures decode to RGBA8 on the CPU) and
		// samples RGBA32F but cannot filter it (lightmaps and fog maps,
		// which would come up speckled). Decode both on the CPU to RGBA8,
		// the same substitutions the Vulkan device makes for that GPU
		// (patch 0002's decoders; GL gets its own below).
		if (IsGLES)
		{
			// BC1: EXT_texture_compression_s3tc. RGBA32F filtering:
			// OES_texture_float_linear. ES 3.0 has no glGetString(GL_EXTENSIONS);
			// enumerate with glGetStringi, as the loader does.
			bool s3tc = false;
			bool floatLinear = false;
			bool anisotropic = false;
			GLint numExtensions = 0;
			glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
			for (GLint i = 0; i < numExtensions; i++)
			{
				const GLubyte* e = glGetStringi(GL_EXTENSIONS, (GLuint)i);
				if (e)
				{
					const char* name = (const char*)e;
					s3tc = s3tc || strcmp(name, "GL_EXT_texture_compression_s3tc") == 0 || strcmp(name, "GL_WEBGL_compressed_texture_s3tc") == 0;
					floatLinear = floatLinear || strcmp(name, "GL_OES_texture_float_linear") == 0;
					anisotropic = anisotropic || strcmp(name, "GL_EXT_texture_filter_anisotropic") == 0;
				}
			}
			GLTextureUploader::SetS3TCSupported(s3tc);
			GLTextureUploader::SetRGBA32FLinearSupported(floatLinear);
			SamplerAnisotropy = anisotropic;
		}

		// GL_MIRROR_CLAMP_TO_EDGE: 4.4 desktop and 3.2 ES core on paper, but
		// optional in practice (the GE8300's ES 3.2 driver rejects it — the
		// on-device probe, plans/gles-sampler-probe.c). Probe once; fall back
		// to CLAMP_TO_EDGE where absent.
		{
			GLuint probe = 0;
			glGenSamplers(1, &probe);
			glSamplerParameteri(probe, GL_TEXTURE_WRAP_S, GL_MIRROR_CLAMP_TO_EDGE);
			SamplerMirrorClampEdge = glGetError() == GL_NO_ERROR;
			while (glGetError() != GL_NO_ERROR) {}
			glDeleteSamplers(1, &probe);
			if (!SamplerMirrorClampEdge)
				LogMessage("GLDrv: GL_MIRROR_CLAMP_TO_EDGE absent; clamping to edge");
		}

		CreateScenePass();
		CreatePresentPass();
		CreateBloomPass();

		Textures.reset(new GLTextureManager(this));
		Uploads.reset(new GLUploadManager(this));
	}
	catch (const std::exception& e)
	{
		LogMessage(std::string("Could not create opengl renderer: ") + e.what());
		Exit();
		return false;
	}

	if (!SetRes(NewX, NewY, Fullscreen))
	{
		Exit();
		return false;
	}

	return true;
}

bool GLRenderDevice::SetRes(int NewX, int NewY, bool Fullscreen)
{
	CurrentSizeX = NewX;
	CurrentSizeY = NewY;
	CurrentFullscreen = Fullscreen;

	BufferCount = UseVSync ? 2 : 3;

	Flush(1);
	return true;
}

void GLRenderDevice::Exit()
{
	if (Exited)
		return;
	Exited = true;

	LogMessage("GLDrv: exit called");

	// Teardown must not throw: the engine closes the window -- and with it
	// the GL context -- before the device is destroyed, so if a frame was
	// still mapped when the game ended (the intro level ends mid-frame),
	// the unmap runs with no context and fails. Forget the mappings and
	// carry on; Exit() is also called from the destructor, where a throw
	// aborts the process.
	try
	{
		UnmapVertices();
	}
	catch (...)
	{
		SceneVertices = nullptr;
		SceneIndexes = nullptr;
	}

	Uploads.reset();
	Textures.reset();
	ReleasePresentPass();
	ReleaseBloomPass();
	ReleaseScenePass();
	ReleaseSceneBuffers();
}

void GLRenderDevice::ResizeSceneBuffers(int width, int height, int multisample)
{
	multisample = std::max(multisample, 1);

	if (SceneBuffers.Width == width && SceneBuffers.Height == height && multisample == SceneBuffers.Multisample && SceneBuffers.ColorBuffer && SceneBuffers.HitBuffer && SceneBuffers.PPHitBuffer && SceneBuffers.StagingHitBuffer && SceneBuffers.DepthBuffer && SceneBuffers.PPImage[0] && SceneBuffers.PPImage[1])
		return;

	SceneBuffers.Framebuffer.reset();
	SceneBuffers.HitFramebuffer.reset();
	SceneBuffers.PPHitFramebuffer.reset();
	for (int i = 0; i < 2; i++)
	{
		SceneBuffers.PPFramebuffer[i].reset();
		SceneBuffers.PPImage[i].reset();
	}
	SceneBuffers.ColorBuffer.reset();
	SceneBuffers.StagingHitBuffer.reset();
	SceneBuffers.PPHitBuffer.reset();
	SceneBuffers.HitBuffer.reset();
	SceneBuffers.DepthBuffer.reset();

	for (PPBlurLevel& level : SceneBuffers.BlurLevels)
	{
		level.VTexture.reset();
		level.VFramebuffer.reset();
		level.HTexture.reset();
		level.HFramebuffer.reset();
	}

	SceneBuffers.Width = width;
	SceneBuffers.Height = height;
	SceneBuffers.Multisample = multisample;

	// The scene and post-process color buffers are 8-bit unless HDR is asked
	// for: half the bandwidth on the handheld's shared-memory SoC, and what
	// the original XOpenGLDrv's own 8-bit buffer was (the overbright the
		// scene shader can write clamps at the write, as it did there).
	GLenum colorInternal = ActiveHdr ? GL_RGBA16F : GL_RGBA8;
	GLenum colorFormat = GL_RGBA;
	GLenum colorType = ActiveHdr ? GL_FLOAT : GL_UNSIGNED_BYTE;

	SceneBuffers.ColorBuffer = std::make_shared<GLTexture2D>();
	if (SceneBuffers.Multisample > 1)
	{
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, SceneBuffers.ColorBuffer->Handle);
		SetDebugName(SceneBuffers.ColorBuffer, "SceneBuffers.ColorBuffer");
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, SceneBuffers.Multisample, colorInternal, SceneBuffers.Width, SceneBuffers.Height, GL_FALSE);
		glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAX_LEVEL, 0);
		ThrowIfGLError("glTexStorage2DMultisample(SceneBuffers.ColorBuffer) failed");
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, SceneBuffers.ColorBuffer->Handle);
		SetDebugName(SceneBuffers.ColorBuffer, "SceneBuffers.ColorBuffer");
		glTexImage2D(GL_TEXTURE_2D, 0, colorInternal, SceneBuffers.Width, SceneBuffers.Height, 0, colorFormat, colorType, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		ThrowIfGLError("glTexImage2D(SceneBuffers.ColorBuffer) failed");
	}

	SceneBuffers.HitBuffer = std::make_shared<GLTexture2D>();
	if (SceneBuffers.Multisample > 1)
	{
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, SceneBuffers.HitBuffer->Handle);
		SetDebugName(SceneBuffers.HitBuffer, "SceneBuffers.HitBuffer");
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, SceneBuffers.Multisample, GL_R32UI, SceneBuffers.Width, SceneBuffers.Height, GL_FALSE);
		glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAX_LEVEL, 0);
		ThrowIfGLError("glTexStorage2DMultisample(SceneBuffers.HitBuffer) failed");
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, SceneBuffers.HitBuffer->Handle);
		SetDebugName(SceneBuffers.HitBuffer, "SceneBuffers.HitBuffer");
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, SceneBuffers.Width, SceneBuffers.Height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		ThrowIfGLError("glTexImage2D(SceneBuffers.HitBuffer) failed");
	}

	SceneBuffers.PPHitBuffer = std::make_shared<GLTexture2D>();
	glBindTexture(GL_TEXTURE_2D, SceneBuffers.PPHitBuffer->Handle);
	SetDebugName(SceneBuffers.PPHitBuffer, "SceneBuffers.PPHitBuffer");
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, SceneBuffers.Width, SceneBuffers.Height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	ThrowIfGLError("glTexImage2D(SceneBuffers.PPHitBuffer) failed");

	// To do: staging buffers can't be textures in OpenGL
	SceneBuffers.StagingHitBuffer = std::make_shared<GLTexture2D>();
	glBindTexture(GL_TEXTURE_2D, SceneBuffers.StagingHitBuffer->Handle);
	SetDebugName(SceneBuffers.StagingHitBuffer, "SceneBuffers.StagingHitBuffer");
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, SceneBuffers.Width, SceneBuffers.Height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	ThrowIfGLError("glTexImage2D(SceneBuffers.StagingHitBuffer) failed");

	SceneBuffers.DepthBuffer = std::make_shared<GLTexture2D>();
	if (SceneBuffers.Multisample > 1)
	{
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, SceneBuffers.DepthBuffer->Handle);
		SetDebugName(SceneBuffers.DepthBuffer, "SceneBuffers.DepthBuffer");
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, SceneBuffers.Multisample, GL_DEPTH_COMPONENT32F, SceneBuffers.Width, SceneBuffers.Height, GL_FALSE);
		glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAX_LEVEL, 0);
		ThrowIfGLError("glTexStorage2DMultisample(SceneBuffers.DepthBuffer) failed");
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, SceneBuffers.DepthBuffer->Handle);
		SetDebugName(SceneBuffers.DepthBuffer, "SceneBuffers.DepthBuffer");
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, SceneBuffers.Width, SceneBuffers.Height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		ThrowIfGLError("glTexImage2D(SceneBuffers.DepthBuffer) failed");
	}

	for (int i = 0; i < 2; i++)
	{
		SceneBuffers.PPImage[i] = std::make_shared<GLTexture2D>();
		glBindTexture(GL_TEXTURE_2D, SceneBuffers.PPImage[i]->Handle);
		SetDebugName(SceneBuffers.PPImage[i], "SceneBuffers.PPImage");
		glTexImage2D(GL_TEXTURE_2D, 0, colorInternal, SceneBuffers.Width, SceneBuffers.Height, 0, colorFormat, colorType, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		ThrowIfGLError("glTexImage2D(SceneBuffers.PPImage) failed");
	}

	SceneBuffers.Framebuffer = std::make_shared<GLFramebuffer>();
	glBindFramebuffer(GL_FRAMEBUFFER, SceneBuffers.Framebuffer->Handle);
	SetDebugName(SceneBuffers.Framebuffer, "SceneBuffers.Framebuffer");
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, SceneBuffers.ColorBuffer->Handle, 0);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, SceneBuffers.HitBuffer->Handle, 0);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, SceneBuffers.DepthBuffer->Handle, 0);
	ThrowIfGLError("Creating SceneBuffers.Framebuffer failed");

	SceneBuffers.HitFramebuffer = std::make_shared<GLFramebuffer>();
	glBindFramebuffer(GL_FRAMEBUFFER, SceneBuffers.HitFramebuffer->Handle);
	SetDebugName(SceneBuffers.HitFramebuffer, "SceneBuffers.HitFramebuffer");
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, SceneBuffers.HitBuffer->Handle, 0);
	ThrowIfGLError("Creating SceneBuffers.HitFramebuffer failed");

	SceneBuffers.PPHitFramebuffer = std::make_shared<GLFramebuffer>();
	glBindFramebuffer(GL_FRAMEBUFFER, SceneBuffers.PPHitFramebuffer->Handle);
	SetDebugName(SceneBuffers.PPHitFramebuffer, "SceneBuffers.PPHitFramebuffer");
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, SceneBuffers.PPHitBuffer->Handle, 0);
	ThrowIfGLError("Creating SceneBuffers.PPHitFramebuffer failed");

	for (int i = 0; i < 2; i++)
	{
		SceneBuffers.PPFramebuffer[i] = std::make_shared<GLFramebuffer>();
		glBindFramebuffer(GL_FRAMEBUFFER, SceneBuffers.PPFramebuffer[i]->Handle);
		SetDebugName(SceneBuffers.PPFramebuffer[i], "SceneBuffers.PPFramebuffer");
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, SceneBuffers.PPImage[i]->Handle, 0);
		ThrowIfGLError("Creating SceneBuffers.PPFramebuffer failed");
	}

	int bloomWidth = width;
	int bloomHeight = height;
	for (PPBlurLevel& level : SceneBuffers.BlurLevels)
	{
		bloomWidth = (bloomWidth + 1) / 2;
		bloomHeight = (bloomHeight + 1) / 2;

		level.VTexture = std::make_shared<GLTexture2D>();
		glBindTexture(GL_TEXTURE_2D, level.VTexture->Handle);
		SetDebugName(level.VTexture, "SceneBuffers.BlurLevels.VTexture");
		glTexImage2D(GL_TEXTURE_2D, 0, colorInternal, bloomWidth, bloomHeight, 0, colorFormat, colorType, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		ThrowIfGLError("glTexImage2D(SceneBuffers.BlurLevels.VTexture) failed");

		level.HTexture = std::make_shared<GLTexture2D>();
		glBindTexture(GL_TEXTURE_2D, level.HTexture->Handle);
		SetDebugName(level.HTexture, "SceneBuffers.BlurLevels.HTexture");
		glTexImage2D(GL_TEXTURE_2D, 0, colorInternal, bloomWidth, bloomHeight, 0, colorFormat, colorType, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		ThrowIfGLError("glTexImage2D(SceneBuffers.BlurLevels.HTexture) failed");

		level.VFramebuffer = std::make_shared<GLFramebuffer>();
		glBindFramebuffer(GL_FRAMEBUFFER, level.VFramebuffer->Handle);
		SetDebugName(level.VFramebuffer, "SceneBuffers.BlurLevels.VFramebuffer");
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, level.VTexture->Handle, 0);
		ThrowIfGLError("Creating SceneBuffers.BlurLevels.VFramebuffer failed");

		level.HFramebuffer = std::make_shared<GLFramebuffer>();
		glBindFramebuffer(GL_FRAMEBUFFER, level.HFramebuffer->Handle);
		SetDebugName(level.HFramebuffer, "SceneBuffers.BlurLevels.HFramebuffer");
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, level.HTexture->Handle, 0);
		ThrowIfGLError("Creating SceneBuffers.BlurLevels.HFramebuffer failed");

		level.Width = bloomWidth;
		level.Height = bloomHeight;
	}
}

void GLRenderDevice::CreateScenePass()
{
	ScenePass.VertexShader = CreateVertexShader("ScenePass.VertexShader", "shaders/Scene.vert");
	std::vector<std::string> sceneDefines;
	if (!DarkClamp)
		sceneDefines.push_back("NO_DARKCLAMP");
	std::vector<std::string> alphaTestDefines = sceneDefines;
	alphaTestDefines.push_back("ALPHATEST");
	ScenePass.FragmentShader = CreateFragmentShader("ScenePass.PixelShader", "shaders/Scene.frag", sceneDefines);
	ScenePass.FragmentShaderAlphaTest = CreateFragmentShader("ScenePass.PixelShaderAlphaTest", "shaders/Scene.frag", alphaTestDefines);
	ScenePass.ShaderProgram = CreateProgram("ScenePass.ShaderProgram", ScenePass.VertexShader, ScenePass.FragmentShader);
	ScenePass.ShaderProgramAlphaTest = CreateProgram("ScenePass.ShaderProgramAlphaTest", ScenePass.VertexShader, ScenePass.FragmentShaderAlphaTest);

	CreateSceneSamplers();

	for (int i = 0; i < 2; i++)
	{
		GLRasterizerDesc rasterizerDesc = {};
		rasterizerDesc.CullEnable = false;
		rasterizerDesc.FrontCounterClockwise = false;
		rasterizerDesc.DepthClipEnable = false; // Avoid clipping the weapon. The UE1 engine clips the geometry anyway.
		rasterizerDesc.MultisampleEnable = i == 1;
		ScenePass.RasterizerState[i] = CreateRasterizerState(rasterizerDesc);
	}

	for (int i = 0; i < 33; i++)
	{
		GLBlendDesc blendDesc = {};
		blendDesc.IndependentBlendEnable = true;
		blendDesc.RenderTarget[0].BlendEnable = true;
		if (i < 32)
		{
			switch (i & 3)
			{
			case 0: // PF_Translucent
				blendDesc.RenderTarget[0].BlendOp = GL_FUNC_ADD;
				blendDesc.RenderTarget[0].BlendOpAlpha = GL_FUNC_ADD;
				blendDesc.RenderTarget[0].SrcBlend = GL_ONE;
				blendDesc.RenderTarget[0].SrcBlendAlpha = GL_ONE;
				blendDesc.RenderTarget[0].DestBlend = GL_ONE_MINUS_SRC_COLOR;
				blendDesc.RenderTarget[0].DestBlendAlpha = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case 1: // PF_Modulated
				blendDesc.RenderTarget[0].BlendOp = GL_FUNC_ADD;
				blendDesc.RenderTarget[0].BlendOpAlpha = GL_FUNC_ADD;
				blendDesc.RenderTarget[0].SrcBlend = GL_DST_COLOR;
				blendDesc.RenderTarget[0].SrcBlendAlpha = GL_DST_ALPHA;
				blendDesc.RenderTarget[0].DestBlend = GL_SRC_COLOR;
				blendDesc.RenderTarget[0].DestBlendAlpha = GL_SRC_ALPHA;
				break;
			case 2: // PF_Highlighted
				blendDesc.RenderTarget[0].BlendOp = GL_FUNC_ADD;
				blendDesc.RenderTarget[0].BlendOpAlpha = GL_FUNC_ADD;
				blendDesc.RenderTarget[0].SrcBlend = GL_ONE;
				blendDesc.RenderTarget[0].SrcBlendAlpha = GL_ONE;
				blendDesc.RenderTarget[0].DestBlend = GL_ONE_MINUS_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlendAlpha = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case 3: // Hmm, is it faster to keep the blend mode enabled or to toggle it?
				blendDesc.RenderTarget[0].BlendOp = GL_FUNC_ADD;
				blendDesc.RenderTarget[0].BlendOpAlpha = GL_FUNC_ADD;
				blendDesc.RenderTarget[0].SrcBlend = GL_ONE;
				blendDesc.RenderTarget[0].SrcBlendAlpha = GL_ONE;
				blendDesc.RenderTarget[0].DestBlend = GL_ZERO;
				blendDesc.RenderTarget[0].DestBlendAlpha = GL_ZERO;
				break;
			}
			if (i & 4) // PF_Invisible
				blendDesc.RenderTarget[0].RenderTargetWriteMask = false;
			else
				blendDesc.RenderTarget[0].RenderTargetWriteMask = true;
		}
		else // PF_SubpixelFont
		{
			blendDesc.RenderTarget[0].BlendOp = GL_FUNC_ADD;
			blendDesc.RenderTarget[0].BlendOpAlpha = GL_FUNC_ADD;
			blendDesc.RenderTarget[0].SrcBlend = GL_CONSTANT_COLOR;
			blendDesc.RenderTarget[0].SrcBlendAlpha = GL_CONSTANT_ALPHA;
			blendDesc.RenderTarget[0].DestBlend = GL_ONE_MINUS_SRC_COLOR;
			blendDesc.RenderTarget[0].DestBlendAlpha = GL_ONE_MINUS_SRC_ALPHA;
			blendDesc.RenderTarget[0].RenderTargetWriteMask = true;
		}
		blendDesc.RenderTarget[1].BlendEnable = false;
		blendDesc.RenderTarget[1].RenderTargetWriteMask = true;
		ScenePass.Pipelines[i].BlendState = CreateBlendState(blendDesc);

		GLDepthStencilDesc depthStencilDesc = {};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthFunc = GL_LEQUAL;
		if (i & 8) // PF_Occlude
			depthStencilDesc.DepthWriteMask = true;
		else
			depthStencilDesc.DepthWriteMask = false;
		ScenePass.Pipelines[i].DepthStencilState = CreateDepthStencilState(depthStencilDesc);

		if ((i & 16) || i == 32) // PF_Masked or PF_SubpixelFont
			ScenePass.Pipelines[i].ShaderProgram = ScenePass.ShaderProgramAlphaTest.get();
		else
			ScenePass.Pipelines[i].ShaderProgram = ScenePass.ShaderProgram.get();

		ScenePass.Pipelines[i].PrimitiveTopology = GL_TRIANGLES;
	}

	// Line pipeline
	for (int i = 0; i < 2; i++)
	{
		GLBlendDesc blendDesc = {};
		blendDesc.IndependentBlendEnable = true;
		blendDesc.RenderTarget[0].BlendEnable = true;
		blendDesc.RenderTarget[0].BlendOp = GL_FUNC_ADD;
		blendDesc.RenderTarget[0].BlendOpAlpha = GL_FUNC_ADD;
		blendDesc.RenderTarget[0].SrcBlend = GL_ONE;
		blendDesc.RenderTarget[0].SrcBlendAlpha = GL_ONE;
		blendDesc.RenderTarget[0].DestBlend = GL_ONE_MINUS_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlendAlpha = GL_ONE_MINUS_SRC_ALPHA;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = true;
		blendDesc.RenderTarget[1].BlendEnable = false;
		blendDesc.RenderTarget[1].RenderTargetWriteMask = true;
		ScenePass.LinePipeline[i].BlendState = CreateBlendState(blendDesc);

		GLDepthStencilDesc depthStencilDesc = {};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthFunc = GL_LEQUAL;
		depthStencilDesc.DepthWriteMask = true;
		ScenePass.LinePipeline[i].DepthStencilState = CreateDepthStencilState(depthStencilDesc);

		ScenePass.LinePipeline[i].ShaderProgram = ScenePass.ShaderProgram.get();
		ScenePass.LinePipeline[i].PrimitiveTopology = GL_LINES;

		if (i == 0)
		{
			ScenePass.LinePipeline[i].MinDepth = 0.0f;
			ScenePass.LinePipeline[i].MaxDepth = 0.1f;
		}
	}

	// Point pipeline
	for (int i = 0; i < 2; i++)
	{
		GLBlendDesc blendDesc = {};
		blendDesc.IndependentBlendEnable = true;
		blendDesc.RenderTarget[0].BlendEnable = true;
		blendDesc.RenderTarget[0].BlendOp = GL_FUNC_ADD;
		blendDesc.RenderTarget[0].BlendOpAlpha = GL_FUNC_ADD;
		blendDesc.RenderTarget[0].SrcBlend = GL_ONE;
		blendDesc.RenderTarget[0].SrcBlendAlpha = GL_ONE;
		blendDesc.RenderTarget[0].DestBlend = GL_ONE_MINUS_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlendAlpha = GL_ONE_MINUS_SRC_ALPHA;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = true;
		blendDesc.RenderTarget[1].BlendEnable = false;
		blendDesc.RenderTarget[1].RenderTargetWriteMask = true;
		ScenePass.PointPipeline[i].BlendState = CreateBlendState(blendDesc);

		GLDepthStencilDesc depthStencilDesc = {};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthFunc = GL_LEQUAL;
		depthStencilDesc.DepthWriteMask = true;
		ScenePass.PointPipeline[i].DepthStencilState = CreateDepthStencilState(depthStencilDesc);

		ScenePass.PointPipeline[i].ShaderProgram = ScenePass.ShaderProgram.get();
		ScenePass.PointPipeline[i].PrimitiveTopology = GL_TRIANGLES;

		if (i == 0)
		{
			ScenePass.PointPipeline[i].MinDepth = 0.0f;
			ScenePass.PointPipeline[i].MaxDepth = 0.1f;
		}
	}

	ScenePass.VertexBuffer = CreateBuffer(GL_ARRAY_BUFFER, GL_STREAM_DRAW, nullptr, SceneVertexBufferSize * sizeof(GLSceneVertex), "ScenePass.VertexBuffer");
	ScenePass.IndexBuffer = CreateBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_STREAM_DRAW, nullptr, SceneIndexBufferSize * sizeof(uint32_t), "ScenePass.IndexBuffer");
	ScenePass.ConstantBuffer = CreateBuffer(GL_UNIFORM_BUFFER, GL_STREAM_DRAW, nullptr, sizeof(GLScenePushConstants), "ScenePass.ConstantBuffer");

	ScenePass.InputLayout = std::make_shared<GLInputLayout>();
	glBindVertexArray(ScenePass.InputLayout->Handle);
	SetDebugName(ScenePass.InputLayout, "ScenePass.InputLayout");
	glBindBuffer(GL_ARRAY_BUFFER, ScenePass.VertexBuffer->Handle);
	glEnableVertexAttribArray(0);
	glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(GLSceneVertex), (const void*)offsetof(GLSceneVertex, Flags));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GLSceneVertex), (const void*)offsetof(GLSceneVertex, Position));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GLSceneVertex), (const void*)offsetof(GLSceneVertex, TexCoord));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(GLSceneVertex), (const void*)offsetof(GLSceneVertex, TexCoord2));
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(GLSceneVertex), (const void*)offsetof(GLSceneVertex, TexCoord3));
	glEnableVertexAttribArray(5);
	glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(GLSceneVertex), (const void*)offsetof(GLSceneVertex, TexCoord4));
	glEnableVertexAttribArray(6);
	glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(GLSceneVertex), (const void*)offsetof(GLSceneVertex, Color));
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLRenderDevice::CreateSceneSamplers()
{
	for (int i = 0; i < 16; i++)
	{
		ScenePass.Samplers[i] = std::make_shared<GLSampler>();
		glBindSampler(0, ScenePass.Samplers[i]->Handle); // We must bind it before it actually exists. So stupid!
		SetDebugName(ScenePass.Samplers[i], "ScenePass.Samplers");

		GLuint sampler = ScenePass.Samplers[i]->Handle;

		int dummyMipmapCount = (i >> 2) & 3;
		GLint addressmode = (i & 2) ? (SamplerMirrorClampEdge ? GL_MIRROR_CLAMP_TO_EDGE : GL_CLAMP_TO_EDGE) : GL_REPEAT;

#if 0
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, (i & 1) ? GL_NEAREST : GL_LINEAR);
		glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, (i & 1) ? GL_NEAREST : GL_LINEAR);
#else
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, (i & 1) ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR);
		glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, (i & 1) ? GL_NEAREST : GL_LINEAR);
		if ((i & 1) && SamplerAnisotropy)
			glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8.0f); // needs EXT_texture_filter_anisotropic
#endif
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, addressmode);
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, addressmode);
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, addressmode);
		glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, (float)dummyMipmapCount);
		if (!IsGLES)
		{
			// GL_TEXTURE_LOD_BIAS is desktop GL only (Mesa tolerates it, the
			// GE8300 rejects it); ES has no sampler LOD bias at all.
			glSamplerParameterf(sampler, GL_TEXTURE_LOD_BIAS, (float)dummyMipmapCount + LODBias);
		}

		ThrowIfGLError("Could not create sampler");
	}

	ScenePass.LODBias = LODBias;
}

void GLRenderDevice::ReleaseSceneSamplers()
{
	for (auto& sampler : ScenePass.Samplers)
	{
		sampler.reset();
	}
	ScenePass.LODBias = 0.0f;
}

void GLRenderDevice::UpdateLODBias()
{
	if (ScenePass.LODBias != LODBias)
	{
		ReleaseSceneSamplers();
		CreateSceneSamplers();
	}
}

void GLRenderDevice::ReleaseScenePass()
{
	ScenePass.VertexShader.reset();
	ScenePass.InputLayout.reset();
	ScenePass.VertexBuffer.reset();
	ScenePass.IndexBuffer.reset();
	ScenePass.ConstantBuffer.reset();
	ScenePass.RasterizerState[0].reset();
	ScenePass.RasterizerState[1].reset();
	ScenePass.ShaderProgram.reset();
	ScenePass.ShaderProgramAlphaTest.reset();
	ReleaseSceneSamplers();
	for (auto& pipeline : ScenePass.Pipelines)
	{
		pipeline.BlendState.reset();
		pipeline.DepthStencilState.reset();
	}
	for (int i = 0; i < 2; i++)
	{
		ScenePass.LinePipeline[i].BlendState.reset();
		ScenePass.LinePipeline[i].DepthStencilState.reset();
		ScenePass.PointPipeline[i].BlendState.reset();
		ScenePass.PointPipeline[i].DepthStencilState.reset();
	}
}

void GLRenderDevice::ReleaseBloomPass()
{
	BloomPass.Extract.reset();
	BloomPass.Combine.reset();
	BloomPass.BlurVertical.reset();
	BloomPass.BlurHorizontal.reset();
	BloomPass.ConstantBuffer.reset();
	BloomPass.AdditiveBlendState.reset();
}

void GLRenderDevice::ReleasePresentPass()
{
	PresentPass.PPStepLayout.reset();
	PresentPass.PPStep.reset();
	PresentPass.PPStepVertexBuffer.reset();
	PresentPass.HitResolve.reset();
	for (auto& shader : PresentPass.Present) shader.reset();
	PresentPass.PresentConstantBuffer.reset();
	PresentPass.DitherTexture.reset();
	PresentPass.BlendState.reset();
	PresentPass.DepthStencilState.reset();
	PresentPass.RasterizerState.reset();
}

void GLRenderDevice::ReleaseSceneBuffers()
{
	SceneBuffers.Framebuffer.reset();
	SceneBuffers.HitFramebuffer.reset();
	SceneBuffers.PPHitFramebuffer.reset();
	for (int i = 0; i < 2; i++)
	{
		SceneBuffers.PPFramebuffer[i].reset();
		SceneBuffers.PPImage[i].reset();
	}
	SceneBuffers.ColorBuffer.reset();
	SceneBuffers.StagingHitBuffer.reset();
	SceneBuffers.PPHitBuffer.reset();
	SceneBuffers.HitBuffer.reset();
	SceneBuffers.DepthBuffer.reset();
	for (PPBlurLevel& level : SceneBuffers.BlurLevels)
	{
		level.VTexture.reset();
		level.VFramebuffer.reset();
		level.HTexture.reset();
		level.HFramebuffer.reset();
	}
}

GLRenderDevice::ScenePipelineState* GLRenderDevice::GetPipeline(uint32_t PolyFlags)
{
	int index;
	if (PolyFlags & PF_Translucent)
	{
		index = 0;
	}
	else if (PolyFlags & PF_Modulated)
	{
		index = 1;
	}
	else if (PolyFlags & PF_Highlighted)
	{
		index = 2;
	}
	else
	{
		index = 3;
	}

	if (PolyFlags & PF_Invisible)
	{
		index |= 4;
	}
	if (PolyFlags & PF_Occlude)
	{
		index |= 8;
	}
	if (PolyFlags & PF_Masked)
	{
		index |= 16;
	}

	if (PolyFlags == PF_SubpixelFont)
	{
		index = 32;
	}

	return &ScenePass.Pipelines[index];
}

void GLRenderDevice::RunBloomPass()
{
	GLTexture2D* srvs[1] = {};

	float blurAmount = 0.6f + BloomAmount * (1.9f / 255.0f);
	GLBloomPushConstants pushconstants;
	ComputeBlurSamples(7, blurAmount, pushconstants.SampleWeights);

	SetBufferData(GL_UNIFORM_BUFFER, GL_STREAM_DRAW, BloomPass.ConstantBuffer.get(), &pushconstants, sizeof(GLBloomPushConstants));

	GLBuffer* cbs[1] = { BloomPass.ConstantBuffer.get() };
	glBindVertexArray(PresentPass.PPStepLayout->Handle);
	SetRasterizerState(PresentPass.RasterizerState.get());
	SetUniformBuffers(0, 1, cbs);
	SetDepthStencilState(PresentPass.DepthStencilState.get());
	SetBlendState(PresentPass.BlendState.get());

	GLViewport viewport = {};
	viewport.MaxDepth = 1.0f;

	// Extract overbright pixels that we want to bloom:
	viewport.Width = (float)SceneBuffers.BlurLevels[0].Width;
	viewport.Height = (float)SceneBuffers.BlurLevels[0].Height;

	srvs[0] = SceneBuffers.PPImage[0].get();
	glBindFramebuffer(GL_FRAMEBUFFER, SceneBuffers.BlurLevels[0].VFramebuffer->Handle);
	{ GLenum glAtt = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &glAtt); } // ES has only glDrawBuffers
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glUseProgram(BloomPass.ExtractProgram->Handle);
	SetViewport(viewport);
	SetTextures(0, 1, srvs);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	// Blur and downscale:
	for (int i = 0; i < SceneBuffers.NumBloomLevels - 1; i++)
	{
		auto& blevel = SceneBuffers.BlurLevels[i];
		auto& next = SceneBuffers.BlurLevels[i + 1];

		viewport.Width = (float)blevel.Width;
		viewport.Height = (float)blevel.Height;
		SetViewport(viewport);
		BlurStep(blevel.VTexture.get(), blevel.HFramebuffer.get(), false);
		BlurStep(blevel.HTexture.get(), blevel.VFramebuffer.get(), true);

		// Linear downscale:
		viewport.Width = (float)next.Width;
		viewport.Height = (float)next.Height;
		srvs[0] = blevel.VTexture.get();

		glBindFramebuffer(GL_FRAMEBUFFER, next.VFramebuffer->Handle);
		{ GLenum glAtt = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &glAtt); } // ES has only glDrawBuffers
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glUseProgram(BloomPass.CombineProgram->Handle);
		SetViewport(viewport);
		SetTextures(0, 1, srvs);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	// Blur and upscale:
	for (int i = SceneBuffers.NumBloomLevels - 1; i > 0; i--)
	{
		auto& blevel = SceneBuffers.BlurLevels[i];
		auto& next = SceneBuffers.BlurLevels[i - 1];

		viewport.Width = (float)blevel.Width;
		viewport.Height = (float)blevel.Height;
		SetViewport(viewport);
		BlurStep(blevel.VTexture.get(), blevel.HFramebuffer.get(), false);
		BlurStep(blevel.HTexture.get(), blevel.VFramebuffer.get(), true);

		// Linear upscale:
		viewport.Width = (float)next.Width;
		viewport.Height = (float)next.Height;
		srvs[0] = blevel.VTexture.get();
		glBindFramebuffer(GL_FRAMEBUFFER, next.VFramebuffer->Handle);
		{ GLenum glAtt = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &glAtt); } // ES has only glDrawBuffers
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glUseProgram(BloomPass.CombineProgram->Handle);
		SetViewport(viewport);
		SetTextures(0, 1, srvs);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	viewport.Width = (float)SceneBuffers.BlurLevels[0].Width;
	viewport.Height = (float)SceneBuffers.BlurLevels[0].Height;
	SetViewport(viewport);
	BlurStep(SceneBuffers.BlurLevels[0].VTexture.get(), SceneBuffers.BlurLevels[0].HFramebuffer.get(), false);
	BlurStep(SceneBuffers.BlurLevels[0].HTexture.get(), SceneBuffers.BlurLevels[0].VFramebuffer.get(), true);

	// Add bloom back to scene post process texture:
	viewport.Width = (float)SceneBuffers.Width;
	viewport.Height = (float)SceneBuffers.Height;
	srvs[0] = SceneBuffers.BlurLevels[0].VTexture.get();
	glBindFramebuffer(GL_FRAMEBUFFER, SceneBuffers.PPFramebuffer[0]->Handle);
	{ GLenum glAtt = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &glAtt); } // ES has only glDrawBuffers
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glUseProgram(BloomPass.CombineProgram->Handle);
	SetBlendState(BloomPass.AdditiveBlendState.get());
	SetViewport(viewport);
	SetTextures(0, 1, srvs);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

void GLRenderDevice::BlurStep(GLTexture2D* input, GLFramebuffer* output, bool vertical)
{
	glBindFramebuffer(GL_FRAMEBUFFER, output->Handle);
	{ GLenum glAtt = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &glAtt); } // ES has only glDrawBuffers
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glUseProgram(vertical ? BloomPass.BlurVerticalProgram->Handle : BloomPass.BlurHorizontalProgram->Handle);
	SetTextures(0, 1, &input);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

float GLRenderDevice::ComputeBlurGaussian(float n, float theta) // theta = Blur Amount
{
	return (float)((1.0f / std::sqrt(2 * 3.14159265359f * theta)) * std::exp(-(n * n) / (2.0f * theta * theta)));
}

void GLRenderDevice::ComputeBlurSamples(int sampleCount, float blurAmount, float* sampleWeights)
{
	sampleWeights[0] = ComputeBlurGaussian(0, blurAmount);

	float totalWeights = sampleWeights[0];

	for (int i = 0; i < sampleCount / 2; i++)
	{
		float weight = ComputeBlurGaussian(i + 1.0f, blurAmount);

		sampleWeights[i * 2 + 1] = weight;
		sampleWeights[i * 2 + 2] = weight;

		totalWeights += weight * 2;
	}

	for (int i = 0; i < sampleCount; i++)
	{
		sampleWeights[i] /= totalWeights;
	}
}

void GLRenderDevice::CreateBloomPass()
{
	BloomPass.Extract = CreateFragmentShader("BloomPass.Extract", "shaders/BloomExtract.frag");
	BloomPass.ExtractProgram = CreateProgram("BloomPass.ExtractProgram", PresentPass.PPStep, BloomPass.Extract);
	BloomPass.Combine = CreateFragmentShader("BloomPass.Combine", "shaders/BloomCombine.frag");
	BloomPass.CombineProgram = CreateProgram("BloomPass.CombineProgram", PresentPass.PPStep, BloomPass.Combine);
	BloomPass.BlurVertical = CreateFragmentShader("BloomPass.BlurVertical", "shaders/Blur.frag", { "BLUR_VERTICAL" });
	BloomPass.BlurVerticalProgram = CreateProgram("BloomPass.BlurVerticalProgram", PresentPass.PPStep, BloomPass.BlurVertical);
	BloomPass.BlurHorizontal = CreateFragmentShader("BloomPass.BlurHorizontal", "shaders/Blur.frag", { "BLUR_HORIZONTAL" });
	BloomPass.BlurHorizontalProgram = CreateProgram("BloomPass.BlurHorizontalProgram", PresentPass.PPStep, BloomPass.BlurHorizontal);

	BloomPass.ConstantBuffer = CreateBuffer(GL_UNIFORM_BUFFER, GL_STREAM_DRAW, nullptr, sizeof(GLBloomPushConstants), "BloomPass.ConstantBuffer");

	GLBlendDesc blendDesc = {};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = true;
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].BlendOp = GL_FUNC_ADD;
	blendDesc.RenderTarget[0].BlendOpAlpha = GL_FUNC_ADD;
	blendDesc.RenderTarget[0].SrcBlend = GL_ONE;
	blendDesc.RenderTarget[0].SrcBlendAlpha = GL_ONE;
	blendDesc.RenderTarget[0].DestBlend = GL_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = GL_ONE;
	BloomPass.AdditiveBlendState = CreateBlendState(blendDesc);
}

void GLRenderDevice::CreatePresentPass()
{
	std::vector<vec2> positions =
	{
		vec2(-1.0, -1.0),
		vec2( 1.0, -1.0),
		vec2(-1.0,  1.0),
		vec2(-1.0,  1.0),
		vec2( 1.0, -1.0),
		vec2( 1.0,  1.0)
	};

	PresentPass.PPStepVertexBuffer = CreateBuffer(GL_ARRAY_BUFFER, GL_STATIC_DRAW, positions.data(), positions.size() * sizeof(vec2), "PresentPass.PPStepVertexBuffer");
	PresentPass.PresentConstantBuffer = CreateBuffer(GL_UNIFORM_BUFFER, GL_STREAM_DRAW, nullptr, sizeof(GLPresentPushConstants), "PresentPass.PresentConstantBuffer");

	PresentPass.PPStepLayout = std::make_shared<GLInputLayout>();
	glBindVertexArray(PresentPass.PPStepLayout->Handle);
	SetDebugName(PresentPass.PPStepLayout, "PresentPass.PPStepLayout");
	glBindBuffer(GL_ARRAY_BUFFER, PresentPass.PPStepVertexBuffer->Handle);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), (const void*)offsetof(vec2, x));
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	PresentPass.PPStep = CreateVertexShader("PresentPass.PPStep", "shaders/PPStep.vert");

	static const char* transferFunctions[2] = { nullptr, "HDR_MODE" };
	static const char* gammaModes[2] = { "GAMMA_MODE_D3D9", "GAMMA_MODE_XOPENGL" };
	static const char* colorModes[4] = { nullptr, "COLOR_CORRECT_MODE0", "COLOR_CORRECT_MODE1", "COLOR_CORRECT_MODE2" };
	for (int i = 0; i < 16; i++)
	{
		std::vector<std::string> defines;
		if (transferFunctions[i & 1]) defines.push_back(transferFunctions[i & 1]);
		if (gammaModes[(i >> 1) & 1]) defines.push_back(gammaModes[(i >> 1) & 1]);
		if (colorModes[(i >> 2) & 3]) defines.push_back(colorModes[(i >> 2) & 3]);

		PresentPass.Present[i] = CreateFragmentShader("PresentPass.Present", "shaders/Present.frag", defines);
		PresentPass.PresentProgram[i] = CreateProgram("PresentPass.PresentProgram", PresentPass.PPStep, PresentPass.Present[i]);
	}

	// To do: vulkan device didn't have this. Figure out why (maybe we don't need it for OpenGL? can we do like the vulkan device does?
	//PresentPass.HitResolve = CreateFragmentShader("PresentPass.HitResolve", "shaders/HitResolve.frag");
	//PresentPass.HitResolveProgram = CreateProgram("PresentPass.HitResolveProgram", PresentPass.PPStep, PresentPass.HitResolve);

	static const float ditherdata[64] =
	{
		.0078125, .2578125, .1328125, .3828125, .0234375, .2734375, .1484375, .3984375,
		.7578125, .5078125, .8828125, .6328125, .7734375, .5234375, .8984375, .6484375,
		.0703125, .3203125, .1953125, .4453125, .0859375, .3359375, .2109375, .4609375,
		.8203125, .5703125, .9453125, .6953125, .8359375, .5859375, .9609375, .7109375,
		.0390625, .2890625, .1640625, .4140625, .0546875, .3046875, .1796875, .4296875,
		.7890625, .5390625, .9140625, .6640625, .8046875, .5546875, .9296875, .6796875,
		.1015625, .3515625, .2265625, .4765625, .1171875, .3671875, .2421875, .4921875,
		.8515625, .6015625, .9765625, .7265625, .8671875, .6171875, .9921875, .7421875
	};

	PresentPass.DitherTexture = std::make_shared<GLTexture>();
	glBindTexture(GL_TEXTURE_2D, PresentPass.DitherTexture->Handle);
	SetDebugName(PresentPass.DitherTexture, "PresentPass.DitherTexture");
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 8, 8, 0, GL_RED, GL_FLOAT, ditherdata);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	ThrowIfGLError("CreateTexture2D(DitherTexture) failed");

	GLBlendDesc blendDesc = {};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = true;
	PresentPass.BlendState = CreateBlendState(blendDesc);

	GLDepthStencilDesc depthStencilDesc = {};
	depthStencilDesc.DepthFunc = GL_ALWAYS;
	PresentPass.DepthStencilState = CreateDepthStencilState(depthStencilDesc);

	GLRasterizerDesc rasterizerDesc = {};
	rasterizerDesc.CullEnable = false;
	PresentPass.RasterizerState = CreateRasterizerState(rasterizerDesc);
}

void GLRenderDevice::Flush(bool AllowPrecache)
{
	DrawBatches();
	ClearTextureCache();

	if (AllowPrecache && UsePrecache)
		PrecacheOnFlip = 1;
}

void GLRenderDevice::MapVertices(bool nextBuffer)
{
	if (StagingVertices.empty())
	{
		StagingVertices.resize(SceneVertexBufferSize);
		StagingIndexes.resize(SceneIndexBufferSize);
	}

	if (nextBuffer)
	{
		// Orphan both stores: the previous frame's draws keep their copy
		// alive while the new frame writes from the start of fresh ones.
		glBindBuffer(GL_ARRAY_BUFFER, ScenePass.VertexBuffer->Handle);
		glBufferData(GL_ARRAY_BUFFER, SceneVertexBufferSize * sizeof(GLSceneVertex), nullptr, GL_STREAM_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ScenePass.IndexBuffer->Handle);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, SceneIndexBufferSize * sizeof(uint32_t), nullptr, GL_STREAM_DRAW);
		GLSceneVertexPos = 0;
		SceneIndexPos = 0;
		UploadedVertexPos = 0;
		UploadedIndexPos = 0;
		Stats.BuffersUsed++;
	}

	SceneVertices = StagingVertices.data();
	SceneIndexes = StagingIndexes.data();
}

void GLRenderDevice::UnmapVertices()
{
	// The staging arrays replaced the mapped buffers; the upload of what was
	// written happens per flush in DrawBatches.
}

void GLRenderDevice::Lock(vec4 InFlashScale, vec4 InFlashFog, vec4 ScreenClear, uint8_t* InHitData, int* InHitSize)
{
	if (Viewport->GetNativePixelWidth() != CurrentSizeX || Viewport->GetNativePixelHeight() != CurrentSizeY)
	{
		if (!SetRes(Viewport->GetNativePixelWidth(), Viewport->GetNativePixelHeight(), CurrentFullscreen))
		{
			Exception::Throw("Could not resize GLRenderDevice");
		}
	}

	nulltex = Textures->GetNullTexture();

	int wantedBufferCount = UseVSync ? 2 : 3;
	if (BufferCount != wantedBufferCount)
	{
		BufferCount = wantedBufferCount;
	}

	if (CurrentSizeX && CurrentSizeY)
	{
		try
		{
			// The scene buffers live at the render size (the window's times
			// Performance.RenderScale); the present pass scales them up.
			ResizeSceneBuffers(GetRenderWidth(), GetRenderHeight(), GetSettingsMultisample());
		}
		catch (const std::exception& e)
		{
			LogMessage(std::string("Could not resize scene buffers: ") + e.what());
			return;
		}
	}

	HitData = InHitData;
	HitSize = InHitSize;

	FlashScale = InFlashScale;
	FlashFog = InFlashFog;

	float color[4] = { ScreenClear.x, ScreenClear.y, ScreenClear.z, ScreenClear.w };
	float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	glBindFramebuffer(GL_FRAMEBUFFER, SceneBuffers.Framebuffer->Handle);
	GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, bufs);

	GLViewport viewport = {};
	viewport.Width = (float)SceneBuffers.Width;
	viewport.Height = (float)SceneBuffers.Height;
	viewport.MaxDepth = 1.0f;
	SetViewport(viewport);

	glDisable(GL_BLEND);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);

	glClearColor(color[0], color[1], color[2], color[3]);
	glClearDepthf(1.0f); // ES has only the f variant; desktop 4.2+ core
	//glClearStencil(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT /* | GL_STENCIL_BUFFER_BIT*/);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ScenePass.IndexBuffer->Handle);
	glBindVertexArray(ScenePass.InputLayout->Handle);

	GLBuffer* cbs[1] = { ScenePass.ConstantBuffer.get() };
	SetUniformBuffers(0, 1, cbs);
	SetRasterizerState(ScenePass.RasterizerState[SceneBuffers.Multisample > 1].get());

	MapVertices(true);

	SceneConstants.HitIndex = 0;
	ForceHitIndex = -1;

	IsLocked = true;

	ThrowIfGLError("Lock failed");
}

GLPresentPushConstants GLRenderDevice::GetGLPresentPushConstants()
{
	GLPresentPushConstants pushconstants;
	pushconstants.HdrScale = 0.8f + HdrScale * (3.0f / 255.0f);
	if (IsOrtho)
	{
		pushconstants.GammaCorrection = { 1.0f };
		pushconstants.Contrast = 1.0f;
		pushconstants.Saturation = 1.0f;
		pushconstants.Brightness = 0.0f;
	}
	else
	{
		float brightness = clamp(Brightness * GammaScale, 0.05f, 2.99f);

		if (GammaMode == 0)
		{
			float invGammaRed = 1.0f / std::max(brightness + GammaOffset + GammaOffsetRed, 0.001f);
			float invGammaGreen = 1.0f / std::max(brightness + GammaOffset + GammaOffsetGreen, 0.001f);
			float invGammaBlue = 1.0f / std::max(brightness + GammaOffset + GammaOffsetBlue, 0.001f);
			pushconstants.GammaCorrection = vec4(invGammaRed, invGammaGreen, invGammaBlue, 0.0f);
		}
		else
		{
			float invGammaRed = (GammaOffset + GammaOffsetRed + 2.0f) > 0.0f ? 1.0f / (GammaOffset + GammaOffsetRed + 1.0f) : 1.0f;
			float invGammaGreen = (GammaOffset + GammaOffsetGreen + 2.0f) > 0.0f ? 1.0f / (GammaOffset + GammaOffsetGreen + 1.0f) : 1.0f;
			float invGammaBlue = (GammaOffset + GammaOffsetBlue + 2.0f) > 0.0f ? 1.0f / (GammaOffset + GammaOffsetBlue + 1.0f) : 1.0f;
			pushconstants.GammaCorrection = vec4(invGammaRed, invGammaGreen, invGammaBlue, brightness);
		}

		// pushconstants.Contrast = clamp(Contrast, 0.1f, 3.f);
		if (Contrast >= 128)
		{
			pushconstants.Contrast = 1.0f + (Contrast - 128) / 127.0f * 3.0f;
		}
		else
		{
			pushconstants.Contrast = std::max(Contrast / 128.0f, 0.1f);
		}

		// pushconstants.Saturation = clamp(Saturation, -1.0f, 1.0f);
		pushconstants.Saturation = 1.0f - 2.0f * (255 - Saturation) / 255.0f;

		// pushconstants.Brightness = clamp(LinearBrightness, -1.8f, 1.8f);
		if (LinearBrightness >= 128)
		{
			pushconstants.Brightness = (LinearBrightness - 128) / 127.0f * 1.8f;
		}
		else
		{
			pushconstants.Brightness = (128 - LinearBrightness) / 128.0f * -1.8f;
		}
	}
	return pushconstants;
}

void GLRenderDevice::Unlock(bool Blit)
{
	if (!IsLocked) // Don't trust the engine.
		return;

	DrawBatches();
	UnmapVertices();

	ThrowIfGLError("Drawing failed");

	Batch.SceneIndexStart = 0;
	GLSceneVertexPos = 0;
	SceneIndexPos = 0;

	if (Blit)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, SceneBuffers.Framebuffer->Handle);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, SceneBuffers.PPFramebuffer[0]->Handle);
		{ GLenum glAtt = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &glAtt); } // ES has only glDrawBuffers
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		// Scene to PP at the render size; the present draw below samples it
		// over the whole window and so scales it up.
		glBlitFramebuffer(0, 0, SceneBuffers.Width, SceneBuffers.Height, 0, 0, SceneBuffers.Width, SceneBuffers.Height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		if (Bloom)
		{
			RunBloomPass();
		}

		GLPresentPushConstants pushconstants = GetGLPresentPushConstants();

		// Select present shader based on what the user is actually using
		int presentShader = 0;
		if (ActiveHdr) presentShader |= 1;
		if (GammaMode == 1) presentShader |= 2;
		if (pushconstants.Brightness != 0.0f || pushconstants.Contrast != 1.0f || pushconstants.Saturation != 1.0f) presentShader |= (clamp(GrayFormula, 0, 2) + 1) << 2;

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		{ GLenum glBack = GL_BACK; glDrawBuffers(1, &glBack); } // ES has only glDrawBuffers
		glReadBuffer(GL_BACK);
		glUseProgram(PresentPass.PresentProgram[presentShader]->Handle);

		GLViewport viewport = {};
		viewport.Width = (float)CurrentSizeX;
		viewport.Height = (float)CurrentSizeY;
		viewport.MaxDepth = 1.0f;
		SetViewport(viewport);

		glBindVertexArray(PresentPass.PPStepLayout->Handle);

		SetBufferData(GL_UNIFORM_BUFFER, GL_STREAM_DRAW, PresentPass.PresentConstantBuffer.get(), &pushconstants, sizeof(GLPresentPushConstants));

		SetRasterizerState(PresentPass.RasterizerState.get());
		GLBuffer* cbs[1] = { PresentPass.PresentConstantBuffer.get() };
		SetUniformBuffers(0, 1, cbs);
		GLTexture2D* psResources[] = { SceneBuffers.PPImage[0].get(), PresentPass.DitherTexture.get() };
		SetTextures(0, 2, psResources);
		SetDepthStencilState(PresentPass.DepthStencilState.get());
		SetBlendState(PresentPass.BlendState.get());
		glDrawArrays(GL_TRIANGLES, 0, 6);

		if (UseVSync)
		{
			// Try use adaptive vsync if available
			if (!Viewport->SetGLSwapInterval(-1))
				Viewport->SetGLSwapInterval(1);
		}
		else
		{
			Viewport->SetGLSwapInterval(0);
		}

		Viewport->SwapGLBuffers();

		Batch.Pipeline = nullptr;
		Batch.Tex = nullptr;
		Batch.Lightmap = nullptr;
		Batch.Detailtex = nullptr;
		Batch.Macrotex = nullptr;
		Batch.SceneIndexStart = 0;

		UpdateLODBias();
	}

	if (HitData)
	{
		// Resolve multisampling
		if (SceneBuffers.Multisample > 1)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, SceneBuffers.PPHitFramebuffer->Handle);
			{ GLenum glAtt = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &glAtt); } // ES has only glDrawBuffers
			glReadBuffer(GL_COLOR_ATTACHMENT0);
			glUseProgram(PresentPass.HitResolveProgram->Handle);

			GLViewport viewport = {};
			viewport.X = (float)HitX;
			viewport.Y = (float)HitY;
			viewport.Width = (float)HitWidth;
			viewport.Height = (float)HitHeight;
			viewport.MaxDepth = 1.0f;
			SetViewport(viewport);

			glBindVertexArray(PresentPass.PPStepLayout->Handle);
			SetRasterizerState(PresentPass.RasterizerState.get());
			GLTexture2D* srvs[1] = { SceneBuffers.HitBuffer.get() };
			SetTextures(0, 1, srvs);
			SetDepthStencilState(PresentPass.DepthStencilState.get());
			SetBlendState(PresentPass.BlendState.get());

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}
		else
		{
			glBindFramebuffer(GL_READ_FRAMEBUFFER, SceneBuffers.PPHitFramebuffer->Handle);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, SceneBuffers.HitFramebuffer->Handle);
			{ GLenum glAtt = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &glAtt); } // ES has only glDrawBuffers
			glReadBuffer(GL_COLOR_ATTACHMENT0);
			glBlitFramebuffer(HitX, HitY, HitX + HitWidth, HitY + HitHeight, HitX, HitY, HitX + HitWidth, HitY + HitHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}

#if 0
		// Copy the hit buffer to a mappable texture, but only the part we want to examine
		Context->CopySubresourceRegion(SceneBuffers.StagingHitBuffer, 0, 0, 0, 0, SceneBuffers.PPHitBuffer, 0, &box);

		// Lock the buffer and look for the last hit
		int hit = 0;
		GL_MAPPED_SUBRESOURCE mapping = {};
		HRESULT result = Context->Map(SceneBuffers.StagingHitBuffer, 0, GL_MAP_READ, 0, &mapping);
		if (SUCCEEDED(result))
		{
			int width = HitWidth;
			int height = HitHeight;
			for (int y = 0; y < height; y++)
			{
				const int* line = (const int*)(((const char*)mapping.pData) + y * mapping.RowPitch);
				for (int x = 0; x < width; x++)
				{
					hit = std::max(hit, line[x]);
				}
			}
			Context->Unmap(SceneBuffers.StagingHitBuffer, 0);
		}
		hit--;

		hit = std::max(hit, ForceHitIndex);

		if (hit >= 0 && hit < (int)HitQueries.size())
		{
			const HitQuery& query = HitQueries[hit];
			memcpy(HitData, HitBuffer.data() + query.Start, query.Count);
			*HitSize = query.Count;
		}
		else
		{
			*HitSize = 0;
		}
#endif
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	{ GLenum glBack = GL_BACK; glDrawBuffers(1, &glBack); } // ES has only glDrawBuffers
	glReadBuffer(GL_BACK);

	HitQueryStack.clear();
	HitQueries.clear();
	HitBuffer.clear();
	HitData = nullptr;
	HitSize = nullptr;

	IsLocked = false;

	ThrowIfGLError("Unlock failed");
}

void GLRenderDevice::PushHit(const uint8_t* Data, int Count)
{
	if (Count <= 0) return;
	HitQueryStack.insert(HitQueryStack.end(), Data, Data + Count);

	SetHitLocation();
}

void GLRenderDevice::PopHit(int Count, bool bForce)
{
	if (bForce) // Force hit what we are popping
		ForceHitIndex = (int)(HitQueries.size() - 1);

	HitQueryStack.resize(HitQueryStack.size() - Count);

	SetHitLocation();
}

void GLRenderDevice::SetHitLocation()
{
	DrawBatches();

	if (!HitQueryStack.empty())
	{
		int index = (int)HitQueries.size();

		HitQuery query;
		query.Start = (int)HitBuffer.size();
		query.Count = (int)HitQueryStack.size();
		HitQueries.push_back(query);

		HitBuffer.insert(HitBuffer.end(), HitQueryStack.begin(), HitQueryStack.end());

		SceneConstants.HitIndex = index + 1;
	}
	else
	{
		SceneConstants.HitIndex = 0;
	}

	SetBufferData(GL_UNIFORM_BUFFER, GL_STREAM_DRAW, ScenePass.ConstantBuffer.get(), &SceneConstants, sizeof(GLScenePushConstants));
}

void GLRenderDevice::DrawComplexSurface(SceneNode* Frame, SurfaceInfo& Surface, SurfaceFacet& Facet)
{
	uint32_t PolyFlags = ApplyPrecedenceRules(Surface.PolyFlags);

	ComplexSurfaceInfo info;
	info.facet = &Facet;
	info.tex = Textures->GetTexture(Surface.Texture, !!(PolyFlags & PF_Masked));
	info.lightmap = Textures->GetTexture(Surface.LightMap, false);
	info.macrotex = Textures->GetTexture(Surface.MacroTexture, false);
	info.detailtex = Textures->GetTexture(Surface.DetailTexture, false);
	info.fogmap = (Surface.FogMap && Surface.FogMap->NumMips > 0 && !Surface.FogMap->Mips[0].Data.empty()) ?
		Textures->GetTexture(Surface.FogMap, false) : nulltex;
	info.editorcolor = nullptr;

	if (Surface.DetailTexture && Surface.FogMap) info.detailtex = nulltex;

	if (info.fogmap != nulltex)
		info.detailtex = info.fogmap;

	SetPipeline(PolyFlags);
	SetDescriptorSet(PolyFlags, info);

	DrawComplexSurfaceFaces(info);

	Stats.ComplexSurfaces++;
}

void GLRenderDevice::DrawComplexSurfaceFaces(const ComplexSurfaceInfo& info)
{
	if (info.facet->VertexCount < 3)
		return;

	uint32_t flags = 0;
	if (info.lightmap != nulltex) flags |= 1;
	if (info.macrotex != nulltex) flags |= 2;
	if (info.detailtex != nulltex && info.fogmap == nulltex) flags |= 4;
	if (info.fogmap != nulltex) flags |= 8;
	if (LightMode == 1) flags |= 64;

	vec3 xaxis = info.facet->MapCoords.XAxis;
	vec3 yaxis = info.facet->MapCoords.YAxis;
	float UDot = dot(xaxis, info.facet->MapCoords.Origin);
	float VDot = dot(yaxis, info.facet->MapCoords.Origin);

	float UPan = UDot + info.tex->PanX;
	float VPan = VDot + info.tex->PanY;
	float LMUPan = UDot + info.lightmap->PanX - 0.5f * info.lightmap->UScale;
	float LMVPan = VDot + info.lightmap->PanY - 0.5f * info.lightmap->VScale;
	float MacroUPan = UDot + info.macrotex->PanX;
	float MacroVPan = VDot + info.macrotex->PanY;
	float DetailUPan = UDot + (info.fogmap == nulltex ? info.detailtex->PanX : info.fogmap->PanX - 0.5f * info.fogmap->UScale);
	float DetailVPan = VDot + (info.fogmap == nulltex ? info.detailtex->PanY : info.fogmap->PanY - 0.5f * info.fogmap->VScale);

	float UMult = info.tex->UMult;
	float VMult = info.tex->VMult;
	float LMUMult = info.lightmap->UMult;
	float LMVMult = info.lightmap->VMult;
	float MacroUMult = info.macrotex->UMult;
	float MacroVMult = info.macrotex->VMult;
	float DetailUMult = info.fogmap == nulltex ? info.detailtex->UMult : info.fogmap->UMult;
	float DetailVMult = info.fogmap == nulltex ? info.detailtex->VMult : info.fogmap->VMult;

	vec4 color = info.editorcolor ? *info.editorcolor : vec4(1.0f);

	auto pts = info.facet->Vertices;
	uint32_t vcount = info.facet->VertexCount;

	uint32_t icount = (vcount - 2) * 3;
	auto alloc = ReserveVertices(vcount, icount);
	if (alloc.vptr)
	{
		GLSceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

		for (uint32_t i = 0; i < vcount; i++)
		{
			vec3 point = pts[i];
			float u = dot(xaxis, point);
			float v = dot(yaxis, point);

			vptr->Flags = flags;
			vptr->Position.x = point.x;
			vptr->Position.y = point.y;
			vptr->Position.z = point.z;
			vptr->TexCoord.s = (u - UPan) * UMult;
			vptr->TexCoord.t = (v - VPan) * VMult;
			vptr->TexCoord2.s = (u - LMUPan) * LMUMult;
			vptr->TexCoord2.t = (v - LMVPan) * LMVMult;
			vptr->TexCoord3.s = (u - MacroUPan) * MacroUMult;
			vptr->TexCoord3.t = (v - MacroVPan) * MacroVMult;
			vptr->TexCoord4.s = (u - DetailUPan) * DetailUMult;
			vptr->TexCoord4.t = (v - DetailVPan) * DetailVMult;
			vptr->Color = color;
			vptr++;
		}

		for (uint32_t i = vpos + 2; i < vpos + vcount; i++)
		{
			*(iptr++) = vpos;
			*(iptr++) = i - 1;
			*(iptr++) = i;
		}

		UseVertices(vcount, icount);
	}
}

void GLRenderDevice::DrawGouraudPolygon(SceneNode* Frame, TextureInfo& Info, const GouraudVertex* Pts, int NumPts, uint32_t PolyFlags)
{
	if (NumPts < 3) return; // This can apparently happen!!

	PolyFlags = ApplyPrecedenceRules(PolyFlags);

	GLCachedTexture* tex = Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));

	SetPipeline(PolyFlags);
	SetDescriptorSet(PolyFlags, tex);

	float UMult = tex->UMult;
	float VMult = tex->VMult;

	int flags = (PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog ? 16 : 0;
	if ((PolyFlags & (PF_Translucent | PF_Modulated)) == 0 && LightMode == 2) flags |= 32;

	auto alloc = ReserveVertices(NumPts, (NumPts - 2) * 3);
	if (alloc.vptr)
	{
		GLSceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

		if (PolyFlags & PF_Modulated)
		{
			GLSceneVertex* vertex = vptr;

			for (int i = 0; i < NumPts; i++)
			{
				const GouraudVertex* P = Pts + i;
				vertex->Flags = flags;
				vertex->Position.x = P->Point.x;
				vertex->Position.y = P->Point.y;
				vertex->Position.z = P->Point.z;
				vertex->TexCoord.s = P->UV.x * UMult;
				vertex->TexCoord.t = P->UV.y * VMult;
				vertex->TexCoord2.s = P->Fog.x;
				vertex->TexCoord2.t = P->Fog.y;
				vertex->TexCoord3.s = P->Fog.z;
				vertex->TexCoord3.t = P->Fog.w;
				vertex->TexCoord4.s = 0.0f;
				vertex->TexCoord4.t = 0.0f;
				vertex->Color.r = 1.0f;
				vertex->Color.g = 1.0f;
				vertex->Color.b = 1.0f;
				vertex->Color.a = 1.0f;
				vertex++;
			}
		}
		else
		{
			GLSceneVertex* vertex = vptr;
			for (int i = 0; i < NumPts; i++)
			{
				const GouraudVertex* P = Pts + i;
				vertex->Flags = flags;
				vertex->Position.x = P->Point.x;
				vertex->Position.y = P->Point.y;
				vertex->Position.z = P->Point.z;
				vertex->TexCoord.s = P->UV.x * UMult;
				vertex->TexCoord.t = P->UV.y * VMult;
				vertex->TexCoord2.s = P->Fog.x;
				vertex->TexCoord2.t = P->Fog.y;
				vertex->TexCoord3.s = P->Fog.z;
				vertex->TexCoord3.t = P->Fog.w;
				vertex->TexCoord4.s = 0.0f;
				vertex->TexCoord4.t = 0.0f;
				vertex->Color.r = P->Light.x;
				vertex->Color.g = P->Light.y;
				vertex->Color.b = P->Light.z;
				vertex->Color.a = 1.0f;
				vertex++;
			}
		}

		uint32_t vstart = vpos;
		uint32_t vcount = NumPts;
		for (uint32_t i = vstart + 2; i < vstart + vcount; i++)
		{
			*(iptr++) = vstart;
			*(iptr++) = i - 1;
			*(iptr++) = i;
		}

		UseVertices(NumPts, (NumPts - 2) * 3);
	}

	Stats.GouraudPolygons++;
}

void GLRenderDevice::DrawTile(SceneNode* Frame, TextureInfo& Info, float X, float Y, float XL, float YL, float U, float V, float UL, float VL, float Z, vec4 Color, vec4 Fog, uint32_t PolyFlags)
{
	PolyFlags = ApplyPrecedenceRules(PolyFlags);

	if (PolyFlags == PF_SubpixelFont)
	{
		AddDrawBatch();
		Batch.BlendConstants[0] = Color.x;
		Batch.BlendConstants[1] = Color.y;
		Batch.BlendConstants[2] = Color.z;
		Batch.BlendConstants[3] = Color.w;
		Color = vec4(1.0f);
	}

	GLCachedTexture* tex = Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));
	float UMult = tex->UMult;
	float VMult = tex->VMult;
	float u0 = U * UMult;
	float v0 = V * VMult;
	float u1 = (U + UL) * UMult;
	float v1 = (V + VL) * VMult;
	bool clamp = (u0 >= 0.0f && u1 <= 1.00001f && v0 >= 0.0f && v1 <= 1.00001f);

	SetPipeline(PolyFlags);
	SetDescriptorSet(PolyFlags, tex, clamp);

	if (SceneBuffers.Multisample > 1)
	{
		XL = std::floor(X + XL + 0.5f);
		YL = std::floor(Y + YL + 0.5f);
		X = std::floor(X + 0.5f);
		Y = std::floor(Y + 0.5f);
		XL = XL - X;
		YL = YL - Y;
	}

	auto alloc = ReserveVertices(4, 6);
	if (alloc.vptr)
	{
		GLSceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

		float r, g, b, a;
		if (PolyFlags & PF_Modulated)
		{
			r = 1.0f;
			g = 1.0f;
			b = 1.0f;
		}
		else
		{
			r = Color.x;
			g = Color.y;
			b = Color.z;
		}
		a = 1.0f;

		float rfx2z = RFX2 * Z;
		float rfy2z = RFY2 * Z;
		X -= Frame->FX2;
		Y -= Frame->FY2;
		XL += X;
		YL += Y;
		U *= UMult;
		UL = U + UL * UMult;
		V *= VMult;
		VL = V + VL * VMult;

		vptr[0].Flags = 0;
		vptr[0].Position.x = rfx2z * X;
		vptr[0].Position.y = rfy2z * Y;
		vptr[0].Position.z = Z;
		vptr[0].TexCoord.s = U;
		vptr[0].TexCoord.t = V;
		vptr[0].TexCoord2.s = 0.0f;
		vptr[0].TexCoord2.t = 0.0f;
		vptr[0].TexCoord3.s = 0.0f;
		vptr[0].TexCoord3.t = 0.0f;
		vptr[0].TexCoord4.s = 0.0f;
		vptr[0].TexCoord4.t = 0.0f;
		vptr[0].Color.r = r;
		vptr[0].Color.g = g;
		vptr[0].Color.b = b;
		vptr[0].Color.a = a;

		vptr[1].Flags = 0;
		vptr[1].Position.x = rfx2z * XL;
		vptr[1].Position.y = rfy2z * Y;
		vptr[1].Position.z = Z;
		vptr[1].TexCoord.s = UL;
		vptr[1].TexCoord.t = V;
		vptr[1].TexCoord2.s = 0.0f;
		vptr[1].TexCoord2.t = 0.0f;
		vptr[1].TexCoord3.s = 0.0f;
		vptr[1].TexCoord3.t = 0.0f;
		vptr[1].TexCoord4.s = 0.0f;
		vptr[1].TexCoord4.t = 0.0f;
		vptr[1].Color.r = r;
		vptr[1].Color.g = g;
		vptr[1].Color.b = b;
		vptr[1].Color.a = a;

		vptr[2].Flags = 0;
		vptr[2].Position.x = rfx2z * XL;
		vptr[2].Position.y = rfy2z * YL;
		vptr[2].Position.z = Z;
		vptr[2].TexCoord.s = UL;
		vptr[2].TexCoord.t = VL;
		vptr[2].TexCoord2.s = 0.0f;
		vptr[2].TexCoord2.t = 0.0f;
		vptr[2].TexCoord3.s = 0.0f;
		vptr[2].TexCoord3.t = 0.0f;
		vptr[2].TexCoord4.s = 0.0f;
		vptr[2].TexCoord4.t = 0.0f;
		vptr[2].Color.r = r;
		vptr[2].Color.g = g;
		vptr[2].Color.b = b;
		vptr[2].Color.a = a;

		vptr[3].Flags = 0;
		vptr[3].Position.x = rfx2z * X;
		vptr[3].Position.y = rfy2z * YL;
		vptr[3].Position.z = Z;
		vptr[3].TexCoord.s = U;
		vptr[3].TexCoord.t = VL;
		vptr[3].TexCoord2.s = 0.0f;
		vptr[3].TexCoord2.t = 0.0f;
		vptr[3].TexCoord3.s = 0.0f;
		vptr[3].TexCoord3.t = 0.0f;
		vptr[3].TexCoord4.s = 0.0f;
		vptr[3].TexCoord4.t = 0.0f;
		vptr[3].Color.r = r;
		vptr[3].Color.g = g;
		vptr[3].Color.b = b;
		vptr[3].Color.a = a;

		iptr[0] = vpos;
		iptr[1] = vpos + 1;
		iptr[2] = vpos + 2;
		iptr[3] = vpos;
		iptr[4] = vpos + 2;
		iptr[5] = vpos + 3;

		UseVertices(4, 6);
	}

	Stats.Tiles++;
}

vec4 GLRenderDevice::ApplyInverseGamma(vec4 color)
{
	if (IsOrtho)
		return color;
	float brightness = clamp(Brightness * GammaScale, 0.05f, 2.99f);
	float gammaRed = std::max(brightness + GammaOffset + GammaOffsetRed, 0.001f);
	float gammaGreen = std::max(brightness + GammaOffset + GammaOffsetGreen, 0.001f);
	float gammaBlue = std::max(brightness + GammaOffset + GammaOffsetBlue, 0.001f);
	return vec4(pow(color.r, gammaRed), pow(color.g, gammaGreen), pow(color.b, gammaBlue), color.a);
}

void GLRenderDevice::Draw3DLine(SceneNode* Frame, vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2)
{
	// P1 = P1.TransformPointBy(Frame->Coords);
	// P2 = P2.TransformPointBy(Frame->Coords);
	if (IsOrtho)
	{
		P1.x = (P1.x) / Frame->Zoom + Frame->FX2;
		P1.y = (P1.y) / Frame->Zoom + Frame->FY2;
		P1.z = 1;
		P2.x = (P2.x) / Frame->Zoom + Frame->FX2;
		P2.y = (P2.y) / Frame->Zoom + Frame->FY2;
		P2.z = 1;

		if (std::abs(P2.x - P1.x) + std::abs(P2.y - P1.y) >= 0.2)
		{
			Draw2DLine(Frame, Color, LineFlags, P1, P2);
		}
		else if (IsOrthoLowDetail)
		{
			Draw2DPoint(Frame, Color, LINE_None, P1.x - 1, P1.y - 1, P1.x + 1, P1.y + 1, P1.z);
		}
	}
	else
	{
		bool occlude = !!(LineFlags & LINE_DepthCued);
		SetPipeline(&ScenePass.LinePipeline[occlude]);
		SetDescriptorSet(PF_Highlighted);
		vec4 color = ApplyInverseGamma(vec4(Color.x, Color.y, Color.z, 1.0f));

		auto alloc = ReserveVertices(2, 2);
		if (alloc.vptr)
		{
			GLSceneVertex* vptr = alloc.vptr;
			uint32_t* iptr = alloc.iptr;
			uint32_t vpos = alloc.vpos;

			vptr[0] = { 0, vec3(P1.x, P1.y, P1.z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color };
			vptr[1] = { 0, vec3(P2.x, P2.y, P2.z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color };

			iptr[0] = vpos;
			iptr[1] = vpos + 1;

			UseVertices(2, 2);
		}
	}
}

void GLRenderDevice::Draw2DLine(SceneNode* Frame, vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2)
{
	bool occlude = !!(LineFlags & LINE_DepthCued);
	SetPipeline(&ScenePass.LinePipeline[occlude]);
	SetDescriptorSet(PF_Highlighted);
	vec4 color = ApplyInverseGamma(vec4(Color.x, Color.y, Color.z, 1.0f));

	auto alloc = ReserveVertices(2, 2);
	if (alloc.vptr)
	{
		GLSceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

		vptr[0] = { 0, vec3(RFX2 * P1.z * (P1.x - Frame->FX2), RFY2 * P1.z * (P1.y - Frame->FY2), P1.z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color };
		vptr[1] = { 0, vec3(RFX2 * P2.z * (P2.x - Frame->FX2), RFY2 * P2.z * (P2.y - Frame->FY2), P2.z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color };

		iptr[0] = vpos;
		iptr[1] = vpos + 1;

		UseVertices(2, 2);
	}
}

void GLRenderDevice::Draw2DPoint(SceneNode* Frame, vec4 Color, uint32_t LineFlags, float X1, float Y1, float X2, float Y2, float Z)
{
	bool occlude = !!(LineFlags & LINE_DepthCued);
	SetPipeline(&ScenePass.PointPipeline[occlude]);
	SetDescriptorSet(PF_Highlighted);
	vec4 color = ApplyInverseGamma(vec4(Color.x, Color.y, Color.z, 1.0f));

	auto alloc = ReserveVertices(4, 6);
	if (alloc.vptr)
	{
		GLSceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

		vptr[0] = { 0, vec3(RFX2 * Z * (X1 - Frame->FX2 - 0.5f), RFY2 * Z * (Y1 - Frame->FY2 - 0.5f), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color };
		vptr[1] = { 0, vec3(RFX2 * Z * (X2 - Frame->FX2 + 0.5f), RFY2 * Z * (Y1 - Frame->FY2 - 0.5f), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color };
		vptr[2] = { 0, vec3(RFX2 * Z * (X2 - Frame->FX2 + 0.5f), RFY2 * Z * (Y2 - Frame->FY2 + 0.5f), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color };
		vptr[3] = { 0, vec3(RFX2 * Z * (X1 - Frame->FX2 - 0.5f), RFY2 * Z * (Y2 - Frame->FY2 + 0.5f), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color };

		iptr[0] = vpos;
		iptr[1] = vpos + 1;
		iptr[2] = vpos + 2;
		iptr[3] = vpos;
		iptr[4] = vpos + 2;
		iptr[5] = vpos + 3;

		UseVertices(4, 6);
	}
}

void GLRenderDevice::ClearZ()
{
	DrawBatches();

	glDepthMask(GL_TRUE);
	glClearDepthf(1.0f); // ES has only the f variant
	glClear(GL_DEPTH_BUFFER_BIT);
}

void GLRenderDevice::ReadPixels(TextureColor* Pixels)
{
	UnmapVertices();

	if (GammaCorrectScreenshots)
	{
		GLPresentPushConstants pushconstants = GetGLPresentPushConstants();

		// Select present shader based on what the user is actually using
		int presentShader = 0;
		if (ActiveHdr) presentShader |= 1;
		if (GammaMode == 1) presentShader |= 2;
		if (pushconstants.Brightness != 0.0f || pushconstants.Contrast != 1.0f || pushconstants.Saturation != 1.0f) presentShader |= (clamp(GrayFormula, 0, 2) + 1) << 2;

		glBindFramebuffer(GL_FRAMEBUFFER, SceneBuffers.PPFramebuffer[1]->Handle);
		{ GLenum glAtt = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &glAtt); } // ES has only glDrawBuffers
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glUseProgram(PresentPass.PresentProgram[presentShader]->Handle);

		GLViewport viewport = {};
		viewport.Width = (float)SceneBuffers.Width;
		viewport.Height = (float)SceneBuffers.Height;
		viewport.MaxDepth = 1.0f;
		SetViewport(viewport);

		glBindVertexArray(PresentPass.PPStepLayout->Handle);

		SetBufferData(GL_UNIFORM_BUFFER, GL_STREAM_DRAW, PresentPass.PresentConstantBuffer.get(), &pushconstants, sizeof(GLPresentPushConstants));

		GLBuffer* cbs[1] = { PresentPass.PresentConstantBuffer.get() };
		GLTexture2D* psResources[] = { SceneBuffers.PPImage[0].get(), PresentPass.DitherTexture.get() };
		SetRasterizerState(PresentPass.RasterizerState.get());
		SetUniformBuffers(0, 1, cbs);
		SetTextures(0, 2, psResources);
		SetDepthStencilState(PresentPass.DepthStencilState.get());
		SetBlendState(PresentPass.BlendState.get());
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	// Read the frame back and hand the engine blue, green, red, alpha, top
	// row first -- the same bytes the Vulkan device's BGRA8 readback gives.
	// The frame the player saw is PPImage[0]; with GammaCorrectScreenshots the
	// present pass above has drawn it into PPImage[1]. The GL scene buffers
	// store the frame top-row-first in their rows (the engine's Y-down
	// convention flows through the device's viewport and tile math), so the
	// direct read of PPImage[0] needs no flip -- verified against the Vulkan
	// device's shots. PPImage[1] is the present pass's output, which samples
	// with 1.0 - texCoord.y: it IS mirrored, and its read flips back.
	GLFramebuffer* fb = GammaCorrectScreenshots ? SceneBuffers.PPFramebuffer[1].get() : SceneBuffers.PPFramebuffer[0].get();
	glBindFramebuffer(GL_FRAMEBUFFER, fb->Handle);
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	int w = SceneBuffers.Width;
	int h = SceneBuffers.Height;
	if (ActiveHdr)
	{
		std::vector<float> col((size_t)w * h * 4);
		glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, col.data());
		ThrowIfGLError("ReadPixels failed");

		for (int y = 0; y < h; y++)
		{
			const float* src = &col[(size_t)(GammaCorrectScreenshots ? (h - 1 - y) : y) * w * 4];
			TextureColor* dest = Pixels + (size_t)y * w;
			for (int x = 0; x < w; x++)
			{
				dest[x].R = (uint8_t)clamp(std::round(src[x * 4 + 2] * 255.0f), 0.0f, 255.0f);
				dest[x].G = (uint8_t)clamp(std::round(src[x * 4 + 1] * 255.0f), 0.0f, 255.0f);
				dest[x].B = (uint8_t)clamp(std::round(src[x * 4 + 0] * 255.0f), 0.0f, 255.0f);
				dest[x].A = (uint8_t)clamp(std::round(src[x * 4 + 3] * 255.0f), 0.0f, 255.0f);
			}
		}
	}
	else
	{
		std::vector<uint8_t> col((size_t)w * h * 4);
		glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, col.data());
		ThrowIfGLError("ReadPixels failed");

		for (int y = 0; y < h; y++)
		{
			const uint8_t* src = &col[(size_t)y * w * 4];
			TextureColor* dest = Pixels + (size_t)y * w;
			for (int x = 0; x < w; x++)
			{
				dest[x].R = src[x * 4 + 2];
				dest[x].G = src[x * 4 + 1];
				dest[x].B = src[x * 4 + 0];
				dest[x].A = src[x * 4 + 3];
			}
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (IsLocked)
		MapVertices(false);
}

void GLRenderDevice::EndFlash()
{
	if (FlashScale != vec4(0.5f, 0.5f, 0.5f, 0.0f) || FlashFog != vec4(0.0f, 0.0f, 0.0f, 0.0f))
	{
		DrawBatches();

		vec4 color(FlashFog.x, FlashFog.y, FlashFog.z, 1.0f - std::min(FlashScale.x * 2.0f, 1.0f));
		vec2 zero2(0.0f);

		SceneConstants.ObjectToProjection = mat4::identity();
		SceneConstants.ObjectToView = mat4::identity();
		SceneConstants.NearClip = vec4(0.0f, 0.0f, 0.0f, 0.0f);

		SetBufferData(GL_UNIFORM_BUFFER, GL_STREAM_DRAW, ScenePass.ConstantBuffer.get(), &SceneConstants, sizeof(GLScenePushConstants));

		SetPipeline(PF_Highlighted);
		SetDescriptorSet(0);

		auto alloc = ReserveVertices(4, 6);
		if (alloc.vptr)
		{
			GLSceneVertex* vptr = alloc.vptr;
			uint32_t* iptr = alloc.iptr;
			uint32_t vpos = alloc.vpos;

			vptr[0] = { 0, vec3(-1.0f, -1.0f, 0.0f), zero2, zero2, zero2, zero2, color };
			vptr[1] = { 0, vec3(1.0f, -1.0f, 0.0f), zero2, zero2, zero2, zero2, color };
			vptr[2] = { 0, vec3(1.0f,  1.0f, 0.0f), zero2, zero2, zero2, zero2, color };
			vptr[3] = { 0, vec3(-1.0f,  1.0f, 0.0f), zero2, zero2, zero2, zero2, color };

			iptr[0] = vpos;
			iptr[1] = vpos + 1;
			iptr[2] = vpos + 2;
			iptr[3] = vpos;
			iptr[4] = vpos + 2;
			iptr[5] = vpos + 3;

			UseVertices(4, 6);
		}

		DrawBatches();
		if (CurrentFrame)
			SetSceneNode(CurrentFrame);
	}
}

void GLRenderDevice::SetSceneNode(SceneNode* Frame)
{
	DrawBatches();

	CurrentFrame = Frame;
	Aspect = Frame->FY / Frame->FX;
	RProjZ = (float)std::tan(radians(Frame->FovAngle) * 0.5);
	RFX2 = 2.0f * RProjZ / Frame->FX;
	RFY2 = 2.0f * RProjZ * Aspect / Frame->FY;

	SceneViewport = {};
	SceneViewport.X = (float)Frame->XB;
	SceneViewport.Y = (float)(SceneBuffers.Height - Frame->YB - Frame->Y);
	SceneViewport.Width = (float)Frame->X;
	SceneViewport.Height = (float)Frame->Y;
	SceneViewport.MinDepth = 0.1f;
	SceneViewport.MaxDepth = 1.0f;
	SetViewport(SceneViewport);

	SceneConstants.ObjectToProjection = mat4::frustum(-RProjZ, RProjZ, -Aspect * RProjZ, Aspect * RProjZ, 1.0f, 32768.0f, handedness::left, clipzrange::negative_positive_w);

	// TBD; do this or do like UE1 does and do the transform on the CPU?
	// maybe optionally do one or the other? transform on CPU can be super slow --Xaleros
	SceneConstants.ObjectToProjection = SceneConstants.ObjectToProjection * Frame->WorldToView * Frame->ObjectToWorld;

	SceneConstants.ObjectToView = Frame->WorldToView * Frame->ObjectToWorld;
	SceneConstants.NearClip = Frame->NearClip;

	SetBufferData(GL_UNIFORM_BUFFER, GL_STREAM_DRAW, ScenePass.ConstantBuffer.get(), &SceneConstants, sizeof(GLScenePushConstants));
}

void GLRenderDevice::SetBufferData(GLenum target, GLenum usage, GLBuffer* buffer, const void* data, size_t size)
{
	glBindBuffer(target, buffer->Handle);
	glBufferData(target, size, data, usage);
}

void GLRenderDevice::SetBlendState(GLBlendState* blendState, const float* blendConstants)
{
	// Per-draw-buffer blend (the i variants) is desktop GL 4.0+ and in no ES
	// version. On ES the non-indexed calls apply to every draw buffer: the
	// blend describes the color attachment, and the integer hit attachment
	// ignores blending by the spec itself.
	if (blendState->desc.IndependentBlendEnable && !IsGLES)
	{
		// To do: this might require OpenGL 4.0 - glBlendFuncSeparatei does, not sure about glEnablei(GL_BLEND)
		for (int i = 0; i < 2; i++)
		{
			const GLBlendDesc::Target& target = blendState->desc.RenderTarget[i];
			GLboolean maskValue = target.RenderTargetWriteMask ? GL_TRUE : GL_FALSE;
			glColorMaski(i, maskValue, maskValue, maskValue, maskValue);
			if (target.BlendEnable)
			{
				glEnablei(GL_BLEND, i);
				glBlendEquationSeparatei(i, target.BlendOp, target.BlendOpAlpha);
				glBlendFuncSeparatei(i, target.SrcBlend, target.DestBlend, target.SrcBlendAlpha, target.DestBlendAlpha);
			}
			else
			{
				glDisablei(GL_BLEND, i);
			}
		}
	}
	else
	{
		const GLBlendDesc::Target& target = blendState->desc.RenderTarget[0];
		GLboolean maskValue = target.RenderTargetWriteMask ? GL_TRUE : GL_FALSE;
		glColorMask(maskValue, maskValue, maskValue, maskValue);
		if (target.BlendEnable)
		{
			glEnable(GL_BLEND);
			glBlendEquationSeparate(target.BlendOp, target.BlendOpAlpha);
			glBlendFuncSeparate(target.SrcBlend, target.DestBlend, target.SrcBlendAlpha, target.DestBlendAlpha);
		}
		else
		{
			glDisable(GL_BLEND);
		}
	}

	if (blendConstants)
		glBlendColor(blendConstants[0], blendConstants[1], blendConstants[2], blendConstants[3]);
}

void GLRenderDevice::SetDepthStencilState(GLDepthStencilState* depthStencilState)
{
	if (depthStencilState->desc.DepthEnable)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);

	glDepthFunc(depthStencilState->desc.DepthFunc);
	glDepthMask(depthStencilState->desc.DepthWriteMask ? GL_TRUE : GL_FALSE);
}

void GLRenderDevice::SetRasterizerState(GLRasterizerState* rasterizerState)
{
	if (rasterizerState->desc.CullEnable)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);

	glFrontFace(rasterizerState->desc.FrontCounterClockwise ? GL_CCW : GL_CW);

	// GL_DEPTH_CLAMP and GL_MULTISAMPLE are desktop GL only (GL_INVALID_ENUM
	// on ES); ES always rasterizes the samples its target carries and clips at
	// the frustum, which is what the two states describe here anyway.
	if (!IsGLES)
	{
		if (rasterizerState->desc.DepthClipEnable)
			glEnable(GL_DEPTH_CLAMP);
		else
			glDisable(GL_DEPTH_CLAMP);

		if (rasterizerState->desc.MultisampleEnable)
			glEnable(GL_MULTISAMPLE);
		else
			glDisable(GL_MULTISAMPLE);
	}
}

void GLRenderDevice::SetUniformBuffers(int start, int count, GLBuffer** buffers)
{
	for (int i = 0; i < count; i++)
	{
		glBindBufferBase(GL_UNIFORM_BUFFER, start + i, buffers[i] ? buffers[i]->Handle : 0);
	}
}

void GLRenderDevice::SetTextures(int start, int count, GLTexture** textures)
{
	for (int i = 0; i < count; i++)
	{
		glActiveTexture(GL_TEXTURE0 + start + i);
		glBindTexture(GL_TEXTURE_2D, textures[i] ? textures[i]->Handle : 0);
	}
	glActiveTexture(GL_TEXTURE0);
}

void GLRenderDevice::SetSamplers(int start, int count, GLSampler** samplers)
{
	for (int i = 0; i < count; i++)
	{
		glBindSampler(start + i, samplers[i] ? samplers[i]->Handle : 0);
	}
}

void GLRenderDevice::SetViewport(const GLViewport& viewport)
{
	glViewport((GLint)std::round(viewport.X), (GLint)std::round(viewport.Y), (GLsizei)std::round(viewport.Width), (GLsizei)std::round(viewport.Height));
	// The f variants: ES has only them, and they are desktop GL 4.2+ core,
	// which the GLSL-4.20 shaders already require.
	glDepthRangef((GLfloat)viewport.MinDepth, (GLfloat)viewport.MaxDepth);
}

void GLRenderDevice::PrecacheTexture(TextureInfo& Info, uint32_t PolyFlags)
{
	PolyFlags = ApplyPrecedenceRules(PolyFlags);
	Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));
}

void GLRenderDevice::ClearTextureCache()
{
	Textures->ClearCache();
}

bool GLRenderDevice::SupportsTextureFormat(TextureFormat Format)
{
	return Uploads->SupportsTextureFormat(Format);
}

void GLRenderDevice::UpdateTextureRect(TextureInfo& Info, int U, int V, int UL, int VL)
{
	Textures->UpdateTextureRect(&Info, U, V, UL, VL);
}

void GLRenderDevice::AddDrawBatch()
{
	if (Batch.SceneIndexStart != SceneIndexPos)
	{
		Batch.SceneIndexEnd = SceneIndexPos;
		QueuedBatches.push_back(Batch);
		Batch.SceneIndexStart = SceneIndexPos;
	}
}

void GLRenderDevice::DrawBatches(bool nextBuffer)
{
	if (Exited)
		return; // a frame may still be in flight when the engine tears us down (the intro ends mid-frame)

	// Nothing queued and nothing written since the last flush: keep the
	// current mapping. SetSceneNode flushes per BSP node -- hundreds a
	// frame -- and every unmap/remap of the stream buffers' unused ranges
		// submits the vendor driver's command buffer (measured at 2/3 of the
		// frame on the GE8300).
	if (!nextBuffer && QueuedBatches.empty() && Batch.SceneIndexStart == SceneIndexPos)
		return;

	AddDrawBatch();

	ThrowIfGLError("DrawBatches failed (before draw)");

	// Upload what the draw code wrote since the last flush, then draw it.
	if (GLSceneVertexPos > UploadedVertexPos || SceneIndexPos > UploadedIndexPos)
	{
		glBindBuffer(GL_ARRAY_BUFFER, ScenePass.VertexBuffer->Handle);
		glBufferSubData(GL_ARRAY_BUFFER, UploadedVertexPos * sizeof(GLSceneVertex), (GLSceneVertexPos - UploadedVertexPos) * sizeof(GLSceneVertex), StagingVertices.data() + UploadedVertexPos);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ScenePass.IndexBuffer->Handle);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, UploadedIndexPos * sizeof(uint32_t), (SceneIndexPos - UploadedIndexPos) * sizeof(uint32_t), StagingIndexes.data() + UploadedIndexPos);
		UploadedVertexPos = GLSceneVertexPos;
		UploadedIndexPos = SceneIndexPos;
	}

	for (const DrawBatchEntry& entry : QueuedBatches)
		DrawEntry(entry);
	QueuedBatches.clear();

	ThrowIfGLError("DrawBatches failed (after draw)");

	MapVertices(nextBuffer);

	Batch.SceneIndexStart = SceneIndexPos;
}

void GLRenderDevice::DrawEntry(const DrawBatchEntry& entry)
{
	size_t icount = entry.SceneIndexEnd - entry.SceneIndexStart;

	GLTexture2D* views[4] =
	{
		entry.Tex->Texture.get(),
		entry.Lightmap->Texture.get(),
		entry.Macrotex->Texture.get(),
		entry.Detailtex->Texture.get()
	};

	GLSampler* samplers[4] =
	{
		ScenePass.Samplers[entry.TexSamplerMode].get(),
		ScenePass.Samplers[0].get(),
		ScenePass.Samplers[entry.MacrotexSamplerMode].get(),
		ScenePass.Samplers[entry.DetailtexSamplerMode].get()
	};

	if (SceneViewport.MinDepth != entry.Pipeline->MinDepth || SceneViewport.MaxDepth != entry.Pipeline->MaxDepth)
	{
		SceneViewport.MinDepth = entry.Pipeline->MinDepth;
		SceneViewport.MaxDepth = entry.Pipeline->MaxDepth;
		SetViewport(SceneViewport);
	}

	glUseProgram(entry.Pipeline->ShaderProgram->Handle);
	SetSamplers(0, 4, samplers);
	SetTextures(0, 4, views);
	SetBlendState(entry.Pipeline->BlendState.get(), entry.BlendConstants);
	SetDepthStencilState(entry.Pipeline->DepthStencilState.get());

	glDrawElements(entry.Pipeline->PrimitiveTopology, (GLsizei)icount, GL_UNSIGNED_INT, (void*)(entry.SceneIndexStart * sizeof(uint32_t)));
	Stats.DrawCalls++;
}

void GLRenderDevice::SetDebugName(GLenum type, GLuint handle, const char* name)
{
	// Note: only exists in OpenGL 4.3 or if GL_KHR_debug extension is reported
	if (UseDebugLayer && glObjectLabel)
	{
		// OpenGL sucks!
		GLint maxlength = 0;
		glGetIntegerv(GL_MAX_LABEL_LENGTH, &maxlength);
		glObjectLabel(type, handle, (GLsizei)std::min((GLint)strlen(name), maxlength), name);
	}
}

std::shared_ptr<GLBlendState> GLRenderDevice::CreateBlendState(const GLBlendDesc& desc)
{
	return std::make_shared<GLBlendState>(desc);
}

std::shared_ptr<GLRasterizerState> GLRenderDevice::CreateRasterizerState(const GLRasterizerDesc& desc)
{
	return std::make_shared<GLRasterizerState>(desc);
}

std::shared_ptr<GLDepthStencilState> GLRenderDevice::CreateDepthStencilState(const GLDepthStencilDesc& desc)
{
	return std::make_shared<GLDepthStencilState>(desc);
}

std::shared_ptr<GLBuffer> GLRenderDevice::CreateBuffer(GLenum target, GLenum usage, const void* data, size_t size, const char* debugName)
{
	auto buffer = std::make_shared<GLBuffer>();
	glBindBuffer(target, buffer->Handle);
	SetDebugName(buffer, debugName);
	glBufferData(target, size, data, usage);
	ThrowIfGLError((std::string("CreateBuffer failed for ") + debugName).c_str());
	return buffer;
}

std::shared_ptr<GLProgram> GLRenderDevice::CreateProgram(const std::string& programName, std::shared_ptr<GLVertexShader> vertexShader, std::shared_ptr<GLFragmentShader> fragmentShader)
{
	auto program = std::make_shared<GLProgram>();
	SetDebugName(program, programName.c_str());
	glAttachShader(program->Handle, vertexShader->Handle);
	glAttachShader(program->Handle, fragmentShader->Handle);
	glLinkProgram(program->Handle);
	GLint status = GL_FALSE;
	glGetProgramiv(program->Handle, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		GLint length = 0;
		glGetProgramiv(program->Handle, GL_INFO_LOG_LENGTH, &length);
		std::string log;
		log.resize(length);
		glGetProgramInfoLog(program->Handle, length, &length, log.data());
		log.resize(length);
		throw std::runtime_error("Could not link program'" + programName + "':" + log);
	}
	return program;
}

std::shared_ptr<GLVertexShader> GLRenderDevice::CreateVertexShader(const std::string& shaderName, const std::string& filename, const std::vector<std::string> defines)
{
	auto shader = std::make_shared<GLVertexShader>();
	SetDebugName(shader, shaderName.c_str());
	CompileGlsl(shader.get(), filename, defines, false);
	return shader;
}

std::shared_ptr<GLFragmentShader> GLRenderDevice::CreateFragmentShader(const std::string& shaderName, const std::string& filename, const std::vector<std::string> defines)
{
	auto shader = std::make_shared<GLFragmentShader>();
	SetDebugName(shader, shaderName.c_str());
	CompileGlsl(shader.get(), filename, defines, true);
	return shader;
}

void GLRenderDevice::CompileGlsl(GLShader* shader, const std::string& filename, const std::vector<std::string> defines, bool isFragmentShader)
{
	std::string code;
	if (IsGLES)
	{
		// GLSL ES 3.20: the shader sources bind their samplers with
		// layout(binding = N), which is 3.20's (3.00 has none); the device is
		// GLES 3.2. An ES fragment shader has no default float precision, so
		// declare highp. The sources are otherwise the same as the desktop
		// ones -- gl_FragCoord.w is 1/w in ES as well.
		code = "#version 320 es\r\n";
		if (isFragmentShader)
			code += "precision highp float;\r\nprecision highp int;\r\nprecision highp sampler2D;\r\n";
	}
	else
	{
		code = "#version 420\r\n";
	}
	for (const std::string& define : defines)
	{
		code += "#define ";
		code += define;
		code += "\r\n";
	}
	code += "#line 1\r\n";
	code += GLFileResource::readAllText(filename);

	GLchar* source = (GLchar*)code.data();
	GLint length = (GLint)code.size();
	glShaderSource(shader->Handle, 1, &source, &length);
	glCompileShader(shader->Handle);

	GLint status = GL_FALSE;
	glGetShaderiv(shader->Handle, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		GLint length = 0;
		glGetShaderiv(shader->Handle, GL_INFO_LOG_LENGTH, &length);
		std::string log;
		log.resize(length);
		glGetShaderInfoLog(shader->Handle, length, &length, log.data());
		log.resize(length);
		throw std::runtime_error("Could not compile shader '" + filename + "':" + log);
	}
}
