
#include "Precomp.h"
#include "UModalWindow.h"
#include "URootWindow.h"

bool UModalWindow::IsCurrentModal()
{
	URootWindow* root = GetRootWindow();
	if (root)
	{
		for (UWindow* child = root->lastChild(); child; child = child->prevSibling())
		{
			if (child->bIsVisible())
			{
				if (auto modal = UObject::TryCast<UModalWindow>(child))
					return modal == this;
			}
		}
	}
	return false;
}

void UModalWindow::SetMouseFocusMode(uint8_t newFocusMode)
{
	focusMode() = newFocusMode;
}

// A letter's accelerator in either case (the original's table holds both).
static int FoldAccelerator(int key)
{
	return (key >= 'A' && key <= 'Z') ? key + 32 : key;
}

// The window and every parent sensitive.
static bool IsSensitiveChain(UWindow* window)
{
	for (UWindow* cur = window; cur; cur = cur->parentOwner())
	{
		if (!cur->bIsSensitive())
			return false;
	}
	return true;
}

// The first window in the original's table order (XModalWindow::
// SetAcceleratorWindows 0x10036a50): the window, then its shown children
// bottom to top, each in turn; a modal inside and an insensitive window's
// children are passed over. One counts that shows, is sensitive with every
// parent, and can take the focus.
static UWindow* FindAcceleratorWindow(UWindow* window, int foldedKey)
{
	int key = window->acceleratorKey();
	if (key > 0 && key < 0xFF && FoldAccelerator(key) == foldedKey && window->IsShown() && IsSensitiveChain(window) && window->bIsSelectable())
		return window;

	for (UWindow* child = window->firstChild(); child; child = child->nextSibling())
	{
		if (!child->bIsVisible() || UObject::TryCast<UModalWindow>(child) || !IsSensitiveChain(child))
			continue;
		if (UWindow* found = FindAcceleratorWindow(child, foldedKey))
			return found;
	}
	return nullptr;
}

// The original's (XModalWindow::GetAcceleratorWindow 0x100372a0): the window
// whose accelerator the key is, a letter in either case. The fork walks the
// modal each time where the original keeps a table it rebuilds when dirty.
UWindow* UModalWindow::GetAcceleratorWindow(int key)
{
	if (key <= 0 || key >= 0xFF)
		return nullptr;
	bDirtyAccelerators() = false;
	return FindAcceleratorWindow(this, FoldAccelerator(key));
}

// The original's (XModalWindow::KeyPressed 0x10037510): the script's say
// first; a key it leaves, while Alt is down and this modal is on top, goes to
// the window whose accelerator it is (AcceleratorKeyPressed: a button
// presses itself).
bool UModalWindow::KeyPressed(std::string key)
{
	if (UTabGroupWindow::KeyPressed(key))
		return true;
	if (key.empty() || !IsCurrentModal() || !IsKeyDown(IK_Alt))
		return false;
	UWindow* window = GetAcceleratorWindow((unsigned char)key[0]);
	return window && window->AcceleratorKeyPressed(key);
}
