//=============================================================================
// CapWader: the UNATCOTroop WadeConsole walks into the water -- its state
// CapMoving, modelled on ScriptedPawn's Idle (reactions blocked, bumps and
// injuries ignored) and out of stasis, walks it to CapDest with MoveTo and
// logs, with "DXWADE:", when the move ends. Its senses are ignored and a wall it meets
// only logs (HitWall probed, so the move goes on along the wall). A fall's
// FallingState (ScriptedPawn.Falling) comes back to it at Done, logged too.
//=============================================================================
class CapWader extends UNATCOTroop;

var vector CapDest;

state CapMoving
{
	ignores Bump, Frob, ReactToInjury, SeePlayer, HearNoise, EnemyNotVisible;

	function HitWall(vector HitNormal, actor Wall)
	{
		Log("DXWADE: event hit wall " $ Wall $ " normal " $ HitNormal $ " at " $ Location);
	}

	function BeginState()
	{
		StandUp();
		BlockReactions(true);
		bCanConverse = False;
		SeekPawn = None;
		EnableCheckDestLoc(false);
		// Out of the player's sight it would otherwise freeze (stasis)
		bStasis = False;
		// FallingState goes on to the next state, else Wandering
		SetNextState('CapMoving', 'Done');
	}

	function EndState()
	{
		ResetReactions();
		bCanConverse = True;
	}

Begin:
	Acceleration = vect(0,0,0);
	Stop;

Go:
	MoveTo(CapDest);
	Log("DXWADE: event move ended at " $ Location $ " timer " $ MoveTimer $ " physics " $ int(Physics));

Done:
	Acceleration = vect(0,0,0);
	Log("DXWADE: event at Done at " $ Location $ " physics " $ int(Physics));
}
