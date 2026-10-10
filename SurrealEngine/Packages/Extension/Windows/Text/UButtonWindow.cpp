
#include "Precomp.h"
#include "UButtonWindow.h"
#include "Packages/Extension/Windows/UGC.h"
#include "Packages/Engine/Resources/USound.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"

// The original's XButtonWindow::Init (0x10007640): no auto repeat, a press
// shown for 0.3 s, a held repeat after 0.5 s every 0.1 s, no sounds, white
// tiles and green text in every state.
void UButtonWindow::InitDefaults()
{
	UTextWindow::InitDefaults();
	bButtonPressed() = false;
	bMousePressed() = false;
	bAutoRepeat() = false;
	activateDelay() = 0.3f;
	initialDelay() = 0.5f;
	repeatRate() = 0.1f;
	lastInputKey() = IK_None;
	pressSound() = nullptr;
	clickSound() = nullptr;
	activateTimer() = 0;
	repeatTime() = 0.0f;
	Color white = { 255, 255, 255, 255 };
	Color green = { 0, 255, 0, 255 };
	ButtonColors = { white, white, white, white, white, white };
	TextColors = { green, green, green, green, green, green };
	ButtonTextures = {};
}

// The appearance by state, as the original chooses it
// (XButtonWindow::ChangeButtonAppearance 0x10008380): focused while the whole
// parent chain is sensitive, else normal; pressed -- a toggle's on -- adds
// one; an insensitive link picks the insensitive pair. This is the blue a
// conversation's focused choice draws in, and a menu's focused button.
int UButtonWindow::AppearanceState()
{
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
	return state;
}

void UButtonWindow::DrawWindow(UGC* gc)
{
	// The state's texture falls back to the pressed one, then the normal one.
	int state = AppearanceState();

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

	// The original's XButtonWindow::Draw (0x100082c0): the state's colours,
	// its texture tiled over the button, then the text window's drawing.
	gc->SetTileColor(tileColor);
	gc->SetTextColor(textColor);
	if (tex)
		gc->DrawPattern(0.0f, 0.0f, Width(), Height(), 0.0f, 0.0f, tex);
	UTextWindow::DrawWindow(gc);
}

// The original's (XButtonWindow::ActivateButton 0x100077e0): up the parents
// until one takes it -- a right click as ButtonActivatedRight.
void UButtonWindow::ActivateButton(EInputKey key)
{
	for (UWindow* cur = this; cur; cur = cur->parentOwner())
	{
		bool handled = key == IK_RightMouse ? cur->ButtonActivatedRight(this) : cur->ButtonActivated(this);
		if (handled)
			break;
	}
}

// Left out: on, a first repeat after 0.5 s, then every 0.1 s
// (XButtonWindow::execEnableAutoRepeat 0x10008ad0).
void UButtonWindow::EnableAutoRepeat(std::optional<bool> bEnable, std::optional<float> newInitialDelay, std::optional<float> newRepeatRate)
{
	bAutoRepeat() = bEnable.value_or(true);
	initialDelay() = newInitialDelay.value_or(0.5f);
	repeatRate() = newRepeatRate.value_or(0.1f);
}

void UButtonWindow::EnableRightMouseClick(std::optional<bool> bEnable)
{
	bEnableRightMouseClick() = !bEnable || *bEnable;
}

// The original's (XButtonWindow::PressButton 0x10007b20): a press from the
// keyboard or a script -- shown pressed for the activate delay, the click
// sound, then the activation.
void UButtonWindow::PressButton(EInputKey key)
{
	bButtonPressed() = true;
	ActivateTimeLeft = GetTickOffset() + activateDelay();
	activateTimer() = 1;
	PlaySound(clickSound(), {}, {}, {}, {});
	lastInputKey() = key;
	ActivateButton(key);
}

void UButtonWindow::SetActivateDelay(std::optional<float> newDelay)
{
	activateDelay() = newDelay.value_or(0.3f);
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

// The original's (XButtonWindow::MouseMoved 0x10007e30): while the mouse
// holds the button down, it shows pressed only with the pointer over it.
void UButtonWindow::MouseMoved(float newX, float newY)
{
	UTextWindow::MouseMoved(newX, newY);
	if (bMousePressed())
		bButtonPressed() = bIsVisible() && IsPointInWindow(newX, newY);
}

// The original's (XButtonWindow::MouseButtonPressed 0x10007bd0): the script
// first; then the left button, or the right where right clicks are on,
// presses the button: one that repeats activates at once, with the click
// sound, and again after its initial delay while held; any other plays its
// press sound and activates on the release.
bool UButtonWindow::MouseButtonPressed(float pointX, float pointY, EInputKey button, int numClicks)
{
	bool handled = UTextWindow::MouseButtonPressed(pointX, pointY, button, numClicks);
	if (button != IK_LeftMouse && !(button == IK_RightMouse && bEnableRightMouseClick()))
		return handled;

	bButtonPressed() = true;
	bMousePressed() = true;
	if (bAutoRepeat())
	{
		repeatTime() = GetTickOffset() + initialDelay();
		PlaySound(clickSound(), {}, {}, {}, {});
		lastInputKey() = button;
		ActivateButton(button);
	}
	else
	{
		PlaySound(pressSound(), {}, {}, {}, {});
	}
	return true;
}

// The original's (XButtonWindow::MouseButtonReleased 0x10007d10): the
// button lets go; released over it, one that does not repeat plays its click
// sound and activates.
bool UButtonWindow::MouseButtonReleased(float pointX, float pointY, EInputKey button, int numClicks)
{
	bool handled = UTextWindow::MouseButtonReleased(pointX, pointY, button, numClicks);
	if (button != IK_LeftMouse && !(button == IK_RightMouse && bEnableRightMouseClick()))
		return handled;

	bButtonPressed() = false;
	bMousePressed() = false;
	repeatTime() = 0.0f;
	lastInputKey() = IK_None;
	if (IsPointInWindow(pointX, pointY) && !bAutoRepeat())
	{
		PlaySound(clickSound(), {}, {}, {}, {});
		ActivateButton(button);
	}
	return true;
}

// The original's (XButtonWindow::SensitivityChanged 0x10007f10): made
// insensitive while the mouse holds it, it lets go.
void UButtonWindow::SensitivityChanged(bool bNewSensitivity)
{
	UTextWindow::SensitivityChanged(bNewSensitivity);
	if (!bNewSensitivity && bMousePressed())
	{
		bButtonPressed() = false;
		bMousePressed() = false;
	}
}

// The original's (XButtonWindow::Tick 0x10008190, Timer 0x100080e0): a
// pressed show ends after the activate delay; held, a repeating button
// activates again every repeatRate while the pointer is over it.
void UButtonWindow::Tick(float timeElapsed)
{
	UTextWindow::Tick(timeElapsed);

	if (activateTimer())
	{
		ActivateTimeLeft -= timeElapsed;
		if (ActivateTimeLeft <= 0.0f)
		{
			bButtonPressed() = false;
			activateTimer() = 0;
		}
	}

	if (bMousePressed() && bAutoRepeat())
	{
		repeatTime() -= timeElapsed;
		if (repeatTime() < 0.0f)
		{
			repeatTime() += repeatRate();
			if (repeatTime() < 0.0f)
				repeatTime() = repeatRate();
			float mouseX = 0.0f, mouseY = 0.0f;
			GetCursorPos(mouseX, mouseY);
			if (IsPointInWindow(mouseX, mouseY))
			{
				PlaySound(clickSound(), {}, {}, {}, {});
				ActivateButton((EInputKey)lastInputKey());
			}
		}
	}
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
