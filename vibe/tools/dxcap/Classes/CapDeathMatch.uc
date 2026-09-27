//=============================================================================
// CapDeathMatch: the net tests' game -- Deus Ex's deathmatch without its
// check that a joining player's console is the stock one (Engine.Console),
// which disconnects the test's scripted client (JoinConsole). Only the
// server needs it: a game info stays on the server.
//=============================================================================
class CapDeathMatch extends DeathMatchGame;

function CheckPlayerConsole(PlayerPawn CheckPlayer)
{
}
