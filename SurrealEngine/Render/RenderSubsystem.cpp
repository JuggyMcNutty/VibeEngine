
#include "Precomp.h"
#include "RenderSubsystem.h"
#include "RenderDevice/RenderDevice.h"
#include "GameWindow.h"
#include "Packages/Engine/USurrealClient.h"
#include "Packages/Engine/UConsole.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Resources/UPalette.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Resources/Level/UPolys.h"
#include "Packages/Extension/Windows/TabGroup/URootWindow.h"
#include "VM/ScriptCall.h"
#include "Engine.h"

RenderSubsystem::RenderSubsystem(RenderDevice* renderdevice) : Device(renderdevice)
{
	// Deus Ex's brightness is its D3DDrv's gamma ramp: 2.5 x Brightness.
	if (engine->LaunchInfo.IsDeusEx())
		Device->GammaScale = 2.5f;
}

void RenderSubsystem::DrawGame(float levelTimeElapsed)
{
	DrawnSinceFlush = true;
	LevelTimeElapsed = levelTimeElapsed;
	AutoUV += levelTimeElapsed * 64.0f;
	engine->Level->Light.Tick(levelTimeElapsed);

	Stats.Frames = 0;
	Stats.Surfaces = 0;
	Stats.Actors = 0;
	Stats.LightmapsUpdated = 0;

	// The screen flash as the original's game engine hands it to the
	// device: the viewport's player's, the scale halved, both clamped to 0-1
	// with no fourth component -- none with the client's ScreenFlashes off,
	// which a net game overrides (dx-reverse-info/d3ddrv-dll.md, the screen
	// flash). The devices draw nothing for a scale of 0.5 and a black fog.
	vec3 flashScale = 0.5f;
	vec3 flashFog = vec3(0.0f, 0.0f, 0.0f);

	UPlayerPawn* player = engine->viewport ? engine->viewport->Actor() : nullptr;
	bool flashes = engine->client->ScreenFlashes || (engine->LevelInfo && engine->LevelInfo->NetMode() != NM_Standalone);
	if (player && flashes)
	{
		vec3 scale = player->FlashScale() * 0.5f;
		vec3 fog = player->FlashFog();
		flashScale = vec3(clamp(scale.x, 0.0f, 1.0f), clamp(scale.y, 0.0f, 1.0f), clamp(scale.z, 0.0f, 1.0f));
		flashFog = vec3(clamp(fog.x, 0.0f, 1.0f), clamp(fog.y, 0.0f, 1.0f), clamp(fog.z, 0.0f, 1.0f));
	}

	Device->Brightness = engine->client->Brightness;
	Device->Lock(vec4(flashScale, 0.0f), vec4(flashFog, 0.0f), vec4(0.0f), nullptr, nullptr);
	FrameInProgress = true;

	ResetCanvas();
	PreRender();

	if (engine->LaunchInfo.ue1Version <= 219 || engine->console->bNoDrawWorld() == false)
	{
		// With the root window's rendering off (a menu over the snapshot,
		// the black background, the credits) the original's scene frame is
		// empty: nothing of the world or the overlays is drawn, and the
		// windows still are, from PostRenderFlash
		// (dx-reverse-info/extension-dll.md, the raw background).
		// Nothing of the world while a joining client waits for its player.
		if (engine->viewport->Actor() && (!engine->dxRootWindow || engine->dxRootWindow->bRender()))
		{
			DrawScene();
			RenderOverlays();
		}
		if (engine->LaunchInfo.IsDeusEx())
			PostRenderFlash();
		Device->EndFlash();
	}

	PostRender();

	Device->Unlock(true);
	FrameInProgress = false;
	FrameDrawn = true;
}

bool RenderSubsystem::ReadLastFrame(Array<TextureColor>& pixels, int& width, int& height)
{
	if (FrameInProgress || !FrameDrawn)
		return false;

	width = Device->GetRenderWidth();
	height = Device->GetRenderHeight();
	if (width <= 0 || height <= 0)
		return false;

	pixels.resize((size_t)width * height);
	Device->ReadPixels(pixels.data());
	return true;
}

void RenderSubsystem::DrawEditorViewport()
{
	Device->Brightness = engine->client->Brightness;
	DrawScene();
}

