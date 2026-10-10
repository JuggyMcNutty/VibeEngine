#pragma once

#include "UWindow.h"

class UTileWindow : public UWindow
{
public:
	using UWindow::UWindow;

	void EnableWrapping(bool bWrapOn);
	void FillParent(bool FillParent);
	void MakeHeightsEqual(bool bEqual);
	void MakeWidthsEqual(bool bEqual);
	void SetChildAlignments(uint8_t newHAlign, uint8_t newVAlign);
	void SetDirections(uint8_t newHDir, uint8_t newVDir);
	void SetMajorSpacing(float newSpacing);
	void SetMargins(float newHMargin, float newVMargin);
	void SetMinorSpacing(float newSpacing);
	void SetOrder(EOrder newOrder);
	void SetOrientation(uint8_t newOrientation);

	void InitDefaults() override;
	void ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight) override;
	void ParentRequestedGranularity(float& hGranularity, float& vGranularity) override;
	void ConfigurationChanged() override;
	bool ChildRequestedReconfiguration(UWindow* childWin) override;
	void ChildRequestedVisibilityChange(UWindow* childWin, bool bNewVisibility) override;
	void ChildAdded(UWindow* child) override;
	void ChildRemoved(UWindow* child) override;

	BitfieldBool bEqualHeight() { return BoolValue(PropOffsets_TileWindow.bEqualHeight); }
	BitfieldBool bEqualWidth() { return BoolValue(PropOffsets_TileWindow.bEqualWidth); }
	BitfieldBool bFillParent() { return BoolValue(PropOffsets_TileWindow.bFillParent); }
	BitfieldBool bWrap() { return BoolValue(PropOffsets_TileWindow.bWrap); }
	uint8_t& hChildAlign() { return Value<uint8_t>(PropOffsets_TileWindow.hChildAlign); }
	uint8_t& hDirection() { return Value<uint8_t>(PropOffsets_TileWindow.hDirection); }
	float& hMargin() { return Value<float>(PropOffsets_TileWindow.hMargin); }
	float& majorSpacing() { return Value<float>(PropOffsets_TileWindow.majorSpacing); }
	float& minorSpacing() { return Value<float>(PropOffsets_TileWindow.minorSpacing); }
	uint8_t& orientation() { return Value<uint8_t>(PropOffsets_TileWindow.orientation); }
	//DynamicArray& rowArray() { return Value<DynamicArray>(PropOffsets_TileWindow.rowArray); }
	uint8_t& vChildAlign() { return Value<uint8_t>(PropOffsets_TileWindow.vChildAlign); }
	uint8_t& vDirection() { return Value<uint8_t>(PropOffsets_TileWindow.vDirection); }
	float& vMargin() { return Value<float>(PropOffsets_TileWindow.vMargin); }

private:
	void ComputeChildSizes(bool bWidthSpecified, float& width, bool bHeightSpecified, float& height);

	// A row (a column, down a vertical tile) of children: how thick it is
	// across, how long along, and how many are in it.
	struct Row
	{
		int Thickness = 0;
		int Length = 0;
		int Count = 0;
	};
	Array<Row> Rows;
};
