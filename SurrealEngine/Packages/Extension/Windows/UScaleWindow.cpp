#include "Precomp.h"
#include "UScaleWindow.h"
#include "UGC.h"
#include "Engine.h"
#include "Packages/Engine/Resources/USound.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"

// The original's scale (Extension.dll XScaleWindow; dx-reverse-info/
// extension-dll.md, Scales and scrolling) keeps a tick position among
// numPositions ticks: a slider's thumb sits on a tick, a scrollbar's spans
// spanRange of them. A value is a tick's place in the value range, and the
// value text the tick's own text if it has one, else the value through the
// value format. A move goes up the parents as ScalePositionChanged (a slider)
// or ScaleRangeChanged (a scrollbar), and a change of the position, span or
// count as ScaleAttributesChanged.

// The original's XScaleWindow::Init (0x1003d200): horizontal, ten ticks for
// the values 0 to 1, the first one, a span of 1, values printed %1.2f, no
// textures (end ticks drawn), white and masked; a held click pages again
// after 0.5 s, then every 0.1 s.
void UScaleWindow::InitDefaults()
{
	UWindow::InitDefaults();
	Color white = { 255, 255, 255, 255 };
	orientation() = (uint8_t)EOrientation::Horizontal;
	scaleTexture() = nullptr;
	thumbTexture() = nullptr;
	tickTexture() = nullptr;
	preCapTexture() = nullptr;
	postCapTexture() = nullptr;
	bRepeatScaleTexture() = false;
	bRepeatThumbTexture() = false;
	bDrawEndTicks() = true;
	bStretchScale() = false;
	bSpanThumb() = false;
	scaleBorderSize() = 0.0f;
	thumbBorderSize() = 0.0f;
	scaleBorderColor() = white;
	thumbBorderColor() = white;
	scaleStyle() = (uint8_t)EDrawStyle::Masked;
	thumbStyle() = (uint8_t)EDrawStyle::Masked;
	tickStyle() = (uint8_t)EDrawStyle::Masked;
	scaleColor() = white;
	thumbColor() = white;
	tickColor() = white;
	scaleWidth() = 0.0f;
	scaleHeight() = 0.0f;
	ThumbWidth() = 0.0f;
	ThumbHeight() = 0.0f;
	tickWidth() = 0.0f;
	tickHeight() = 0.0f;
	preCapWidth() = 0.0f;
	preCapHeight() = 0.0f;
	postCapWidth() = 0.0f;
	postCapHeight() = 0.0f;
	startOffset() = 0.0f;
	endOffset() = 0.0f;
	marginWidth() = 0.0f;
	marginHeight() = 0.0f;
	numPositions() = 10;
	currentPos() = 0;
	spanRange() = 1;
	thumbStep() = 1;
	fromValue() = 0.0f;
	toValue() = 1.0f;
	valueFmt() = "%1.2f";
	bDraggingThumb() = false;
	initialDelay() = 0.5f;
	repeatRate() = 0.1f;
	initialPos() = -1;
	setSound() = nullptr;
	clickSound() = nullptr;
	dragSound() = nullptr;
	scaleX() = 0.0f;
	scaleY() = 0.0f;
	scaleW() = 0.0f;
	scaleH() = 0.0f;
	thumbX() = 0.0f;
	thumbY() = 0.0f;
	thumbW() = 0.0f;
	thumbH() = 0.0f;
	tickX() = 0.0f;
	tickY() = 0.0f;
	tickW() = 0.0f;
	tickH() = 0.0f;
	preCapXOff() = 0.0f;
	preCapYOff() = 0.0f;
	preCapW() = 0.0f;
	preCapH() = 0.0f;
	postCapXOff() = 0.0f;
	postCapYOff() = 0.0f;
	postCapW() = 0.0f;
	postCapH() = 0.0f;
	absStartScale() = 0.0f;
	absEndScale() = 0.0f;
	repeatDir() = 0;
	RemainingTime() = 0.0f;
}

void UScaleWindow::SetScaleOrientation(uint8_t newOrientation)
{
	orientation() = newOrientation;
	AskParentForReconfigure();
}

// The textures' setters (XScaleWindow::SetScaleTexture 0x1003d700 and the
// rest): a size left out is 0 -- the texture's own -- and none is below 0;
// the scale is laid out again.
void UScaleWindow::SetScaleTexture(UObject* NewTexture, std::optional<float> newWidth, std::optional<float> NewHeight, std::optional<float> newStart, std::optional<float> newEnd)
{
	scaleTexture() = UObject::Cast<UTexture>(NewTexture);
	scaleWidth() = std::max(newWidth.value_or(0.0f), 0.0f);
	scaleHeight() = std::max(NewHeight.value_or(0.0f), 0.0f);
	startOffset() = newStart.value_or(0.0f);
	endOffset() = newEnd.value_or(0.0f);
	AskParentForReconfigure();
}

