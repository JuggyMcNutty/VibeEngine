//=============================================================================
// CoronaLight: a light that can be spawned and moved, with a corona -- a
// dynamic light (neither bStatic nor bNoDelete), as no Deus Ex map places
// one. CoronaConsole gives it Light169's skin and look.
//=============================================================================
class CoronaLight extends Light;

defaultproperties
{
	bStatic=False
	bNoDelete=False
	bMovable=True
	bCorona=True
	LightType=LT_Steady
	LightBrightness=255
	LightRadius=40
}
