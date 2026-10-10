
#include "Precomp.h"
#include "UEditWindow.h"
#include "VM/ScriptCall.h"
#include "Packages/Core/Properties/UStringProperty.h"
#include "Engine.h"
#include "Packages/Extension/Windows/UGC.h"
#include "Packages/Engine/Resources/UFont.h"
#include "Packages/Engine/Resources/USound.h"

// The original's undo is a list of changes -- each a position, the text
// removed and the text put in -- capped at maxUndos, with typing straight
// after the last change joining it; the script's Ctrl+Z and Ctrl+Y call Undo
// and Redo (extension-dll.md, Small).
void UEditWindow::AddUndo(int pos, std::string removed, std::string inserted)
{
	undoList.resize(undoIndex);   // a new change drops what redo held

	if (!undoList.empty() && removed.empty() && !inserted.empty())
	{
		EditChange& last = undoList.back();
		if (last.removed.empty() && pos == last.pos + (int)last.inserted.size())
		{
			last.inserted += inserted;
			return;
		}
	}

	undoList.push_back({ pos, std::move(removed), std::move(inserted) });
	if (maxUndos() > 0 && (int)undoList.size() > maxUndos())
		undoList.erase(undoList.begin());
	undoIndex = (int)undoList.size();
}

void UEditWindow::ApplyChange(int pos, const std::string& from, const std::string& to)
{
	std::string text = Text();
	if (pos < 0 || pos + (int)from.size() > (int)text.size())
		return;
	text = text.substr(0, pos) + to + text.substr(pos + from.size());
	insertPos() = pos + (int)to.size();
	selectStart() = insertPos();
	selectEnd() = insertPos();
	StoreText(text);
	SetTextChangedFlag(true);
	DispatchTextChanged(true);
}

void UEditWindow::ClearUndo()
{
	undoList.clear();
	undoIndex = 0;
}

void UEditWindow::Redo()
{
	if (undoIndex >= (int)undoList.size())
		return;
	const EditChange& change = undoList[undoIndex];
	undoIndex++;
	ApplyChange(change.pos, change.removed, change.inserted);
}

void UEditWindow::Undo()
{
	if (undoIndex <= 0)
		return;
	undoIndex--;
	const EditChange& change = undoList[undoIndex];
	ApplyChange(change.pos, change.inserted, change.removed);
}

void UEditWindow::SetMaxUndos(int newMaxUndos)
{
	maxUndos() = newMaxUndos;
}

void UEditWindow::Copy()
{
	int selStart = 0, selCount = 0;
	GetSelectedArea(selStart, selCount);
	if (selCount > 0)
	{
		engine->window->SetClipboardText(GetText().substr(selStart, selCount));
	}
	else
	{
		engine->window->SetClipboardText(GetText());
	}
}

void UEditWindow::Cut()
{
	Copy();
	int selStart = 0, selCount = 0;
	GetSelectedArea(selStart, selCount);
	if (selCount > 0)
		DeleteChar(false, true);
}

void UEditWindow::Paste()
{
	InsertText(engine->window->GetClipboardText(), true, {});
}

void UEditWindow::DeleteChar(std::optional<bool> bBefore, std::optional<bool> bUndo)
{
	std::string text = Text();
	std::string removed;
	int removedPos = 0;
	int selStart = 0, selCount = 0;
	GetSelectedArea(selStart, selCount);
	if (selCount > 0)
	{
		removed = text.substr(selStart, selCount);
		removedPos = selStart;
		text = text.substr(0, selStart) + text.substr(selStart + selCount);
		if (insertPos() >= selStart + selCount)
			insertPos() -= selCount;
		else if (insertPos() > selStart)
			insertPos() = selStart;
	}
	else if (bBefore.has_value() && bBefore.value())
	{
		if (insertPos() > 0)
		{
			removed = text.substr(insertPos() - 1, 1);
			removedPos = insertPos() - 1;
			text.erase(text.begin() + (insertPos() - 1));
			insertPos()--;
		}
	}
	else
	{
		if (insertPos() < (int)text.size())
		{
			removed = text.substr(insertPos(), 1);
			removedPos = insertPos();
			text.erase(text.begin() + insertPos());
		}
	}
	selectStart() = insertPos();
	selectEnd() = insertPos();
	StoreText(text);
	SetTextChangedFlag(true);
	DispatchTextChanged(true);

	if (bUndo.value_or(false) && !removed.empty())
		AddUndo(removedPos, std::move(removed), {});
}

