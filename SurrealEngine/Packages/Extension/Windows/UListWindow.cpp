
#include "Precomp.h"
#include "UListWindow.h"
#include "Engine.h"
#include "GameWindow.h"
#include "Packages/Engine/Resources/UFont.h"
#include "Packages/Engine/Resources/USound.h"
#include "Packages/Extension/Windows/UGC.h"
#include <algorithm>

// COLTYPE_String, COLTYPE_Float, COLTYPE_Time
enum { ColTypeString = 0, ColTypeFloat = 1, ColTypeTime = 2 };

// MOVELIST_Up .. MOVELIST_End
enum { MoveListUp = 0, MoveListDown = 1, MoveListPageUp = 2, MoveListPageDown = 3, MoveListHome = 4, MoveListEnd = 5 };

static int CompareText(const std::string& a, const std::string& b, bool caseSensitive)
{
	if (caseSensitive)
	{
		int cmp = a.compare(b);
		return cmp < 0 ? -1 : (cmp > 0 ? 1 : 0);
	}
	size_t n = std::min(a.size(), b.size());
	for (size_t i = 0; i < n; i++)
	{
		int ca = std::tolower((unsigned char)a[i]);
		int cb = std::tolower((unsigned char)b[i]);
		if (ca != cb)
			return ca < cb ? -1 : 1;
	}
	return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
}

void UListWindow::InitWindow()
{
	focusLine() = -1;
	anchorLine() = -1;
	lastIndex() = -1;
	// The original's defaults: the delimiter ';'; auto sort off,
	// auto-expanding columns, multiple selection and hot keys (column 0) on;
	// a column margin of 3 and a row margin of 1; one column.
	Delimiter() = ";";
	bMultiSelect() = true;
	bAutoExpandColumns() = true;
	bHotKeys() = true;
	hotKeyCol() = 0;
	colMargin() = 3.0f;
	rowMargin() = 1.0f;
	focusThickness() = 1.0f;
	SetNumColumns(1);
	UWindow::InitWindow();
}

// A row's text split at the delimiter's first character, a field a column:
// a field past the last column is dropped, a column past the last field
// gets an empty one, and a field is at most 2,047 characters (the
// original's FillRow, dx-reverse-info/extension-dll.md, lists). True when a
// column widened to a field.
bool UListWindow::FillRow(Item& item, const std::string& rowStr)
{
	item.cells.resize(columns.size());
	char delim = Delimiter().empty() ? '\0' : Delimiter()[0];
	size_t pos = 0;
	bool expanded = false;
	for (int colIndex = 0; colIndex < (int)columns.size(); colIndex++)
	{
		size_t end = rowStr.size();
		if (delim != '\0')
			end = std::min(rowStr.find(delim, pos), rowStr.size());
		std::string field = pos < end ? rowStr.substr(pos, std::min<size_t>(end - pos, 2047)) : std::string();
		pos = end < rowStr.size() ? end + 1 : end;
		SetFieldByString(item, colIndex, field);
		expanded |= AutoExpandColumn(colIndex, item.cells[colIndex].text);
	}
	return expanded;
}

// A field set from text: a float or time column keeps the number read from
// it and shows that number; a string column keeps the text, its number 0.
void UListWindow::SetFieldByString(Item& item, int colIndex, const std::string& fieldStr)
{
	if (item.cells.size() < columns.size())
		item.cells.resize(columns.size());
	const Column& col = columns[colIndex];
	Cell& cell = item.cells[colIndex];
	if (col.type == ColTypeFloat || col.type == ColTypeTime)
	{
		cell.value = StringToFloat(fieldStr);
		cell.text = FieldConvertToString(col, cell.value);
	}
	else
	{
		cell.value = 0.0f;
		cell.text = fieldStr;
	}
}

// A field set from a number: it shows as FieldConvertToString makes it; a
// string column keeps that text, its number 0.
void UListWindow::SetFieldByValue(Item& item, int colIndex, float value)
{
	if (item.cells.size() < columns.size())
		item.cells.resize(columns.size());
	const Column& col = columns[colIndex];
	Cell& cell = item.cells[colIndex];
	cell.text = FieldConvertToString(col, value);
	cell.value = (col.type == ColTypeFloat || col.type == ColTypeTime) ? value : 0.0f;
}

// The text a number shows as: through a float column's format, %f for any
// other column. A format is the script's; one that is not a single
// floating-point conversion is taken as %f, where the original would print
// whatever it asks for.
std::string UListWindow::FieldConvertToString(const Column& col, float value)
{
	std::string format = "%f";
	if (col.type == ColTypeFloat)
	{
		int conversions = 0;
		bool valid = true;
		const std::string& f = col.format;
		for (size_t i = 0; i < f.size() && valid; i++)
		{
			if (f[i] != '%')
				continue;
			i++;
			if (i < f.size() && f[i] == '%')
				continue;
			while (i < f.size() && strchr("-+ #0", f[i]))
				i++;
			while (i < f.size() && isdigit((unsigned char)f[i]))
				i++;
			if (i < f.size() && f[i] == '.')
			{
				i++;
				while (i < f.size() && isdigit((unsigned char)f[i]))
					i++;
			}
			valid = i < f.size() && strchr("fFeEgGaA", f[i]) != nullptr;
			conversions++;
		}
		if (valid && conversions == 1)
			format = f;
	}
	char buffer[256];
	snprintf(buffer, sizeof(buffer), format.c_str(), (double)value);
	return buffer;
}

