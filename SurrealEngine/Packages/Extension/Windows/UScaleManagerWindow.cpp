#include "Precomp.h"
#include "UScaleManagerWindow.h"
#include "UScaleWindow.h"
#include "Text/UButtonWindow.h"

// The original's scale manager (Extension.dll XScaleManagerWindow; dx-
// reverse-info/extension-dll.md, Scales and scrolling) lines its shown
// children up along its orientation -- a scroll area's arrow, scale and arrow
// -- each at its preferred size, the scale (and a value field) stretched to
// fill what is left. Its arrows step its scale, and it keeps their sensitivity
// and the value field up to date.

// The original's XScaleManagerWindow::Init (0x100419f0): horizontal, the
// value field stretched, no margins or spacing, children centred across.
void UScaleManagerWindow::InitDefaults()
{
	UWindow::InitDefaults();
	decButton() = nullptr;
	incButton() = nullptr;
	valueField() = nullptr;
	Scale() = nullptr;
	orientation() = (uint8_t)EOrientation::Horizontal;
	bStretchScaleField() = false;
	bStretchValueField() = true;
	marginWidth() = 0.0f;
	marginHeight() = 0.0f;
	Spacing() = 0.0f;
	childHAlign() = (uint8_t)EHAlign::Center;
	childVAlign() = (uint8_t)EVAlign::Center;
}

void UScaleManagerWindow::SetManagerAlignments(uint8_t newHAlign, uint8_t newVAlign)
{
	if (childHAlign() != newHAlign || childVAlign() != newVAlign)
	{
		childHAlign() = newHAlign;
		childVAlign() = newVAlign;
		AskParentForReconfigure();
	}
}

void UScaleManagerWindow::SetManagerMargins(std::optional<float> newMarginWidth, std::optional<float> newMarginHeight)
{
	float width = newMarginWidth.value_or(0.0f);
	float height = newMarginHeight.value_or(0.0f);
	if (marginWidth() != width || marginHeight() != height)
	{
		marginWidth() = width;
		marginHeight() = height;
		AskParentForReconfigure();
	}
}

void UScaleManagerWindow::SetManagerOrientation(uint8_t newOrientation)
{
	orientation() = newOrientation;
	AskParentForReconfigure();
}

void UScaleManagerWindow::SetMarginSpacing(std::optional<float> newSpacing)
{
	float spacing = newSpacing.value_or(0.0f);
	if (Spacing() != spacing)
	{
		Spacing() = spacing;
		AskParentForReconfigure();
	}
}

// The parts are taken only from the manager's own descendants
// (XScaleManagerWindow::IsDescendant 0x10041d10).
bool UScaleManagerWindow::IsDescendant(UWindow* window)
{
	while (window && window != this)
		window = window->parentOwner();
	return window != nullptr;
}

void UScaleManagerWindow::SetScale(UObject* NewScale)
{
	UScaleWindow* scale = UObject::Cast<UScaleWindow>(NewScale);
	if (!IsDescendant(scale))
		scale = nullptr;
	if (Scale() != scale)
	{
		Scale() = scale;
		AskParentForReconfigure();
		ChangeValueField();
	}
}

void UScaleManagerWindow::SetScaleButtons(UObject* newDecButton, UObject* newIncButton)
{
	UButtonWindow* dec = UObject::Cast<UButtonWindow>(newDecButton);
	UButtonWindow* inc = UObject::Cast<UButtonWindow>(newIncButton);
	decButton() = IsDescendant(dec) ? dec : nullptr;
	incButton() = IsDescendant(inc) ? inc : nullptr;
}

void UScaleManagerWindow::SetValueField(UObject* newValueField)
{
	UTextWindow* field = UObject::Cast<UTextWindow>(newValueField);
	if (!IsDescendant(field))
		field = nullptr;
	if (valueField() != field)
	{
		valueField() = field;
		AskParentForReconfigure();
		ChangeValueField();
	}
}

void UScaleManagerWindow::StretchScaleField(std::optional<bool> bNewStretch)
{
	bStretchScaleField() = bNewStretch.value_or(true);
	AskParentForReconfigure();
}

void UScaleManagerWindow::StretchValueField(std::optional<bool> bNewStretch)
{
	bStretchValueField() = bNewStretch.value_or(true);
	AskParentForReconfigure();
}

