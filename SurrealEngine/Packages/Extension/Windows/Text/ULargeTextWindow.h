#pragma once

#include "UTextWindow.h"

class ULargeTextWindow : public UTextWindow
{
public:
	using UTextWindow::UTextWindow;

	void SetVerticalSpacing(std::optional<float> newVSpace);

	void InitDefaults() override;
	void ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight) override;
	void ParentRequestedGranularity(float& hGranularity, float& vGranularity) override;

	int CountRows(UGC* gc, float wrap, float& widest);

	float& lineHeight() { return Value<float>(PropOffsets_LargeTextWindow.lineHeight); }
	//DynamicArray& queryRowData() { return Value<DynamicArray>(PropOffsets_LargeTextWindow.queryRowData); }
	//XTextParams& queryTextParams() { return Value<XTextParams>(PropOffsets_LargeTextWindow.queryTextParams); }
	//DynamicArray& rowData() { return Value<DynamicArray>(PropOffsets_LargeTextWindow.rowData); }
	//DynamicArray& tempRowData() { return Value<DynamicArray>(PropOffsets_LargeTextWindow.tempRowData); }
	//XTextParams& textParams() { return Value<XTextParams>(PropOffsets_LargeTextWindow.textParams); }
	float& vSpace() { return Value<float>(PropOffsets_LargeTextWindow.vSpace); }
};
