#include "Precomp.h"
#include "UScrollAreaWindow.h"
#include "UScaleManagerWindow.h"
#include "UScaleWindow.h"
#include "TabGroup/UClipWindow.h"
#include "Text/UButtonWindow.h"
#include "Engine.h"
#include "Package/PackageManager.h"
#include "Packages/Core/UClass.h"

// The original's scroll area (Extension.dll XScrollAreaWindow; dx-reverse-
// info/extension-dll.md, Scales and scrolling): a clip window with a scale
// manager beside it on each axis -- arrow, scale, arrow. The clip window tells
// the scales their ranges in its units (ClipAttributesChanged) and its
// position (ClipPositionChanged); a scale's move moves the clip's child
// (ScaleRangeChanged).

// The original's XScrollAreaWindow::Init (0x10042d80): margins and a
// scrollbar distance of 3, scrollbars hidden while not needed; the managers
// made hidden, then the clip window, then each manager's arrow, scale and
// arrow. The scales are scrollbars of one tick spanning one, stretched
// along their manager; the scales, the arrows and the area take no keyboard
// focus -- what the area holds does -- and the arrows repeat while held.
// Only vertical scrolling is on.
void UScrollAreaWindow::InitDefaults()
{
	UWindow::InitDefaults();
	bHideScrollbars() = true;
	bHLastShow() = false;
	bVLastShow() = false;
	marginWidth() = 3.0f;
	marginHeight() = 3.0f;
	scrollbarDistance() = 3.0f;

	UClass* managerClass = engine->packages->FindClass("Extension.ScaleManagerWindow");
	UClass* buttonClass = engine->packages->FindClass("Extension.ButtonWindow");
	UClass* scaleClass = engine->packages->FindClass("Extension.ScaleWindow");
	hScaleMgr() = UObject::Cast<UScaleManagerWindow>(NewChild(managerClass, false));
	vScaleMgr() = UObject::Cast<UScaleManagerWindow>(NewChild(managerClass, false));
	ClipWindow() = UObject::Cast<UClipWindow>(NewChild(engine->packages->FindClass("Extension.ClipWindow"), true));
	LeftButton() = UObject::Cast<UButtonWindow>(hScaleMgr()->NewChild(buttonClass, true));
	hScale() = UObject::Cast<UScaleWindow>(hScaleMgr()->NewChild(scaleClass, true));
	RightButton() = UObject::Cast<UButtonWindow>(hScaleMgr()->NewChild(buttonClass, true));
	UpButton() = UObject::Cast<UButtonWindow>(vScaleMgr()->NewChild(buttonClass, true));
	vScale() = UObject::Cast<UScaleWindow>(vScaleMgr()->NewChild(scaleClass, true));
	DownButton() = UObject::Cast<UButtonWindow>(vScaleMgr()->NewChild(buttonClass, true));

	hScaleMgr()->SetManagerOrientation((uint8_t)EOrientation::Horizontal);
	hScaleMgr()->SetScaleButtons(LeftButton(), RightButton());
	hScaleMgr()->SetScale(hScale());
	hScaleMgr()->StretchScaleField(true);
	vScaleMgr()->SetManagerOrientation((uint8_t)EOrientation::Vertical);
	vScaleMgr()->SetScaleButtons(UpButton(), DownButton());
	vScaleMgr()->SetScale(vScale());
	vScaleMgr()->StretchScaleField(true);

	for (UScaleWindow* scale : { hScale(), vScale() })
	{
		scale->SetScaleOrientation((uint8_t)(scale == hScale() ? EOrientation::Horizontal : EOrientation::Vertical));
		scale->SetSelectability(false);
		scale->SetNumTicks(1);
		scale->SetThumbSpan(1);
		scale->EnableStretchedScale(true);
	}
	for (UButtonWindow* button : { LeftButton(), RightButton(), UpButton(), DownButton() })
	{
		button->SetSelectability(false);
		button->EnableAutoRepeat(true, 0.5f, 0.1f);
	}
	EnableScrolling(false, true);
	SetSelectability(false);
}

// The original's (XScrollAreaWindow::EnableScrolling 0x10043160): each
// axis's manager shown or hidden; the clip window makes its child as wide
// (or high) as itself on an axis that does not scroll. Left out, both on.
void UScrollAreaWindow::EnableScrolling(std::optional<bool> bHScrolling, std::optional<bool> bVScrolling)
{
	bool horizontal = bHScrolling.value_or(true);
	bool vertical = bVScrolling.value_or(true);
	if (hScaleMgr())
		hScaleMgr()->SetVisibility(horizontal);
	if (vScaleMgr())
		vScaleMgr()->SetVisibility(vertical);
	if (ClipWindow())
		ClipWindow()->ForceChildSize(!horizontal, !vertical);
}

