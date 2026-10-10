//=============================================================================
// SizeConsole: the size a new window takes. From the menu map, the main menu
// shown, a plain window is put on it, and in it a plain window, an empty
// text window, an empty label placed as the computers place theirs (SetPos,
// SetText("")), and a text window given text and then emptied. Each is
// logged ("DXSIZE:") as made, on the next frames, and with its preferred
// size asked (QueryPreferredSize). Then an exit.
//=============================================================================
class SizeConsole extends Console;

var float RunTime;
var int Step;
var Window Holder;
var Window Plain;
var TextWindow Empty;
var MenuUISmallLabelWindow Label;
var TextWindow Emptied;

function LogSize(string Label, Window W)
{
	local float PW, PH;

	W.QueryPreferredSize(PW, PH);
	Log("DXSIZE: " $ Label $ ": at " $ W.x $ "," $ W.y $ " size " $ W.width $ "x" $ W.height $ ", preferred " $ PW $ "x" $ PH);
}

function LogAll(string When)
{
	Log("DXSIZE: " $ When);
	LogSize("  holder", Holder);
	LogSize("  plain window", Plain);
	LogSize("  empty text window", Empty);
	LogSize("  empty label", Label);
	LogSize("  emptied text window", Emptied);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);

	switch (Step)
	{
	case 0:
		if (RunTime < 3.0)
			return;
		DeusExPlayer(P).ShowMainMenu();
		break;
	case 1:
		if (RunTime < 4.0)
			return;
		Holder = Root.GetTopWindow().NewChild(class'Window');
		Holder.SetPos(10, 10);
		Holder.SetSize(300, 200);
		Plain = Holder.NewChild(class'Window');
		Empty = TextWindow(Holder.NewChild(class'TextWindow'));
		Label = MenuUISmallLabelWindow(Holder.NewChild(class'MenuUISmallLabelWindow'));
		Label.SetPos(50, 105);
		Label.SetText("");
		Emptied = TextWindow(Holder.NewChild(class'TextWindow'));
		Emptied.SetPos(50, 150);
		Emptied.SetText("Some text");
		LogAll("as made");
		break;
	case 2:
		LogAll("the next frame");
		Emptied.SetText("");
		break;
	case 3:
		if (RunTime < 5.0)
			return;
		LogAll("a second on, the text window emptied");
		break;
	case 4:
		if (RunTime < 6.0)
			return;
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	default:
		return;
	}
	Step++;
}
