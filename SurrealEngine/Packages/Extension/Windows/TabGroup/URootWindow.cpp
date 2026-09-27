
#include "Precomp.h"
#include "URootWindow.h"
#include "Utils/Logger.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/Resources/USound.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"
#include "Packages/Extension/Windows/UGC.h"
#include "Engine.h"
#include "GameWindow.h"
#include "RenderDevice/RenderDevice.h"
#include "Package/PackageManager.h"
#include "Packages/DeusEx/UDeusExSaveInfo.h"
#include "Packages/Engine/Resources/UPalette.h"
#include "Render/RenderSubsystem.h"

void URootWindow::EnablePositionalSound(std::optional<bool> bEnable)
{
	bPositionalSound() = !bEnable || *bEnable;
}

void URootWindow::EnableRendering(std::optional<bool> newRender)
{
	bRender() = !newRender || *newRender;
}

UObject* URootWindow::GenerateSnapshot(std::optional<bool> bFilter)
{
	return MakeSnapshot(nullptr, engine->packages->GetTransientPackage());
}

// The original's (dx-reverse-info/extension-dll.md, save pictures): the frame last
// drawn, averaged down to the size SetSnapshotSize gave -- each pixel the
// mean of the box of pixels it covers, its channels scaled by 256/255 --
// and kept as the mean of its three channels in an 8-bit texture with a
// grey palette, its sizes rounded up to powers of two. Into the texture
// given, or a new one in the package given.
UTexture* URootWindow::MakeSnapshot(UTexture* texture, Package* package)
{
	int width = snapshotWidth();
	int height = snapshotHeight();
	if (width <= 0 || height <= 0)
		return nullptr;

	Array<TextureColor> pixels;
	int srcWidth = 0, srcHeight = 0;
	if (!engine->render || !engine->render->ReadLastFrame(pixels, srcWidth, srcHeight))
		return nullptr;

	if (!texture)
	{
		UClass* textureClass = engine->packages->FindClass("Engine.Texture");
		texture = UObject::Cast<UTexture>(package->NewObject("Snapshot", textureClass, ObjectFlags::NoFlags));
	}

	UPalette* palette = texture->Palette();
	if (!palette || palette->package != texture->package)
	{
		UClass* paletteClass = engine->packages->FindClass("Engine.Palette");
		palette = UObject::Cast<UPalette>(texture->package->NewObject("SnapshotPalette", paletteClass, ObjectFlags::NoFlags));
	}
	palette->Colors.resize(256);
	for (uint32_t i = 0; i < 256; i++)
		palette->Colors[i] = i | (i << 8) | (i << 16) | 0xff000000;

	int usize = 1, ubits = 0;
	while (usize < width) { usize <<= 1; ubits++; }
	int vsize = 1, vbits = 0;
	while (vsize < height) { vsize <<= 1; vbits++; }

	UnrealMipmap mip;
	mip.Width = usize;
	mip.Height = vsize;
	mip.UBits = ubits;
	mip.VBits = vbits;
	mip.Data.resize((size_t)usize * vsize, 0);

	// The box walks the frame in steps of the frame's size over the
	// snapshot's, a step of whole pixels from where it truncates to.
	float xStep = srcWidth / (float)width;
	float yStep = srcHeight / (float)height;
	int dstY = 0;
	for (float srcY = 0.0f; srcY < (float)srcHeight; srcY += yStep, dstY++)
	{
		int dstX = 0;
		for (float srcX = 0.0f; srcX < (float)srcWidth; srcX += xStep, dstX++)
		{
			if (dstX >= width || dstY >= height)
				continue;

			double sum[3] = {};
			int count = 0;
			for (int i = 0; (float)i < xStep; i++)
			{
				for (int j = 0; (float)j < yStep; j++)
				{
					if ((float)i + srcX < (float)srcWidth && (float)j + srcY < (float)srcHeight)
					{
						const TextureColor& p = pixels[((size_t)srcY + j) * srcWidth + (size_t)srcX + i];
						sum[0] += p.R;
						sum[1] += p.G;
						sum[2] += p.B;
						count++;
					}
				}
			}
			if (count == 0)
				continue;

			int grey = 0;
			for (double channel : sum)
			{
				float scaled = (float)(channel / 255.0 / count) * 256.0f;
				grey += std::clamp((int)std::nearbyint(scaled - 0.5f), 0, 255);
			}
			mip.Data[(size_t)dstY * usize + dstX] = (uint8_t)(grey / 3);
		}
	}

	texture->Format() = (uint8_t)TextureFormat::P8;
	texture->Palette() = palette;
	texture->USize() = usize;
	texture->VSize() = vsize;
	texture->UBits() = (uint8_t)ubits;
	texture->VBits() = (uint8_t)vbits;
	texture->UClamp() = usize;
	texture->VClamp() = vsize;
	texture->MaxColor() = { 255, 255, 255, 255 };
	texture->bRealtimeChanged() = true;
	texture->UncompressedMipmaps.clear();
	texture->UncompressedMipmaps.push_back(std::move(mip));
	texture->UsedMipmaps = texture->UncompressedMipmaps;
	texture->UsedFormat = TextureFormat::P8;
	texture->TextureModified = true;
	return texture;
}