float UListWindow::StringToFloat(const std::string& text)
{
	// The original's: a sign; octal after a leading 0 and hex after 0x; a
	// decimal point or comma; ' adds the number so far as hours, : or " as
	// minutes; anything else ends it.
	size_t pos = 0;
	size_t len = text.size();
	double sign = 1.0;
	if (pos < len && (text[pos] == '-' || text[pos] == '+'))
	{
		if (text[pos] == '-')
			sign = -1.0;
		pos++;
	}

	double total = 0.0;
	double value = 0.0;
	while (true)
	{
		value = 0.0;
		int base = 10;
		if (pos < len && text[pos] == '0')
		{
			if (pos + 1 < len && (text[pos + 1] == 'x' || text[pos + 1] == 'X'))
			{
				base = 16;
				pos += 2;
			}
			else
			{
				base = 8;
			}
		}
		while (pos < len)
		{
			char c = text[pos];
			int digit;
			if (c >= '0' && c <= '9')
				digit = c - '0';
			else if (base == 16 && c >= 'a' && c <= 'f')
				digit = c - 'a' + 10;
			else if (base == 16 && c >= 'A' && c <= 'F')
				digit = c - 'A' + 10;
			else
				break;
			if (digit >= base)
				break;
			value = value * base + digit;
			pos++;
		}
		if (pos < len && (text[pos] == '.' || text[pos] == ','))
		{
			pos++;
			double fraction = 1.0;
			while (pos < len && text[pos] >= '0' && text[pos] <= '9')
			{
				fraction /= 10.0;
				value += (text[pos] - '0') * fraction;
				pos++;
			}
		}
		if (pos < len && text[pos] == '\'')
		{
			total += value * 3600.0;
			pos++;
			continue;
		}
		if (pos < len && (text[pos] == ':' || text[pos] == '"'))
		{
			total += value * 60.0;
			pos++;
			continue;
		}
		break;
	}
	return (float)(sign * (total + value));
}

std::string UListWindow::FieldDisplayText(const Item& item, int colIndex)
{
	if (colIndex < 0 || (size_t)colIndex >= item.cells.size())
		return {};
	return item.cells[colIndex].text;
}

float UListWindow::MeasureText(UFont* colFont, const std::string& text)
{
	UFont* font = colFont ? colFont : normalFont();
	if (!font)
		return 0.0f;
	float x = 0.0f;
	for (char c : text)
		x += (float)font->GetGlyph(c).USize;
	return x;
}

// The original's row size: the tallest column font's line, with the row
// margin above and below (dx-reverse-info/extension-dll.md, lists).
float UListWindow::GetLineHeight()
{
	float height = 0.0f;
	bool anyFont = false;
	for (const Column& col : columns)
	{
		UFont* font = col.font ? col.font : normalFont();
		if (!font)
			continue;
		height = std::max(height, (float)font->GetGlyph(' ').VSize);
		anyFont = true;
	}
	if (!anyFont)
	{
		if (!normalFont())
			return lineSize() > 0.0f ? lineSize() : 0.0f;
		height = (float)normalFont()->GetGlyph(' ').VSize;
	}
	return height + rowMargin() * 2.0f;
}

// The original's: the visible columns side by side, a line for each row --
// what the clip window around a list sizes it to, so every change to the
// rows or the columns asks the parent to lay the list out again.
void UListWindow::ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight)
{
	float width = 0.0f;
	for (const Column& col : columns)
	{
		if (!col.hidden)
			width += col.width;
	}
	preferredWidth = width;
	preferredHeight = (float)items.size() * GetLineHeight();
}

void UListWindow::ParentRequestedGranularity(float& hGranularity, float& vGranularity)
{
	hGranularity = 1.0f;
	vGranularity = GetLineHeight();
}

// With auto-expanding columns, each field set widens its column to the
// field's text plus both margins; true when it widened.
bool UListWindow::AutoExpandColumn(int colIndex, const std::string& displayText)
{
	if (!bAutoExpandColumns())
		return false;
	if (colIndex < 0 || (size_t)colIndex >= columns.size())
		return false;
	Column& col = columns[colIndex];
	float width = MeasureText(col.font, displayText) + colMargin() * 2.0f;
	if (width <= col.width)
		return false;
	col.width = width;
	return true;
}

int UListWindow::AddRow(const std::string& rowStr, std::optional<int> clientData)
{
	int id = nextRowId++;
	Item item;
	item.id = id;
	if (clientData.has_value())
		item.clientInt = clientData.value();
	items.push_back(std::move(item));
	FillRow(items.back(), rowStr);
	// With auto sort on, a new row goes in at its place.
	if (bAutoSort())
		Sort();
	AskParentForReconfigure();
	return id;
}

void UListWindow::AddSortColumn(int colIndex, std::optional<bool> bReverse, std::optional<bool> bCaseSensitive)
{
	// Adds the column as the last key.
	if (colIndex < 0 || (size_t)colIndex >= columns.size())
		return;
	sortColumns.erase(std::remove(sortColumns.begin(), sortColumns.end(), colIndex), sortColumns.end());
	sortColumns.push_back(colIndex);
	columns[colIndex].sortReverse = bReverse.value_or(false);
	columns[colIndex].sortCaseSensitive = bCaseSensitive.value_or(false);
	SortChanged();
}

void UListWindow::DeleteAllRows()
{
	bool changed = GetNumSelectedRows() > 0;
	items.clear();
	nextRowId = 1;
	focusLine() = -1;
	anchorLine() = -1;
	if (changed)
		DispatchListSelectionChanged();
	AskParentForReconfigure();
}

