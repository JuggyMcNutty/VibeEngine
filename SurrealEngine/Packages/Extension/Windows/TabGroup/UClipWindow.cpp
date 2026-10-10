#include "Precomp.h"
#include "UClipWindow.h"
#include <cmath>

// The original's clip window (Extension.dll XClipWindow; dx-reverse-info/
// extension-dll.md, Scales and scrolling) shows a window larger than itself,
// moved by whole units: its child's granularity (a list's row, a text's line)
// while it snaps to units, else pixels. It keeps the child's position in units
// (childH, childV), its own size and the child's in units, and tells its
// parents of them: ClipAttributesChanged when the sizes change,
// ClipPositionChanged when the position does.

// The original's XClipWindow::Init (0x1000b320): no preferred size in units,
// at the top left, snapping to units and filling the window.
void UClipWindow::InitDefaults()
{
	UTabGroupWindow::InitDefaults();
	prefHUnits() = -1;
	prefVUnits() = -1;
	hMult() = 1.0f;
	vMult() = 1.0f;
	childH() = 0;
	childV() = 0;
	bForceChildWidth() = false;
	bForceChildHeight() = false;
	bSnapToUnits() = true;
	bFillWindow() = true;
	areaHSize() = 0;
	areaVSize() = 0;
	childHSize() = 0;
	childVSize() = 0;
}

// The topmost child that shows (XClipWindow::GetChild 0x1000bb40).
UWindow* UClipWindow::GetChildWindow()
{
	for (UWindow* child = lastChild(); child; child = child->prevSibling())
	{
		if (child->bIsVisible())
			return child;
	}
	return nullptr;
}

UObject* UClipWindow::GetChild()
{
	return GetChildWindow();
}

// A unit is the child's granularity while the clip snaps to units, else a
// pixel (XClipWindow::GetChildUnits 0x1000bd50).
void UClipWindow::GetChildUnits(UWindow* child, float& hUnit, float& vUnit)
{
	hUnit = 1.0f;
	vUnit = 1.0f;
	if (bSnapToUnits() && child)
		child->QueryGranularity(hUnit, vUnit);
}

// The child kept over the clip window where it can be (ClampChildPosition
// 0x1000bdf0).
void UClipWindow::ClampChildPosition(float& x, float& y, float childWidth, float childHeight)
{
	x = std::min(std::max(x, Width() - childWidth), 0.0f);
	y = std::min(std::max(y, Height() - childHeight), 0.0f);
}

// The original's (XClipWindow::GetChildPreferredSize 0x1000be60): the child
// as wide (or high) as the space given on an axis where its size is forced,
// at its preferred size for the rest; its position in units held with it.
void UClipWindow::GetChildPreferredSize(UWindow* child, bool bWidthSpecified, float width, bool bHeightSpecified, float height)
{
	if (!child)
		return;
	bool forceWidth = bWidthSpecified && bForceChildWidth();
	bool forceHeight = bHeightSpecified && bForceChildHeight();
	if (forceWidth)
	{
		child->holdX() = 0.0f;
		child->holdWidth() = width;
		if (forceHeight)
		{
			child->holdY() = 0.0f;
			child->holdHeight() = height;
		}
		else
		{
			child->holdY() = (float)childV();
			child->holdHeight() = child->QueryPreferredHeight(width);
		}
	}
	else
	{
		child->holdX() = (float)childH();
		if (forceHeight)
		{
			child->holdY() = 0.0f;
			child->holdWidth() = child->QueryPreferredWidth(height);
			child->holdHeight() = height;
		}
		else
		{
			child->holdY() = (float)childV();
			child->QueryPreferredSize(child->holdWidth(), child->holdHeight());
		}
	}
}

// The original's (XClipWindow::GetClipPreferredSize 0x1000bfa0): a
// preferred size in units counts as given; on an axis not given, the
// child's size.
void UClipWindow::GetClipPreferredSize(UWindow* child, bool bWidthSpecified, float& width, bool bHeightSpecified, float& height)
{
	if (!child)
	{
		UTabGroupWindow::ParentRequestedPreferredSize(bWidthSpecified, width, bHeightSpecified, height);
		return;
	}
	float hUnit = 1.0f, vUnit = 1.0f;
	GetChildUnits(child, hUnit, vUnit);
	if (prefHUnits() >= 0 && !bWidthSpecified)
	{
		bWidthSpecified = true;
		width = prefHUnits() * hUnit;
	}
	if (prefVUnits() >= 0 && !bHeightSpecified)
	{
		bHeightSpecified = true;
		height = prefVUnits() * vUnit;
	}
	GetChildPreferredSize(child, bWidthSpecified, width, bHeightSpecified, height);
	if (!bWidthSpecified)
		width = child->holdWidth();
	if (!bHeightSpecified)
		height = child->holdHeight();
}