void UScaleWindow::SetThumbTexture(UObject* NewTexture, std::optional<float> newWidth, std::optional<float> NewHeight)
{
	thumbTexture() = UObject::Cast<UTexture>(NewTexture);
	ThumbWidth() = std::max(newWidth.value_or(0.0f), 0.0f);
	ThumbHeight() = std::max(NewHeight.value_or(0.0f), 0.0f);
	AskParentForReconfigure();
}

void UScaleWindow::SetTickTexture(UObject* newTickTexture, std::optional<bool> newDrawEndTicks, std::optional<float> newWidth, std::optional<float> NewHeight)
{
	tickTexture() = UObject::Cast<UTexture>(newTickTexture);
	bDrawEndTicks() = newDrawEndTicks.value_or(true);
	tickWidth() = std::max(newWidth.value_or(0.0f), 0.0f);
	tickHeight() = std::max(NewHeight.value_or(0.0f), 0.0f);
	AskParentForReconfigure();
}

void UScaleWindow::SetThumbCaps(UObject* preCap, UObject* postCap, std::optional<float> newPreCapWidth, std::optional<float> newPreCapHeight, std::optional<float> newPostCapWidth, std::optional<float> newPostCapHeight)
{
	preCapTexture() = UObject::Cast<UTexture>(preCap);
	postCapTexture() = UObject::Cast<UTexture>(postCap);
	preCapWidth() = std::max(newPreCapWidth.value_or(0.0f), 0.0f);
	preCapHeight() = std::max(newPreCapHeight.value_or(0.0f), 0.0f);
	postCapWidth() = std::max(newPostCapWidth.value_or(0.0f), 0.0f);
	postCapHeight() = std::max(newPostCapHeight.value_or(0.0f), 0.0f);
	AskParentForReconfigure();
}

// The original's (XScaleWindow::EnableStretchedScale 0x1003da70): a
// stretched scale fills the window along its length, its texture repeated.
// Left out, on.
void UScaleWindow::EnableStretchedScale(std::optional<bool> bNewStretch)
{
	bool stretch = bNewStretch.value_or(true);
	if (bStretchScale() != stretch)
	{
		bStretchScale() = stretch;
		bRepeatScaleTexture() = stretch;
		AskParentForReconfigure();
	}
}

void UScaleWindow::SetBorderPattern(UObject* NewTexture)
{
	borderPattern() = UObject::Cast<UTexture>(NewTexture);
}

// The original's (XScaleWindow::SetScaleBorder 0x1003db30, SetThumbBorder
// 0x1003dbd0): the colour is always taken, and a new size lays the scale out
// again. Left out: 0 and white.
void UScaleWindow::SetScaleBorder(std::optional<float> newBorderSize, std::optional<Color> NewColor)
{
	float size = newBorderSize.value_or(0.0f);
	scaleBorderColor() = NewColor.value_or(Color{ 255, 255, 255, 255 });
	if (scaleBorderSize() != size)
	{
		scaleBorderSize() = size;
		AskParentForReconfigure();
	}
}

void UScaleWindow::SetThumbBorder(std::optional<float> newBorderSize, std::optional<Color> NewColor)
{
	float size = newBorderSize.value_or(0.0f);
	thumbBorderColor() = NewColor.value_or(Color{ 255, 255, 255, 255 });
	if (thumbBorderSize() != size)
	{
		thumbBorderSize() = size;
		AskParentForReconfigure();
	}
}

void UScaleWindow::SetScaleStyle(uint8_t NewStyle)
{
	scaleStyle() = NewStyle;
}

void UScaleWindow::SetThumbStyle(uint8_t NewStyle)
{
	thumbStyle() = NewStyle;
}

void UScaleWindow::SetTickStyle(uint8_t NewStyle)
{
	tickStyle() = NewStyle;
}

void UScaleWindow::SetScaleColor(const Color& NewColor)
{
	scaleColor() = NewColor;
}

void UScaleWindow::SetThumbColor(const Color& NewColor)
{
	thumbColor() = NewColor;
}

void UScaleWindow::SetTickColor(const Color& NewColor)
{
	tickColor() = NewColor;
}