void UListWindow::DeleteRow(int rowId)
{
	int index = RowIdToIndex(rowId);
	if (index < 0)
		return;
	bool changed = false;
	ChangeSelectRow(index, false, changed);
	items.erase(items.begin() + index);
	if (focusLine() == index)
		focusLine() = -1;
	else if (focusLine() > index)
		focusLine()--;
	if (anchorLine() == index)
		anchorLine() = -1;
	else if (anchorLine() > index)
		anchorLine()--;
	if (changed)
		DispatchListSelectionChanged();
	AskParentForReconfigure();
}

void UListWindow::EnableAutoExpandColumns(std::optional<bool> bAutoExpand)
{
	bAutoExpandColumns() = bAutoExpand.has_value() ? bAutoExpand.value() : true;
	// Turning it on widens every column.
	if (bAutoExpandColumns())
		ResizeColumns(true);
}

void UListWindow::EnableAutoSort(std::optional<bool> bNewAutoSort)
{
	bAutoSort() = bNewAutoSort.has_value() ? bNewAutoSort.value() : true;
	if (bAutoSort())
		Sort();
}

void UListWindow::EnableHotKeys(std::optional<bool> bEnable)
{
	bHotKeys() = bEnable.has_value() ? bEnable.value() : true;
}

// Turned off, the first selected row stays selected and the rest are let go.
void UListWindow::EnableMultiSelect(std::optional<bool> bEnableMultiSelect)
{
	bool enable = bEnableMultiSelect.value_or(true);
	if (bMultiSelect() == enable)
		return;
	bMultiSelect() = enable;
	if (!enable && GetNumSelectedRows() > 0)
	{
		bool changed = false;
		for (int i = (int)items.size() - 1; i >= 0; i--)
		{
			if (items[i].selected && GetNumSelectedRows() > 1)
				ChangeSelectRow(i, false, changed);
		}
		if (changed)
			DispatchListSelectionChanged();
	}
}

uint8_t UListWindow::GetColumnAlignment(int colIndex)
{
	if (colIndex < 0 || (size_t)colIndex >= columns.size())
		return 0;
	return (uint8_t)columns[colIndex].align;
}

void UListWindow::GetColumnColor(int colIndex, Color& colColor)
{
	if (colIndex < 0 || (size_t)colIndex >= columns.size())
		colColor = { 255, 255, 255, 255 };
	else
		colColor = columns[colIndex].color;
}

UObject* UListWindow::GetColumnFont(int colIndex)
{
	if (colIndex < 0 || (size_t)colIndex >= columns.size())
		return nullptr;
	return columns[colIndex].font;
}

std::string UListWindow::GetColumnTitle(int colIndex)
{
	if (colIndex < 0 || (size_t)colIndex >= columns.size())
		return {};
	return columns[colIndex].title;
}

uint8_t UListWindow::GetColumnType(int colIndex)
{
	if (colIndex < 0 || (size_t)colIndex >= columns.size())
		return 0;
	return columns[colIndex].type;
}

// A hidden column's width reads 0.
float UListWindow::GetColumnWidth(int colIndex)
{
	if (colIndex < 0 || (size_t)colIndex >= columns.size() || columns[colIndex].hidden)
		return 0;
	return columns[colIndex].width;
}

std::string UListWindow::GetField(int rowId, int colIndex)
{
	int rowIndex = RowIdToIndex(rowId);
	if (rowIndex == -1)
		return {};
	if (colIndex < 0 || (size_t)colIndex >= columns.size() || (size_t)colIndex >= items[rowIndex].cells.size())
		return {};
	return items[rowIndex].cells[colIndex].text;
}

void UListWindow::GetFieldMargins(float& marginWidth, float& marginHeight)
{
	marginWidth = colMargin();
	marginHeight = rowMargin();
}

float UListWindow::GetFieldValue(int rowId, int colIndex)
{
	int rowIndex = RowIdToIndex(rowId);
	if (rowIndex == -1)
		return 0.0f;
	if (colIndex < 0 || (size_t)colIndex >= items[rowIndex].cells.size())
		return 0.0f;
	return items[rowIndex].cells[colIndex].value;
}

int UListWindow::GetFocusRow()
{
	if (focusLine() < 0 || (size_t)focusLine() >= items.size())
		return 0;
	return items[focusLine()].id;
}

int UListWindow::GetNumColumns()
{
	return (int)columns.size();
}

int UListWindow::GetNumRows()
{
	return (int)items.size();
}

int UListWindow::GetNumSelectedRows()
{
	int count = 0;
	for (auto& item : items)
	{
		if (item.selected)
			count++;
	}
	return count;
}

int UListWindow::GetPageSize()
{
	// The rows that fit the list's clipped height, at least 1. The list
	// lives in a clip window, whose height is the visible part.
	float lineHeight = GetLineHeight();
	if (lineHeight <= 0.0f)
		return 1;
	UWindow* parent = parentOwner();
	float clippedHeight = parent ? parent->Height() : Height();
	return std::max(1, (int)(clippedHeight / lineHeight));
}

int UListWindow::GetRowClientInt(int rowId)
{
	int rowIndex = RowIdToIndex(rowId);
	if (rowIndex == -1)
		return 0;
	return items[rowIndex].clientInt;
}

