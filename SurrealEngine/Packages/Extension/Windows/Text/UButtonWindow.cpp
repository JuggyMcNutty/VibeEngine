
#include "Precomp.h"
#include "UButtonWindow.h"
#include "Packages/Extension/Windows/UGC.h"
#include "Packages/Engine/Resources/USound.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"

void UButtonWindow::DrawWindow(UGC* gc)
{
	// The appearance by state, as the original chooses it
	// (XButtonWindow::ChangeButtonAppearance 0x10008380): focused while the
	// whole parent chain is sensitive, else normal; pressed adds one; an
	// insensitive link picks the insensitive pair. The state's texture falls
	// back to the pressed one, then the normal one. This is the blue a
	// conversation's focused choice draws in, and a menu's focused button.
	int state = 0;
	bool chainSensitive = true;
	for (UWindow* i = this; i; i = i->parentOwner())
	{
		if (!i->bIsSensitive())
		{
			chainSensitive = false;
			break;
		}
	}
	if (!chainSensitive)
		state = 4;
	else if (IsFocusWindow())
		state = 2;
	if (bButtonPressed())
		state++;

	UTexture* tex = nullptr;
	Color tileColor = { 255, 255, 255, 255 };
	Color textColor = { 255, 255, 255, 255 };
	UTexture* textures[6] = { ButtonTextures.Normal, ButtonTextures.Pressed, ButtonTextures.NormalFocus, ButtonTextures.PressedFocus, ButtonTextures.NormalInsensitive, ButtonTextures.PressedInsensitive };
	Color tileColors[6] = { ButtonColors.Normal, ButtonColors.Pressed, ButtonColors.NormalFocus, ButtonColors.PressedFocus, ButtonColors.NormalInsensitive, ButtonColors.PressedInsensitive };
	Color textColors[6] = { TextColors.Normal, TextColors.Pressed, TextColors.NormalFocus, TextColors.PressedFocus, TextColors.NormalInsensitive, TextColors.PressedInsensitive };
	tex = textures[state];
	if (!tex && !(bButtonPressed() && (tex = ButtonTextures.Pressed)))
		tex = ButtonTextures.Normal;
	tileColor = tileColors[state];
	textColor = textColors[state];

	if (tex)
	{
		gc->SetTileColor(tileColor);
		gc->DrawStretchedTexture(
			0.0f, 0.0f, Width(), Height(),
			0.0f, 0.0f, (float)tex->USize(), (float)tex->VSize(),
			tex);
	}

	if (normalFont())
	{
		float xMargin = hMargin();
		float yMargin = vMargin();
		float w = Width() - 2.0f * xMargin;
		float h = Height() - 2.0f * yMargin;
		if (w > 0.0f && h > 0.0f)
		{
			gc->SetTextColor(textColor);
			gc->SetAlignments(HAlign(), VAlign());
			gc->DrawText(xMargin, yMargin, w, h, Text());
		}
		// DrawDebugBox(gc);
	}

	// Note: we are intentionally not calling UTextWindow::DrawWindow
	UWindow::DrawWindow(gc);
}

void UButtonWindow::ActivateButton(EInputKey key)
{
	for (UWindow* cur = this; cur; cur = cur->parentOwner())
	{
		if (cur->ButtonActivated(this))
			break;
	}
}

void UButtonWindow::EnableAutoRepeat(std::optional<bool> bEnable, std::optional<float> newInitialDelay, std::optional<float> newRepeatRate)
{
	if (bEnable)
		bAutoRepeat() = *bEnable;
	if (newInitialDelay)
		initialDelay() = *newInitialDelay;
	if (newRepeatRate)
		repeatRate() = *newRepeatRate;

	repeatTime() = 0.0f; // To do: this field is probably used to figure out when its time to repeat
}

void UButtonWindow::EnableRightMouseClick(std::optional<bool> bEnable)
{
	bEnableRightMouseClick() = !bEnable || *bEnable;
}

void UButtonWindow::PressButton(std::optional<uint8_t> Key)
{
	// How does this differ from activate? It animates first maybe? use activateTimer() for that?
	ActivateButton(Key ? (EInputKey)(*Key) : IK_LeftMouse);
}

void UButtonWindow::SetActivateDelay(std::optional<float> newDelay)
{
	if (newDelay)
		activateDelay() = *newDelay;
}

