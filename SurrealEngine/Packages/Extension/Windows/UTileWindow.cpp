
#include "Precomp.h"
#include "UTileWindow.h"

// The original's tile (Extension.dll XTileWindow; dx-reverse-info/
// extension-dll.md, Tiles and tab groups) lines its shown children up along
// its orientation, from the side its directions say, wrapping into further
// rows (columns, down a vertical tile) where it wraps; each row as thick as
// its thickest child, and each child placed across its row by the child
// alignment -- Full making it as thick as the row.

// The original's XTileWindow::Init (0x10048360): left to right, wrapping and
// filling the parent, equal heights, margins of 3, 1 between children and
// between rows, children made as thick as their row.
void UTileWindow::InitDefaults()
{
	UWindow::InitDefaults();
	hMargin() = 3.0f;
	vMargin() = 3.0f;
	minorSpacing() = 1.0f;
	majorSpacing() = 1.0f;
	bWrap() = true;
	bFillParent() = true;
	bEqualWidth() = false;
	bEqualHeight() = true;
	orientation() = (uint8_t)EOrientation::Horizontal;
	hDirection() = (uint8_t)EHDirection::LeftToRight;
	vDirection() = (uint8_t)EVDirection::TopToBottom;
	hChildAlign() = (uint8_t)EHAlign::Full;
	vChildAlign() = (uint8_t)EVAlign::Full;
}

// The setters (XTileWindow::SetMargins 0x100150f0 and the rest) each lay the
// tile out again.
void UTileWindow::EnableWrapping(bool bWrapOn)
{
	bWrap() = bWrapOn;
	AskParentForReconfigure();
}

void UTileWindow::FillParent(bool FillParent)
{
	bFillParent() = FillParent;
	AskParentForReconfigure();
}

void UTileWindow::MakeHeightsEqual(bool bEqual)
{
	bEqualHeight() = bEqual;
	AskParentForReconfigure();
}

void UTileWindow::MakeWidthsEqual(bool bEqual)
{
	bEqualWidth() = bEqual;
	AskParentForReconfigure();
}

void UTileWindow::SetChildAlignments(uint8_t newHAlign, uint8_t newVAlign)
{
	hChildAlign() = newHAlign;
	vChildAlign() = newVAlign;
	AskParentForReconfigure();
}

void UTileWindow::SetDirections(uint8_t newHDir, uint8_t newVDir)
{
	hDirection() = newHDir;
	vDirection() = newVDir;
	AskParentForReconfigure();
}

void UTileWindow::SetMajorSpacing(float newSpacing)
{
	majorSpacing() = newSpacing;
	AskParentForReconfigure();
}

void UTileWindow::SetMargins(float newHMargin, float newVMargin)
{
	hMargin() = newHMargin;
	vMargin() = newVMargin;
	AskParentForReconfigure();
}

void UTileWindow::SetMinorSpacing(float newSpacing)
{
	minorSpacing() = newSpacing;
	AskParentForReconfigure();
}

void UTileWindow::SetOrientation(uint8_t newOrientation)
{
	orientation() = newOrientation;
	AskParentForReconfigure();
}

// The original's (XTileWindow::SetOrder 0x10048560): an order is an
// orientation, the two directions and whether it wraps.
void UTileWindow::SetOrder(EOrder newOrder)
{
	struct OrderSettings
	{
		EOrientation Orientation;
		EHDirection HDirection;
		EVDirection VDirection;
		bool Wrap;
	};
	const EHDirection ltr = EHDirection::LeftToRight, rtl = EHDirection::RightToLeft;
	const EVDirection ttb = EVDirection::TopToBottom, btt = EVDirection::BottomToTop;
	const EOrientation h = EOrientation::Horizontal, v = EOrientation::Vertical;
	OrderSettings settings = { h, ltr, ttb, true };
	switch (newOrder)
	{
	case EOrder::Right: settings = { h, ltr, ttb, false }; break;
	case EOrder::Left: settings = { h, rtl, ttb, false }; break;
	case EOrder::Down: settings = { v, ltr, ttb, false }; break;
	case EOrder::Up: settings = { v, ltr, btt, false }; break;
	case EOrder::RightThenUp: settings = { h, ltr, btt, true }; break;
	case EOrder::LeftThenDown: settings = { h, rtl, ttb, true }; break;
	case EOrder::LeftThenUp: settings = { h, rtl, btt, true }; break;
	case EOrder::DownThenRight: settings = { v, ltr, ttb, true }; break;
	case EOrder::DownThenLeft: settings = { v, rtl, ttb, true }; break;
	case EOrder::UpThenRight: settings = { v, ltr, btt, true }; break;
	case EOrder::UpThenLeft: settings = { v, rtl, btt, true }; break;
	default: break;
	}
	orientation() = (uint8_t)settings.Orientation;
	hDirection() = (uint8_t)settings.HDirection;
	vDirection() = (uint8_t)settings.VDirection;
	bWrap() = settings.Wrap;
	AskParentForReconfigure();
}

