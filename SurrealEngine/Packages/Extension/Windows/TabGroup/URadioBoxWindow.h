#pragma once

#include "UTabGroupWindow.h"

class UToggleWindow;

class URadioBoxWindow : public UTabGroupWindow
{
public:
	using UTabGroupWindow::UTabGroupWindow;

	UObject* GetEnabledToggle();

	void InitDefaults() override;
	void ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight) override;
	void ConfigurationChanged() override;
	void ChildRequestedVisibilityChange(UWindow* childWin, bool bNewVisibility) override;
	bool ToggleChanged(UWindow* button, bool bNewToggle) override;
	void DescendantAdded(UWindow* descendant) override;
	void DescendantRemoved(UWindow* descendant) override;

	BitfieldBool bOneCheck() { return BoolValue(PropOffsets_RadioBoxWindow.bOneCheck); }
	UToggleWindow*& currentSelection() { return Value<UToggleWindow*>(PropOffsets_RadioBoxWindow.currentSelection); }
	//DynamicArray& toggleButtons() { return Value<DynamicArray>(PropOffsets_RadioBoxWindow.toggleButtons); }

private:
	// The toggles this box holds -- those with no nearer radio box (the
	// script's native toggleButtons).
	Array<UToggleWindow*> toggleButtons;
};
