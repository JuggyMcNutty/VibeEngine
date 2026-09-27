//=============================================================================
// SaveHelper: SaveConsole's save, made from an actor's tick -- the original
// ticks its console inside a draw (UConsole::PreRender), and a save paints
// its progress with a draw of its own, which the original's OpenGLDrv stops
// on ("LockCount==0"). The helper is in the save, but expires on the first
// tick after a load: destroyed before the save, it was collected during it,
// and the original crashed back in its tick.
//=============================================================================
class SaveHelper extends Info;

var int Slot;
var string Description;

event Tick(float Delta)
{
	local DeusExPlayer P;

	P = DeusExPlayer(Owner);
	Disable('Tick');
	LifeSpan = 0.01;
	if (P != None)
	{
		P.SaveGame(Slot, Description);
		Log("DXSAVE: saved to slot " $ Slot);
	}
}