bool URootWindow::IsPositionalSoundEnabled()
{
	return bPositionalSound();
}

bool URootWindow::IsRenderingEnabled()
{
	return bRender();
}

// With movement locked the pointer stays where it is; with buttons locked the
// UI takes and ignores them. The Customize Keys screen locks movement while
// it waits for a key (extension-dll.md, The root window).
void URootWindow::LockMouse(std::optional<bool> bLockMove, std::optional<bool> bLockButton)
{
	bMouseMoveLocked() = bLockMove.value_or(false);
	bMouseButtonLocked() = bLockButton.value_or(false);
}

void URootWindow::SetDefaultEditCursor(std::optional<UObject*> newEditCursor)
{
	if (newEditCursor)
		defaultEditCursor() = UObject::Cast<UTexture>(*newEditCursor);
}

void URootWindow::SetDefaultMovementCursors(std::optional<UObject*> newMovementCursor, std::optional<UObject*> newHorizontalMovementCursor, std::optional<UObject*> newVerticalMovementCursor, std::optional<UObject*> newTopLeftMovementCursor, std::optional<UObject*> newTopRightMovementCursor)
{
	if (newMovementCursor)
		DefaultMoveCursor() = UObject::Cast<UTexture>(*newMovementCursor);
	if (newHorizontalMovementCursor)
		defaultHorizontalMoveCursor() = UObject::Cast<UTexture>(*newHorizontalMovementCursor);
	if (newVerticalMovementCursor)
		defaultVerticalMoveCursor() = UObject::Cast<UTexture>(*newVerticalMovementCursor);
	if (newTopLeftMovementCursor)
		defaultTopLeftMoveCursor() = UObject::Cast<UTexture>(*newTopLeftMovementCursor);
	if (newTopRightMovementCursor)
		defaultTopRightMoveCursor() = UObject::Cast<UTexture>(*newTopRightMovementCursor);
}

void URootWindow::SetRawBackground(std::optional<UObject*> NewTexture, std::optional<Color> NewColor)
{
	if (NewTexture)
		rawBackground() = UObject::Cast<UTexture>(*NewTexture);
	if (NewColor)
		rawColor() = *NewColor;
}

void URootWindow::SetRawBackgroundSize(float newWidth, float NewHeight)
{
	rawBackgroundWidth() = newWidth;
	rawBackgroundHeight() = NewHeight;
}

void URootWindow::SetRenderViewport(float newX, float newY, float newWidth, float NewHeight)
{
	renderX() = newX;
	renderY() = newY;
	renderWidth() = newWidth;
	renderHeight() = NewHeight;
	RenderViewportSet = true;
}