UObject* UListWindow::GetRowClientObject(int rowId)
{
	int rowIndex = RowIdToIndex(rowId);
	if (rowIndex == -1)
		return 0;
	return items[rowIndex].clientObj;
}

int UListWindow::GetSelectedRow()
{
	// The focus row if it is selected, else the first selected row.
	if (focusLine() >= 0 && (size_t)focusLine() < items.size() && items[focusLine()].selected)
		return items[focusLine()].id;
	for (auto& item : items)
	{
		if (item.selected)
		{
			return item.id;
		}
	}
	return 0;
}

void UListWindow::HideColumn(int colIndex, std::optional<bool> bHide)
{
	if (colIndex < 0 || (size_t)colIndex >= columns.size())
		return;
	columns[colIndex].hidden = bHide.has_value() ? bHide.value() : true;
	AskParentForReconfigure();
}

int UListWindow::IndexToRowId(int index)
{
	if (index < 0 || (size_t)index >= items.size())
		return -1;
	return items[index].id;
}

bool UListWindow::IsAutoExpandColumnsEnabled()
{
	return bAutoExpandColumns();
}

bool UListWindow::IsAutoSortEnabled()
{
	return bAutoSort();
}

bool UListWindow::IsColumnHidden(int colIndex)
{
	if (colIndex < 0 || (size_t)colIndex >= columns.size())
		return false;
	return columns[colIndex].hidden;
}

bool UListWindow::IsMultiSelectEnabled()
{
	return bMultiSelect();
}

bool UListWindow::IsRowSelected(int rowId)
{
	int rowIndex = RowIdToIndex(rowId);
	if (rowIndex == -1)
		return false;
	return items[rowIndex].selected;
}

void UListWindow::ModifyRow(int rowId, const std::string& rowStr)
{
	int rowIndex = RowIdToIndex(rowId);
	if (rowIndex == -1)
		return;
	FillRow(items[rowIndex], rowStr);
	// With auto sort on, a changed row is moved to its place.
	if (bAutoSort())
		Sort();
	AskParentForReconfigure();
}

void UListWindow::MoveRow(uint8_t Move, std::optional<bool> bSelect, std::optional<bool> bClearRows, std::optional<bool> bDrag)
{
	// Moves the focus row, clamped to the rows; with no focus every move but
	// to the end lands on the first row. The list's script sends the arrow
	// keys, Page Up and Down, Home and End here.
	int last = (int)items.size() - 1;
	int index = (focusLine() >= 0 && focusLine() <= last) ? focusLine() : -65535;
	switch (Move)
	{
	case MoveListUp: index--; break;
	case MoveListDown: index++; break;
	case MoveListPageUp: index -= GetPageSize(); break;
	case MoveListPageDown: index += GetPageSize(); break;
	case MoveListHome: index = 0; break;
	case MoveListEnd: index = last; break;
	default: break;
	}
	index = std::max(std::min(index, last), 0);
	if (index > last)
		SetFocusLine(-1, true, !bDrag.value_or(false));
	else
		SetRow(items[index].id, bSelect.value_or(true), bClearRows.value_or(true), bDrag.value_or(false));
}

void UListWindow::ChangeSelectRow(int index, bool bSelect, bool& changed)
{
	if (items[index].selected != bSelect)
	{
		items[index].selected = bSelect;
		changed = true;
	}
}

// The original's MoveToRow: with bSelect the selection is cleared (or, when
// spanning, the old span from the focus to the anchor is), then the row
// selected or toggled, or the span from the anchor to it selected; a single
// selection list always clears and never spans. With bMoveFocus the row
// takes the focus, the anchor too unless spanning. ListSelectionChanged goes
// up the parents only when the selection changed; a new focus row plays the
// move sound.
void UListWindow::MoveToRow(int index, bool bSelect, bool bClearRows, bool bInvert, bool bSpan, bool bMoveFocus)
{
	if (index < 0 || (size_t)index >= items.size())
		return;

	bool changed = false;
	int oldFocus = focusLine();
	if (!bMultiSelect())
	{
		bClearRows = true;
		bSpan = false;
	}
	if (bSelect)
	{
		int focusIndex = (focusLine() >= 0 && (size_t)focusLine() < items.size()) ? focusLine() : 0;
		int anchorIndex = focusIndex;
		if (anchorLine() >= 0 && (size_t)anchorLine() < items.size())
			anchorIndex = anchorLine();
		if (bClearRows)
		{
			for (int i = 0; i < (int)items.size(); i++)
				ChangeSelectRow(i, false, changed);
		}
		else if (bSpan)
		{
			for (int i = std::min(focusIndex, anchorIndex); i <= std::max(focusIndex, anchorIndex); i++)
				ChangeSelectRow(i, false, changed);
		}
		if (!bSpan)
		{
			ChangeSelectRow(index, bInvert ? !items[index].selected : true, changed);
		}
		else
		{
			for (int i = std::min(anchorIndex, index); i <= std::max(anchorIndex, index); i++)
				ChangeSelectRow(i, true, changed);
		}
	}
	if (bMoveFocus)
		SetFocusLine(index, true, !bSpan);
	if (changed)
		DispatchListSelectionChanged();
	if (focusLine() != oldFocus && focusLine() >= 0 && moveSound())
		PlaySound(moveSound(), {}, {}, {}, {});
}

void UListWindow::SetFocusLine(int index, bool bShow, bool bAnchor)
{
	focusLine() = index;
	if (bAnchor)
		anchorLine() = index;
	if (bShow)
		ShowFocusRow();
}

