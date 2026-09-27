//=============================================================================
// JoinConsole: the joining side of a net test -- from the menu map it opens
// the server on this machine (ServeConsole's), and once in the game its
// player stands 5 s, walks forward 5 s as with the key held, and stands
// again; its place, and every other player's as this side has it, is logged
// each second, and shot at the stops. Exits 25 s into the game -- or back in
// the menu, dropped or never in after 40 s.
//=============================================================================
class JoinConsole extends Console;

var float MenuTime, GameTime, LogTime;
var int Step;

event Tick(float Delta)
{
	local PlayerPawn P;
	local Pawn Other;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;

	if (P.Level.NetMode != NM_Client)
	{
		MenuTime += Delta;
		if (Step == 0 && MenuTime > 3.0)
		{
			Log("DXNET: opening 127.0.0.1:7790");
			P.ConsoleCommand("open 127.0.0.1:7790");
			Step = 1;
		}
		else if (Step >= 1 && Step < 5 && (GameTime > 0.0 || MenuTime > 40.0))
		{
			// Dropped back to the menu, or never in: exit, so the log is
			// written.
			Log("DXNET: in the menu, " $ GameTime $ " s of game; exiting");
			P.ConsoleCommand("exit");
			Step = 5;
		}
		return;
	}

	GameTime += Delta;
	if (GameTime - LogTime >= 1.0)
	{
		LogTime = GameTime;
		Log("DXNET: t=" $ int(GameTime) $ " at " $ P.Location $ " state " $ P.GetStateName() $ " physics " $ P.Physics);
		foreach P.AllActors(class'Pawn', Other)
			if (Other != P)
				Log("DXNET: t=" $ int(GameTime) $ " other " $ Other.Name $ " at " $ Other.Location $ " velocity " $ Other.Velocity);
	}

	if (Step == 1 && GameTime > 5.0)
	{
		Log("DXNET: standing at " $ P.Location);
		P.ConsoleCommand("shot");
		Step = 2;
	}
	else if (Step == 2)
	{
		P.aBaseY = 300.0;
		if (GameTime > 10.0)
		{
			P.aBaseY = 0.0;
			Step = 3;
		}
	}
	else if (Step == 3 && GameTime > 15.0)
	{
		Log("DXNET: walked to " $ P.Location);
		P.ConsoleCommand("shot");
		Step = 4;
	}
	else if (Step == 4 && GameTime > 25.0)
	{
		Log("DXNET: exiting at " $ P.Location);
		P.ConsoleCommand("exit");
		Step = 5;
	}
}