// The original's (XScrollAreaWindow::ComputeChildSizes 0x100432d0): the
// clip window within the margins, each shown scrollbar beside it at the
// scrollbar distance. With auto-hide, a scrollbar shows only while the
// clip's child does not fit: the sizes are tried with the bars as they were
// last shown, then each bar is put in or taken out as the fit asks, a try
// for each, and all shown if that does not settle it. A hidden bar's
// manager keeps its place, at no width (or height). The size asked for is
// the clip's and the bars' plus the margins -- across, the original adds the
// margin width, not the height. With the clip window hidden, 10 by 10.
void UScrollAreaWindow::ComputeChildSizes(bool bWidthSpecified, float width, float* outWidth, bool bHeightSpecified, float height, float* outHeight)
{
	UScaleManagerWindow* hManager = hScaleMgr() && hScaleMgr()->bIsVisible() ? hScaleMgr() : nullptr;
	UScaleManagerWindow* vManager = vScaleMgr() && vScaleMgr()->bIsVisible() ? vScaleMgr() : nullptr;
	UClipWindow* clip = ClipWindow();
	float resultWidth = 10.0f, resultHeight = 10.0f;

	if (clip && UObject::Cast<UWindow>(clip)->bIsVisible())
	{
		float hBarHeight = 0.0f, vBarWidth = 0.0f;
		float hRoom = 0.0f, vRoom = 0.0f;
		bool showH = false, showV = false;
		bool autoH = false, autoV = false;
		if (hManager)
		{
			float unused = 0.0f;
			hManager->QueryPreferredSize(unused, hBarHeight);
			hRoom = hBarHeight + scrollbarDistance();
			showH = true;
		}
		if (vManager)
		{
			float unused = 0.0f;
			vManager->QueryPreferredSize(vBarWidth, unused);
			vRoom = vBarWidth + scrollbarDistance();
			showV = true;
		}
		if (bHideScrollbars())
		{
			if (hManager)
			{
				autoH = true;
				showH = bHLastShow();
			}
			if (vManager)
			{
				autoV = true;
				showV = bVLastShow();
			}
		}

		float areaWidth = bWidthSpecified ? width - (marginWidth() + marginWidth()) : 0.0f;
		float areaHeight = bHeightSpecified ? height - (marginHeight() + marginHeight()) : 0.0f;
		float clipWidth = 0.0f, clipHeight = 0.0f;
		bool needH = false, needV = false;
		int tries = 0;
		int maxTries = (autoH ? 2 : 1) * (autoV ? 2 : 1);
		while (true)
		{
			if (++tries > maxTries)
			{
				if (autoH)
					showH = true;
				if (autoV)
					showV = true;
			}
			float hTake = showH && bHeightSpecified ? hRoom : 0.0f;
			float vTake = showV && bWidthSpecified ? vRoom : 0.0f;
			clip->QueryClipPreferredSize(bWidthSpecified, areaWidth - vTake, clipWidth, needH, bHeightSpecified, areaHeight - hTake, clipHeight, needV);
			if (tries > maxTries)
				break;
			if (autoH && needH && !showH)
				showH = true;
			else if (autoV && needV && !showV)
				showV = true;
			else if (autoH && !needH && showH)
				showH = false;
			else if (autoV && !needV && showV)
				showV = false;
			else
				break;
		}
		bHLastShow() = showH;
		bVLastShow() = showV;

		UWindow* clipWin = clip;
		clipWin->holdX() = marginWidth();
		clipWin->holdY() = marginHeight();
		clipWin->holdWidth() = clipWidth;
		clipWin->holdHeight() = clipHeight;
		if (hManager)
		{
			hManager->holdX() = clipWin->holdX();
			hManager->holdY() = scrollbarDistance() + clipWin->holdHeight() + clipWin->holdY();
			hManager->holdWidth() = clipWin->holdWidth();
			hManager->holdHeight() = showH ? hBarHeight : 0.0f;
		}
		if (vManager)
		{
			vManager->holdX() = scrollbarDistance() + clipWin->holdWidth() + clipWin->holdX();
			vManager->holdY() = clipWin->holdY();
			vManager->holdHeight() = clipWin->holdHeight();
			vManager->holdWidth() = showV ? vBarWidth : 0.0f;
		}
		resultWidth = marginWidth() + marginWidth() + clipWin->holdWidth();
		if (showV)
			resultWidth += vRoom;
		resultHeight = marginWidth() + marginWidth() + clipWin->holdHeight();
		if (showH)
			resultHeight += hRoom;
	}
	else
	{
		for (UScaleManagerWindow* manager : { hManager, vManager })
		{
			if (manager)
			{
				manager->holdX() = 0.0f;
				manager->holdY() = 0.0f;
				manager->holdWidth() = 0.0f;
				manager->holdHeight() = 0.0f;
			}
		}
	}

	if (outWidth)
		*outWidth = resultWidth;
	if (outHeight)
		*outHeight = resultHeight;
}

void UScrollAreaWindow::ChildRequestedVisibilityChange(UWindow* childWin, bool bNewVisibility)
{
	childWin->SetChildVisibility(bNewVisibility);
	AskParentForReconfigure();
}