void UClipWindow::ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight)
{
	GetClipPreferredSize(GetChildWindow(), bWidthSpecified, preferredWidth, bHeightSpecified, preferredHeight);
}

// The original's (XClipWindow::QueryClipPreferredSize 0x1000bbd0), which a
// scroll area asks: the clip window's size for the space given (its own set
// size counting as given, else 10), and whether the child is wider or higher
// than that -- whether the axis needs to scroll.
void UClipWindow::QueryClipPreferredSize(bool bWidthSpecified, float width, float& outWidth, bool& bNeedHScroll, bool bHeightSpecified, float height, float& outHeight, bool& bNeedVScroll)
{
	if (!bWidthSpecified && FixedWidth)
	{
		bWidthSpecified = true;
		width = hardcodedWidth();
	}
	if (!bHeightSpecified && FixedHeight)
	{
		bHeightSpecified = true;
		height = hardcodedHeight();
	}
	float w = bWidthSpecified ? width : 10.0f;
	float h = bHeightSpecified ? height : 10.0f;
	UWindow* child = GetChildWindow();
	GetClipPreferredSize(child, bWidthSpecified, w, bHeightSpecified, h);
	bNeedHScroll = child && w < child->holdWidth();
	bNeedVScroll = child && h < child->holdHeight();
	outWidth = bWidthSpecified ? width : w;
	outHeight = bHeightSpecified ? height : h;
}

// The original's (XClipWindow::ReconfigureChild 0x1000c0e0): the child
// placed at its position in units, filling the clip window if smaller, and
// kept over it; the clip's and the child's sizes in units -- what of the
// child shows, and all of it -- sent up when they change.
void UClipWindow::ReconfigureChild(UWindow* child, int col, int row, float width, float height)
{
	GetChildUnits(child, hMult(), vMult());
	float x = -hMult() * (float)col;
	float y = -vMult() * (float)row;
	int newChildHSize = (int)std::ceil(width / hMult());
	int newChildVSize = (int)std::ceil(height / vMult());
	if (bFillWindow())
	{
		width = std::max(width, Width());
		height = std::max(height, Height());
	}
	int newAreaHSize = newChildHSize - (int)std::ceil((width - Width()) / hMult());
	int newAreaVSize = newChildVSize - (int)std::ceil((height - Height()) / vMult());
	ClampChildPosition(x, y, width, height);
	bool changed = childHSize() != newChildHSize || childVSize() != newChildVSize || areaHSize() != newAreaHSize || areaVSize() != newAreaVSize;
	child->ConfigureChild(x, y, width, height);
	childHSize() = newChildHSize;
	childVSize() = newChildVSize;
	areaHSize() = newAreaHSize;
	areaVSize() = newAreaVSize;
	if (changed)
	{
		for (UWindow* cur = this; cur; cur = cur->parentOwner())
		{
			if (cur->ClipAttributesChanged(this, areaHSize(), areaVSize(), childHSize(), childVSize()))
				break;
		}
	}
}

// The original's (XClipWindow::ConfigurationChanged 0x1000c560): the child
// laid out over the clip window; any other shown child to nothing.
void UClipWindow::ConfigurationChanged()
{
	UWindow* child = GetChildWindow();
	for (UWindow* cur = lastChild(); cur; cur = cur->prevSibling())
	{
		if (!cur->bIsVisible())
			continue;
		if (cur == child)
		{
			GetChildPreferredSize(cur, true, Width(), true, Height());
			ReconfigureChild(cur, (int)(int64_t)cur->holdX(), (int)(int64_t)cur->holdY(), cur->holdWidth(), cur->holdHeight());
		}
		else
		{
			cur->ConfigureChild(0.0f, 0.0f, 0.0f, 0.0f);
		}
	}
}

void UClipWindow::ChildRequestedVisibilityChange(UWindow* childWin, bool bNewVisibility)
{
	childWin->SetChildVisibility(bNewVisibility);
	AskParentForReconfigure();
}