// The original's (XScaleWindow::SetScaleMargins 0x1003dcd0): none below 0.
void UScaleWindow::SetScaleMargins(std::optional<float> newMarginWidth, std::optional<float> newMarginHeight)
{
	float width = std::max(newMarginWidth.value_or(0.0f), 0.0f);
	float height = std::max(newMarginHeight.value_or(0.0f), 0.0f);
	if (marginWidth() != width || marginHeight() != height)
	{
		marginWidth() = width;
		marginHeight() = height;
		AskParentForReconfigure();
	}
}

// The original's (XScaleWindow::SetNumTicks 0x1003dda0): at least one tick.
void UScaleWindow::SetNumTicks(int newNumTicks)
{
	newNumTicks = std::max(newNumTicks, 1);
	if (numPositions() != newNumTicks)
	{
		numPositions() = newNumTicks;
		ChangeThumbPosition(currentPos(), true, true);
	}
}

int UScaleWindow::GetNumTicks()
{
	return numPositions();
}

// The original's (XScaleWindow::SetThumbSpan 0x1003de40): a span of 1 or
// more makes a scrollbar, its thumb's texture repeated along it; less makes
// a slider and keeps the old span. Left out, 1.
void UScaleWindow::SetThumbSpan(std::optional<int> newRange)
{
	int range = newRange.value_or(1);
	if (spanRange() != range || bSpanThumb() != (range >= 1))
	{
		bSpanThumb() = range >= 1;
		bRepeatThumbTexture() = range >= 1;
		if (range >= 1)
			spanRange() = range;
		ChangeThumbPosition(currentPos(), true, true);
	}
}

int UScaleWindow::GetThumbSpan()
{
	return bSpanThumb() ? spanRange() : 0;
}

// The original's (XScaleWindow::SetRanges 0x1003df40), which a scroll area
// sets from its clip window: the span (at least 1) and the count of ticks.
void UScaleWindow::SetRanges(int newSpan, int newNumTicks)
{
	newSpan = std::max(newSpan, 1);
	if (spanRange() != newSpan || numPositions() != newNumTicks)
	{
		spanRange() = newSpan;
		numPositions() = newNumTicks;
		ChangeThumbPosition(currentPos(), true, true);
	}
}

void UScaleWindow::SetTickPosition(int newPosition)
{
	ChangeThumbPosition(newPosition, false, true);
}

int UScaleWindow::GetTickPosition()
{
	return currentPos();
}

void UScaleWindow::SetValueRange(float newFrom, float newTo)
{
	if (fromValue() != newFrom || toValue() != newTo)
	{
		fromValue() = newFrom;
		toValue() = newTo;
		ChangeScalePosition(true);
	}
}

// A value goes to the nearest tick (XScaleWindow::SetValue 0x1003e120), and
// reads back as that tick's value (GetValue 0x1003e1b0).
void UScaleWindow::SetValue(float NewValue)
{
	ChangeThumbPosition(ValueToTick(NewValue), false, true);
}

float UScaleWindow::GetValue()
{
	return TickToValue(currentPos());
}

// The original's (XScaleWindow::GetValues 0x1003e230): the position's value,
// and a scrollbar's span end -- for a slider, the range's first value.
void UScaleWindow::GetValues(float& outFromValue, float& outToValue)
{
	outFromValue = TickToValue(currentPos());
	outToValue = bSpanThumb() ? TickToValue(currentPos() + spanRange()) : fromValue();
}

void UScaleWindow::SetValueFormat(const std::string& newFmt)
{
	valueFmt() = newFmt;
	ChangeScalePosition(true);
}

// The original's (XScaleWindow::GetValueString 0x1003e460): the tick's own
// text, else its value through the value format.
std::string UScaleWindow::GetValueString()
{
	int pos = currentPos();
	if (pos >= 0 && (size_t)pos < enumStrings.size() && !enumStrings[pos].empty())
		return enumStrings[pos];
	return FormatScriptFloat(valueFmt(), GetValue());
}

// The original's (XScaleWindow::SetEnumeration 0x1003e580): ticks 0 to 511
// take a text; the position's own sends the move again.
void UScaleWindow::SetEnumeration(int tickPos, const std::string& newStr)
{
	if (tickPos < 0 || tickPos >= 512)
		return;
	if ((size_t)tickPos >= enumStrings.size())
		enumStrings.resize(tickPos + 1);
	enumStrings[tickPos] = newStr;
	if (tickPos == currentPos())
		ChangeScalePosition(true);
}

void UScaleWindow::ClearAllEnumerations()
{
	enumStrings.clear();
	ChangeScalePosition(true);
}

