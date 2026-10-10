#include "Precomp.h"
#include "UTabGroupWindow.h"

// The original's tab group (Extension.dll XTabGroupWindow; dx-reverse-info/
// extension-dll.md, Tiles and tab groups) -- every modal window is one --
// sizes itself to its children unless told not to, or sizes them to itself.

// The original's XTabGroupWindow::Init (0x10044d90): sized to its children.
void UTabGroupWindow::InitDefaults()
{
	UWindow::InitDefaults();
	bSizeParentToChildren() = true;
	bSizeChildrenToParent() = false;
	tabGroupIndex() = -1;
	firstAbsX() = 0.0f;
	firstAbsY() = 0.0f;
}

// The original's (XTabGroupWindow::ParentRequestedPreferredSize 0x10045700):
// sized to its children, the largest of its shown children's preferred
// sizes -- each with its place in it, unless they are sized to it; else the
// script's say. (A menu's own window turns this off for its script's.)
void UTabGroupWindow::ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight)
{
	if (!bSizeParentToChildren() && !bSizeChildrenToParent())
	{
		UWindow::ParentRequestedPreferredSize(bWidthSpecified, preferredWidth, bHeightSpecified, preferredHeight);
		return;
	}
	float largestWidth = 0.0f, largestHeight = 0.0f;
	for (UWindow* child = lastChild(); child; child = child->prevSibling())
	{
		if (!child->bIsVisible())
			continue;
		float width = 0.0f, height = 0.0f;
		child->QueryPreferredSize(bWidthSpecified, preferredWidth, &width, bHeightSpecified, preferredHeight, &height);
		if (!bSizeChildrenToParent())
		{
			// The original's place is its alignment margins; a window the
			// fork's SetPos placed holds it in its position.
			width += child->X() + child->hMargin0();
			height += child->Y() + child->vMargin0();
			if ((EHAlign)child->winHAlign() == EHAlign::Full)
				width += child->hMargin1();
			if ((EVAlign)child->winVAlign() == EVAlign::Full)
				height += child->vMargin1();
		}
		largestWidth = std::max(largestWidth, width);
		largestHeight = std::max(largestHeight, height);
	}
	preferredWidth = largestWidth;
	preferredHeight = largestHeight;
}

// The original's (XTabGroupWindow::ConfigurationChanged 0x10045640): its
// shown children sized to it, else the script's layout.
void UTabGroupWindow::ConfigurationChanged()
{
	if (!bSizeChildrenToParent())
	{
		UWindow::ConfigurationChanged();
		return;
	}
	for (UWindow* child = lastChild(); child; child = child->prevSibling())
	{
		if (child->bIsVisible())
			child->ConfigureChild(0.0f, 0.0f, Width(), Height());
	}
}

// The original's (XTabGroupWindow::ChildRequestedVisibilityChange
// 0x10045870): a child that hides lays the group out again.
void UTabGroupWindow::ChildRequestedVisibilityChange(UWindow* childWin, bool bNewVisibility)
{
	childWin->SetChildVisibility(bNewVisibility);
	if (!bNewVisibility)
		AskParentForReconfigure();
}