void UButtonWindow::SetButtonColors(std::optional<Color> Normal, std::optional<Color> pressed, std::optional<Color> normalFocus, std::optional<Color> pressedFocus, std::optional<Color> normalInsensitive, std::optional<Color> pressedInsensitive)
{
	if (Normal)
		ButtonColors.Normal = *Normal;
	if (pressed)
		ButtonColors.Pressed = *pressed;
	if (normalFocus)
		ButtonColors.NormalFocus = *normalFocus;
	if (pressedFocus)
		ButtonColors.PressedFocus = *pressedFocus;
	if (normalInsensitive)
		ButtonColors.NormalInsensitive = *normalInsensitive;
	if (pressedInsensitive)
		ButtonColors.PressedInsensitive = *pressedInsensitive;
}

void UButtonWindow::SetButtonSounds(std::optional<UObject*> newPressSound, std::optional<UObject*> newClickSound)
{
	if (newPressSound)
		pressSound() = UObject::Cast<USound>(*newPressSound);
	if (newClickSound)
		clickSound() = UObject::Cast<USound>(*newClickSound);
}

void UButtonWindow::SetButtonTextures(std::optional<UObject*> Normal, std::optional<UObject*> pressed, std::optional<UObject*> normalFocus, std::optional<UObject*> pressedFocus, std::optional<UObject*> normalInsensitive, std::optional<UObject*> pressedInsensitive)
{
	if (Normal)
		ButtonTextures.Normal = UObject::Cast<UTexture>(*Normal);
	if (pressed)
		ButtonTextures.Pressed = UObject::Cast<UTexture>(*pressed);
	if (normalFocus)
		ButtonTextures.NormalFocus = UObject::Cast<UTexture>(*normalFocus);
	if (pressedFocus)
		ButtonTextures.PressedFocus = UObject::Cast<UTexture>(*pressedFocus);
	if (normalInsensitive)
		ButtonTextures.NormalInsensitive = UObject::Cast<UTexture>(*normalInsensitive);
	if (pressedInsensitive)
		ButtonTextures.PressedInsensitive = UObject::Cast<UTexture>(*pressedInsensitive);
}

void UButtonWindow::SetTextColors(std::optional<Color> Normal, std::optional<Color> pressed, std::optional<Color> normalFocus, std::optional<Color> pressedFocus, std::optional<Color> normalInsensitive, std::optional<Color> pressedInsensitive)
{
	if (Normal)
		TextColors.Normal = *Normal;
	if (pressed)
		TextColors.Pressed = *pressed;
	if (normalFocus)
		TextColors.NormalFocus = *normalFocus;
	if (pressedFocus)
		TextColors.PressedFocus = *pressedFocus;
	if (normalInsensitive)
		TextColors.NormalInsensitive = *normalInsensitive;
	if (pressedInsensitive)
		TextColors.PressedInsensitive = *pressedInsensitive;
}

void UButtonWindow::MouseMoved(float newX, float newY)
{
	if (bMousePressed())
	{
		bButtonPressed() = newX >= 0.0f && newX < Width() && newY >= 0.0f && newY < Height();
	}
	else
	{
		UTextWindow::MouseMoved(newX, newY);
	}
}

bool UButtonWindow::MouseButtonPressed(float pointX, float pointY, EInputKey button, int numClicks)
{
	SetFocusWindow(this);

	if (UTextWindow::MouseButtonPressed(pointX, pointY, button, numClicks))
		return true;

	bMousePressed() = true;
	bButtonPressed() = true;
	GrabMouse();
	return true;
}

bool UButtonWindow::MouseButtonReleased(float pointX, float pointY, EInputKey button, int numClicks)
{
	if (bMousePressed())
	{
		UngrabMouse();
		bMousePressed() = false;
		bButtonPressed() = false;
		if (pointX >= 0.0f && pointX < Width() && pointY >= 0.0f && pointY < Height())
			ActivateButton(button);
		return true;
	}

	return UTextWindow::MouseButtonReleased(pointX, pointY, button, numClicks);
}

void UButtonWindow::Mark(GCMarker& marker)
{
	UTextWindow::Mark(marker);
	marker.SetField("ButtonTextures");
	marker.Mark(ButtonTextures.Normal);
	marker.Mark(ButtonTextures.Pressed);
	marker.Mark(ButtonTextures.NormalFocus);
	marker.Mark(ButtonTextures.PressedFocus);
	marker.Mark(ButtonTextures.NormalInsensitive);
	marker.Mark(ButtonTextures.PressedInsensitive);
}
