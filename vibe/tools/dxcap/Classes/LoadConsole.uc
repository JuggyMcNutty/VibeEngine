//=============================================================================
// LoadConsole: a save the other engine wrote, loaded -- from the menu map, 3 s
// in, slot 9 (SaveConsole's) is loaded, and 5 s into the loaded level what it
// holds is logged as SaveConsole logs it; then an exit. With nothing loaded
// after 60 s it says so and exits.
//=============================================================================
class LoadConsole extends SaveConsole;

var float RunTime;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super(Console).Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;

	if (Step == 0 && RunTime > 3.0)
	{
		Log("DXSAVE: loading slot 9");
		DeusExPlayer(P).LoadGame(9);
		Step = 1;
	}
	else if (Step == 1 && CurrentMap ~= "01_NYC_UNATCOIsland" && MapTime > 5.0)
	{
		Census(P, "loaded");
		Log("DXSAVE: exiting");
		P.ConsoleCommand("exit");
		Step = 2;
	}
	else if (Step == 1 && RunTime > 60.0)
	{
		Log("DXSAVE: nothing loaded after 60 s, in " $ MapOf(P) $ "; exiting");
		P.ConsoleCommand("exit");
		Step = 2;
	}
}
