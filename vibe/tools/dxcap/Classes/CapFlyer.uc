//=============================================================================
// CapFlyer: the Seagull FlightConsole flies on a fixed course -- its state
// CapMoving, CapWader's, takes it to CapDest with MoveTo, or with
// MoveToward to CapTarget when one is set, and logs, with "DXFLIGHT:", when
// the move ends; at Drop it falls and waits for its landing (WaitForLanding,
// which a fall of over 2.5 s ends with LongFall). A wall it meets only logs,
// and a fall's FallingState comes back to it at Done, logged too.
//=============================================================================
class CapFlyer extends Seagull;

var vector CapDest;
var Actor CapTarget;

state CapMoving
{
	ignores Bump, Frob, ReactToInjury, SeePlayer, HearNoise, EnemyNotVisible;

	function HitWall(vector HitNormal, actor Wall)
	{
		Log("DXFLIGHT: event hit wall " $ Wall $ " normal " $ HitNormal $ " at " $ Location);
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
	if (CapTarget != None)
		MoveToward(CapTarget);
	else
		MoveTo(CapDest);
	Log("DXFLIGHT: event move ended at " $ Location $ " timer " $ MoveTimer $ " physics " $ int(Physics));
	Goto('Done');

Drop:
	SetPhysics(PHYS_Falling);
	WaitForLanding();
	Log("DXFLIGHT: event landed at " $ Location $ " physics " $ int(Physics));

Done:
	Acceleration = vect(0,0,0);
	Log("DXFLIGHT: event at Done at " $ Location $ " physics " $ int(Physics));
}