// The value field shows the scale's value text
// (XScaleManagerWindow::ChangeValueField 0x10041da0).
void UScaleManagerWindow::ChangeValueField()
{
	if (valueField() && Scale())
		valueField()->SetText(Scale()->GetValueString());
}

// The original's (XScaleManagerWindow::AddToBoundingBox 0x10041e40): along
// the orientation the sizes add up, with the spacing between; across, the
// largest.
void UScaleManagerWindow::AddToBoundingBox(float& width, float& height, float childWidth, float childHeight, float spacing)
{
	if ((EOrientation)orientation() == EOrientation::Vertical)
	{
		height = childHeight + height + spacing;
		width = std::max(width, childWidth);
	}
	else
	{
		width = childWidth + width + spacing;
		height = std::max(height, childHeight);
	}
}

// The original's (XScaleManagerWindow::ParentRequestedPreferredSize
// 0x100420e0): the shown children lined up, plus the margins.
void UScaleManagerWindow::ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight)
{
	preferredWidth = 0.0f;
	preferredHeight = 0.0f;
	float spacing = 0.0f;
	for (UWindow* child = firstChild(); child; child = child->nextSibling())
	{
		if (!child->bIsVisible())
			continue;
		float width = 0.0f, height = 0.0f;
		child->QueryPreferredSize(width, height);
		AddToBoundingBox(preferredWidth, preferredHeight, width, height, spacing);
		spacing = Spacing();
	}
	preferredWidth += marginWidth() + marginWidth();
	preferredHeight += marginHeight() + marginHeight();
}

void UScaleManagerWindow::ChildRequestedVisibilityChange(UWindow* childWin, bool bNewVisibility)
{
	childWin->SetChildVisibility(bNewVisibility);
	AskParentForReconfigure();
}

// The original's (XScaleManagerWindow::ComputeChildConfig 0x10041ea0): the
// child at its preferred size after the last one along the orientation, and
// placed across it by the child alignment -- Full filling it within the
// margins. Down a vertical manager, Full sets the child's height to the
// manager's width less the margins, as the original has it.
void UScaleManagerWindow::ComputeChildConfig(UWindow* child, float& offset, float spacing)
{
	child->QueryPreferredSize(child->holdWidth(), child->holdHeight());
	child->holdX() = 0.0f;
	child->holdY() = 0.0f;
	float start = spacing + offset;
	if ((EOrientation)orientation() == EOrientation::Vertical)
	{
		child->holdY() = start;
		switch ((EHAlign)childHAlign())
		{
		case EHAlign::Left: child->holdX() = marginWidth(); break;
		case EHAlign::Center: child->holdX() = (Width() - child->holdWidth()) * 0.5f; break;
		case EHAlign::Right: child->holdX() = Width() - child->holdWidth() - marginWidth(); break;
		default:
			child->holdX() = marginWidth();
			child->holdHeight() = Width() - (marginWidth() + marginWidth());
			break;
		}
		offset = child->holdY() + child->holdHeight();
	}
	else
	{
		child->holdX() = start;
		switch ((EVAlign)childVAlign())
		{
		case EVAlign::Top: child->holdY() = marginHeight(); break;
		case EVAlign::Center: child->holdY() = (Height() - child->holdHeight()) * 0.5f; break;
		case EVAlign::Bottom: child->holdY() = Height() - child->holdHeight() - marginHeight(); break;
		default:
			child->holdY() = marginHeight();
			child->holdHeight() = Height() - (marginHeight() + marginHeight());
			break;
		}
		offset = child->holdWidth() + child->holdX();
	}
}

// The original's (XScaleManagerWindow::ComputeChildOffset 0x10042070): the
// child moved along by what was added before it, and lengthened by its own
// share.
void UScaleManagerWindow::ComputeChildOffset(UWindow* child, float& offset, float extra)
{
	if ((EOrientation)orientation() == EOrientation::Vertical)
	{
		child->holdY() += offset;
		child->holdHeight() += extra;
	}
	else
	{
		child->holdX() += offset;
		child->holdWidth() += extra;
	}
	offset += extra;
}

