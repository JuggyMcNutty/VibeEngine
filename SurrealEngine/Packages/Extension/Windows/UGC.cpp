
#include "Precomp.h"
#include "UGC.h"
#include "Engine.h"
#include "Render/RenderSubsystem.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Resources/UFont.h"
#include "Packages/Engine/Resources/UPalette.h"

// |p0 to |p7's colours (XGC::GetNextChar's palette, Extension.dll 0x100294d0).
static const Color TextPalette[8] =
{
	{ 0, 0, 0, 255 }, { 255, 255, 255, 255 }, { 255, 0, 0, 255 }, { 0, 255, 0, 255 },
	{ 255, 255, 0, 255 }, { 0, 0, 255, 255 }, { 255, 0, 255, 255 }, { 0, 255, 255, 255 }
};

void UGC::ClearZ()
{
	// Only used by ActorDisplayWindow.DrawWindow
	engine->render->Device->ClearZ();
}

void UGC::CopyGC(UObject* Copy)
{
	// Not used directly by scripts
	LogUnimplemented("GC.CopyGC");
}

// The actor draws through the renderer into the scene being drawn: with the
// GC's style, the glow and unlit given, its draw scale multiplied and a given
// skin replacing every skin, as if not hidden, over the depth buffer when
// bClearZ asks -- and all of it put back afterwards (extension-dll.md, Actors
// in a window). The vision augmentation passes no skin (its script swaps the
// skins itself), never constrains and never clears; bConstrain is not
// honoured, the augmentation's calls covering the whole view.
void UGC::DrawActor(UObject* Actor, std::optional<bool> bClearZ, std::optional<bool> bConstrain, std::optional<bool> bUnlit, std::optional<float> DrawScale, std::optional<float> ScaleGlow, std::optional<UObject*> Skin)
{
	if (!bDrawEnabled())
		return;

	UActor* actor = UObject::TryCast<UActor>(Actor);
	if (!actor)
		return;

	// UActor::Style() reads by value; the write goes through the property.
	uint8_t& actorStyle = actor->Value<uint8_t>(PropOffsets_Actor.Style);
	uint8_t oldStyle = actorStyle;
	float oldScaleGlow = actor->ScaleGlow();
	bool oldUnlit = actor->bUnlit();
	float oldDrawScale = actor->DrawScale();
	UTexture* oldSkin = actor->Skin();
	UTexture* oldMultiSkins[8];
	for (int i = 0; i < 8; i++)
		oldMultiSkins[i] = actor->MultiSkins()[i];

	actorStyle = Style();
	if (ScaleGlow)
		actor->ScaleGlow() = *ScaleGlow;
	if (bUnlit)
		actor->bUnlit() = *bUnlit;
	if (DrawScale)
		actor->DrawScale() *= *DrawScale;
	if (Skin && *Skin)
	{
		UTexture* tex = UObject::TryCast<UTexture>(*Skin);
		actor->Skin() = tex;
		for (int i = 0; i < 8; i++)
			actor->MultiSkins()[i] = tex;
	}

	engine->render->DrawActor(actor, false, bClearZ.value_or(false));

	actorStyle = oldStyle;
	actor->ScaleGlow() = oldScaleGlow;
	actor->bUnlit() = oldUnlit;
	actor->DrawScale() = oldDrawScale;
	actor->Skin() = oldSkin;
	for (int i = 0; i < 8; i++)
		actor->MultiSkins()[i] = oldMultiSkins[i];
}

