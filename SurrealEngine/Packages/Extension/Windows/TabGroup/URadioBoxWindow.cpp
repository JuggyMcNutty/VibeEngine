#include "Precomp.h"
#include "URadioBoxWindow.h"
#include "Packages/Extension/Windows/Text/UToggleWindow.h"

// The original's radio box (Extension.dll XRadioBoxWindow; dx-reverse-info/
// extension-dll.md, Buttons): one of the toggles it holds is on at a time; its
// children are sized to it.

// The original's XRadioBoxWindow::Init (0x10038660): one on at a time,
// none yet.
void URadioBoxWindow::InitDefaults()
{
	UTabGroupWindow::InitDefaults();
	toggleButtons.clear();
	bOneCheck() = true;
	currentSelection() = nullptr;
}

UObject* URadioBoxWindow::GetEnabledToggle()
{
	return currentSelection();
}

// The largest of its shown children's preferred sizes
// (XRadioBoxWindow::ParentRequestedPreferredSize 0x100388e0).
void URadioBoxWindow::ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight)
{
	float largestWidth = 0.0f, largestHeight = 0.0f;
	for (UWindow* child = lastChild(); child; child = child->prevSibling())
	{
		if (!child->bIsVisible())
			continue;
		float width = 0.0f, height = 0.0f;
		child->QueryPreferredSize(bWidthSpecified, preferredWidth, &width, bHeightSpecified, preferredHeight, &height);
		largestWidth = std::max(largestWidth, width);
		largestHeight = std::max(largestHeight, height);
	}
	preferredWidth = largestWidth;
	preferredHeight = largestHeight;
}

// Its shown children sized to it (XRadioBoxWindow::ConfigurationChanged
// 0x10038840).
void URadioBoxWindow::ConfigurationChanged()
{
	for (UWindow* child = lastChild(); child; child = child->prevSibling())
	{
		if (child->bIsVisible())
			child->ConfigureChild(0.0f, 0.0f, Width(), Height());
	}
}

void URadioBoxWindow::ChildRequestedVisibilityChange(UWindow* childWin, bool bNewVisibility)
{
	childWin->SetChildVisibility(bNewVisibility);
	if (!bNewVisibility)
		AskParentForReconfigure();
}

// The original's (XRadioBoxWindow::ToggleChanged 0x10038a60): a toggle of
// its own turned on becomes the one on, the last one turned off; the one on
// cannot be turned off while one must be on, and another's turning off is
// taken without a word. The rest goes to the script.
bool URadioBoxWindow::ToggleChanged(UWindow* button, bool bNewToggle)
{
	UToggleWindow* toggle = UObject::TryCast<UToggleWindow>(button);
	if (toggle)
	{
		if (bNewToggle)
		{
			if (std::find(toggleButtons.begin(), toggleButtons.end(), toggle) != toggleButtons.end())
			{
				UToggleWindow* previous = currentSelection();
				if (previous == toggle)
					return true;
				currentSelection() = toggle;
				if (previous)
					previous->SetToggle(false);
			}
		}
		else if (bOneCheck())
		{
			if (currentSelection() == toggle)
				toggle->SetToggle(true);
			return true;
		}
		else
		{
			if (currentSelection() != toggle)
				return true;
			currentSelection() = nullptr;
		}
	}
	return UTabGroupWindow::ToggleChanged(button, bNewToggle);
}

// The original's (XRadioBoxWindow::DescendantAdded 0x10038b90): the script
// first; a toggle whose nearest radio box is this one is held.
void URadioBoxWindow::DescendantAdded(UWindow* descendant)
{
	UTabGroupWindow::DescendantAdded(descendant);
	UToggleWindow* toggle = UObject::TryCast<UToggleWindow>(descendant);
	if (!toggle)
		return;
	UWindow* box = toggle;
	while (box && !UObject::TryCast<URadioBoxWindow>(box))
		box = box->parentOwner();
	if (box == this)
		toggleButtons.push_back(toggle);
}

// The original's (XRadioBoxWindow::DescendantRemoved 0x10038cb0): a toggle
// that goes is let go; then the script.
void URadioBoxWindow::DescendantRemoved(UWindow* descendant)
{
	auto it = std::find(toggleButtons.begin(), toggleButtons.end(), descendant);
	if (it != toggleButtons.end())
	{
		if (currentSelection() == *it)
			currentSelection() = nullptr;
		toggleButtons.erase(it);
	}
	UTabGroupWindow::DescendantRemoved(descendant);
}
