
#include "Precomp.h"
#include "ULargeTextWindow.h"
#include "Packages/Extension/Windows/UGC.h"
#include "Engine.h"

// The original's XLargeTextWindow::Init (0x1002d480): lines 1 apart, its
// text not its accelerator.
void ULargeTextWindow::InitDefaults()
{
	UTextWindow::InitDefaults();
	vSpace() = 1.0f;
	lineHeight() = 0.0f;
	EnableTextAsAccelerator(false);
}

void ULargeTextWindow::SetVerticalSpacing(std::optional<float> newVSpace)
{
	// UNUSED from scripts.
	LogUnimplemented("LargeTextWindow.SetVerticalSpacing");
}

// The original's (XLargeTextWindow::ParentRequestedGranularity 0x1002e380,
// ComputeLineHeight 0x1002d9d0): a line down -- the font's height, at least
// 1, plus the vertical spacing -- a pixel across; then the script's say.
void ULargeTextWindow::ParentRequestedGranularity(float& hGranularity, float& vGranularity)
{
	UGC* gc = engine->dxgc;
	UGC::TextSettings saved = gc->UseTextSettings(this);
	vGranularity = std::max(gc->GetFontHeight(false), 1.0f) + vSpace();
	hGranularity = 1.0f;
	gc->RestoreTextSettings(saved);
	UWindow::ParentRequestedGranularity(hGranularity, vGranularity);
}