// The original's (XScaleWindow::MoveThumb 0x1003e860): a step is the thumb
// step, a page a scrollbar's span or a slider's 4 ticks.
void UScaleWindow::MoveThumb(uint8_t MoveThumb)
{
	int page = bSpanThumb() ? spanRange() : 4;
	int pos = 0;
	switch ((EMoveThumb)MoveThumb)
	{
	case EMoveThumb::End: pos = numPositions(); break;
	case EMoveThumb::Prev: pos = currentPos() - 1; break;
	case EMoveThumb::Next: pos = currentPos() + 1; break;
	case EMoveThumb::StepUp: pos = currentPos() - thumbStep(); break;
	case EMoveThumb::StepDown: pos = currentPos() + thumbStep(); break;
	case EMoveThumb::PageUp: pos = currentPos() - page; break;
	case EMoveThumb::PageDown: pos = currentPos() + page; break;
	default: pos = 0; break;
	}
	ChangeThumbPosition(pos, false, true);
}

void UScaleWindow::SetThumbStep(int NewStep)
{
	thumbStep() = std::max(NewStep, 1);
}

void UScaleWindow::SetScaleSounds(std::optional<UObject*> newSetSound, std::optional<UObject*> newClickSound, std::optional<UObject*> newDragSound)
{
	setSound() = UObject::Cast<USound>(newSetSound.value_or(nullptr));
	clickSound() = UObject::Cast<USound>(newClickSound.value_or(nullptr));
	dragSound() = UObject::Cast<USound>(newDragSound.value_or(nullptr));
}

// The original's (XScaleWindow::PlayScaleSound 0x1003e9c0): from the thumb's
// centre; a volume of -1, as left out, is the window's own.
void UScaleWindow::PlayScaleSound(UObject* newsound, std::optional<float> Volume, std::optional<float> Pitch)
{
	float volume = Volume.value_or(-1.0f);
	PlaySound(newsound, volume == -1.0f ? std::optional<float>() : volume, Pitch.value_or(1.0f), thumbX() + thumbW() * 0.5f, thumbY() + thumbH() * 0.5f);
}

// The original's (XScaleWindow::TickToValue 0x1003ebb0): the ticks share the
// range evenly, a scrollbar's one more than it has; with no room, the middle.
float UScaleWindow::TickToValue(int tick)
{
	double positions = bSpanThumb() ? numPositions() : numPositions() - 1;
	if (positions <= 0.0)
		return (float)((toValue() - fromValue()) * 0.5 + fromValue());
	return (float)((toValue() - fromValue()) * (double)tick / positions + fromValue());
}

// The original's (XScaleWindow::ValueToTick 0x1003ec20): the nearest tick,
// halves away from 0.
int UScaleWindow::ValueToTick(float value)
{
	double positions = bSpanThumb() ? numPositions() : numPositions() - 1;
	double tick = toValue() == fromValue() ? 0.0 : positions * (value - fromValue()) / (toValue() - fromValue());
	return tick >= 0.0 ? (int)(int64_t)(tick + 0.5) : (int)(int64_t)(tick - 0.5);
}

// The original's (XScaleWindow::TickToPixel 0x1003ea70, PixelToTick
// 0x1003eb10): a slider's ticks spread over the scale's length, a
// scrollbar's over its length less the thumb's.
float UScaleWindow::TickToPixel(int tick)
{
	double length;
	int positions;
	if (bSpanThumb())
	{
		positions = numPositions() - spanRange();
		length = absEndScale() - absStartScale() + 1.0 - ((EOrientation)orientation() == EOrientation::Vertical ? thumbH() : thumbW());
	}
	else
	{
		length = absEndScale() - absStartScale();
		positions = numPositions() - 1;
	}
	positions = std::max(positions, 1);
	return (float)(int64_t)((double)tick * length / positions + absStartScale() + 0.5);
}

int UScaleWindow::PixelToTick(float pixel)
{
	double length;
	int positions;
	if (bSpanThumb())
	{
		positions = numPositions() - spanRange();
		length = absEndScale() - absStartScale() + 1.0 - ((EOrientation)orientation() == EOrientation::Vertical ? thumbH() : thumbW());
	}
	else
	{
		length = absEndScale() - absStartScale();
		positions = numPositions() - 1;
	}
	length = std::max(length, 1.0);
	return (int)(int64_t)((pixel - absStartScale()) * positions / length + 0.5);
}