// The original's (XClipWindow::ChildRequestedShowArea 0x1000c360): the child
// moved by the least whole units that bring the area into the clip window --
// its start when it does not fit.
void UClipWindow::ChildRequestedShowArea(UWindow* child, float showX, float showY, float showWidth, float showHeight)
{
	GetChildUnits(child, hMult(), vMult());
	if (hMult() <= 0.0f || vMult() <= 0.0f)
		return;

	float x = showX + 0.005f;
	float y = showY + 0.005f;
	float w = showWidth - 0.01f;
	float h = showHeight - 0.01f;
	int col = (int)std::floor(x / hMult());
	int cols = (int)std::ceil((x + w) / hMult()) - col;
	int row = (int)std::floor(y / vMult());
	int rows = (int)std::ceil((y + h) / vMult()) - row;

	auto show = [](int start, int count, int current, int area)
		{
			int offset = start - current;
			if (area < count)
			{
				if (offset < area - count)
					return start + count - area;
				if (offset <= 0)
					return current;
				return start;
			}
			if (offset > area - count)
				return start + count - area;
			if (offset >= 0)
				return current;
			return start;
		};
	SetChildPosition(show(col, cols, childH(), areaHSize()), show(row, rows, childV(), areaVSize()));
}

// The original's (XClipWindow::SetChildPosition 0x1000b470): the child moved
// to the position in units -- not along an axis where its size is forced --
// kept over the clip window, and the position sent up.
void UClipWindow::SetChildPosition(int newX, int newY)
{
	if (childH() == newX && childV() == newY)
		return;
	childH() = newX;
	childV() = newY;
	UWindow* child = GetChildWindow();
	if (!child)
		return;
	float x = bForceChildWidth() ? 0.0f : -hMult() * (float)childH();
	float y = bForceChildHeight() ? 0.0f : -vMult() * (float)childV();
	ClampChildPosition(x, y, child->Width(), child->Height());
	child->ConfigureChild(x, y, child->Width(), child->Height());
	for (UWindow* cur = this; cur; cur = cur->parentOwner())
	{
		if (cur->ClipPositionChanged(this, childH(), childV()))
			break;
	}
}

void UClipWindow::GetChildPosition(int& pNewX, int& pNewY)
{
	pNewX = childH();
	pNewY = childV();
}

// The original's (XClipWindow::ForceChildSize 0x1000b9f0). Left out, both on.
void UClipWindow::ForceChildSize(std::optional<bool> bNewForceChildWidth, std::optional<bool> bNewForceChildHeight)
{
	bool forceWidth = bNewForceChildWidth.value_or(true);
	bool forceHeight = bNewForceChildHeight.value_or(true);
	if (bForceChildWidth() != forceWidth || bForceChildHeight() != forceHeight)
	{
		bForceChildWidth() = forceWidth;
		bForceChildHeight() = forceHeight;
		AskParentForReconfigure();
	}
}

void UClipWindow::EnableSnapToUnits(std::optional<bool> bNewSnapToUnits)
{
	bool snap = bNewSnapToUnits.value_or(true);
	if (bSnapToUnits() != snap)
	{
		bSnapToUnits() = snap;
		AskParentForReconfigure();
	}
}

void UClipWindow::GetUnitSize(int& pAreaHSize, int& pAreaVSize, int& pChildHSize, int& pChildVSize)
{
	pAreaHSize = areaHSize();
	pAreaVSize = areaVSize();
	pChildHSize = childHSize();
	pChildVSize = childVSize();
}

// The preferred size in units (XClipWindow::SetUnitSize 0x1000b610 and the
// rest): at least one unit; reset, none.
void UClipWindow::SetUnitSize(int hUnits, int vUnits)
{
	hUnits = std::max(hUnits, 1);
	vUnits = std::max(vUnits, 1);
	if (prefHUnits() != hUnits || prefVUnits() != vUnits)
	{
		prefHUnits() = hUnits;
		prefVUnits() = vUnits;
		AskParentForReconfigure();
	}
}

void UClipWindow::SetUnitWidth(int hUnits)
{
	hUnits = std::max(hUnits, 1);
	if (prefHUnits() != hUnits)
	{
		prefHUnits() = hUnits;
		AskParentForReconfigure();
	}
}

void UClipWindow::SetUnitHeight(int vUnits)
{
	vUnits = std::max(vUnits, 1);
	if (prefVUnits() != vUnits)
	{
		prefVUnits() = vUnits;
		AskParentForReconfigure();
	}
}

void UClipWindow::ResetUnitSize()
{
	if (prefHUnits() >= 0 || prefVUnits() >= 0)
	{
		prefHUnits() = -1;
		prefVUnits() = -1;
		AskParentForReconfigure();
	}
}

void UClipWindow::ResetUnitWidth()
{
	if (prefHUnits() >= 0)
	{
		prefHUnits() = -1;
		AskParentForReconfigure();
	}
}

// The original's (XClipWindow::ResetUnitHeight 0x1000b910) resets the width
// in units, not the height; no script calls it.
void UClipWindow::ResetUnitHeight()
{
	if (prefVUnits() >= 0)
	{
		prefHUnits() = -1;
		AskParentForReconfigure();
	}
}