void RenderSubsystem::DrawVideoFrame(TextureInfo* frame, TextureInfo* background)
{
	vec3 flashScale = 0.5f;
	vec3 flashFog = vec3(0.0f, 0.0f, 0.0f);
	Device->Brightness = 0.4f;// engine->client->Brightness;
	Device->Lock(vec4(flashScale, 0.0f), vec4(flashFog, 0.0f), vec4(0.0f), nullptr, nullptr);
	FrameInProgress = true;
	ResetCanvas();
	Device->SetSceneNode(&Canvas.Frame);

	float sizeX = (float)(int)(engine->viewport->ViewportWidth() / (float)Canvas.uiscale);
	float sizeY = (float)(int)(engine->viewport->ViewportHeight() / (float)Canvas.uiscale);

	Rectf clipBox = Rectf::xywh(0.0f, 0.0f, sizeX, sizeY);
	Rectf dest = clipBox;

	if (frame)
	{
		Rectf src = Rectf::xywh(0.0f, 0.0f, (float)frame->USize, (float)frame->VSize);
		DrawTile(*frame, dest, src, clipBox, 1.0f, vec4(1.0f), vec4(0.0f), PF_TwoSided);
	}

	if (background)
	{
		Rectf src = Rectf::xywh(0.0f, 0.0f, (float)background->USize, (float)background->VSize);
		DrawTile(*background, dest, src, clipBox, 1.0f, vec4(1.0f), vec4(0.0f), PF_TwoSided | PF_Highlighted);
	}

	Device->EndFlash();
	Device->Unlock(true);
	FrameInProgress = false;
}

void RenderSubsystem::UpdateTexture(UTexture* tex)
{
	if (tex && tex->FrameCounter != TextureFrameCounter)
	{
		tex->Update(LevelTimeElapsed);
		tex->FrameCounter = TextureFrameCounter;
	}
}

void RenderSubsystem::UpdateTextureInfo(TextureInfo& info, BspSurface& surface, UTexture* texture, float ZoneUPanSpeed, float ZoneVPanSpeed)
{
	UpdateTextureInfo(info, texture);

	info.Pan.x = -(float)surface.PanU;
	info.Pan.y = -(float)surface.PanV;
	if (surface.PolyFlags & PF_AutoUPan) info.Pan.x -= AutoUV * ZoneUPanSpeed;
	if (surface.PolyFlags & PF_AutoVPan) info.Pan.y -= AutoUV * ZoneVPanSpeed;
}

void RenderSubsystem::UpdateTextureInfo(TextureInfo& info, const Poly& poly, UTexture* texture, float ZoneUPanSpeed, float ZoneVPanSpeed)
{
	UpdateTextureInfo(info, texture);

	info.Pan.x = -(float)poly.PanU;
	info.Pan.y = -(float)poly.PanV;
	if (poly.PolyFlags & PF_AutoUPan) info.Pan.x -= AutoUV * ZoneUPanSpeed;
	if (poly.PolyFlags & PF_AutoVPan) info.Pan.y -= AutoUV * ZoneVPanSpeed;
}

void RenderSubsystem::UpdateTextureInfo(TextureInfo& info, UTexture* texture)
{
	info.Texture = texture;
	info.CacheID = (uint64_t)(ptrdiff_t)texture;

	if (!info.Texture)
		return;

	info.UScale = texture->DrawScale();
	info.VScale = texture->DrawScale();
	info.Format = texture->UsedFormat;
	info.Mips = texture->UsedMipmaps.data();
	info.NumMips = (int)texture->UsedMipmaps.size();
	info.USize = texture->USize();
	info.VSize = texture->VSize();
	if (texture->Palette())
		info.Palette = (TextureColor*)texture->Palette()->Colors.data();

	info.bRealtimeChanged = texture->TextureModified;
	if (texture->TextureModified)
		texture->TextureModified = false;
}

void RenderSubsystem::PurgeDying()
{
	CoronaStates.erase(std::remove_if(CoronaStates.begin(), CoronaStates.end(), [](const CoronaState& state) { return GC::IsDying(state.Light); }), CoronaStates.end());
	CoronaDynamicLights.erase(std::remove_if(CoronaDynamicLights.begin(), CoronaDynamicLights.end(), [](UActor* light) { return GC::IsDying(light); }), CoronaDynamicLights.end());
	IteratorActors.erase(std::remove_if(IteratorActors.begin(), IteratorActors.end(), [](UActor* actor) { return GC::IsDying(actor); }), IteratorActors.end());
}

void RenderSubsystem::FlushDevice()
{
	Device->Flush(true);
	DrawnSinceFlush = false;
}

void RenderSubsystem::OnMapLoaded()
{
	Device->Flush(true);
	DrawnSinceFlush = false;
	engine->Level->Light.OnMapLoaded();
	// Nothing of the level before: its lights are not this one's.
	IteratorActors.clear();
	CoronaStates.clear();
	CoronaDynamicLights.clear();
}