// The original's (XScaleWindow::ChangeThumbPosition 0x1003f170): the
// position held to the ticks; a new one moves the thumb and is sent up, and
// a new one or a forced change sends the attributes.
void UScaleWindow::ChangeThumbPosition(int newPosition, bool bForce, bool bFinal)
{
	int pos = newPosition;
	if (bSpanThumb())
		pos = std::min(pos, numPositions() - spanRange());
	else if (pos >= numPositions())
		pos = numPositions() - 1;
	pos = std::max(pos, 0);

	bool changed = bForce;
	if (currentPos() != pos)
	{
		changed = true;
		currentPos() = pos;
		ComputeThumbConfig();
		ChangeScalePosition(bFinal);
	}
	if (changed)
	{
		ChangeScaleAttributes();
		ComputeThumbConfig();
	}
}

// The original's (XScaleWindow::ChangeScalePosition 0x1003f5a0): a
// scrollbar's span, or a slider's tick and value, up the parents until one
// takes it.
void UScaleWindow::ChangeScalePosition(bool bFinal)
{
	if (bSpanThumb())
	{
		int fromTick = currentPos();
		int toTick = fromTick + spanRange();
		float from = TickToValue(fromTick);
		float to = TickToValue(toTick);
		for (UWindow* cur = this; cur; cur = cur->parentOwner())
		{
			if (cur->ScaleRangeChanged(this, fromTick, toTick, from, to, bFinal))
				break;
		}
	}
	else
	{
		int tick = currentPos();
		float value = TickToValue(tick);
		for (UWindow* cur = this; cur; cur = cur->parentOwner())
		{
			if (cur->ScalePositionChanged(this, tick, value, bFinal))
				break;
		}
	}
}

// The original's (XScaleWindow::ChangeScaleAttributes 0x1003f6c0): only from
// a scale that shows.
void UScaleWindow::ChangeScaleAttributes()
{
	if (!IsShown())
		return;
	for (UWindow* cur = this; cur; cur = cur->parentOwner())
	{
		if (cur->ScaleAttributesChanged(this, currentPos(), spanRange(), numPositions()))
			break;
	}
}

// The original's (XScaleWindow::ComputeTextureSize 0x1003ecb0): the size
// given, else the texture's, plus the border on both sides.
void UScaleWindow::ComputeTextureSize(UTexture* tex, float width, float height, float border, float& outWidth, float& outHeight)
{
	outWidth = width > 0.0f ? width : (tex ? (float)tex->USize() : 0.0f);
	outHeight = height > 0.0f ? height : (tex ? (float)tex->VSize() : 0.0f);
	outWidth += border + border;
	outHeight += border + border;
}

// The original's (XScaleWindow::ComputeThumbConfig 0x1003ed60): the thumb is
// its texture with its caps added along the scale, centred across it. A
// scrollbar's is as long as its span's share of the scale -- 6 at least, the
// scale at most, the caps cut to fit -- placed at its first tick; a slider's
// is centred on its tick.
void UScaleWindow::ComputeThumbConfig()
{
	bool vertical = (EOrientation)orientation() == EOrientation::Vertical;
	ComputeTextureSize(thumbTexture(), ThumbWidth(), ThumbHeight(), thumbBorderSize(), thumbW(), thumbH());
	preCapXOff() = 0.0f;
	preCapYOff() = 0.0f;
	postCapXOff() = 0.0f;
	postCapYOff() = 0.0f;
	ComputeTextureSize(preCapTexture(), preCapWidth(), preCapHeight(), 0.0f, preCapW(), preCapH());
	ComputeTextureSize(postCapTexture(), postCapWidth(), postCapHeight(), 0.0f, postCapW(), postCapH());
	float borders = thumbBorderSize() + thumbBorderSize();
	if (vertical)
	{
		thumbH() = preCapH() + postCapH() + thumbH();
		preCapW() = thumbW() - borders;
		postCapW() = thumbW() - borders;
	}
	else
	{
		thumbW() = preCapW() + postCapW() + thumbW();
		preCapH() = thumbH() - borders;
		postCapH() = thumbH() - borders;
	}
	thumbX() = (float)(int64_t)((Width() - thumbW()) * 0.5f);
	thumbY() = (float)(int64_t)((Height() - thumbH()) * 0.5f);

	if (bSpanThumb())
	{
		int positions = std::max(numPositions(), 1);
		float length = (float)(int64_t)((absEndScale() + 1.0 - absStartScale()) * spanRange() / positions + 0.5);
		float scaleLength = absEndScale() - absStartScale() + 1.0f;
		if (vertical)
		{
			thumbH() = std::max(length, 6.0f);
			if (scaleLength < thumbH())
				thumbH() = scaleLength;
			float excess = borders + preCapH() + postCapH() - thumbH();
			if (excess > 0.0f)
			{
				float half = (float)(int64_t)(excess * 0.5f);
				preCapH() -= excess - half;
				postCapH() -= half;
				postCapYOff() += half;
			}
			thumbY() = TickToPixel(currentPos());
		}
		else
		{
			thumbW() = std::max(length, 6.0f);
			if (scaleLength < thumbW())
				thumbW() = scaleLength;
			float excess = borders + preCapW() + postCapW() - thumbW();
			if (excess > 0.0f)
			{
				float half = (float)(int64_t)(excess * 0.5f);
				preCapW() -= excess - half;
				postCapW() -= half;
				postCapXOff() += half;
			}
			thumbX() = TickToPixel(currentPos());
		}
	}
	else if (vertical)
	{
		thumbY() = (float)(int64_t)(TickToPixel(currentPos()) - thumbH() * 0.5f + 0.5f);
	}
	else
	{
		thumbX() = (float)(int64_t)(TickToPixel(currentPos()) - thumbW() * 0.5f + 0.5f);
	}
}

