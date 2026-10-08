//=============================================================================
// PerfTurnConsole: PerfConsole with the view turning about the start at 45
// degrees a second, a turn every 8 s, so the frames measured draw what a
// player looking round sees, not one view.
//=============================================================================
class PerfTurnConsole extends PerfConsole;

function rotator ViewAt(float T)
{
	local rotator R;
	R = StartRot;
	R.Yaw += int(8192.0 * T);
	return R;
}