void UEditWindow::EnableEditing(std::optional<bool> bEdit)
{
	bEditable() = bEdit ? *bEdit : true;
}

void UEditWindow::EnableSingleLineEditing(std::optional<bool> bSingle)
{
	bSingleLine() = bSingle ? *bSingle : true;
}

void UEditWindow::EnableUppercaseOnly(std::optional<bool> bUppercase)
{
	bUppercaseOnly() = bUppercase ? *bUppercase : true;
}

int UEditWindow::GetInsertionPoint()
{
	return insertPos();
}

// With nothing selected, 0 and 0: the original's (XEditWindow::
// GetSelectedArea 0x1001d5c0) keeps no selection as -1, held to 0.
void UEditWindow::GetSelectedArea(int& startPos, int& Count)
{
	int start = selectStart();
	int end = selectEnd();
	if (end < start)
		std::swap(start, end);
	startPos = end > start ? start : 0;
	Count = end - start;
}

bool UEditWindow::HasTextChanged()
{
	return textChanged;
}

// The original's (XEditWindow::SetText 0x1001e260): the whole text
// selected and replaced as typing replaces it -- the undo list cleared, the
// change announced (TextChanged) -- then the insertion point at the start.
void UEditWindow::SetText(const std::string& NewText)
{
	selectStart() = 0;
	insertPos() = (int)Text().size();
	selectEnd() = insertPos();
	InsertText(NewText, false, false);
	SetInsertionPoint(0, false);
}

// The original's (XEditWindow::AppendText 0x1001e310): put in at the end as
// typing puts it, then the insertion point at the start.
void UEditWindow::AppendText(const std::string& NewText)
{
	insertPos() = (int)Text().size();
	selectStart() = insertPos();
	selectEnd() = insertPos();
	InsertText(NewText, false, false);
	SetInsertionPoint(0, false);
}

// The text an edit leaves, laid out again (XEditWindow::ReplaceText
// 0x1001fe20); the insertion point stays where the edit put it.
void UEditWindow::StoreText(const std::string& text)
{
	if (Text() != text)
	{
		Text() = text;
		AskParentForReconfigure();
	}
}

