#include "Precomp.h"
#include "UToggleWindow.h"
#include "Packages/Engine/Resources/USound.h"

// The original's toggle (Extension.dll XToggleWindow) keeps its state in the
// button's pressed flag, so a toggle that is on draws as a pressed button.

// The original's XToggleWindow::Init (0x10049820): no sounds.
void UToggleWindow::InitDefaults()
{
	UButtonWindow::InitDefaults();
	enableSound() = nullptr;
	disableSound() = nullptr;
}

// The original's (XToggleWindow::ChangeToggle 0x10049910): ToggleChanged
// with the state, up the parents until one takes it.
void UToggleWindow::ChangeToggle()
{
	for (UWindow* cur = this; cur; cur = cur->parentOwner())
	{
		if (cur->ToggleChanged(this, bButtonPressed()))
			break;
	}
}

bool UToggleWindow::GetToggle()
{
	return bButtonPressed();
}

void UToggleWindow::SetToggle(bool bNewToggle)
{
	if (bButtonPressed() != bNewToggle)
	{
		bButtonPressed() = bNewToggle;
		ChangeToggle();
	}
}

void UToggleWindow::SetToggleSounds(std::optional<UObject*> newEnableSound, std::optional<UObject*> newDisableSound)
{
	enableSound() = UObject::Cast<USound>(newEnableSound.value_or(nullptr));
	disableSound() = UObject::Cast<USound>(newDisableSound.value_or(nullptr));
}

// The original's (XToggleWindow::PressButton 0x10049c30): from the keyboard
// or a script, the toggle flips, then plays the sound of the state it took.
void UToggleWindow::PressButton(EInputKey key)
{
	SetToggle(!bButtonPressed());
	PlaySound(bButtonPressed() ? enableSound() : disableSound(), {}, {}, {}, {});
}

// The original's (XToggleWindow::MouseButtonPressed 0x10049a80): the left
// button is taken and nothing shows until the release.
bool UToggleWindow::MouseButtonPressed(float pointX, float pointY, EInputKey button, int numClicks)
{
	bool handled = UWindow::MouseButtonPressed(pointX, pointY, button, numClicks);
	return button == IK_LeftMouse ? true : handled;
}

// The original's (XToggleWindow::MouseButtonReleased 0x10049b30): the left
// button let go over the toggle flips it, after the sound of the state it
// leaves -- the enable sound when it turns off, as the original has it.
bool UToggleWindow::MouseButtonReleased(float pointX, float pointY, EInputKey button, int numClicks)
{
	bool handled = UWindow::MouseButtonReleased(pointX, pointY, button, numClicks);
	if (button != IK_LeftMouse)
		return handled;

	if (IsPointInWindow(pointX, pointY))
	{
		PlaySound(bButtonPressed() ? enableSound() : disableSound(), {}, {}, {}, {});
		SetToggle(!bButtonPressed());
	}
	return true;
}