void URootWindow::ResetRenderViewport()
{
	RenderViewportSet = false;
}

void URootWindow::SetSnapshotSize(float newWidth, float NewHeight)
{
	snapshotWidth() = (int)newWidth;
	snapshotHeight() = (int)NewHeight;
}

void URootWindow::ShowCursor(std::optional<bool> bShow)
{
	bCursorVisible() = !bShow || *bShow;
}

void URootWindow::StretchRawBackground(std::optional<bool> bStretch)
{
	bStretchRawBackground() = !bStretch || *bStretch;
}

void URootWindow::WindowReady()
{
	SetRootCursorPos(GetVirtualWidth() * 0.5f, GetVirtualHeight() * 0.5f);
	UModalWindow::WindowReady();
}

bool URootWindow::IsCursorVisible()
{
	// The pointer is drawn while a window takes the mouse -- a modal one,
	// here -- and ShowCursor has not hidden it, as the original's
	// XRootWindow::PaintWindows draws it (dx-reverse-info/extension-dll.md):
	// the multiplayer message window hides it while it is up.
	if (!bCursorVisible())
		return false;
	for (UWindow* cur = firstChild(); cur; cur = cur->nextSibling())
	{
		if (UObject::TryCast<UModalWindow>(cur))
		{
			return true;
		}
	}
	return false;
}

void URootWindow::PostDrawWindow(UGC* gc)
{
	UModalWindow::PostDrawWindow(gc);
	if (IsCursorVisible())
	{
		// Find the cursor based on where the mouse is hovering:
		UTexture* cursor = nullptr;
		float relativeX = 0.0f, relativeY = 0.0f;
		UWindow* focus = GetCursorFocus(relativeX, relativeY);
		if (!focus)
			focus = this;
		while (focus)
		{
			if (UTexture* tex = UObject::Cast<UTexture>(focus->defaultCursor()))
			{
				cursor = tex;
				break;
			}
			focus = focus->parentOwner();
		}

		// Draw the cursor if we found one
		if (cursor)
		{
			Color white = { 255, 255, 255, 255 };
			gc->SetStyle(EDrawStyle::Masked);
			gc->SetTileColor(white);
			float hotspotX = cursor->USize() * 0.5f;
			float hotspotY = cursor->VSize() * 0.5f;
			gc->DrawIcon(MouseX() - hotspotX, MouseY() - hotspotY, cursor);
		}
	}
}

static UWindow* CommonAncestor(UWindow* a, UWindow* b)
{
	if (a == b)
		return a;

	std::vector<UWindow*> list1;
	std::vector<UWindow*> list2;
	list1.reserve(16);
	list2.reserve(16);
	for (UWindow* w = a; w != nullptr; w = w->parentOwner())
		list1.push_back(w);
	for (UWindow* w = b; w != nullptr; w = w->parentOwner())
		list2.push_back(w);

	if (list1.empty() || list2.empty() || list1.back() != list2.back())
		return nullptr;

	auto it1 = list1.rbegin();
	auto it2 = list2.rbegin();
	while (it1 != list1.rend() && it2 != list2.rend())
	{
		if (*it1 != *it2)
		{
			return *(--it1);
		}
		++it1;
		++it2;
	}

	if (it1 == list1.rend())
		return *(--it1);
	else if (it2 == list2.rend())
		return *(--it2);

	return nullptr;
}