// The original's XGC::DrawText (0x10028180): the text clipped to its box,
// placed down it by the vertical alignment, broken into lines at the word
// wrap's width (the box's, cut to whole pixels, with word wrap on; none
// with it off), each line placed across by the horizontal alignment (a
// centred one by whole pixels), a line as tall as its tallest character or
// a space, plus the GC's line spacing.
void UGC::DrawText(float DestX, float DestY, float destWidth, float destHeight, const std::string& textStr)
{
	if (!bDrawEnabled())
		return;

	float wrap = bWordWrap() ? (float)(int)destWidth : 0.0f;

	PushClip(ScaleRect(Rectf::xywh(offsetX + DestX, offsetY + DestY, destWidth, destHeight)));

	auto valign = (EVAlign)VAlign();
	if (valign != EVAlign::Top)
	{
		float textWidth = 0.0f, textHeight = 0.0f;
		GetTextExtent(wrap, textWidth, textHeight, textStr);
		if (valign == EVAlign::Center)
			DestY += (float)((int)(destHeight - textHeight) / 2);
		else if (valign == EVAlign::Bottom)
			DestY = DestY + destHeight - textHeight;
	}

	TextState state;
	state.color = TextColor();
	bool bold = false;
	UFont* font = normalFont();
	auto halign = (EHAlign)HAlign();
	const uint8_t* text = (const uint8_t*)textStr.data();
	const uint8_t* end = text + textStr.size();
	while (true)
	{
		TextState lineState = state;
		float lineWidth = 0.0f;
		int lineLength = 0;
		const uint8_t* line = GetLine(lineState, wrap, text, end, lineWidth, lineLength);
		if (!line)
			break;

		float x = DestX;
		if (halign == EHAlign::Center)
			x = (float)((int)(destWidth - lineWidth) / 2) + DestX;
		else if (halign == EHAlign::Right)
			x = DestX + destWidth - lineWidth;

		float lineHeight = 0.0f;
		const uint8_t* c = line;
		const uint8_t* lineEnd = line + lineLength;
		while (uint8_t ch = GetNextChar(c, state, lineEnd))
		{
			if (state.bold != bold)
			{
				font = state.bold ? boldFont() : normalFont();
				bold = state.bold;
			}
			if (ch != '\n' && font)
			{
				float charHeight = 0.0f;
				DrawChar(font, ch, state.color, state.accel, x, DestY, x, charHeight);
				lineHeight = std::max(lineHeight, charHeight);
			}
		}
		if (lineHeight < 1.0f && font)
			lineHeight = (float)font->GetPageGlyph(' ').VSize;
		DestY += lineHeight + textVSpacing();
		state = lineState;
	}

	PopClip();
}

void UGC::DrawBorders(float DestX, float DestY, float destWidth, float destHeight, float leftMargin, float rightMargin, float TopMargin, float BottomMargin, UObject** borders, std::optional<bool> bStretchHorizontally, std::optional<bool> bStretchVertically)
{
	// The original's XGC::DrawBorders (Extension.dll 0x10028df0;
	// extension-dll.md, Borders): nine pieces laid out by four margins.
	if (!bDrawEnabled() || destWidth <= 0.0f || destHeight <= 0.0f)
		return;

	bool stretchAcross = bStretchHorizontally && *bStretchHorizontally;
	bool stretchDown = bStretchVertically && *bStretchVertically;

	UTexture* tl = UObject::Cast<UTexture>(borders[0]);
	UTexture* tr = UObject::Cast<UTexture>(borders[1]);
	UTexture* bl = UObject::Cast<UTexture>(borders[2]);
	UTexture* br = UObject::Cast<UTexture>(borders[3]);
	UTexture* left = UObject::Cast<UTexture>(borders[4]);
	UTexture* right = UObject::Cast<UTexture>(borders[5]);
	UTexture* top = UObject::Cast<UTexture>(borders[6]);
	UTexture* bottom = UObject::Cast<UTexture>(borders[7]);
	UTexture* center = UObject::Cast<UTexture>(borders[8]);

	DestX += offsetX;
	DestY += offsetY;

	// Each side's margin is the largest of its own textures -- left from the
	// two left corners and the left edge, and so on -- and a margin given
	// above 0 replaces it. A box narrower or shorter than two of them has both
	// shrink in proportion.
	auto U = [](UTexture* tex) { return tex ? (float)tex->USize() : 0.0f; };
	auto V = [](UTexture* tex) { return tex ? (float)tex->VSize() : 0.0f; };
	float marginLeft = std::max({ U(tl), U(bl), U(left) });
	float marginRight = std::max({ U(tr), U(br), U(right) });
	float marginTop = std::max({ V(tl), V(tr), V(top) });
	float marginBottom = std::max({ V(bl), V(br), V(bottom) });
	if (leftMargin > 0.0f)
		marginLeft = leftMargin;
	if (rightMargin > 0.0f)
		marginRight = rightMargin;
	if (TopMargin > 0.0f)
		marginTop = TopMargin;
	if (BottomMargin > 0.0f)
		marginBottom = BottomMargin;

	float innerWidth = destWidth - (marginLeft + marginRight);
	float innerHeight = destHeight - (marginTop + marginBottom);
	if (innerWidth < 0.0f)
	{
		float scale = destWidth / (marginLeft + marginRight);
		innerWidth = 0.0f;
		marginLeft *= scale;
		marginRight *= scale;
	}
	if (innerHeight < 0.0f)
	{
		float scale = destHeight / (marginTop + marginBottom);
		innerHeight = 0.0f;
		marginTop *= scale;
		marginBottom *= scale;
	}

	float x1 = DestX + marginLeft;
	float x2 = x1 + innerWidth;
	float y1 = DestY + marginTop;
	float y2 = y1 + innerHeight;

	// Each piece fills its band of the box, read from the texture so that its
	// inner side lies on the margin line (a corner's inner corner, an edge's
	// inner side), as the original's DrawIconPattern draws it: a source size
	// of 0 tiles one texel a pixel, any other stretches -- the edges and the
	// centre tile unless stretching is asked for, which the game never asks.
	auto piece = [&](UTexture* tex, float x, float y, float w, float h, float sx, float sy, float sw, float sh)
		{
			if (!tex)
				return;
			Rectf dest = Rectf::xywh(x, y, w, h);
			Rectf src = Rectf::xywh(sx, sy, sw != 0.0f ? sw : w, sh != 0.0f ? sh : h);
			DrawTile(tex, ScaleRect(dest), src, tileColor(), EffectivePolyFlags());
		};
	piece(tl, DestX, DestY, marginLeft, marginTop, U(tl) - marginLeft, V(tl) - marginTop, marginLeft, marginTop);
	piece(tr, x2, DestY, marginRight, marginTop, 0.0f, V(tr) - marginTop, marginRight, marginTop);
	piece(bl, DestX, y2, marginLeft, marginBottom, U(bl) - marginLeft, 0.0f, marginLeft, marginBottom);
	piece(br, x2, y2, marginRight, marginBottom, 0.0f, 0.0f, marginRight, marginBottom);
	piece(left, DestX, y1, marginLeft, innerHeight, U(left) - marginLeft, 0.0f, marginLeft, stretchDown ? V(left) : 0.0f);
	piece(right, x2, y1, marginRight, innerHeight, 0.0f, 0.0f, marginRight, stretchDown ? V(right) : 0.0f);
	piece(top, x1, DestY, innerWidth, marginTop, 0.0f, V(top) - marginTop, stretchAcross ? U(top) : 0.0f, marginTop);
	piece(bottom, x1, y2, innerWidth, marginBottom, 0.0f, 0.0f, stretchAcross ? U(bottom) : 0.0f, marginBottom);
	piece(center, x1, y1, innerWidth, innerHeight, 0.0f, 0.0f, stretchAcross ? U(center) : 0.0f, stretchDown ? V(center) : 0.0f);
}