// The original's (XScaleWindow::ConfigurationChanged 0x1003f8f0): the scale
// centred in the window -- a stretched one filling it along its length within
// the margins -- its ticks running from its start offset to its end offset,
// inside its border; then the thumb, and the ticks' size, centred.
void UScaleWindow::ConfigurationChanged()
{
	bool vertical = (EOrientation)orientation() == EOrientation::Vertical;
	ComputeTextureSize(scaleTexture(), scaleWidth(), scaleHeight(), scaleBorderSize(), scaleW(), scaleH());
	if (bStretchScale())
	{
		if (vertical)
			scaleH() = Height() - (marginHeight() + marginHeight());
		else
			scaleW() = Width() - (marginWidth() + marginWidth());
	}
	scaleX() = (float)(int64_t)((Width() - scaleW()) * 0.5f);
	scaleY() = (float)(int64_t)((Height() - scaleH()) * 0.5f);
	float end;
	if (vertical)
	{
		absStartScale() = scaleY() + startOffset();
		end = scaleY() + scaleH();
	}
	else
	{
		absStartScale() = scaleX() + startOffset();
		end = scaleX() + scaleW();
	}
	absEndScale() = end - (endOffset() + 1.0f);
	absStartScale() += scaleBorderSize();
	absEndScale() -= scaleBorderSize();
	if (absEndScale() < absStartScale())
	{
		float middle = (absEndScale() - absStartScale()) * 0.5f + absStartScale();
		absStartScale() = middle;
		absEndScale() = middle;
	}
	ComputeThumbConfig();
	ComputeTextureSize(tickTexture(), tickWidth(), tickHeight(), 0.0f, tickW(), tickH());
	tickX() = (float)(int64_t)((Width() - tickW()) * 0.5f);
	tickY() = (float)(int64_t)((Height() - tickH()) * 0.5f);
}

// The original's (XScaleWindow::ParentRequestedPreferredSize 0x1003f780):
// the largest of the scale, the thumb and a tick, plus the margins.
void UScaleWindow::ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight)
{
	float width = 0.0f, height = 0.0f;
	ComputeTextureSize(scaleTexture(), scaleWidth(), scaleHeight(), scaleBorderSize(), width, height);
	preferredWidth = width;
	preferredHeight = height;
	ComputeTextureSize(thumbTexture(), ThumbWidth(), ThumbHeight(), thumbBorderSize(), width, height);
	preferredWidth = std::max(preferredWidth, width);
	preferredHeight = std::max(preferredHeight, height);
	ComputeTextureSize(tickTexture(), tickWidth(), tickHeight(), 0.0f, width, height);
	preferredWidth = std::max(preferredWidth, width);
	preferredHeight = std::max(preferredHeight, height);
	preferredWidth += marginWidth() + marginWidth();
	preferredHeight += marginHeight() + marginHeight();
}