void UListWindow::PlayListSound(UObject* listSound, std::optional<float> Volume, std::optional<float> Pitch)
{
	if (listSound)
		PlaySound(listSound, Volume, Pitch, {}, {});
}

void UListWindow::RemoveSortColumn(int colIndex)
{
	sortColumns.erase(std::remove(sortColumns.begin(), sortColumns.end(), colIndex), sortColumns.end());
	SortChanged();
}

void UListWindow::ResetSortColumns(std::optional<bool> bSort)
{
	// True, the default: every column a key, in column order; false, none.
	// Reverse and case are cleared.
	sortColumns.clear();
	for (size_t i = 0; i < columns.size(); i++)
	{
		columns[i].sortReverse = false;
		columns[i].sortCaseSensitive = false;
		if (bSort.value_or(true))
			sortColumns.push_back((int)i);
	}
	SortChanged();
}

void UListWindow::ResizeColumns(std::optional<bool> bExpandOnly)
{
	// Widens every column to its fields plus both margins; without
	// bExpandOnly every column first shrinks to its margins.
	for (int colIndex = 0; colIndex < (int)columns.size(); colIndex++)
	{
		Column& col = columns[colIndex];
		if (!bExpandOnly.value_or(false))
			col.width = colMargin() * 2.0f;
		for (auto& item : items)
			col.width = std::max(col.width, MeasureText(col.font, FieldDisplayText(item, colIndex)) + colMargin() * 2.0f);
	}
	AskParentForReconfigure();
}

int UListWindow::RowIdToIndex(int rowId)
{
	int index = 0;
	for (auto& item : items)
	{
		if (item.id == rowId)
		{
			return index;
		}
		index++;
	}
	return -1;
}

// A single selection list only lets go of its rows.
void UListWindow::SelectAllRows(std::optional<bool> bSelect)
{
	bool selected = bMultiSelect() ? bSelect.value_or(true) : false;
	bool changed = false;
	for (int i = 0; i < (int)items.size(); i++)
		ChangeSelectRow(i, selected, changed);
	if (changed)
		DispatchListSelectionChanged();
}

// Selecting a row of a single selection list lets go of the others first.
void UListWindow::SelectRow(int rowId, std::optional<bool> bSelect)
{
	bool selected = bSelect.value_or(true);
	int rowIndex = RowIdToIndex(rowId);
	if (rowIndex == -1 || items[rowIndex].selected == selected)
		return;
	bool changed = false;
	if (!bMultiSelect() && selected && GetNumSelectedRows() > 0)
	{
		for (int i = 0; i < (int)items.size(); i++)
			ChangeSelectRow(i, false, changed);
	}
	ChangeSelectRow(rowIndex, selected, changed);
	if (changed)
		DispatchListSelectionChanged();
}

// The selection moved to a row, the focus staying where it is.
void UListWindow::SelectToRow(int rowId, std::optional<bool> bClearRows, std::optional<bool> bInvert, std::optional<bool> bSpanRows)
{
	int rowIndex = RowIdToIndex(rowId);
	if (rowIndex != -1)
		MoveToRow(rowIndex, true, bClearRows.value_or(true), bInvert.value_or(false), bSpanRows.value_or(false), false);
}

void UListWindow::SetColumnAlignment(int colIndex, uint8_t newAlign)
{
	if (colIndex < 0 || (size_t)colIndex >= columns.size())
		return;
	columns[colIndex].align = (EHAlign)newAlign;
}

void UListWindow::SetColumnColor(int colIndex, const Color& NewColor)
{
	if (colIndex < 0 || (size_t)colIndex >= columns.size())
		return;
	columns[colIndex].color = NewColor;
}

void UListWindow::SetColumnFont(int colIndex, UObject* NewFont)
{
	if (colIndex < 0 || (size_t)colIndex >= columns.size())
		return;
	columns[colIndex].font = UObject::Cast<UFont>(NewFont);
	AskParentForReconfigure();
}

void UListWindow::SetColumnTitle(int colIndex, const std::string& Title)
{
	if (colIndex < 0 || (size_t)colIndex >= columns.size())
		return;
	columns[colIndex].title = Title;
	AskParentForReconfigure();
}

// Each row's field is set again from its text as the new type takes it; the
// format defaults to %f for a float column, %02h:%02m for a time one.
void UListWindow::SetColumnType(int colIndex, uint8_t newType, std::optional<std::string> newFmt)
{
	if (colIndex < 0 || (size_t)colIndex >= columns.size())
		return;
	Column& col = columns[colIndex];
	col.type = newType;
	if (newFmt.has_value())
		col.format = *newFmt;
	else
		col.format = newType == ColTypeFloat ? "%f" : (newType == ColTypeTime ? "%02h:%02m" : "");
	for (auto& item : items)
	{
		std::string text = (size_t)colIndex < item.cells.size() ? item.cells[colIndex].text : std::string();
		SetFieldByString(item, colIndex, text);
	}
	if (bAutoExpandColumns())
		ResizeColumns(true);
}

void UListWindow::SetColumnWidth(int colIndex, float newWidth)
{
	if (colIndex < 0 || (size_t)colIndex >= columns.size())
		return;
	columns[colIndex].width = newWidth;
	AskParentForReconfigure();
}

void UListWindow::SetDelimiter(const std::string& newDelimiter)
{
	Delimiter() = newDelimiter;
}