void UGC::DrawBox(float DestX, float DestY, float destWidth, float destHeight, float OrgX, float OrgY, float boxThickness, UObject* tX)
{
	if (!bDrawEnabled())
		return;

	UTexture* tex = UObject::Cast<UTexture>(tX);
	if (tex)
	{
		float swidth = (float)tex->USize();
		float sheight = (float)tex->VSize();
		Rectf src = Rectf::xywh(0.0f, 0.0f, swidth, sheight);
		uint32_t polyflags = EffectivePolyFlags();
		Color color = tileColor();

		Rectf top = Rectf::xywh(offsetX + DestX, offsetY + DestY, destWidth, boxThickness);
		Rectf bottom = Rectf::xywh(offsetX + DestX, offsetY + DestY + destHeight - boxThickness, destWidth, boxThickness);
		Rectf left = Rectf::xywh(offsetX + DestX, offsetY + DestY, boxThickness, destHeight);
		Rectf right = Rectf::xywh(offsetX + DestX + destWidth - boxThickness, offsetY + DestY, boxThickness, destHeight);

		DrawTile(tex, ScaleRect(top), src, color, polyflags);
		DrawTile(tex, ScaleRect(bottom), src, color, polyflags);
		DrawTile(tex, ScaleRect(left), src, color, polyflags);
		DrawTile(tex, ScaleRect(right), src, color, polyflags);
	}
}

void UGC::DrawIcon(float DestX, float DestY, UObject* tX)
{
	if (!bDrawEnabled())
		return;

	UTexture* tex = UObject::Cast<UTexture>(tX);
	if (tex)
	{
		float swidth = (float)tex->USize();
		float sheight = (float)tex->VSize();
		Rectf dest = Rectf::xywh(offsetX + DestX, offsetY + DestY, swidth, sheight);
		Rectf src = Rectf::xywh(0.0f, 0.0f, swidth, sheight);
		DrawTile(tex, ScaleRect(dest), src, tileColor(), EffectivePolyFlags());
	}
}