// The original's (XScrollAreaWindow::ConfigurationChanged 0x100437c0).
void UScrollAreaWindow::ConfigurationChanged()
{
	ComputeChildSizes(true, Width(), nullptr, true, Height(), nullptr);
	UWindow* parts[] = { hScaleMgr(), vScaleMgr(), ClipWindow() };
	for (UWindow* part : parts)
	{
		if (part && part->bIsVisible())
			part->ConfigureChild(part->holdX(), part->holdY(), part->holdWidth(), part->holdHeight());
	}
}

void UScrollAreaWindow::ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight)
{
	ComputeChildSizes(bWidthSpecified, preferredWidth, &preferredWidth, bHeightSpecified, preferredHeight, &preferredHeight);
}

// The original's (XScrollAreaWindow::DescendantRemoved 0x10043980): a part
// that goes is let go.
void UScrollAreaWindow::DescendantRemoved(UWindow* descendant)
{
	if (descendant == hScaleMgr())
		hScaleMgr() = nullptr;
	else if (descendant == vScaleMgr())
		vScaleMgr() = nullptr;
	else if (descendant == hScale())
		hScale() = nullptr;
	else if (descendant == vScale())
		vScale() = nullptr;
	else if (descendant == LeftButton())
		LeftButton() = nullptr;
	else if (descendant == RightButton())
		RightButton() = nullptr;
	else if (descendant == UpButton())
		UpButton() = nullptr;
	else if (descendant == DownButton())
		DownButton() = nullptr;
	else if (descendant == ClipWindow())
		ClipWindow() = nullptr;
}

// The original's (XScrollAreaWindow::ScaleRangeChanged 0x10043a50): the
// script first; a scale's first tick is the clip's child position on its
// axis.
bool UScrollAreaWindow::ScaleRangeChanged(UWindow* scale, int fromTick, int toTick, float fromValue, float toValue, bool bFinal)
{
	bool handled = UWindow::ScaleRangeChanged(scale, fromTick, toTick, fromValue, toValue, bFinal);
	UClipWindow* clip = ClipWindow();
	if (!clip)
		return handled;
	int col = 0, row = 0;
	clip->GetChildPosition(col, row);
	if (scale == hScale())
	{
		col = fromTick;
		handled = true;
	}
	else if (scale == vScale())
	{
		row = fromTick;
		handled = true;
	}
	clip->SetChildPosition(col, row);
	return handled;
}

// The original's (XScrollAreaWindow::ClipAttributesChanged 0x10043b50): the
// script first; the clip window's size in units is a scale's span, its
// child's (at least 1) the count of ticks.
bool UScrollAreaWindow::ClipAttributesChanged(UWindow* clip, int newClipWidth, int newClipHeight, int newChildWidth, int newChildHeight)
{
	bool handled = UWindow::ClipAttributesChanged(clip, newClipWidth, newClipHeight, newChildWidth, newChildHeight);
	if (clip != ClipWindow())
		return handled;
	if (hScale())
		hScale()->SetRanges(newClipWidth, std::max(newChildWidth, 1));
	if (vScale())
		vScale()->SetRanges(newClipHeight, std::max(newChildHeight, 1));
	return true;
}

// The original's (XScrollAreaWindow::ClipPositionChanged 0x10043c40): the
// script first; the scales follow the clip's position.
bool UScrollAreaWindow::ClipPositionChanged(UWindow* clip, int newCol, int newRow)
{
	bool handled = UWindow::ClipPositionChanged(clip, newCol, newRow);
	if (clip != ClipWindow())
		return handled;
	if (hScale())
		hScale()->SetTickPosition(newCol);
	if (vScale())
		vScale()->SetTickPosition(newRow);
	return true;
}

// The original's (XScrollAreaWindow::MouseButtonPressed 0x10043d10): the
// script first; the wheel steps the vertical scale while it shows.
bool UScrollAreaWindow::MouseButtonPressed(float pointX, float pointY, EInputKey button, int numClicks)
{
	bool handled = UWindow::MouseButtonPressed(pointX, pointY, button, numClicks);
	if (button != IK_MouseWheelUp && button != IK_MouseWheelDown)
		return handled;
	if (vScale() && vScale()->IsShown())
		vScale()->MoveThumb((uint8_t)(button == IK_MouseWheelUp ? EMoveThumb::StepUp : EMoveThumb::StepDown));
	return true;
}

// Left out, on (XScrollAreaWindow::execAutoHideScrollbars 0x10044000).
void UScrollAreaWindow::AutoHideScrollbars(std::optional<bool> bHide)
{
	bool hide = bHide.value_or(true);
	if (bHideScrollbars() != hide)
	{
		bHideScrollbars() = hide;
		AskParentForReconfigure();
	}
}

void UScrollAreaWindow::SetAreaMargins(float newMarginWidth, float newMarginHeight)
{
	if (marginWidth() != newMarginWidth || marginHeight() != newMarginHeight)
	{
		marginWidth() = newMarginWidth;
		marginHeight() = newMarginHeight;
		AskParentForReconfigure();
	}
}

void UScrollAreaWindow::SetScrollbarDistance(float newDistance)
{
	if (scrollbarDistance() != newDistance)
	{
		scrollbarDistance() = newDistance;
		AskParentForReconfigure();
	}
}