// The original's (XScaleWindow::Draw 0x1003fb10), which sends the script no
// DrawWindow: the scale, the ticks (the end ones only if asked), then the
// thumb with its caps.
void UScaleWindow::DrawWindow(UGC* gc)
{
	Color black = { 0, 0, 0, 255 };
	DrawScaleTexture(gc, scaleTexture(), scaleX(), scaleY(), scaleW(), scaleH(), bRepeatScaleTexture(), scaleStyle(), scaleColor(), scaleBorderSize(), scaleBorderColor(),
		nullptr, nullptr, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

	if (tickTexture())
	{
		int count = bSpanThumb() ? numPositions() + 1 : numPositions();
		int first = 0;
		if (!bDrawEndTicks())
		{
			first = 1;
			count--;
		}
		for (int i = first; i < count; i++)
		{
			if ((EOrientation)orientation() == EOrientation::Vertical)
			{
				float y = TickToPixel(i) - (float)(int64_t)(tickH() * 0.5f);
				DrawScaleTexture(gc, tickTexture(), tickX(), y, tickW(), tickH(), false, tickStyle(), tickColor(), 0.0f, black,
					nullptr, nullptr, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
			}
			else
			{
				float x = TickToPixel(i) - (float)(int64_t)(tickW() * 0.5f);
				DrawScaleTexture(gc, tickTexture(), x, tickY(), tickW(), tickH(), false, tickStyle(), tickColor(), 0.0f, black,
					nullptr, nullptr, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
			}
		}
	}

	DrawScaleTexture(gc, thumbTexture(), thumbX(), thumbY(), thumbW(), thumbH(), bRepeatThumbTexture(), thumbStyle(), thumbColor(), thumbBorderSize(), thumbBorderColor(),
		preCapTexture(), postCapTexture(), preCapXOff(), preCapYOff(), preCapW(), preCapH(), postCapXOff(), postCapYOff(), postCapW(), postCapH());
}

// The original's (XScaleWindow::DrawScaleTexture 0x1003f250): the border in
// the border pattern, then within it the caps at either end along the scale
// and the texture between, tiled across the scale and along it if repeated.
void UScaleWindow::DrawScaleTexture(UGC* gc, UTexture* tex, float x, float y, float w, float h, bool bRepeat, uint8_t style, const Color& color, float borderSize, const Color& borderColor,
	UTexture* preCap, UTexture* postCap, float preCapXOff, float preCapYOff, float preCapW, float preCapH, float postCapXOff, float postCapYOff, float postCapW, float postCapH)
{
	uint8_t oldStyle = gc->GetStyle();
	Color oldTileColor = gc->tileColor();
	bool vertical = (EOrientation)orientation() == EOrientation::Vertical;

	gc->SetStyle((EDrawStyle)style);
	if (borderSize > 0.0f)
	{
		if (borderPattern())
		{
			gc->SetTileColor(borderColor);
			gc->DrawBox(x, y, w, h, 0.0f, 0.0f, borderSize, borderPattern());
		}
		x += borderSize;
		y += borderSize;
		w -= borderSize + borderSize;
		h -= borderSize + borderSize;
	}
	gc->SetTileColor(color);

	if (preCap && preCapW > 0.0f && preCapH > 0.0f && w > 0.0f && h > 0.0f)
	{
		if (vertical)
		{
			gc->DrawIconPattern(x, y, preCapW, preCapH, preCapXOff, preCapYOff, 0.0f, preCapH, preCap);
			y += preCapH;
			h -= preCapH;
		}
		else
		{
			gc->DrawIconPattern(x, y, preCapW, preCapH, preCapXOff, preCapYOff, preCapW, 0.0f, preCap);
			x += preCapW;
			w -= preCapW;
		}
	}
	if (postCap && postCapW > 0.0f && postCapH > 0.0f && w > 0.0f && h > 0.0f)
	{
		if (vertical)
		{
			gc->DrawIconPattern(x, y + h - postCapH, postCapW, postCapH, postCapXOff, postCapYOff, 0.0f, postCapH, postCap);
			h -= postCapH;
		}
		else
		{
			gc->DrawIconPattern(x + w - postCapW, y, postCapW, postCapH, postCapXOff, postCapYOff, postCapW, 0.0f, postCap);
			w -= postCapW;
		}
	}
	if (tex && w > 0.0f && h > 0.0f)
	{
		if (vertical)
			gc->DrawIconPattern(x, y, w, h, 0.0f, 0.0f, 0.0f, bRepeat ? 0.0f : h, tex);
		else
			gc->DrawIconPattern(x, y, w, h, 0.0f, 0.0f, bRepeat ? 0.0f : w, 0.0f, tex);
	}

	gc->SetStyle((EDrawStyle)oldStyle);
	gc->SetTileColor(oldTileColor);
}

// The original's (XScaleWindow::MouseButtonPressed 0x1003fdc0): the script
// first; then the left button on the thumb starts a drag; on a slider's
// scale it drags from there, the thumb jumping to the tick; on a scrollbar's,
// it pages toward the click, again after the initial delay while held.
bool UScaleWindow::MouseButtonPressed(float pointX, float pointY, EInputKey button, int numClicks)
{
	bool handled = UWindow::MouseButtonPressed(pointX, pointY, button, numClicks);
	if (button != IK_LeftMouse)
		return handled;

	bool vertical = (EOrientation)orientation() == EOrientation::Vertical;
	if (pointX >= thumbX() && thumbW() + thumbX() > pointX && pointY >= thumbY() && thumbH() + thumbY() > pointY)
	{
		bDraggingThumb() = true;
		initialPos() = currentPos();
		mousePos() = vertical ? pointY - thumbY() : pointX - thumbX();
		PlayScaleSound(clickSound(), {}, {});
		return true;
	}

	if (!bSpanThumb())
	{
		initialPos() = currentPos();
		bDraggingThumb() = true;
		ChangeThumbPosition(PixelToTick(vertical ? pointY : pointX), false, false);
		PlayScaleSound(setSound(), {}, {});
		return true;
	}

	repeatDir() = (vertical ? pointY < thumbY() : pointX < thumbX()) ? 2 : 1;
	MoveThumb((uint8_t)(repeatDir() == 2 ? EMoveThumb::PageUp : EMoveThumb::PageDown));
	PlayScaleSound(setSound(), {}, {});
	RemainingTime() = GetTickOffset() + initialDelay();
	if (RemainingTime() <= 0.0f)
		RemainingTime() = repeatRate();
	if (RemainingTime() <= 0.0f)
		repeatDir() = 0;
	return true;
}

// The original's (XScaleWindow::MouseMoved 0x10040030): a drag moves the
// thumb with the pointer -- a scrollbar's keeps the grip's place on it -- the
// drag sound on each new tick; the move is sent as not final.
void UScaleWindow::MouseMoved(float newX, float newY)
{
	UWindow::MouseMoved(newX, newY);
	if (!bDraggingThumb())
		return;

	bool vertical = (EOrientation)orientation() == EOrientation::Vertical;
	float pixel = vertical ? newY : newX;
	if (bSpanThumb())
		pixel -= mousePos();
	int oldPos = currentPos();
	ChangeThumbPosition(PixelToTick(pixel), false, false);
	if (oldPos != currentPos())
		PlayScaleSound(dragSound(), {}, {});
}

// The original's (XScaleWindow::MouseButtonReleased 0x10040150): a drag that
// moved sends its end as final, with the set sound; paging stops.
bool UScaleWindow::MouseButtonReleased(float pointX, float pointY, EInputKey button, int numClicks)
{
	bool handled = UWindow::MouseButtonReleased(pointX, pointY, button, numClicks);
	if (button != IK_LeftMouse)
		return handled;

	if (bDraggingThumb() && currentPos() != initialPos())
	{
		ChangeScalePosition(true);
		PlayScaleSound(setSound(), {}, {});
	}
	repeatDir() = 0;
	bDraggingThumb() = false;
	return true;
}

// The original's (XScaleWindow::Tick 0x10040250): a held click beside a
// scrollbar's thumb pages on every repeatRate while the pointer is in the
// scale and still beside the thumb on that side. Across, the original
// measures the far side by the thumb's height.
void UScaleWindow::Tick(float timeElapsed)
{
	UWindow::Tick(timeElapsed);
	if (repeatDir() == 0)
		return;
	RemainingTime() -= timeElapsed;
	if (RemainingTime() >= 0.0f)
		return;

	float mouseX = 0.0f, mouseY = 0.0f;
	GetCursorPos(mouseX, mouseY);
	if (mouseX >= 0.0f && mouseX < Width() && mouseY >= 0.0f && mouseY < Height())
	{
		int oldPos = currentPos();
		bool vertical = (EOrientation)orientation() == EOrientation::Vertical;
		float pointer = vertical ? mouseY : mouseX;
		float thumbStart = vertical ? thumbY() : thumbX();
		if (pointer < thumbStart && repeatDir() == 2)
			MoveThumb((uint8_t)EMoveThumb::PageUp);
		if (thumbH() + thumbStart <= pointer && repeatDir() == 1)
			MoveThumb((uint8_t)EMoveThumb::PageDown);
		if (oldPos != currentPos())
			PlayScaleSound(setSound(), {}, {});
	}
	RemainingTime() += repeatRate();
	if (RemainingTime() < 0.0f)
		RemainingTime() = 0.05f;
}