// The original's XGC::DrawIconPattern (0x10028770): the texture from the
// origin, stretched over the box along an axis given a source size and tiled
// 1:1 along one given 0.
void UGC::DrawIconPattern(float DestX, float DestY, float destWidth, float destHeight, float OrgX, float OrgY, float srcWidth, float srcHeight, UTexture* tex)
{
	if (!bDrawEnabled() || !tex)
		return;

	Rectf dest = Rectf::xywh(offsetX + DestX, offsetY + DestY, destWidth, destHeight);
	Rectf src = Rectf::xywh(OrgX, OrgY, srcWidth != 0.0f ? srcWidth : destWidth, srcHeight != 0.0f ? srcHeight : destHeight);
	DrawTile(tex, ScaleRect(dest), src, tileColor(), EffectivePolyFlags());
}

void UGC::DrawPattern(float DestX, float DestY, float destWidth, float destHeight, float OrgX, float OrgY, UObject* tX)
{
	if (!bDrawEnabled())
		return;

	UTexture* tex = UObject::Cast<UTexture>(tX);
	if (tex)
	{
		Rectf dest = Rectf::xywh(offsetX + DestX, offsetY + DestY, destWidth, destHeight);
		Rectf src = Rectf::xywh(OrgX, OrgY, destWidth, destHeight);
		DrawTile(tex, ScaleRect(dest), src, tileColor(), EffectivePolyFlags());
	}
}

void UGC::DrawStretchedTexture(float DestX, float DestY, float destWidth, float destHeight, float srcX, float srcY, float srcWidth, float srcHeight, UObject* tX)
{
	if (!bDrawEnabled())
		return;

	UTexture* tex = UObject::Cast<UTexture>(tX);
	if (tex)
	{
		Rectf dest = Rectf::xywh(offsetX + DestX, offsetY + DestY, destWidth, destHeight);
		Rectf src = Rectf::xywh(srcX, srcY, srcWidth, srcHeight);
		DrawTile(tex, ScaleRect(dest), src, tileColor(), EffectivePolyFlags());
	}
}

void UGC::DrawTexture(float DestX, float DestY, float destWidth, float destHeight, float srcX, float srcY, UObject* tX)
{
	if (!bDrawEnabled())
		return;

	UTexture* tex = UObject::Cast<UTexture>(tX);
	if (tex)
	{
		Rectf dest = Rectf::xywh(offsetX + DestX, offsetY + DestY, destWidth, destHeight);
		Rectf src = Rectf::xywh(srcX, srcY, destWidth, destHeight);
		DrawTile(tex, ScaleRect(dest), src, tileColor(), EffectivePolyFlags());
	}
}

Rectf UGC::ScaleRect(const Rectf& box)
{
	float scale = UWindow::GetVirtualScale();
	return Rectf(box.left * scale, box.top * scale, box.right * scale, box.bottom * scale);
}

uint32_t UGC::EffectivePolyFlags()
{
	uint32_t polyflags = PolyFlags();
	if (bMasked())
		polyflags |= PF_Masked;
	if (bModulated())
		polyflags |= PF_Modulated;
	if (!bSmoothed())
		polyflags |= PF_NoSmooth;
	if (bTranslucent())
		polyflags |= PF_Translucent;
	return polyflags;
}

uint32_t UGC::EffectiveTextPolyFlags()
{
	uint32_t polyflags = textPolyFlags() | PF_NoSmooth;
	if (bTextTranslucent())
		polyflags |= PF_Translucent;
	else
		polyflags |= PF_Masked;
	return polyflags;
}

void UGC::EnableDrawing(bool newDrawEnabled)
{
	// Only set by ActorDisplayWindow.DrawWindow and always set to true.
	bDrawEnabled() = newDrawEnabled;
}

void UGC::EnableMasking(bool bNewMasking)
{
	bMasked() = bNewMasking;
}

void UGC::EnableModulation(bool bNewModulation)
{
	bModulated() = bNewModulation;
}

void UGC::EnableSmoothing(bool bNewSmoothing)
{
	bSmoothed() = bNewSmoothing;
}

void UGC::EnableSpecialText(bool bNewSpecialText)
{
	SpecialTextEnabled = bNewSpecialText;
}

void UGC::EnableTranslucency(bool bNewTranslucency)
{
	bTranslucent() = bNewTranslucency;
}

void UGC::EnableTranslucentText(bool bNewTranslucency)
{
	bTextTranslucent() = bNewTranslucency;
}

void UGC::EnableWordWrap(bool bNewWordWrap)
{
	bWordWrap() = bNewWordWrap;
}

void UGC::GetAlignments(uint8_t& outHAlign, uint8_t& outVAlign)
{
	outHAlign = HAlign();
	outVAlign = VAlign();
}