void UListWindow::SetField(int rowId, int colIndex, const std::string& fieldStr)
{
	if (colIndex < 0 || (size_t)colIndex >= columns.size())
		return;
	int rowIndex = RowIdToIndex(rowId);
	if (rowIndex == -1)
		return;
	Item& item = items[rowIndex];
	SetFieldByString(item, colIndex, fieldStr);
	bool expanded = AutoExpandColumn(colIndex, item.cells[colIndex].text);
	if (bAutoSort())
		Sort();
	if (expanded)
		AskParentForReconfigure();
}

void UListWindow::SetFieldMargins(float newMarginWidth, float newMarginHeight)
{
	colMargin() = newMarginWidth;
	rowMargin() = newMarginHeight;
	AskParentForReconfigure();
}

void UListWindow::SetFieldValue(int rowId, int colIndex, float NewValue)
{
	if (colIndex < 0 || (size_t)colIndex >= columns.size())
		return;
	int rowIndex = RowIdToIndex(rowId);
	if (rowIndex == -1)
		return;
	Item& item = items[rowIndex];
	SetFieldByValue(item, colIndex, NewValue);
	bool expanded = AutoExpandColumn(colIndex, item.cells[colIndex].text);
	if (bAutoSort())
		Sort();
	if (expanded)
		AskParentForReconfigure();
}

void UListWindow::SetFocusColor(const Color& NewColor)
{
	focusColor() = NewColor;
}

void UListWindow::SetFocusRow(int rowId, std::optional<bool> bMoveTo, std::optional<bool> bAnchor)
{
	SetFocusLine(RowIdToIndex(rowId), bMoveTo.value_or(true), bAnchor.value_or(true));
}

void UListWindow::SetFocusTexture(UObject* NewTexture)
{
	focusTexture() = UObject::Cast<UTexture>(NewTexture);
}

void UListWindow::SetFocusThickness(float newThickness)
{
	focusThickness() = newThickness;
}

void UListWindow::SetHighlightColor(const Color& NewColor)
{
	highlightColor() = NewColor;
}

void UListWindow::SetHighlightTextColor(const Color& NewColor)
{
	highlightTextColor = NewColor;
}

void UListWindow::SetHighlightTexture(UObject* NewTexture)
{
	highlightTexture() = UObject::Cast<UTexture>(NewTexture);
}

void UListWindow::SetHotKeyColumn(int colIndex)
{
	hotKeyCol() = colIndex;
}

void UListWindow::SetListSounds(std::optional<UObject*> newActivateSound, std::optional<UObject*> newMoveSound)
{
	if (newActivateSound.has_value())
		ActivateSound() = UObject::Cast<USound>(newActivateSound.value());
	if (newMoveSound.has_value())
		moveSound() = UObject::Cast<USound>(newMoveSound.value());
}

void UListWindow::SetNumColumns(int newCols)
{
	if (newCols < 0)
		newCols = 0;
	size_t oldCount = columns.size();
	columns.resize(newCols);
	sortColumns.erase(std::remove_if(sortColumns.begin(), sortColumns.end(), [&](int c) { return c >= newCols; }), sortColumns.end());
	for (size_t i = oldCount; i < columns.size(); i++)
	{
		// A new column: 20 wide plus both margins, left-aligned, the
		// window's text colour and font, string, and a sort key.
		Column& col = columns[i];
		col.width = 20.0f + colMargin() * 2.0f;
		col.align = EHAlign::Left;
		col.color = TextColor();
		col.font = normalFont();
		col.type = ColTypeString;
		sortColumns.push_back((int)i);
	}
	// Every row gets an empty field for each new column, or loses those of
	// the columns gone.
	for (auto& item : items)
		item.cells.resize(columns.size());
	AskParentForReconfigure();
}

// The row selected and given the focus as a click does; no row leaves no
// focus.
void UListWindow::SetRow(int rowId, std::optional<bool> bSelect, std::optional<bool> bClearRows, std::optional<bool> bDrag)
{
	int rowIndex = RowIdToIndex(rowId);
	if (rowIndex != -1)
		MoveToRow(rowIndex, bSelect.value_or(true), bClearRows.value_or(true), false, bDrag.value_or(false), true);
	else
		SetFocusLine(-1, true, !bDrag.value_or(false));
}

void UListWindow::SetRowClientInt(int rowId, int clientInt)
{
	int rowIndex = RowIdToIndex(rowId);
	if (rowIndex == -1)
		return;
	items[rowIndex].clientInt = clientInt;
}

void UListWindow::SetRowClientObject(int rowId, UObject* clientObj)
{
	int rowIndex = RowIdToIndex(rowId);
	if (rowIndex == -1)
		return;
	items[rowIndex].clientObj = clientObj;
}

void UListWindow::SetSortColumn(int colIndex, std::optional<bool> bReverse, std::optional<bool> bCaseSensitive)
{
	// That column first, then every other column in column order, their
	// reverse and case flags cleared.
	if (colIndex < 0 || (size_t)colIndex >= columns.size())
		return;
	for (auto& col : columns)
	{
		col.sortReverse = false;
		col.sortCaseSensitive = false;
	}
	columns[colIndex].sortReverse = bReverse.value_or(false);
	columns[colIndex].sortCaseSensitive = bCaseSensitive.value_or(false);
	sortColumns.clear();
	sortColumns.push_back(colIndex);
	for (int i = 0; i < (int)columns.size(); i++)
	{
		if (i != colIndex)
			sortColumns.push_back(i);
	}
	SortChanged();
}