// The original's (XScaleManagerWindow::ConfigurationChanged 0x10042260):
// the shown children lined up from the margin; what room is left (or is
// missing) goes to the scale and the value field that stretch -- shared
// evenly if both do -- and the children after them move along.
void UScaleManagerWindow::ConfigurationChanged()
{
	bool vertical = (EOrientation)orientation() == EOrientation::Vertical;
	float spacing = vertical ? marginHeight() : marginWidth();
	float end = vertical ? Height() - marginHeight() : Width() - marginWidth();
	float offset = 0.0f;
	for (UWindow* child = firstChild(); child; child = child->nextSibling())
	{
		if (!child->bIsVisible())
			continue;
		ComputeChildConfig(child, offset, spacing);
		spacing = Spacing();
	}

	bool stretchScale = Scale() && Scale()->parentOwner() == this && Scale()->bIsVisible() && bStretchScaleField();
	bool stretchValue = valueField() && UObject::Cast<UWindow>(valueField())->parentOwner() == this && UObject::Cast<UWindow>(valueField())->bIsVisible() && bStretchValueField();
	if (offset != end && (stretchScale || stretchValue))
	{
		float left = end - offset;
		float scaleShare, valueShare;
		if (stretchScale && !stretchValue)
		{
			scaleShare = left;
			valueShare = 0.0f;
		}
		else if (stretchScale)
		{
			scaleShare = left * 0.5f;
			valueShare = left - scaleShare;
		}
		else
		{
			scaleShare = 0.0f;
			valueShare = left;
		}
		float moved = 0.0f;
		for (UWindow* child = firstChild(); child; child = child->nextSibling())
		{
			if (!child->bIsVisible())
				continue;
			if (child == Scale())
				ComputeChildOffset(child, moved, scaleShare);
			else if (child == UObject::Cast<UWindow>(valueField()))
				ComputeChildOffset(child, moved, valueShare);
			else
				ComputeChildOffset(child, moved, 0.0f);
		}
	}

	for (UWindow* child = firstChild(); child; child = child->nextSibling())
	{
		if (child->bIsVisible())
			child->ConfigureChild(child->holdX(), child->holdY(), child->holdWidth(), child->holdHeight());
	}
}

// The original's (XScaleManagerWindow::ChildRemoved 0x10042480): a part
// that goes is let go.
void UScaleManagerWindow::ChildRemoved(UWindow* child)
{
	UWindow::ChildRemoved(child);
	if (child == UObject::Cast<UWindow>(decButton()))
		decButton() = nullptr;
	else if (child == UObject::Cast<UWindow>(incButton()))
		incButton() = nullptr;
	else if (child == Scale())
		Scale() = nullptr;
	else if (child == UObject::Cast<UWindow>(valueField()))
		valueField() = nullptr;
}

// The original's (XScaleManagerWindow::ScalePositionChanged 0x10042580): the
// script first; its scale's move updates the value field.
bool UScaleManagerWindow::ScalePositionChanged(UWindow* scale, int newTickPosition, float newValue, bool bFinal)
{
	bool handled = UWindow::ScalePositionChanged(scale, newTickPosition, newValue, bFinal);
	if (scale == Scale())
		ChangeValueField();
	return handled;
}

// The original's (XScaleManagerWindow::ScaleAttributesChanged 0x10042640):
// the script first; the decrement arrow is sensitive unless the scale is at
// its start, the increment arrow unless its span reaches the end.
bool UScaleManagerWindow::ScaleAttributesChanged(UWindow* scale, int tickPosition, int tickSpan, int numTicks)
{
	bool handled = UWindow::ScaleAttributesChanged(scale, tickPosition, tickSpan, numTicks);
	if (scale == Scale())
	{
		if (decButton())
			UObject::Cast<UWindow>(decButton())->SetSensitivity(tickPosition > 0);
		if (incButton())
			UObject::Cast<UWindow>(incButton())->SetSensitivity(tickSpan + tickPosition < numTicks);
	}
	return handled;
}

// The original's (XScaleManagerWindow::ButtonActivated 0x10042720): the
// script first; an arrow steps the scale by its thumb step.
bool UScaleManagerWindow::ButtonActivated(UWindow* button)
{
	bool handled = UWindow::ButtonActivated(button);
	if (!Scale())
		return handled;
	if (button == UObject::Cast<UWindow>(incButton()))
		Scale()->MoveThumb((uint8_t)EMoveThumb::StepDown);
	else if (button == UObject::Cast<UWindow>(decButton()))
		Scale()->MoveThumb((uint8_t)EMoveThumb::StepUp);
	else
		return handled;
	return true;
}