// The original's (XTileWindow::ComputeChildSizes 0x10048a90): each shown
// child at its preferred size -- one that fills its parent and does not
// wrap gives each child the space across less the margins -- made as wide
// and high as the widest and highest where those are equal. Then the
// children go into rows along the orientation, a new row where the next
// would pass the space given less the margins (where the tile wraps); with
// a size given each way, each child is placed in its row (holdX..), else
// the size the rows take, plus the margins, is the tile's.
void UTileWindow::ComputeChildSizes(bool bWidthSpecified, float& width, bool bHeightSpecified, float& height)
{
	bool vertical = (EOrientation)orientation() == EOrientation::Vertical;
	bool fillWidth = false, fillHeight = false;
	float fillSize = 0.0f;
	if (!bWrap() && bFillParent())
	{
		if (vertical && bWidthSpecified)
		{
			fillWidth = true;
			fillSize = std::max(width - (hMargin() + hMargin()), 0.0f);
		}
		else if (!vertical && bHeightSpecified)
		{
			fillHeight = true;
			fillSize = std::max(height - (vMargin() + vMargin()), 0.0f);
		}
	}

	float widest = 0.0f, highest = 0.0f;
	for (UWindow* child = lastChild(); child; child = child->prevSibling())
	{
		if (!child->bIsVisible())
		{
			child->holdWidth() = 0.0f;
			child->holdHeight() = 0.0f;
			child->holdX() = 0.0f;
			child->holdY() = 0.0f;
			continue;
		}
		if (fillWidth)
		{
			child->holdWidth() = fillSize;
			child->holdHeight() = child->QueryPreferredHeight(fillSize);
		}
		else if (fillHeight)
		{
			child->holdHeight() = fillSize;
			child->holdWidth() = child->QueryPreferredWidth(fillSize);
		}
		else
		{
			child->QueryPreferredSize(child->holdWidth(), child->holdHeight());
		}
		if (bEqualWidth())
			widest = std::max(widest, child->holdWidth());
		if (bEqualHeight())
			highest = std::max(highest, child->holdHeight());
	}
	if (bEqualWidth() || bEqualHeight())
	{
		for (UWindow* child = firstChild(); child; child = child->nextSibling())
		{
			if (!child->bIsVisible())
				continue;
			if (bEqualWidth())
				child->holdWidth() = widest;
			if (bEqualHeight())
				child->holdHeight() = highest;
		}
	}

	float limit = 1000000.0f;
	if (bWrap())
	{
		if (bWidthSpecified && !vertical)
			limit = width - (hMargin() + hMargin());
		if (bHeightSpecified && vertical)
			limit = height - (vMargin() + vMargin());
	}

	Rows.clear();
	Row row;
	row.Thickness = (int)(int64_t)fillSize;
	for (UWindow* child = firstChild(); child; child = child->nextSibling())
	{
		if (!child->bIsVisible())
			continue;
		float along = vertical ? child->holdHeight() : child->holdWidth();
		float across = vertical ? child->holdWidth() : child->holdHeight();
		if (row.Count > 0 && row.Length + along > limit)
		{
			row.Length = (int)(int64_t)(row.Length - minorSpacing());
			Rows.push_back(row);
			row = Row();
			row.Thickness = (int)(int64_t)fillSize;
		}
		row.Length = (int)(int64_t)(row.Length + minorSpacing() + along);
		if (row.Thickness < across)
			row.Thickness = (int)(int64_t)across;
		row.Count++;
	}
	if (row.Count > 0)
	{
		row.Length = (int)(int64_t)(row.Length - minorSpacing());
		Rows.push_back(row);
	}

	if (bWidthSpecified && bHeightSpecified)
	{
		size_t rowIndex = 0;
		float x = hMargin();
		float y = vMargin();
		for (UWindow* child = firstChild(); child && rowIndex < Rows.size(); child = child->nextSibling())
		{
			if (!child->bIsVisible())
				continue;
			Row& current = Rows[rowIndex];
			if (!vertical)
			{
				child->holdY() = (EVDirection)vDirection() == EVDirection::BottomToTop ? height - (current.Thickness + y) : y;
				child->holdX() = (EHDirection)hDirection() == EHDirection::RightToLeft ? width - (x + child->holdWidth()) : x;
				switch ((EVAlign)vChildAlign())
				{
				case EVAlign::Center: child->holdY() += (float)((int)(int64_t)(current.Thickness - child->holdHeight()) / 2); break;
				case EVAlign::Bottom: child->holdY() += current.Thickness - child->holdHeight(); break;
				case EVAlign::Full: child->holdHeight() = (float)current.Thickness; break;
				default: break;
				}
				x = minorSpacing() + child->holdWidth() + x;
				if (--current.Count < 1)
				{
					rowIndex++;
					x = hMargin();
					y = current.Thickness + majorSpacing() + y;
				}
			}
			else
			{
				child->holdY() = (EVDirection)vDirection() == EVDirection::BottomToTop ? height - (y + child->holdHeight()) : y;
				child->holdX() = (EHDirection)hDirection() == EHDirection::RightToLeft ? width - (current.Thickness + x) : x;
				switch ((EHAlign)hChildAlign())
				{
				case EHAlign::Center: child->holdX() += (float)((int)(int64_t)(current.Thickness - child->holdWidth()) / 2); break;
				case EHAlign::Right: child->holdX() += current.Thickness - child->holdWidth(); break;
				case EHAlign::Full: child->holdWidth() = (float)current.Thickness; break;
				default: break;
				}
				y = minorSpacing() + child->holdHeight() + y;
				if (--current.Count < 1)
				{
					rowIndex++;
					y = vMargin();
					x = current.Thickness + majorSpacing() + x;
				}
			}
		}
		return;
	}

	float totalWidth = 0.0f, totalHeight = 0.0f;
	for (size_t i = 0; i < Rows.size(); i++)
	{
		if (vertical)
		{
			totalHeight = std::max(totalHeight, (float)Rows[i].Length);
			totalWidth += Rows[i].Thickness;
			if (i > 0)
				totalWidth += majorSpacing();
		}
		else
		{
			totalWidth = std::max(totalWidth, (float)Rows[i].Length);
			totalHeight += Rows[i].Thickness;
			if (i > 0)
				totalHeight += majorSpacing();
		}
	}
	if (!bWidthSpecified)
		width = hMargin() + hMargin() + totalWidth;
	if (!bHeightSpecified)
		height = vMargin() + vMargin() + totalHeight;
}

