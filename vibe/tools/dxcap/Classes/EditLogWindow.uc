//=============================================================================
// EditLogWindow: a window of the package's own that logs, with "DXEDIT:",
// every TextChanged an edit window below it announces, and lets it go on.
// EditConsole puts its edit window in one.
//=============================================================================
class EditLogWindow extends Window;

event bool TextChanged(window edit, bool bModified)
{
	Log("DXEDIT:   TextChanged, modified " $ bModified $ ", text '" $ EditWindow(edit).GetText() $ "'");
	return false;
}