void UListWindow::SortChanged()
{
	// Changing the keys with auto sort on sorts again.
	if (bAutoSort())
		Sort();
}

bool UListWindow::RowLess(const Item& a, const Item& b)
{
	// Each key in turn: a float or time column compares numbers, a string
	// column text with or without case, either reversed on request. Rows
	// equal in every key keep their order.
	for (int colIndex : sortColumns)
	{
		const Column& col = columns[colIndex];
		int cmp;
		if (col.type == ColTypeFloat || col.type == ColTypeTime)
		{
			float av = (size_t)colIndex < a.cells.size() ? a.cells[colIndex].value : 0.0f;
			float bv = (size_t)colIndex < b.cells.size() ? b.cells[colIndex].value : 0.0f;
			cmp = av < bv ? -1 : (av > bv ? 1 : 0);
		}
		else
		{
			const std::string& at = (size_t)colIndex < a.cells.size() ? a.cells[colIndex].text : std::string();
			const std::string& bt = (size_t)colIndex < b.cells.size() ? b.cells[colIndex].text : std::string();
			cmp = CompareText(at, bt, col.sortCaseSensitive);
		}
		if (col.sortReverse)
			cmp = -cmp;
		if (cmp != 0)
			return cmp < 0;
	}
	return false;
}

void UListWindow::Sort()
{
	// The focus and the anchor are rows, not places: they follow their rows.
	int focusId = (focusLine() >= 0 && (size_t)focusLine() < items.size()) ? items[focusLine()].id : 0;
	int anchorId = (anchorLine() >= 0 && (size_t)anchorLine() < items.size()) ? items[anchorLine()].id : 0;
	std::stable_sort(items.begin(), items.end(), [this](const Item& a, const Item& b) { return RowLess(a, b); });
	focusLine() = focusId ? RowIdToIndex(focusId) : -1;
	anchorLine() = anchorId ? RowIdToIndex(anchorId) : -1;
}

void UListWindow::ShowFocusRow()
{
	// Asks the parent to scroll the focus row into view.
	float lineHeight = GetLineHeight();
	if (focusLine() < 0 || lineHeight <= 0.0f)
		return;
	AskParentToShowArea(0.0f, focusLine() * lineHeight, Width(), lineHeight);
}

void UListWindow::ToggleRowSelection(int rowId)
{
	SelectRow(rowId, !IsRowSelected(rowId));
}

void UListWindow::DrawWindow(UGC* gc)
{
	UFont* font = normalFont();
	if (!font)
		return;

	float w = Width();
	float lineHeight = GetLineHeight();
	lineSize() = lineHeight;

	float y = 0.0f;
	int lineIndex = 0;
	for (auto& item : items)
	{
		if (lineIndex == focusLine())
		{
			gc->SetTextColor(highlightTextColor);
			gc->SetTileColor(focusColor());
			if (focusTexture())
				gc->DrawTexture(0.0f, y, w, lineHeight, 0.0f, 0.0f, focusTexture());
		}

		if (item.selected)
		{
			float t = focusThickness();
			gc->SetTextColor(highlightTextColor);
			gc->SetTileColor(highlightColor());
			if (highlightTexture())
				gc->DrawTexture(t, y + t, std::max(w - 2.0f * t, 0.0f), std::max(lineHeight - t * 2.0f, 0.0f), 0.0f, 0.0f, highlightTexture());
		}
		else
		{
			gc->SetTextColor(TextColor());
			gc->SetTileColor(tileColor());
		}

		float x = 0.0f;
		for (int colIndex = 0; colIndex < (int)columns.size(); colIndex++)
		{
			auto& col = columns[colIndex];
			if (col.hidden)
				continue;
			if ((size_t)colIndex < item.cells.size())
			{
				UFont* colFont = col.font ? col.font : font;
				gc->SetFont(colFont);
				gc->SetAlignments((uint8_t)col.align, (uint8_t)EVAlign::Center);

				//if (!item.selected)
				//	gc->SetTextColor(col.color);

				gc->DrawText(x, y, col.width, lineHeight, FieldDisplayText(item, colIndex));
			}
			x += col.width;
		}
		y += lineHeight;
		lineIndex++;
	}
}

// A list shown with rows and no focus row selects and focuses its first.
void UListWindow::VisibilityChanged(bool bNewVisibility)
{
	UWindow::VisibilityChanged(bNewVisibility);
	if (bNewVisibility && focusLine() < 0 && !items.empty())
		SetRow(items[0].id, true, true, false);
}

// A left click selects the row under the pointer, the last when below them
// all: Shift spans from the anchor, Ctrl toggles. It starts a drag, which
// selects the span to each row the pointer then crosses.
bool UListWindow::MouseButtonPressed(float pointX, float pointY, EInputKey button, int numClicks)
{
	bool handled = UWindow::MouseButtonPressed(pointX, pointY, button, numClicks);
	if (button != IK_LeftMouse)
		return handled;
	if (lineSize() > 0.0f && !items.empty())
	{
		int index = std::clamp((int)(pointY / lineSize()), 0, (int)items.size() - 1);
		bool shift = IsKeyDown(IK_Shift);
		bool ctrl = IsKeyDown(IK_Ctrl);
		MoveToRow(index, true, !ctrl, ctrl, shift, true);
		lastIndex() = index;
		bDragging() = true;
		remainingDelay() = GetTickOffset() + 0.1f;
	}
	return true;
}