// The original's XEditWindow::InsertText (0x1001e3c0): each character
// through the filter -- a single line drops line breaks, FilterChar may
// change or refuse one -- and nothing done when none passes; a change
// without undo clears the undo list. What passes replaces the selection at
// the insertion point, cut to what MaxSize leaves room for.
bool UEditWindow::InsertText(std::optional<std::string> InsertText, std::optional<bool> bUndo, std::optional<bool> bSelect)
{
	const std::string& input = InsertText.value_or("");
	std::string filtered;
	for (char c : input)
	{
		if (bSingleLine() && c == '\n')
			continue;
		std::string ch(1, c);
		if (FilterChar(ch) && !ch.empty())
			filtered += ch[0];
	}
	if (!input.empty() && filtered.empty())
		return false;

	if (!bUndo.value_or(false))
	{
		ClearUndo();
		SetTextChangedFlag(true);
	}

	std::string text = Text();
	int selStart = 0, selCount = 0;
	GetSelectedArea(selStart, selCount);
	int insertLength = (int)filtered.size();
	if (maxSize() > 0)
	{
		if ((int)text.size() > maxSize())
		{
			StoreText(text.substr(0, maxSize()));
			ClearUndo();
			SetTextChangedFlag(true);
			SetInsertionPoint(0, false);
			return true;
		}
		if (selCount < insertLength)
		{
			int over = insertLength + (int)text.size() - maxSize() - selCount;
			if (over > 0)
			{
				insertLength -= over;
				if (insertLength < 0)
					return true;
			}
		}
		filtered = filtered.substr(0, insertLength);
	}
	if (selCount <= 0 && filtered.empty())
		return true;

	std::string removed;
	if (selCount > 0)
	{
		removed = text.substr(selStart, selCount);
		text = text.substr(0, selStart) + text.substr(selStart + selCount);
		if (insertPos() >= selStart + selCount)
			insertPos() -= selCount;
		else if (insertPos() > selStart)
			insertPos() = selStart;
	}
	int changePos = insertPos();
	text = text.substr(0, insertPos()) + filtered + text.substr(insertPos());
	insertPos() += (int)filtered.size();
	selectStart() = insertPos();
	selectEnd() = insertPos();
	StoreText(text);
	SetTextChangedFlag(true);
	DispatchTextChanged(true);

	if (bUndo.value_or(false))
		AddUndo(changePos, std::move(removed), filtered);

	return true;
}

// The original's XEditWindow::FilterChar (0x10021830): upper case when only
// upper case is allowed, then the script's FilterChar, which may refuse the
// character or give another; with none, every character passes.
bool UEditWindow::FilterChar(std::string& ch)
{
	if (bUppercaseOnly() && ch[0] >= 'a' && ch[0] <= 'z')
		ch[0] -= 32;
	if (!FindEventFunction(this, "FilterChar"))
		return true;
	auto stringProp = GC::Alloc<UStringProperty>("", nullptr, ObjectFlags::NoFlags);
	return CallEvent(this, "FilterChar", { ExpressionValue::Variable(&ch, stringProp) }).ToBool();
}

bool UEditWindow::IsEditingEnabled()
{
	return bEditable();
}

bool UEditWindow::IsSingleLineEditingEnabled()
{
	return bSingleLine();
}

void UEditWindow::MoveInsertionPoint(uint8_t moveInsert, std::optional<bool> bDrag)
{
	switch ((EMoveInsert)moveInsert)
	{
	case EMoveInsert::Left:
		SetInsertionPoint(std::max(insertPos() - 1, 0), bDrag);
		break;
	case EMoveInsert::Right:
		SetInsertionPoint(std::min(insertPos() + 1, (int)Text().size()), bDrag);
		break;
	case EMoveInsert::WordLeft:
		SetInsertionPoint(FindPreviousBreakCharacter(insertPos() - 1), bDrag);
		break;
	case EMoveInsert::WordRight:
		SetInsertionPoint(FindNextBreakCharacter(insertPos() + 1), bDrag);
		break;
	case EMoveInsert::StartOfLine:
		if (bSingleLine())
		{
			SetInsertionPoint(0, bDrag);
		}
		else
		{
			// To do: find prev newline
		}
		break;
	case EMoveInsert::EndOfLine:
		if (bSingleLine())
		{
			SetInsertionPoint((int)Text().size(), bDrag);
		}
		else
		{
			// To do: find next newline
		}
		break;
	case EMoveInsert::Up:
		if (!bSingleLine())
		{
			// To do: need multiline support
		}
		break;
	case EMoveInsert::Down:
		if (!bSingleLine())
		{
			// To do: need multiline support
		}
		break;
	case EMoveInsert::PageUp:
		if (!bSingleLine())
		{
			// To do: need multiline support
		}
		break;
	case EMoveInsert::PageDown:
		if (!bSingleLine())
		{
			// To do: need multiline support
		}
		break;
	case EMoveInsert::Home:
		SetInsertionPoint(0, bDrag);
		break;
	case EMoveInsert::End:
		SetInsertionPoint((int)Text().size(), bDrag);
		break;
	}
}

