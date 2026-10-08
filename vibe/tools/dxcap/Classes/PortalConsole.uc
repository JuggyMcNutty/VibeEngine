//=============================================================================
// PortalConsole: a map opened at a portal -- the console's open with
// "#<portal>" -- logged with "DXPORTAL:" and the console's clock
// (vibe/docs/DEVELOPMENT.md, scripted runs).
//
// From the menu map (the fork started on DX.dx too), it opens
// 01_NYC_UNATCOIsland#tunnel_drop_nyc; there it logs the level's URL, where
// the player stands, and each teleporter whose tag is the portal's, with
// the player's distance from it; then it exits.
//=============================================================================
class PortalConsole extends Console;

var float CapTime, MapTime;
var string CurrentMap;
var int Phase;

function string MapOf(PlayerPawn P)
{
	local string URL;
	local int i;
	URL = P.Level.GetLocalURL();
	i = InStr(URL, "/");
	while (i >= 0)
	{
		URL = Mid(URL, i + 1);
		i = InStr(URL, "/");
	}
	i = InStr(URL, ".");
	if (i >= 0)
		URL = Left(URL, i);
	i = InStr(URL, "?");
	if (i >= 0)
		URL = Left(URL, i);
	i = InStr(URL, "#");
	if (i >= 0)
		URL = Left(URL, i);
	return URL;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local Teleporter T;
	local string M;
	local int Found;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	CapTime += Delta;
	M = MapOf(P);
	if (M != CurrentMap)
	{
		Log("DXCAP: now in " $ M $ " at " $ CapTime);
		CurrentMap = M;
		MapTime = 0;
	}
	MapTime += Delta;

	if (Phase == 0)
	{
		if (MapTime > 3.0)
		{
			Log("DXPORTAL: opening 01_NYC_UNATCOIsland#tunnel_drop_nyc");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland#tunnel_drop_nyc");
			Phase = 1;
		}
	}
	else if (Phase == 1)
	{
		if (M ~= "01_NYC_UNATCOIsland" && MapTime > 3.0)
		{
			Log("DXPORTAL: the level's URL " $ P.Level.GetLocalURL());
			Log("DXPORTAL: the player at " $ P.Location);
			foreach P.AllActors(class'Teleporter', T)
			{
				if (string(T.Tag) ~= "tunnel_drop_nyc")
				{
					Log("DXPORTAL: " $ T.Name $ " (tag " $ T.Tag $ ") at " $ T.Location
					    $ ", the player " $ VSize(P.Location - T.Location) $ " from it");
					Found++;
				}
			}
			Log("DXPORTAL: " $ Found $ " teleporters tagged tunnel_drop_nyc; exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
