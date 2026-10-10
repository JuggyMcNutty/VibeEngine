
#include "Precomp.h"
#include "ULargeTextWindow.h"
#include "Packages/Extension/Windows/UGC.h"
#include "Engine.h"

// The original's XLargeTextWindow::Init (0x1002d480): lines 1 apart, its
// text not its accelerator.
void ULargeTextWindow::InitDefaults()
{
	UTextWindow::InitDefaults();
	vSpace() = 1.0f;
	lineHeight() = 0.0f;
	EnableTextAsAccelerator(false);
}

void ULargeTextWindow::SetVerticalSpacing(std::optional<float> newVSpace)
{
	// UNUSED from scripts.
	LogUnimplemented("LargeTextWindow.SetVerticalSpacing");
}

// The rows of the window's text wrapped at a width (0 for none), as the GC
// breaks them, and the widest (XLargeTextWindow::GenerateLines 0x1002da70).
int ULargeTextWindow::CountRows(UGC* gc, float wrap, float& widest)
{
	int rows = 0;
	widest = 0.0f;
	UGC::TextState state;
	const uint8_t* text = (const uint8_t*)Text().data();
	const uint8_t* end = text + Text().size();
	while (true)
	{
		float lineWidth = 0.0f;
		int lineLength = 0;
		if (!gc->GetLine(state, wrap, text, end, lineWidth, lineLength))
			break;
		widest = std::max(widest, lineWidth);
		rows++;
	}
	return rows;
}

// The original's (XLargeTextWindow::ParentRequestedPreferredSize 0x1002e1d0,
// GetTextExtent 0x1002e130): its rows each the font's height, at least 1
// (ComputeLineHeight 0x1002d9d0), its vertical spacing between them and none
// after the last -- an empty text one row high -- as wide as the widest,
// wrapped at the width given less the margins with word wrap on. With no
// height given, a line limit its rows pass (as laid out at its own width)
// holds it to that many rows of the font's height and spacing, its vertical
// spacing between. Then the margins; a width not given at least the minimum.
void ULargeTextWindow::ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight)
{
	UGC* gc = engine->dxgc;
	UGC::TextSettings saved = gc->UseTextSettings(this);

	float wrap = (bWidthSpecified && bWordWrap()) ? std::max(preferredWidth - (hMargin() + hMargin()), 0.0f) : 0.0f;
	float lineHeight = std::max(gc->GetFontHeight(false), 1.0f);
	float widest = 0.0f;
	int rows = CountRows(gc, wrap, widest);
	preferredWidth = widest;
	preferredHeight = rows > 0 ? rows * lineHeight + (rows - 1) * vSpace() : lineHeight;

	if (!bHeightSpecified && (minLines() >= 0 || MaxLines() >= 0))
	{
		float fontHeight = gc->GetFontHeight(true);
		float ownWidest = 0.0f;
		int ownRows = CountRows(gc, bWordWrap() ? Width() - (hMargin() + hMargin()) : 0.0f, ownWidest);
		if (minLines() >= 0 && minLines() > ownRows)
			preferredHeight = (minLines() - 1) * vSpace() + minLines() * fontHeight;
		if (MaxLines() >= 0 && ownRows > MaxLines())
			preferredHeight = (MaxLines() - 1) * vSpace() + MaxLines() * fontHeight;
	}

	preferredWidth += hMargin() + hMargin();
	preferredHeight += vMargin() + vMargin();
	if (!bWidthSpecified && preferredWidth < MinWidth())
		preferredWidth = MinWidth();

	gc->RestoreTextSettings(saved);
}

// The original's (XLargeTextWindow::ParentRequestedGranularity 0x1002e380,
// ComputeLineHeight 0x1002d9d0): a line down -- the font's height, at least
// 1, plus the vertical spacing -- a pixel across; then the script's say.
void ULargeTextWindow::ParentRequestedGranularity(float& hGranularity, float& vGranularity)
{
	UGC* gc = engine->dxgc;
	UGC::TextSettings saved = gc->UseTextSettings(this);
	vGranularity = std::max(gc->GetFontHeight(false), 1.0f) + vSpace();
	hGranularity = 1.0f;
	gc->RestoreTextSettings(saved);
	UWindow::ParentRequestedGranularity(hGranularity, vGranularity);
}