void UEditWindow::SetInsertionPoint(int NewPos, std::optional<bool> bDrag)
{
	insertPos() = std::clamp(NewPos, 0, (int)Text().size());
	if (!bDrag.has_value() || !bDrag.value())
		selectStart() = insertPos();
	selectEnd() = insertPos();
	blinkDelay() = 0.0f;
}

void UEditWindow::PlayEditSound(UObject* sound, std::optional<float> Volume, std::optional<float> Pitch)
{
	// What is the difference between this function and UWindow::PlaySound?
	PlaySound(sound, Volume, Pitch, {}, {});
}

void UEditWindow::SetEditCursor(std::optional<UObject*> newCursor, std::optional<UObject*> newCursorShadow, std::optional<Color> NewColor)
{
	if (newCursor.has_value())
		editCursor() = UObject::Cast<UTexture>(newCursor.value());
	if (newCursorShadow.has_value())
		editCursorShadow() = UObject::Cast<UTexture>(newCursorShadow.value());
	if (NewColor.has_value())
		editCursorColor() = NewColor.value();
}

void UEditWindow::SetEditSounds(std::optional<UObject*> newTypeSound, std::optional<UObject*> newDeleteSound, std::optional<UObject*> newEnterSound, std::optional<UObject*> newMoveSound)
{
	if (newTypeSound.has_value())
		typeSound() = UObject::Cast<USound>(newTypeSound.value());
	if (newDeleteSound.has_value())
		deleteSound() = UObject::Cast<USound>(newDeleteSound.value());
	if (newEnterSound.has_value())
		enterSound() = UObject::Cast<USound>(newEnterSound.value());
	if (newMoveSound.has_value())
		moveSound() = UObject::Cast<USound>(newMoveSound.value());
}

void UEditWindow::SetInsertionPointBlinkRate(std::optional<float> newBlinkStart, std::optional<float> newBlinkPeriod)
{
	if (newBlinkStart.has_value())
		blinkStart() = newBlinkStart.value();
	if (newBlinkPeriod.has_value())
		blinkPeriod() = newBlinkPeriod.value();
}

void UEditWindow::SetInsertionPointTexture(std::optional<UObject*> NewTexture, std::optional<Color> NewColor)
{
	if (NewTexture.has_value())
		insertTexture() = UObject::Cast<UTexture>(NewTexture.value());
	if (NewColor.has_value())
		insertColor() = NewColor.value();
}

void UEditWindow::SetInsertionPointType(uint8_t newType, std::optional<float> prefWidth, std::optional<float> prefHeight)
{
	insertType() = newType;
	if (prefWidth.has_value())
		insertPrefWidth() = prefWidth.value();
	if (prefHeight.has_value())
		insertPrefHeight() = prefHeight.value();
}

void UEditWindow::SetMaxSize(int newMaxSize)
{
	maxSize() = newMaxSize;
}

void UEditWindow::SetSelectedArea(int startPos, int Count)
{
	selectStart() = std::clamp(startPos, 0, (int)Text().size());
	selectEnd() = std::clamp(startPos + Count, 0, (int)Text().size());
	insertPos() = selectEnd();
}

void UEditWindow::SetSelectedAreaTextColor(std::optional<Color> NewColor)
{
	if (NewColor.has_value())
		selectColor() = NewColor.value();
}

void UEditWindow::SetSelectedAreaTexture(std::optional<UObject*> NewTexture, std::optional<Color> NewColor)
{
	if (NewTexture.has_value())
		selectTexture() = UObject::Cast<UTexture>(NewTexture.value());
	if (NewColor.has_value())
		selectColor() = NewColor.value();
}

void UEditWindow::ClearTextChangedFlag()
{
	textChanged = false;
}

void UEditWindow::SetTextChangedFlag(std::optional<bool> bSet)
{
	textChanged = bSet ? *bSet : true;
}

