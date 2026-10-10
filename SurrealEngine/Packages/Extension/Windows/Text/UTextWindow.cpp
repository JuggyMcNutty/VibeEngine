
#include "Precomp.h"
#include "UTextWindow.h"
#include "Utils/Logger.h"
#include "Engine.h"
#include "Packages/Engine/UCanvas.h"
#include "Packages/Engine/Resources/UFont.h"
#include "Packages/Extension/Windows/UGC.h"

void UTextWindow::AppendText(const std::string& NewText)
{
	Text() += NewText;
	if (!NewText.empty())
	{
		TextModifiedByScript();
		AskParentForReconfigure();
	}
}

void UTextWindow::EnableTextAsAccelerator(std::optional<bool> bEnable)
{
	std::string accelText = bEnable ? Text() : "";
	SetAcceleratorText(accelText.length() > 0 ? accelText : "");
}

std::string UTextWindow::GetText()
{
	return Text();
}

int UTextWindow::GetTextLength()
{
	return (int)Text().size();
}

int UTextWindow::GetTextPart(int startPos, int Count, std::string& OutText)
{
	int start = std::max(startPos, 0);
	int end = std::min(startPos + Count, (int)Text().size());
	OutText = Text().substr(start, end - start);
	return (int)OutText.size();
}

void UTextWindow::ResetLines()
{
	// UNUSED from scripts.
	LogUnimplemented("TextWindow.ResetLines");
}

void UTextWindow::ResetMinWidth()
{
	// UNUSED from scripts.
	LogUnimplemented("TextWindow.ResetMinWidth");
}

void UTextWindow::SetLines(int newMinLines, int newMaxLines)
{
	if (minLines() != newMinLines || MaxLines() != newMaxLines)
	{
		minLines() = newMinLines;
		MaxLines() = newMaxLines;
		AskParentForReconfigure();
	}
}

void UTextWindow::SetMaxLines(int newMaxLines)
{
	if (MaxLines() != newMaxLines)
	{
		MaxLines() = newMaxLines;
		AskParentForReconfigure();
	}
}

void UTextWindow::SetMinLines(int newMinLines)
{
	if (minLines() != newMinLines)
	{
		minLines() = newMinLines;
		AskParentForReconfigure();
	}
}

void UTextWindow::SetMinWidth(float newMinWidth)
{
	if (MinWidth() != newMinWidth)
	{
		MinWidth() = newMinWidth;
		AskParentForReconfigure();
	}
}

void UTextWindow::SetText(const std::string& NewText)
{
	if (Text() != NewText)
	{
		Text() = NewText;
		TextModifiedByScript();
		AskParentForReconfigure();
	}
}

void UTextWindow::SetTextAlignments(uint8_t newHAlign, uint8_t newVAlign)
{
	if (HAlign() != newHAlign || VAlign() != newVAlign)
	{
		HAlign() = newHAlign;
		VAlign() = newVAlign;
		AskParentForReconfigure();
	}
}

void UTextWindow::SetTextMargins(float newHMargin, float newVMargin)
{
	if (hMargin() != newHMargin || vMargin() != newVMargin)
	{
		hMargin() = newHMargin;
		vMargin() = newVMargin;
		AskParentForReconfigure();
	}
}

void UTextWindow::SetWordWrap(bool bNewWordWrap)
{
	if (bWordWrap() != bNewWordWrap)
	{
		bWordWrap() = bNewWordWrap;
		AskParentForReconfigure();
	}
}

// The original's XTextWindow::Init (0x10045af0): margins of 3, centred both
// ways, word wrap on, no line limits, the text its own accelerator.
void UTextWindow::InitDefaults()
{
	UWindow::InitDefaults();
	hMargin() = 3.0f;
	vMargin() = 3.0f;
	MinWidth() = 0.0f;
	HAlign() = (uint8_t)EHAlign::Center;
	VAlign() = (uint8_t)EVAlign::Center;
	bWordWrap() = true;
	minLines() = -1;
	MaxLines() = -1;
	Text() = "";
	EnableTextAsAccelerator(true);
}

// The original's XTextWindow::ParentRequestedPreferredSize (0x100465a0),
// measured with the window's own fonts and text settings: the text's extent
// (wrapped at the width given less the margins, with word wrap on and a
// width given), held within the line limits when the height is not given,
// plus the margins; with no text, the background's size, else as the base
// window asks; never narrower than the minimum width when no width is
// given.
void UTextWindow::ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight)
{
	UGC* gc = engine->dxgc;
	UGC::TextSettings saved = gc->UseTextSettings(this);

	if (!Text().empty())
	{
		if (bHeightSpecified || (minLines() < 0 && MaxLines() < 0))
		{
			float wrap = (bWidthSpecified && bWordWrap()) ? preferredWidth - (hMargin() + hMargin()) : 0.0f;
			gc->GetTextExtent(wrap, preferredWidth, preferredHeight, Text());
		}
		else
		{
			if (bWidthSpecified && bWordWrap())
			{
				float width = 0.0f;
				gc->GetTextExtent(preferredWidth - (hMargin() + hMargin()), width, preferredHeight, Text());
			}
			else
			{
				gc->GetTextExtent(0.0f, preferredWidth, preferredHeight, Text());
			}
			float fontHeight = gc->GetFontHeight(true);
			float height = preferredHeight;
			if (minLines() >= 0 && minLines() * fontHeight > preferredHeight)
				height = minLines() * fontHeight;
			if (MaxLines() >= 0 && preferredHeight > MaxLines() * fontHeight)
				height = MaxLines() * fontHeight;
			preferredHeight = height;
		}
		preferredWidth += hMargin() + hMargin();
		preferredHeight += vMargin() + vMargin();
	}
	else if (!Background())
	{
		// The base window's: the script's say. A width not asked for is
		// the minimum width then -- 0 for an empty text window -- and a
		// height not asked for stays -1, the window's own height.
		UWindow::ParentRequestedPreferredSize(bWidthSpecified, preferredWidth, bHeightSpecified, preferredHeight);
	}
	else
	{
		preferredHeight = (float)Background()->VSize();
		if (!bHeightSpecified)
		{
			if (minLines() >= 0)
				preferredHeight = minLines() * gc->GetFontHeight(true);
			else if (MaxLines() >= 0)
				preferredHeight = MaxLines() * gc->GetFontHeight(true);
		}
		preferredWidth = (float)Background()->USize();
	}

	if (!bWidthSpecified && preferredWidth < MinWidth())
		preferredWidth = MinWidth();

	gc->RestoreTextSettings(saved);
}

// The original's (XTextWindow::ParentRequestedGranularity 0x10046880): a
// line down -- the font's height with the line spacing -- a pixel across;
// then the script's say.
void UTextWindow::ParentRequestedGranularity(float& hGranularity, float& vGranularity)
{
	UGC* gc = engine->dxgc;
	UGC::TextSettings saved = gc->UseTextSettings(this);
	vGranularity = gc->GetFontHeight(true);
	hGranularity = 1.0f;
	gc->RestoreTextSettings(saved);
	UWindow::ParentRequestedGranularity(hGranularity, vGranularity);
}

// The original's XTextWindow::Draw (0x10046930): the GC set to the text's
// alignments and word wrap, the script's DrawWindow, then the text within
// the margins.
void UTextWindow::DrawWindow(UGC* gc)
{
	gc->SetAlignments(HAlign(), VAlign());
	gc->EnableWordWrap(bWordWrap());
	UWindow::DrawWindow(gc);
	gc->DrawText(hMargin(), vMargin(), Width() - (hMargin() + hMargin()), Height() - (vMargin() + vMargin()), Text());
}