bool URootWindow::SetRootFocusWindow(UWindow* newFocusWindow)
{
	UWindow* oldFocusWindow = FocusWindow();
	if (oldFocusWindow != newFocusWindow)
	{
		UWindow* ancestor = CommonAncestor(oldFocusWindow, newFocusWindow);
		if (oldFocusWindow)
		{
			if (oldFocusWindow->unfocusSound())
				PlaySound(oldFocusWindow->unfocusSound(), {}, {}, {}, {});

			oldFocusWindow->FocusLeftWindow();
			if (oldFocusWindow != ancestor)
				for (UWindow* w = oldFocusWindow->parentOwner(); w && w != ancestor; w = w->parentOwner())
				{
					w->FocusLeftDescendant(oldFocusWindow);
				}
		}
		FocusWindow() = newFocusWindow;
		if (newFocusWindow)
		{
			// Note: this order is in reverse. Hopefully it doesn't matter.
			newFocusWindow->FocusEnteredWindow();
			if(newFocusWindow != ancestor)
				for (UWindow* w = newFocusWindow->parentOwner(); w && w != ancestor; w = w->parentOwner())
				{
					w->FocusEnteredDescendant(newFocusWindow);
				}

			if (newFocusWindow->focusSound())
				PlaySound(newFocusWindow->focusSound(), {}, {}, {}, {});
		}
	}
	return true;
}

void URootWindow::SetRootCursorPos(float newMouseX, float newMouseY)
{
	// Clip cursor to the entire screen, not the root window box:

	newMouseX += UsedX;
	newMouseY += UsedY;

	float scale = GetVirtualScale();
	float realWidth = std::ceil(engine->viewport->ViewportWidth() / scale);
	float realHeight = std::ceil(engine->viewport->ViewportHeight() / scale);

	newMouseX = std::max(newMouseX, 0.0f);
	newMouseY = std::max(newMouseY, 0.0f);
	newMouseX = std::min(newMouseX, realWidth);
	newMouseY = std::min(newMouseY, realHeight);

	newMouseX -= UsedX;
	newMouseY -= UsedY;

	// Apply the new cursor pos:

	prevMouseX() = MouseX();
	prevMouseY() = MouseY();
	MouseX() = newMouseX;
	MouseY() = newMouseY;

	float relativeX = 0.0f, relativeY = 0.0f;
	UWindow* focus = GetCursorFocus(relativeX, relativeY);

	auto lastFocus = UObject::Cast<UWindow>(lastMouseWindow());
	if (lastFocus != focus)
	{
		if (lastFocus)
		{
			if (UWindow* ancestor = CommonAncestor(lastFocus, focus))
			{
				for (UWindow* w = lastFocus; w != ancestor; w = w->parentOwner())
				{
					w->MouseLeftWindow();
				}
			}
		}
		lastMouseWindow() = focus;
		focus->MouseEnteredWindow();
	}

	focus->MouseMoved(relativeX, relativeY);
}

UWindow* URootWindow::GetCursorFocus(float& relativeX, float& relativeY)
{
	if (UWindow* grab = grabbedWindow())
	{
		ConvertCoordinates(this, MouseX(), MouseY(), grab, relativeX, relativeY);
		return grab;
	}

	if (UWindow* cursor = FindWindow(MouseX(), MouseY(), relativeX, relativeY))
		return cursor;

	relativeX = MouseX();
	relativeY = MouseY();
	return this;
}

bool URootWindow::OnWindowMouseMove(const Point& pos)
{
#if 0 // We currently handle this in OnWindowRawMouseMove
	if (IsCursorVisible())
		SetRootCursorPos((float)pos.x / scale, (float)pos.y / scale);
#endif
	return IsModalOpen();
}

bool URootWindow::OnWindowMouseDown(const Point& pos, EInputKey key)
{
	if (bMouseButtonLocked())
		return true;

	float relativeX = 0.0f, relativeY = 0.0f;
	UWindow* focus = GetCursorFocus(relativeX, relativeY);

	if (!focus->bIsSensitive())
		return IsModalOpen();

	if (focus->RawMouseButtonPressed(relativeX, relativeY, key, EInputType::IST_Press))
		return true;

	if (focus->bIsSelectable())
		SetRootFocusWindow(focus);

	int numClicks = 1; // What is this?
	for (UWindow* cur = focus; cur; cur = cur->parentOwner())
	{
		if (cur->MouseButtonPressed(relativeX, relativeY, key, numClicks))
			return true;
	}

	return IsModalOpen();
}