// The original's (XEditWindow::ParentRequestedPreferredSize 0x10020ac0): a
// large text window's, a single line one row high when no height is given,
// and a width not given a space wider.
void UEditWindow::ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight)
{
	ULargeTextWindow::ParentRequestedPreferredSize(bWidthSpecified, preferredWidth, bHeightSpecified, preferredHeight);

	UGC* gc = engine->dxgc;
	UGC::TextSettings saved = gc->UseTextSettings(this);
	if (bSingleLine() && !bHeightSpecified)
		preferredHeight = vMargin() + vMargin() + std::max(gc->GetFontHeight(false), 1.0f);
	if (!bWidthSpecified)
	{
		float spaceWidth = 0.0f, spaceHeight = 0.0f;
		gc->GetTextExtent(0.0f, spaceWidth, spaceHeight, " ");
		preferredWidth += spaceWidth;
	}
	gc->RestoreTextSettings(saved);
}

// The original's XEditWindow::Init (0x1001cea0): text from the top left, 3
// in from the sides, editable and on several lines -- no game script makes
// one single-line, so Enter is the script's line break everywhere -- a white
// insertion point, grey selection, up to 200000 characters and 64 undos, a
// cursor blinking each second after 0.75 s, no special text, and selectable.
void UEditWindow::InitDefaults()
{
	ULargeTextWindow::InitDefaults();
	HAlign() = (uint8_t)EHAlign::Left;
	VAlign() = (uint8_t)EVAlign::Top;
	hMargin() = 3.0f;
	vMargin() = 0.0f;
	bEditable() = true;
	bSingleLine() = false;
	bUppercaseOnly() = false;
	insertPos() = 0;
	insertHookPos() = 0;
	insertType() = 0;
	selectStart() = 0;
	selectEnd() = 0;
	maxSize() = 200000;
	insertColor() = { 255, 255, 255, 255 };
	selectColor() = { 196, 196, 196, 255 };
	inverseColor() = { 0, 0, 0, 255 };
	maxUndos() = 64;
	blinkStart() = 0.75f;
	blinkPeriod() = 1.0f;
	dragDelay() = 0.0f;
	bSpecialText() = false;
	bIsSelectable() = true;
}

void UEditWindow::Tick(float timeElapsed)
{
	ULargeTextWindow::Tick(timeElapsed);

	blinkDelay() -= timeElapsed;
	if (blinkDelay() < -blinkPeriod())
		blinkDelay() = blinkPeriod();
}

void UEditWindow::DrawWindow(UGC* gc)
{
	UWindow::DrawWindow(gc);

	gc->SetFont(normalFont());
	gc->SetAlignments(HAlign(), VAlign());
	gc->bWordWrap() = !bSingleLine();

	float w = Width();
	float h = Height();

	int selStart = 0, selCount = 0;
	GetSelectedArea(selStart, selCount);
	if (selCount > 0)
	{
		// To do: this doesn't work for multi line edit (we must split on a per line basis)

		std::string beforeText = Text().substr(0, selStart);
		std::string selectionText = Text().substr(selStart, selCount);
		std::string afterText = Text().substr(selStart + selCount);

		float x = 0.0f;
		float xExtent = 0.0f, yExtent = 0.0f;
		gc->SetTextColor(TextColor());
		gc->DrawText(x, 0.0f, w - x, h, beforeText);
		gc->GetTextExtent(w - x, xExtent, yExtent, beforeText);
		x += xExtent - 1.0f;

		gc->GetTextExtent(w - x, xExtent, yExtent, selectionText);
		if (auto tex = selectTexture())
		{
			gc->SetTileColor(selectColor());
			gc->DrawStretchedTexture(x, 0.0f, xExtent - 1.0f, h, 0.0f, 0.0f, (float)tex->USize(), (float)tex->VSize(), tex);
		}
		gc->SetTextColor(TextColor());
		gc->DrawText(x, 0.0f, w - x, h, selectionText);
		x += xExtent - 1.0f;

		gc->SetTextColor(TextColor());
		gc->DrawText(x, 0.0f, w - x, h, afterText);
	}
	else
	{
		gc->SetTextColor(TextColor());
		gc->DrawText(0.0f, 0.0f, Width(), Height(), Text());
	}

	if (IsFocusWindow() && blinkDelay() < 0.0f)
	{
		if (auto tex = insertTexture())
		{
			float xExtent = 0.0f, yExtent = 0.0f;
			std::string beforeInsertPoint = Text().substr(0, insertPos());
			gc->GetTextExtent(w, xExtent, yExtent, beforeInsertPoint);
			float x = std::max(xExtent - 2.0f, 0.0f);

			// Only EInsertionPointType::Insert is in use according to the scripts
			// EInsertionPointType type = (EInsertionPointType)insertType();

			gc->SetTileColor(insertColor());
			gc->DrawStretchedTexture(x, 0.0f, 1.0f, h, 0.0f, 0.0f, (float)tex->USize(), (float)tex->VSize(), tex);
		}
	}

	// DrawDebugBox(gc);
}

