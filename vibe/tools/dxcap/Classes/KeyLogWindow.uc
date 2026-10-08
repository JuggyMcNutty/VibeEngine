//=============================================================================
// KeyLogWindow: a modal of the package's own that logs, with "DXKEYPAD:",
// every key the root hands it (RawKeyPressed) and lets it go on. KeypadConsole
// shows it while vibe/tools/dxcap/timelines/stray-release.txt sends a key's
// release with no press before it.
//=============================================================================
class KeyLogWindow extends ModalWindow;

event InitWindow()
{
	Super.InitWindow();
	SetSize(100, 100);
}

event bool RawKeyPressed(EInputKey key, EInputState iState, bool bRepeat)
{
	Log("DXKEYPAD: the logging window got key " $ int(key) $ " state " $ int(iState));
	return false;
}