void UTileWindow::ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight)
{
	ComputeChildSizes(bWidthSpecified, preferredWidth, bHeightSpecified, preferredHeight);
}

// The original's (XTileWindow::ConfigurationChanged 0x100489c0): the
// children placed in the tile's own size.
void UTileWindow::ConfigurationChanged()
{
	float width = Width(), height = Height();
	ComputeChildSizes(true, width, true, height);
	for (UWindow* child = firstChild(); child; child = child->nextSibling())
	{
		if (child->bIsVisible())
			child->ConfigureChild(child->holdX(), child->holdY(), child->holdWidth(), child->holdHeight());
	}
}

// A child's request goes on up, the tile laid out again with its parent
// (XTileWindow::ChildRequestedReconfiguration 0x100421d0 takes none).
bool UTileWindow::ChildRequestedReconfiguration(UWindow* childWin)
{
	return false;
}

void UTileWindow::ChildRequestedVisibilityChange(UWindow* childWin, bool bNewVisibility)
{
	childWin->SetChildVisibility(bNewVisibility);
	AskParentForReconfigure();
}

// A child added or removed lays the tile out afresh, its own size asked
// again (the fork's layout keeps windows its parent places by bConfigured).
void UTileWindow::ChildAdded(UWindow* child)
{
	UWindow::ChildAdded(child);
	bConfigured() = false;
}

void UTileWindow::ChildRemoved(UWindow* child)
{
	UWindow::ChildRemoved(child);
	bConfigured() = false;
}

// The original's (XTileWindow::ParentRequestedGranularity 0x10048860): the
// script's say; then with equal widths (or heights), a child and its
// spacing -- the topmost shown child's.
void UTileWindow::ParentRequestedGranularity(float& hGranularity, float& vGranularity)
{
	UWindow::ParentRequestedGranularity(hGranularity, vGranularity);
	UWindow* top = UObject::Cast<UWindow>(GetTopChild(true));
	if (!top)
		return;
	bool vertical = (EOrientation)orientation() == EOrientation::Vertical;
	if (bEqualWidth())
		hGranularity = (vertical ? majorSpacing() : minorSpacing()) + top->Width();
	if (bEqualHeight())
		vGranularity = (vertical ? minorSpacing() : majorSpacing()) + top->Height();
}
