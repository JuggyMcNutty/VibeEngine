//=============================================================================
// DynLoadConsole: Object.DynamicLoadObject with a group in the name, logged
// with "DXDYNLOAD:" from the menu map and an exit -- Effects' LaserBeam1, a
// FireTexture in the group Laser, by its group, without it and by a group
// that is not there; as a Texture, not its own class; a name not in the
// package (MayFail); and DeusEx's JCDentonMale class by a group that is not
// there (dx-reverse-info core-dll.md, the natives).
//=============================================================================
class DynLoadConsole extends Console;

var float RunTime;
var bool bDone;

function Probe(string Name, class<Object> Cls, optional bool bMayFail)
{
	local Object O;

	O = DynamicLoadObject(Name, Cls, bMayFail);
	if (O == None)
		Log("DXDYNLOAD: " $ Name $ " as " $ Cls.Name $ " = None");
	else if (Cls == class'Class')
		Log("DXDYNLOAD: " $ Name $ " as " $ Cls.Name $ " = class " $ O.Name);
	else
		Log("DXDYNLOAD: " $ Name $ " as " $ Cls.Name $ " = " $ O.Class.Name $ " " $ O.Name $ " in " $ O.Outer.Name);
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || bDone)
		return;
	RunTime += Delta;
	if (RunTime < 2.0)
		return;
	Probe("Effects.Laser.LaserBeam1", class'FireTexture');
	Probe("Effects.LaserBeam1", class'FireTexture');
	Probe("Effects.Wrong.LaserBeam1", class'FireTexture');
	Probe("Effects.Laser.LaserBeam1", class'Texture');
	Probe("Effects.Laser.NoSuchTexture", class'FireTexture', true);
	Probe("DeusEx.Wrong.JCDentonMale", class'Class');
	Log("DXDYNLOAD: done");
	bDone = true;
	Viewport.Actor.ConsoleCommand("exit");
}
