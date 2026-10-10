#pragma once

#include "Packages/Extension/Windows/UWindow.h"

class UTabGroupWindow : public UWindow
{
public:
	using UWindow::UWindow;

	void InitDefaults() override;
	void ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight) override;
	void ConfigurationChanged() override;
	void ChildRequestedVisibilityChange(UWindow* childWin, bool bNewVisibility) override;

	BitfieldBool bSizeChildrenToParent() { return BoolValue(PropOffsets_TabGroupWindow.bSizeChildrenToParent); }
	BitfieldBool bSizeParentToChildren() { return BoolValue(PropOffsets_TabGroupWindow.bSizeParentToChildren); }
	//DynamicArray& colMajorWindowList() { return Value<DynamicArray>(PropOffsets_TabGroupWindow.colMajorWindowList); }
	float& firstAbsX() { return Value<float>(PropOffsets_TabGroupWindow.firstAbsX); }
	float& firstAbsY() { return Value<float>(PropOffsets_TabGroupWindow.firstAbsY); }
	//DynamicArray& rowMajorWindowList() { return Value<DynamicArray>(PropOffsets_TabGroupWindow.rowMajorWindowList); }
	int& tabGroupIndex() { return Value<int>(PropOffsets_TabGroupWindow.tabGroupIndex); }
};