// The taller of the two fonts' spaces, with the line spacing when asked
// (XGC::GetFontHeight, 0x10027d60).
float UGC::GetFontHeight(std::optional<bool> bIncludeSpace)
{
	float height = 0.0f;
	if (normalFont())
		height = std::max(height, (float)normalFont()->GetPageGlyph(' ').VSize);
	if (boldFont())
		height = std::max(height, (float)boldFont()->GetPageGlyph(' ').VSize);
	if (bIncludeSpace.value_or(false))
		height += textVSpacing();
	return height;
}

void UGC::GetFonts(UObject*& outNormalFont, UObject*& outBoldFont)
{
	outNormalFont = normalFont();
	outBoldFont = boldFont();
}

uint8_t UGC::GetHorizontalAlignment()
{
	return HAlign();
}

uint8_t UGC::GetStyle()
{
	return Style();
}

void UGC::GetTextColor(Color& outTextColor)
{
	outTextColor = TextColor();
}

// The text's size as DrawText lays it out at that wrap width (0 for none):
// its widest line, and its lines' heights each with the line spacing
// (XGC::GetTextExtent, 0x10027bb0).
void UGC::GetTextExtent(float destWidth, float& xExtent, float& yExtent, const std::string& textStr)
{
	TextState state;
	bool bold = false;
	UFont* font = normalFont();
	float width = 0.0f, height = 0.0f, lineHeight = 0.0f;
	const uint8_t* text = (const uint8_t*)textStr.data();
	const uint8_t* end = text + textStr.size();
	while (true)
	{
		float lineWidth = 0.0f;
		int lineLength = 0;
		const uint8_t* line = GetLine(state, destWidth, text, end, lineWidth, lineLength);
		if (!line)
			break;
		const uint8_t* c = line;
		const uint8_t* lineEnd = line + lineLength;
		while (uint8_t ch = GetNextChar(c, state, lineEnd))
		{
			if (bold != state.bold)
			{
				font = state.bold ? boldFont() : normalFont();
				bold = state.bold;
			}
			if (ch != '\n' && font)
				lineHeight = std::max(lineHeight, (float)font->GetPageGlyph(ch).VSize);
		}
		if (lineHeight < 1.0f && font)
			lineHeight = (float)font->GetPageGlyph(' ').VSize;
		width = std::max(width, lineWidth);
		height += lineHeight + textVSpacing();
		lineHeight = 0.0f;
	}
	xExtent = width;
	yExtent = height;
}

float UGC::GetTextVSpacing()
{
	return textVSpacing();
}

void UGC::GetTileColor(Color& outTileColor)
{
	outTileColor = tileColor();
}

uint8_t UGC::GetVerticalAlignment()
{
	return VAlign();
}

void UGC::Intersect(float ClipX, float ClipY, float clipWidth, float clipHeight)
{
	// Not called directly by script. Seems we only clip by window then?
	LogUnimplemented("GC.Intersect");
}

bool UGC::IsDrawingEnabled()
{
	return bDrawEnabled();
}

bool UGC::IsMaskingEnabled()
{
	return bMasked();
}

bool UGC::IsModulationEnabled()
{
	return bModulated();
}

bool UGC::IsSmoothingEnabled()
{
	return bSmoothed();
}

bool UGC::IsSpecialTextEnabled()
{
	return SpecialTextEnabled;
}

bool UGC::IsTranslucencyEnabled()
{
	return bTranslucent();
}

bool UGC::IsTranslucentTextEnabled()
{
	return bTextTranslucent();
}

bool UGC::IsWordWrapEnabled()
{
	return bWordWrap();
}

void UGC::PopGC(std::optional<int> gcNum)
{
	// Not used directly by scripts
	LogUnimplemented("GC.PopGC");
}

int UGC::PushGC()
{
	// Not used directly by scripts
	LogUnimplemented("GC.PushGC");
	return 0;
}

void UGC::SetAlignments(uint8_t newHAlign, uint8_t newVAlign)
{
	HAlign() = newHAlign;
	VAlign() = newVAlign;
}

void UGC::SetBaselineData(std::optional<float> newBaselineOffset, std::optional<float> newUnderlineHeight)
{
	if (newBaselineOffset)
		baselineOffset() = *newBaselineOffset;
	if (newUnderlineHeight)
		underlineHeight() = *newUnderlineHeight;
}

void UGC::SetBoldFont(UObject* newBoldFont)
{
	boldFont() = UObject::Cast<UFont>(newBoldFont);
}

void UGC::SetFont(UObject* NewFont)
{
	normalFont() = UObject::Cast<UFont>(NewFont);
	boldFont() = UObject::Cast<UFont>(NewFont);
}

void UGC::SetFonts(UObject* newNormalFont, UObject* newBoldFont)
{
	normalFont() = UObject::Cast<UFont>(newNormalFont);
	boldFont() = UObject::Cast<UFont>(newBoldFont);
}

