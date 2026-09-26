
#include "Precomp.h"
#include "UEditWindow.h"
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
	SetText(text);
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
	SetText(text);
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

void UEditWindow::GetSelectedArea(int& startPos, int& Count)
{
	int start = selectStart();
	int end = selectEnd();
	if (end < start)
		std::swap(start, end);
	startPos = start;
	Count = end - start;
}

bool UEditWindow::HasTextChanged()
{
	return textChanged;
}

void UEditWindow::TextModifiedByScript()
{
	// This happens if script code calls UTextWindow::AppendText or UTextWindow::SetText
	insertPos() = (int)Text().size();
	selectStart() = insertPos();
	selectEnd() = insertPos();
}

bool UEditWindow::InsertText(std::optional<std::string> InsertText, std::optional<bool> bUndo, std::optional<bool> bSelect)
{
	// Note: bSelect is never set to true

	if (InsertText.has_value())
	{
		if (InsertText.value() == "|n" && bSingleLine())
		{
			for (UWindow* cur = this; cur != nullptr; cur = cur->parentOwner())
			{
				if (EditActivated(this, HasTextChanged()))
					break;
			}
			return true;
		}

		std::string text = Text();
		std::string removed;
		int selStart = 0, selCount = 0;
		GetSelectedArea(selStart, selCount);
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
		text = text.substr(0, insertPos()) + InsertText.value() + text.substr(insertPos());
		insertPos() += (int)InsertText.value().size();
		selectStart() = insertPos();
		selectEnd() = insertPos();
		SetText(text);
		SetTextChangedFlag(true);
		DispatchTextChanged(true);

		if (bUndo.value_or(false))
			AddUndo(changePos, std::move(removed), InsertText.value());
	}

	return true; // Unknown what this means. Script doesn't seem to use it for anything.
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

void UEditWindow::InitWindow()
{
	ULargeTextWindow::InitWindow();
	blinkPeriod() = 0.5f;
	bEditable() = true;
	bSingleLine() = true; // Does it default to single or multi line?
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

bool UEditWindow::KeyPressed(std::string key)
{
	if (ULargeTextWindow::KeyPressed(key))
		return true;

	if (key.empty() || !IsEditingEnabled())
		return false;

	if (key.front() >= 32)
		InsertText(key, true, IsKeyDown(IK_Shift));

	return true;
}

bool UEditWindow::VirtualKeyPressed(EInputKey key, bool bRepeat)
{
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
