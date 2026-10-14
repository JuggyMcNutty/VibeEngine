
#include "Precomp.h"
#include "LauncherSettings.h"
#include "Utils/JsonValue.h"
#include "Utils/File.h"

LauncherSettings& LauncherSettings::Get()
{
	static LauncherSettings settings;
	return settings;
}

static std::string GetSettingsFilename()
{
	return (Directory::localAppData() / "SurrealEngine/Settings.json").string();
}

LauncherSettings::LauncherSettings()
{
	try
	{
		JsonValue settings = JsonValue::parse(File::read_all_text(GetSettingsFilename()));

		std::string rendevtype = settings["RenderDevice"]["Type"].to_string();
		if (rendevtype == "Vulkan")
			RenderDevice.Type = RenderDeviceType::Vulkan;
		else if (rendevtype == "D3D11")
			RenderDevice.Type = RenderDeviceType::D3D11;
		else if (rendevtype == "OpenGL")
			RenderDevice.Type = RenderDeviceType::OpenGL;
		else if (rendevtype == "GLES")
			RenderDevice.Type = RenderDeviceType::GLES;

		std::string rendevaa = settings["RenderDevice"]["Antialias"].to_string();
		if (rendevaa == "Off")
			RenderDevice.Antialias = AntialiasMode::Off;
		else if (rendevaa == "MSAA2x")
			RenderDevice.Antialias = AntialiasMode::MSAA2x;
		else if (rendevaa == "MSAA4x")
			RenderDevice.Antialias = AntialiasMode::MSAA4x;

		std::string rendevlight = settings["RenderDevice"]["Light"].to_string();
		if (rendevlight == "Normal")
			RenderDevice.Light = LightMode::Normal;
		else if (rendevlight == "OneX")
			RenderDevice.Light = LightMode::OneX;
		else if (rendevlight == "BrighterActors")
			RenderDevice.Light = LightMode::BrighterActors;

		std::string rendevgamma = settings["RenderDevice"]["Gamma"].to_string();
		if (rendevgamma == "D3D9")
			RenderDevice.Gamma = GammaMode::D3D9;
		else if (rendevgamma == "XOpenGL")
			RenderDevice.Gamma = GammaMode::XOpenGL;

		RenderDevice.GammaCorrectScreenshots = settings["RenderDevice"]["GammaCorrectScreenshots"].to_boolean();
		RenderDevice.UseVSync = settings["RenderDevice"]["UseVSync"].to_boolean();
		RenderDevice.Hdr = settings["RenderDevice"]["Hdr"].to_boolean();
		RenderDevice.HdrScale = settings["RenderDevice"]["HdrScale"].to_int();
		RenderDevice.Bloom = settings["RenderDevice"]["Bloom"].to_boolean();
		RenderDevice.BloomAmount = settings["RenderDevice"]["BloomAmount"].to_int();
		RenderDevice.UseDebugLayer = settings["RenderDevice"]["UseDebugLayer"].to_boolean();

		// Absent members keep their defaults: a missing bool would otherwise
		// read as false and quietly turn controller support off.
		const JsonValue& pad = settings["Gamepad"];
		if (!pad["Enabled"].is_undefined()) Gamepad.Enabled = pad["Enabled"].to_boolean();
		if (!pad["DeadZone"].is_undefined()) Gamepad.DeadZone = (float)pad["DeadZone"].to_number();
		if (!pad["LookSensitivityX"].is_undefined()) Gamepad.LookSensitivityX = (float)pad["LookSensitivityX"].to_number();
		if (!pad["LookSensitivityY"].is_undefined()) Gamepad.LookSensitivityY = (float)pad["LookSensitivityY"].to_number();
		if (!pad["InvertY"].is_undefined()) Gamepad.InvertY = pad["InvertY"].to_boolean();
		if (!pad["CursorSpeed"].is_undefined()) Gamepad.CursorSpeed = (float)pad["CursorSpeed"].to_number();

		const JsonValue& perf = settings["Performance"];
		if (!perf["RenderScale"].is_undefined()) Performance.RenderScale = (float)perf["RenderScale"].to_number();
	}
	catch (...)
	{
	}
}