// The original's XEditWindow::KeyPressed (0x10021290): the script's first;
// then, editing on, a typed character other than the console's ` and ~ is
// inserted with undo, and plays the type sound.
bool UEditWindow::KeyPressed(std::string key)
{
	bool handled = ULargeTextWindow::KeyPressed(key);
	if (bEditable() && key.size() == 1 && key[0] != '`' && key[0] != '~' && (uint8_t)key[0] >= 0x20)
	{
		if (InsertText(key, true, false))
		{
			PlayEditSound(typeSound(), {}, {});
			return true;
		}
	}
	return handled;
}

// The original's XEditWindow::VirtualKeyPressed (0x10021480): Enter plays the
// enter sound, and in an editable single line activates the edit
// (EditActivated up the parents) without the script's hearing of it; all
// else is the script's, its Enter inserting a line break.
bool UEditWindow::VirtualKeyPressed(EInputKey key, bool bRepeat)
{
	if (key == IK_Enter)
	{
		PlayEditSound(enterSound(), {}, {});
		if (bSingleLine() && bEditable())
		{
			for (UWindow* cur = this; cur != nullptr; cur = cur->parentOwner())
			{
				if (cur->EditActivated(this, HasTextChanged()))
					break;
			}
			return true;
		}
	}
	return ULargeTextWindow::VirtualKeyPressed(key, bRepeat);
}

bool UEditWindow::MouseButtonPressed(float pointX, float pointY, EInputKey button, int numClicks)
{
	SetFocusWindow(this);
	ULargeTextWindow::MouseButtonPressed(pointX, pointY, button, numClicks);
	return true;
}

bool UEditWindow::MouseButtonReleased(float pointX, float pointY, EInputKey button, int numClicks)
{
	ULargeTextWindow::MouseButtonReleased(pointX, pointY, button, numClicks);
	return true;
}

void UEditWindow::DispatchTextChanged(bool modified)
{
	for (UWindow* cur = this; cur != nullptr; cur = cur->parentOwner())
	{
		if (cur->TextChanged(this, modified))
			break;
	}
}

int UEditWindow::FindNextBreakCharacter(int search_start)
{
	if (search_start >= int(Text().size()) - 1)
		return (int)Text().size();

	size_t pos = Text().find_first_of(break_characters, search_start);
	if (pos == std::string::npos)
		return (int)Text().size();
	return (int)pos;
}

int UEditWindow::FindPreviousBreakCharacter(int search_start)
{
	if (search_start <= 0)
		return 0;
	size_t pos = Text().find_last_of(break_characters, search_start);
	if (pos == std::string::npos)
		return 0;
	return (int)pos;
}

const std::string UEditWindow::break_characters = " ::;,.-";