void UGC::SetHorizontalAlignment(uint8_t newHAlign)
{
	HAlign() = newHAlign;
}

void UGC::SetNormalFont(UObject* newNormalFont)
{
	normalFont() = UObject::Cast<UFont>(newNormalFont);
}

// The original's (Extension.dll XGC::SetStyle 0x10027170): DSTY_None stops
// drawing and leaves the rest; any other style draws, masked, translucent or
// modulated or none of them. Text keeps its own flags, which only
// EnableTranslucentText sets: text is never modulated.
void UGC::SetStyle(EDrawStyle NewStyle)
{
	Style() = (uint8_t)NewStyle;
	if (NewStyle == EDrawStyle::None)
	{
		bDrawEnabled() = false;
		return;
	}
	bDrawEnabled() = true;
	bMasked() = NewStyle == EDrawStyle::Masked;
	bTranslucent() = NewStyle == EDrawStyle::Translucent;
	bModulated() = NewStyle == EDrawStyle::Modulated;
}

void UGC::SetTextColor(const Color& newTextColor)
{
	TextColor() = newTextColor;
}

void UGC::SetTextVSpacing(float newVSpacing)
{
	textVSpacing() = newVSpacing;
}

void UGC::SetTileColor(const Color& newTileColor)
{
	tileColor() = newTileColor;
}

void UGC::SetVerticalAlignment(uint8_t newVAlign)
{
	VAlign() = newVAlign;
}

void UGC::DrawTile(UTexture* tex, const Rectf& dest, const Rectf& src, const Color& c, uint32_t flags)
{
	vec4 color(c.R / 255.0f, c.G / 255.0f, c.B / 255.0f, 1.0f/*c.A / 255.0f*/);
	float Z = 1.0f;
	vec4 fog(0.0f);

	TextureInfo texinfo;
	texinfo.CacheID = (uint64_t)(ptrdiff_t)tex;
	texinfo.Texture = tex;
	texinfo.Format = texinfo.Texture->UsedFormat;
	texinfo.Mips = tex->UsedMipmaps.data();
	texinfo.NumMips = (int)tex->UsedMipmaps.size();
	texinfo.USize = tex->USize();
	texinfo.VSize = tex->VSize();
	if (tex->Palette())
		texinfo.Palette = (TextureColor*)tex->Palette()->Colors.data();

	if (dest.left >= dest.right || dest.top >= dest.bottom)
		return;

	// A texture whose pixels changed since it was last drawn goes to the
	// device again (a snapshot taken into the same texture, for one).
	texinfo.bRealtimeChanged = tex->TextureModified;
	tex->TextureModified = false;

	if (dest.left >= clipBox.left && dest.top >= clipBox.top && dest.right <= clipBox.right && dest.bottom <= clipBox.bottom)
	{
		engine->render->DrawTile(
			texinfo,
			dest.left,
			dest.top,
			dest.right - dest.left,
			dest.bottom - dest.top,
			src.left,
			src.top,
			src.right - src.left,
			src.bottom - src.top,
			Z,
			color,
			fog,
			flags);
	}
	else
	{
		Rectf d = dest;
		Rectf s = src;

		float scaleX = (s.right - s.left) / (d.right - d.left);
		float scaleY = (s.bottom - s.top) / (d.bottom - d.top);

		if (d.left < clipBox.left)
		{
			s.left += scaleX * (clipBox.left - d.left);
			d.left = clipBox.left;
		}
		if (d.right > clipBox.right)
		{
			s.right += scaleX * (clipBox.right - d.right);
			d.right = clipBox.right;
		}
		if (d.top < clipBox.top)
		{
			s.top += scaleY * (clipBox.top - d.top);
			d.top = clipBox.top;
		}
		if (d.bottom > clipBox.bottom)
		{
			s.bottom += scaleY * (clipBox.bottom - d.bottom);
			d.bottom = clipBox.bottom;
		}

		if (d.left < d.right && d.top < d.bottom)
			engine->render->DrawTile(
				texinfo,
				d.left,
				d.top,
				d.right - d.left,
				d.bottom - d.top,
				s.left,
				s.top,
				s.right - s.left,
				s.bottom - s.top,
				Z, color, fog, flags);
	}
}