bool URootWindow::OnWindowMouseDoubleclick(const Point& pos, EInputKey key)
{
	// Is this numClicks = 2?
	return IsModalOpen();
}

bool URootWindow::OnWindowMouseUp(const Point& pos, EInputKey key)
{
	if (bMouseButtonLocked())
		return true;

	float relativeX = 0.0f, relativeY = 0.0f;
	UWindow* focus = GetCursorFocus(relativeX, relativeY);

	if (focus->RawMouseButtonPressed(relativeX, relativeY, key, EInputType::IST_Release))
		return true;

	if (!focus->bIsSensitive())
		return IsModalOpen();

	int numClicks = 1; // What is this?
	for (UWindow* cur = focus; cur; cur = cur->parentOwner())
	{
		if (cur->MouseButtonReleased(relativeX, relativeY, key, numClicks))
			return true;
	}

	return IsModalOpen();
}

bool URootWindow::OnWindowMouseWheel(const Point& pos, EInputKey key)
{
	if (!OnWindowMouseDown(pos, key))
		return false;

	OnWindowMouseUp(pos, key);
	return true;
}

bool URootWindow::OnWindowRawMouseMove(int dx, int dy)
{
	if (IsCursorVisible() && !bMouseMoveLocked())
	{
		// Deltas are window pixels; the UI is laid out in render pixels
		float mouseSpeed = engine->window->GetRenderDevice()->GetRenderScale() / GetVirtualScale();
		SetRootCursorPos(MouseX() + dx * mouseSpeed, MouseY() + dy * mouseSpeed);
	}
	return IsModalOpen();
}

bool URootWindow::OnWindowKeyChar(std::string chars)
{
	UWindow* focus = FocusWindow();
	if (!focus)
		return IsModalOpen();

	if (focus->KeyPressed(chars))
		return true;

	// To do: fire these for edit windows
	// event bool TextChanged(window edit, bool bModified)
	// event bool EditActivated(window edit, bool bModified)

	return IsModalOpen();
}

bool URootWindow::OnWindowKeyDown(EInputKey key)
{
	UWindow* focus = FocusWindow();
	if (!focus)
		return IsModalOpen();

	// To do: this shouldn't just check the focus window. It needs to build an accelerator table for all windows
	if (engine->window->GetKeyState(IK_Alt) && focus->acceleratorKey() != 0)
	{
		std::string chars(1, (char)focus->acceleratorKey());
		EInputKey accelKey = (EInputKey)(uint8_t)chars.front();
		if (focus->AcceleratorKeyPressed(chars))
			return true;
	}

	bool repeat = false; // To do: can surrealwidgets tell us this?

	if (focus->RawKeyPressed(key, EInputType::IST_Press, repeat))
		return true;

	if (focus->VirtualKeyPressed(key, repeat))
		return true;

	return IsModalOpen();
}

bool URootWindow::OnWindowKeyUp(EInputKey key)
{
	UWindow* focus = FocusWindow();
	if (!focus)
		return IsModalOpen();

	if (focus->RawKeyPressed(key, EInputType::IST_Release, false))
		return true;

	// To do: fire these for specific window types
	// event bool ButtonActivated(Window button)
	// event bool ToggleChanged(Window button, bool bNewToggle)
	// event bool BoxOptionSelected(Window box, int buttonNumber)
	// event bool ListRowActivated(window list, int rowId)
	// event bool ListSelectionChanged(window list, int numSelections, int focusRowId)

	return IsModalOpen();
}

bool URootWindow::IsModalOpen()
{
	for (UWindow* child = lastChild(); child; child = child->prevSibling())
	{
		if (UObject::TryCast<UModalWindow>(child))
			return true;
	}
	return false;
}