void UListWindow::MouseMoved(float newX, float newY)
{
	UWindow::MouseMoved(newX, newY);
	if (!bDragging() || lineSize() <= 0.0f || items.empty())
		return;
	int index = std::clamp((int)(newY / lineSize()), 0, (int)items.size() - 1);
	if (index != lastIndex())
	{
		lastIndex() = index;
		MoveToRow(index, true, false, false, true, true);
	}
}

// A left release ends the drag; a double click's activates the row under
// the pointer.
bool UListWindow::MouseButtonReleased(float pointX, float pointY, EInputKey button, int numClicks)
{
	bool handled = UWindow::MouseButtonReleased(pointX, pointY, button, numClicks);
	if (button != IK_LeftMouse)
		return handled;
	bDragging() = false;
	if (numClicks > 1 && lineSize() > 0.0f && !items.empty())
	{
		int index = (int)(pointY / lineSize());
		if (index >= 0 && index < (int)items.size())
		{
			SetFocusLine(index, true, true);
			ActivateRow();
		}
	}
	return true;
}

// Hot keys: letters, digits and _ typed within a second of each other find
// the next row whose hot key column starts with them, case-blind; the same
// letter again steps on to the next row starting with it.
bool UListWindow::KeyPressed(std::string key)
{
	bool handled = UWindow::KeyPressed(key);
	if (key.size() == 1 && IsHotKeyValid(key[0]) && bHotKeys())
	{
		int index = FindRowByKey(key[0]);
		if (index >= 0)
			SetRow(items[index].id, true, true, false);
		return true;
	}
	return handled;
}

bool UListWindow::IsHotKeyValid(char key)
{
	return (key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9') || key == '_';
}

void UListWindow::ClearHotKeyString()
{
	hotKeyString().clear();
	hotKeyTimer() = 0.0f;
}

int UListWindow::FindRowByKey(char key)
{
	if (!bHotKeys() || hotKeyCol() < 0 || hotKeyCol() >= (int)columns.size() || !IsHotKeyValid(key) || items.empty())
		return -1;

	std::string& typed = hotKeyString();
	typed += key;
	hotKeyTimer() = 1.0f;

	bool sameLetter = true;
	for (char c : typed)
		sameLetter &= (c == typed[0]);
	int matchLen = sameLetter ? 1 : (int)typed.size();

	int index = (focusLine() >= 0 && (size_t)focusLine() < items.size()) ? focusLine() : 0;
	if (matchLen > 1)
		index--;
	for (size_t count = 0; count < items.size(); count++)
	{
		if (++index >= (int)items.size())
			index = 0;
		const std::string& text = (size_t)hotKeyCol() < items[index].cells.size() ? items[index].cells[hotKeyCol()].text : std::string();
		bool match = true;
		for (int i = 0; i < matchLen && match; i++)
		{
			char a = i < (int)text.size() ? text[i] : '\0';
			match = std::tolower((unsigned char)a) == std::tolower((unsigned char)typed[i]);
		}
		if (match)
			return index;
	}
	return -1;
}

// Enter activates the focus row; the arrows and Escape clear the hot keys
// typed.
bool UListWindow::VirtualKeyPressed(EInputKey key, bool bRepeat)
{
	bool handled = UWindow::VirtualKeyPressed(key, bRepeat);
	if (key == IK_Left || key == IK_Right || key == IK_Up || key == IK_Down || key == IK_Escape)
		ClearHotKeyString();
	if (key == IK_Enter)
	{
		ActivateRow();
		return true;
	}
	return handled;
}

// While dragging, the pointer's place is taken again every 0.1 s, so a
// drag held past the list's edge scrolls on; the hot keys typed are let go
// after a second.
void UListWindow::Tick(float timeElapsed)
{
	if (bDragging())
	{
		remainingDelay() -= timeElapsed;
		if (remainingDelay() < 0.0f)
		{
			remainingDelay() = 0.1f;
			float mouseX = 0.0f, mouseY = 0.0f;
			GetCursorPos(mouseX, mouseY);
			MouseMoved(mouseX, mouseY);
		}
	}
	if (hotKeyTimer() > 0.0f)
	{
		hotKeyTimer() -= timeElapsed;
		if (hotKeyTimer() <= 0.0f)
			ClearHotKeyString();
	}
	UWindow::Tick(timeElapsed);
}

// The activate sound, then, when the list and its parents show,
// ListRowActivated up the parents for the focus row.
void UListWindow::ActivateRow()
{
	if (ActivateSound())
		PlaySound(ActivateSound(), {}, {}, {}, {});
	for (UWindow* cur = this; cur; cur = cur->parentOwner())
	{
		if (!cur->bIsVisible())
			return;
	}
	DispatchListRowActivated();
}

void UListWindow::DispatchListRowActivated()
{
	int rowId = GetFocusRow();
	for (UWindow* cur = this; cur; cur = cur->parentOwner())
	{
		if (cur->ListRowActivated(this, rowId))
			break;
	}
}

void UListWindow::DispatchListSelectionChanged()
{
	int numSelections = GetNumSelectedRows();
	int focusRowId = GetFocusRow();
	for (UWindow* cur = this; cur; cur = cur->parentOwner())
	{
		if (cur->ListSelectionChanged(this, numSelections, focusRowId))
			break;
	}
}

void UListWindow::Mark(GCMarker& marker)
{
	UWindow::Mark(marker);
	marker.SetField("columns");
	for (Column& column : columns)
		marker.Mark(column.font);
	marker.SetField("items");
	for (Item& item : items)
		marker.Mark(item.clientObj);
}
