#include "Precomp.h"
#include "UCheckboxWindow.h"
#include "Packages/Extension/Windows/UGC.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"
#include "Engine.h"

void UCheckboxWindow::SetCheckboxColor(const Color& NewColor)
{
	checkboxColor() = NewColor;
}

void UCheckboxWindow::SetCheckboxSpacing(float newSpacing)
{
	if (checkboxSpacing() != newSpacing)
	{
		checkboxSpacing() = newSpacing;
		AskParentForReconfigure();
	}
}

void UCheckboxWindow::SetCheckboxStyle(uint8_t NewStyle)
{
	checkboxStyle() = NewStyle;
}

// The original's (XCheckboxWindow::SetCheckboxTextures 0x1000a840): a size
// left out is 0, the textures' own.
void UCheckboxWindow::SetCheckboxTextures(std::optional<UObject*> newToggleOff, std::optional<UObject*> newToggleOn, std::optional<float> newTextureWidth, std::optional<float> newTextureHeight)
{
	UTexture* off = UObject::Cast<UTexture>(newToggleOff.value_or(nullptr));
	UTexture* on = UObject::Cast<UTexture>(newToggleOn.value_or(nullptr));
	float width = newTextureWidth.value_or(0.0f);
	float height = newTextureHeight.value_or(0.0f);
	if (toggleOff() != off || toggleOn() != on || textureWidth() != width || textureHeight() != height)
	{
		toggleOff() = off;
		toggleOn() = on;
		textureWidth() = width;
		textureHeight() = height;
		AskParentForReconfigure();
	}
}

void UCheckboxWindow::ShowCheckboxOnRightSide(std::optional<bool> bRight)
{
	bRightSide() = !bRight || *bRight;
}

// The original's XCheckboxWindow::Init (0x1000a710): the box on the left, 3
// apart from the text, at the textures' size, white and masked.
void UCheckboxWindow::InitDefaults()
{
	UToggleWindow::InitDefaults();
	textureWidth() = 0.0f;
	textureHeight() = 0.0f;
	bRightSide() = false;
	checkboxColor() = { 255, 255, 255, 255 };
	checkboxSpacing() = 3.0f;
	checkboxStyle() = (uint8_t)EDrawStyle::Masked;
}

// The original's (XCheckboxWindow::ComputeTextureSize 0x1000a9c0): the size
// given, else the larger of the two textures'; the box and its spacing take
// the room beside the text, none when the box has no width.
void UCheckboxWindow::ComputeTextureSize(float& textureW, float& textureH, float& textureSpace)
{
	if (textureWidth() > 0.0f)
	{
		textureW = textureWidth();
	}
	else
	{
		textureW = 0.0f;
		if (toggleOn() && toggleOn()->USize() > 0)
			textureW = (float)toggleOn()->USize();
		if (toggleOff() && (float)toggleOff()->USize() > textureW)
			textureW = (float)toggleOff()->USize();
	}
	if (textureHeight() > 0.0f)
	{
		textureH = textureHeight();
	}
	else
	{
		textureH = 0.0f;
		if (toggleOn() && toggleOn()->VSize() > 0)
			textureH = (float)toggleOn()->VSize();
		if (toggleOff() && (float)toggleOff()->VSize() > textureH)
			textureH = (float)toggleOff()->VSize();
	}
	textureSpace = textureW > 0.0f ? checkboxSpacing() + textureW : 0.0f;
}

// The original's (XCheckboxWindow::Draw 0x1000aad0): the script's DrawWindow,
// then the text in the button's colour for its state beside the box, a line
// down from the top margin, and the box -- on or off -- centred down the
// window. No button texture is drawn.
void UCheckboxWindow::DrawWindow(UGC* gc)
{
	gc->SetAlignments(HAlign(), VAlign());
	gc->EnableWordWrap(bWordWrap());
	UWindow::DrawWindow(gc);

	UTexture* tex = bButtonPressed() ? toggleOn() : toggleOff();
	float textureW = 0.0f, textureH = 0.0f, textureSpace = 0.0f;
	ComputeTextureSize(textureW, textureH, textureSpace);
	float boxY = (float)(int)((Height() - textureH) * 0.5f);
	float textW = Width() - (hMargin() + hMargin() + textureSpace);
	float textH = Height() - (vMargin() + vMargin());
	float textX, boxX;
	if (bRightSide())
	{
		textX = hMargin();
		boxX = Width() - (textureSpace + hMargin());
	}
	else
	{
		boxX = hMargin();
		textX = textureSpace + hMargin();
	}

	Color textColors[6] = { TextColors.Normal, TextColors.Pressed, TextColors.NormalFocus, TextColors.PressedFocus, TextColors.NormalInsensitive, TextColors.PressedInsensitive };
	gc->SetTextColor(textColors[AppearanceState()]);
	gc->DrawText(textX, vMargin() + 1.0f, textW, textH, Text());

	if (tex)
	{
		gc->SetStyle((EDrawStyle)checkboxStyle());
		gc->SetTileColor(checkboxColor());
		gc->DrawIconPattern(boxX, boxY, textureW, textureH, 0.0f, 0.0f, textureW, textureH, tex);
	}
}

// The original's (XCheckboxWindow::ParentRequestedPreferredSize 0x1000acb0):
// as a text window measures, plus the box's room beside the text; never
// smaller than the box within the margins.
void UCheckboxWindow::ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight)
{
	UGC* gc = engine->dxgc;
	UGC::TextSettings saved = gc->UseTextSettings(this);

	float textureW = 0.0f, textureH = 0.0f, textureSpace = 0.0f;
	ComputeTextureSize(textureW, textureH, textureSpace);

	if (!Text().empty())
	{
		if (bHeightSpecified || (minLines() < 0 && MaxLines() < 0))
		{
			float wrap = (bWidthSpecified && bWordWrap()) ? preferredWidth - (hMargin() + hMargin() + textureSpace) : 0.0f;
			gc->GetTextExtent(wrap, preferredWidth, preferredHeight, Text());
		}
		else
		{
			// A width given is the wrap width as it is, and stays the width.
			if (bWidthSpecified)
			{
				float width = 0.0f;
				gc->GetTextExtent(preferredWidth, width, preferredHeight, Text());
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
		preferredWidth = hMargin() + hMargin() + preferredWidth + textureSpace;
		preferredHeight = vMargin() + vMargin() + preferredHeight;
		gc->RestoreTextSettings(saved);
	}
	else if (!Background())
	{
		gc->RestoreTextSettings(saved);
		UTextWindow::ParentRequestedPreferredSize(bWidthSpecified, preferredWidth, bHeightSpecified, preferredHeight);
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
		gc->RestoreTextSettings(saved);
	}

	preferredHeight = std::max(preferredHeight, vMargin() + vMargin() + textureH);
	preferredWidth = std::max(preferredWidth, hMargin() + hMargin() + textureW);
}