// The next character to draw, the codes before it applied to the state
// (XGC::GetNextChar, 0x100294d0). With special text on: |b bold, |c and up
// to three hex bytes a colour, |p0 to |p7 a palette colour, |& the next
// character an accelerator (underlined), each undone by |! (|!c and |!p
// back to the GC's text colour); any other |x is x itself. 0 at the end.
uint8_t UGC::GetNextChar(const uint8_t*& p, TextState& state, const uint8_t* end)
{
	auto lower = [](uint8_t c) -> uint8_t { return (c >= 'A' && c <= 'Z') ? c + 32 : c; };

	state.accel = false;
	while (p < end)
	{
		if (!SpecialTextEnabled || *p != '|')
			return *p++;
		p++;
		if (p >= end)
			break;
		bool inverse = false;
		uint8_t code = lower(*p);
		if (code == '!')
		{
			inverse = true;
			p++;
			if (p >= end)
				break;
			code = lower(*p);
		}
		switch (code)
		{
		case 'b':
			p++;
			state.bold = !inverse;
			break;
		case 'c':
			p++;
			if (inverse)
				state.color = TextColor();
			else
				ReadColor(p, state.color, end);
			state.ownColor = !inverse;
			break;
		case 'p':
			p++;
			if (p < end)
			{
				if (inverse)
				{
					state.color = TextColor();
					state.ownColor = false;
				}
				else if (*p >= '0' && *p < '8')
				{
					state.color = TextPalette[*p - '0'];
					state.ownColor = true;
					p++;
				}
			}
			break;
		case '&':
			p++;
			state.accel = true;
			break;
		default:
			return *p++;
		}
	}
	p = std::min(p, end);
	return 0;
}

// |c's colour: up to three bytes of two hex digits each, red, green, blue;
// a byte cut short ends it (XGC::ReadColor and GetColorByte, 0x100293d0,
// 0x100292f0).
void UGC::ReadColor(const uint8_t*& p, Color& color, const uint8_t* end)
{
	auto hex = [](uint8_t c, int& value) -> bool
	{
		if (c >= '0' && c <= '9') { value = c - '0'; return true; }
		if (c >= 'a' && c <= 'f') { value = c - 'a' + 10; return true; }
		if (c >= 'A' && c <= 'F') { value = c - 'A' + 10; return true; }
		return false;
	};
	auto colorByte = [&](uint8_t& out) -> bool
	{
		out = 0;
		int value = 0;
		if (p >= end || !hex(*p, value))
			return false;
		out = (uint8_t)(value << 4);
		p++;
		if (p < end && hex(*p, value))
		{
			out |= (uint8_t)value;
			p++;
			return true;
		}
		return false;
	};
	uint8_t r = 0, g = 0, b = 0;
	if (colorByte(r) && colorByte(g))
		colorByte(b);
	color = { r, g, b, 255 };
}

// One line from the text (XGC::ParseLine, 0x10029ae0): up to a line break
// or the text's end, or, past the wrap width (none at 0 or less), to the
// last space before the word that crossed it -- or before the character
// that did, with no space on the line. The first character always goes.
// Its width counts the spaces it ends with, but for a line broken at them.
// False when no line is left: the text's end, unless a line break came
// last.
bool UGC::ParseLine(const uint8_t* text, TextState state, const uint8_t* end, float wrap, const uint8_t*& next, TextState& stateOut, int& length, float& width)
{
	const uint8_t* lineStart = text;
	const uint8_t* wordStart = nullptr;
	const uint8_t* spaceStart = nullptr;
	TextState wordState;
	bool wrapped = false;
	bool firstChar = true;
	bool inSpace = true;
	if (wrap <= 0.0f)
		wrap = 500000.0f;
	UFont* font = state.bold ? boldFont() : normalFont();
	bool afterBreak = state.afterBreak;
	state.afterBreak = false;
	float lineWidth = 0.0f;
	float widthAtSpace = 0.0f;

	const uint8_t* charStart = text;
	const uint8_t* lineEnd = nullptr;
	float resultWidth = 0.0f;
	uint8_t ch = 0;
	while (true)
	{
		charStart = text;
		TextState before = state;
		ch = GetNextChar(text, state, end);
		if (ch == 0 || ch == '\n')
			break;
		if ((ch >= 9 && ch <= 13) || ch == ' ')
		{
			if (!inSpace)
			{
				inSpace = true;
				spaceStart = charStart;
				widthAtSpace = lineWidth;
			}
		}
		else if (inSpace)
		{
			if (wrapped)
			{
				// Past the wrap in the spaces after a word: the line ends at
				// them, the next starts with this word.
				state = before;
				next = charStart;
				lineEnd = spaceStart;
				resultWidth = widthAtSpace;
				goto done;
			}
			inSpace = false;
			wordStart = charStart;
			wordState = before;
		}
		if (before.bold != state.bold)
			font = state.bold ? boldFont() : normalFont();
		if (font)
		{
			float newWidth = (float)font->GetPageGlyph(ch).USize + lineWidth;
			if (newWidth > wrap && !firstChar)
			{
				if (!spaceStart)
				{
					state = before;
					next = charStart;
					lineEnd = charStart;
					resultWidth = lineWidth;
					goto done;
				}
				if (wordStart > spaceStart)
				{
					state = wordState;
					next = wordStart;
					lineEnd = spaceStart;
					resultWidth = widthAtSpace;
					goto done;
				}
				wrapped = true;
			}
			lineWidth = newWidth;
		}
		firstChar = false;
	}
	if (wrapped)
	{
		lineEnd = spaceStart;
		resultWidth = widthAtSpace;
	}
	else
	{
		lineEnd = charStart;
		resultWidth = lineWidth;
	}
	if (ch == '\n')
		state.afterBreak = true;
	next = text;

done:
	stateOut = state;
	length = (int)(lineEnd - lineStart);
	width = resultWidth;
	return next > lineStart || afterBreak;
}

