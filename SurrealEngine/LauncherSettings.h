#pragma once

enum class RenderDeviceType
{
	Vulkan,
	D3D11,
	OpenGL,
	GLES
};

enum class AntialiasMode
{
	Off,
	MSAA2x,
	MSAA4x
};

enum class LightMode
{
	Normal,
	OneX,
	BrighterActors
};

enum class GammaMode
{
	D3D9,
	XOpenGL
};

class LauncherSettings
{
public:
	static LauncherSettings& Get();

	struct
	{
		RenderDeviceType Type = RenderDeviceType::Vulkan;
		bool UseVSync = true;
		AntialiasMode Antialias = AntialiasMode::MSAA4x;
		LightMode Light = LightMode::Normal;
		GammaMode Gamma = GammaMode::D3D9;
		bool GammaCorrectScreenshots = false;
		bool Hdr = false;
		int HdrScale = 128;
		bool Bloom = false;
		int BloomAmount = 128;
		bool UseDebugLayer = false;
	} RenderDevice;

	// Controller support (Engine::UpdateGamepad).
	struct
	{
		bool Enabled = true;
		float DeadZone = 0.2f;
		float LookSensitivityX = 1.0f;
		float LookSensitivityY = 1.0f;
		bool InvertY = false;
		float CursorSpeed = 1.0f;
	} Gamepad;

	// Speed for slow hardware.
	struct
	{
		float RenderScale = 1.0f; // the scene's size as a fraction of the window's (RenderDevice::GetRenderScale)
	} Performance;

private:
	LauncherSettings();
};