// The next line, its start, or none (XGC::GetLine, 0x10029d80).
const uint8_t* UGC::GetLine(TextState& state, float wrap, const uint8_t*& text, const uint8_t* end, float& width, int& length)
{
	if (!text)
		return nullptr;
	const uint8_t* lineStart = text;
	if (!ParseLine(lineStart, state, end, wrap, text, state, length, width))
		return nullptr;
	return lineStart;
}

// One character at a place in the window, moving the place on by its width
// and giving its height (XGC::DrawChar, 0x10029e40): at whole pixels, clipped
// to the GC's box; an accelerator underlined with the underline texture,
// underlineHeight tall, baselineOffset up from the character's foot, one
// pixel short of its width.
void UGC::DrawChar(UFont* font, uint8_t ch, const Color& color, bool accel, float x, float y, float& outX, float& outHeight)
{
	FontGlyph glyph = font->GetPageGlyph(ch);
	float px = x + offsetX;
	float py = y + offsetY;
	outX = x + (float)glyph.USize;
	outHeight = (float)glyph.VSize;

	auto whole = [](float v) { return (float)(int64_t)(v + 0.1f); };
	uint32_t polyflags = EffectiveTextPolyFlags();
	if (glyph.Texture && glyph.USize > 0 && glyph.VSize > 0)
	{
		Rectf dest = Rectf::xywh(whole(px), whole(py), (float)glyph.USize, (float)glyph.VSize);
		Rectf src = Rectf::xywh((float)glyph.StartU, (float)glyph.StartV, (float)glyph.USize, (float)glyph.VSize);
		DrawTile(glyph.Texture, ScaleRect(dest), src, color, polyflags);
	}
	if (accel && underlineTexture() && underlineHeight() > 0.0f && glyph.USize > 1)
	{
		float w = (float)(glyph.USize - 1);
		Rectf dest = Rectf::xywh(whole(px), whole((float)glyph.VSize - baselineOffset() + py), whole(w), whole(underlineHeight()));
		Rectf src = Rectf::xywh(0.0f, 0.0f, dest.right - dest.left, dest.bottom - dest.top);
		DrawTile(underlineTexture(), ScaleRect(dest), src, color, polyflags);
	}
}

void UGC::ResetClip(Rectf box)
{
	clipStack.clear();
	clipBox = box;
}

void UGC::PushClip(Rectf box)
{
	clipStack.push_back(clipBox);
	clipBox.left = std::max(clipBox.left, box.left);
	clipBox.top = std::max(clipBox.top, box.top);
	clipBox.right = std::min(clipBox.right, box.right);
	clipBox.bottom = std::min(clipBox.bottom, box.bottom);
	clipBox.right = std::max(clipBox.right, clipBox.left);
	clipBox.bottom = std::max(clipBox.bottom, clipBox.top);
}

void UGC::PopClip()
{
	clipBox = clipStack.back();
	clipStack.pop_back();
}

UGC::TextSettings UGC::UseTextSettings(UWindow* window)
{
	TextSettings saved = { normalFont(), boldFont(), SpecialTextEnabled, textVSpacing() };
	normalFont() = window->normalFont();
	boldFont() = window->boldFont();
	SpecialTextEnabled = window->bSpecialText();
	textVSpacing() = window->textVSpacing();
	return saved;
}

void UGC::RestoreTextSettings(const TextSettings& settings)
{
	normalFont() = settings.normalFont;
	boldFont() = settings.boldFont;
	SpecialTextEnabled = settings.specialText;
	textVSpacing() = settings.vspacing;
}
