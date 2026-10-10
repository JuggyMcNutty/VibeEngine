//=============================================================================
// ScrollConsole: a scroll area and a slider used with the mouse, the mouse
// pressed by the fork's timeline (vibe/tools/dxcap/timelines/scroll.txt, on
// the same clock: seconds from the start). From the menu map, the keyboard
// screen is opened (its key list scrolls); on the schedule below the UI
// cursor is put on a part of the scrollbar -- the down arrow, the up arrow,
// the track below the thumb, the track above it, the thumb -- or on the list,
// and the timeline clicks there, holds, drags (the cursor moved down the
// track here while the thumb is held) or turns the wheel. After each, the
// scale's tick, span and count, the clip window's position and sizes and the
// arrows' sensitivity are logged ("DXSCROLL:"). Then the list's focus row is
// set far down and back, the clip following it; then the Sound screen's
// first slider is clicked near its start and its thumb dragged, its value
// text logged. Then an exit.
//
// 8 s   cursor on the down arrow      timeline: click at 9
// 12    cursor on the down arrow      timeline: held 13 to 14.5
// 16    cursor on the up arrow        timeline: click at 17
// 20    cursor at the track's foot    timeline: click at 21
// 24    cursor at the track's head    timeline: click at 25
// 28    the list to its top, the cursor on the thumb
//                                     timeline: press at 29, release at 31;
//                                     the cursor dragged 100 down, 29.3-30.3
// 32    cursor on the list            timeline: wheel down at 33, 33.2, 33.4
// 36                                  timeline: wheel up at 37
// 40    the focus row near the end, 42 the fourth row
// 44    the Sound screen; 46 cursor near the slider's start, click at 47
// 50    cursor on the slider's thumb  timeline: press at 51, release at 53;
//                                     the cursor dragged 96 right, 51.3-52.1
// 55    exit
//=============================================================================
class ScrollConsole extends Console;

var float RunTime;
var int Step;
var DeusExRootWindow Root;
var ScrollAreaWindow Area;
var ListWindow List;
var ScaleWindow Slider;
var MenuUISliderButtonWindow SliderButton;
var float DragX, DragY;
var int DragSteps;
var float NextDrag;

function Window FindClass(Window W, class<Window> Wanted)
{
	local Window C, Found;

	if (ClassIsChildOf(W.Class, Wanted))
		return W;
	for (C = W.GetBottomChild(); C != None; C = C.GetHigherSibling())
	{
		Found = FindClass(C, Wanted);
		if (Found != None)
			return Found;
	}
	return None;
}

function LogState(string Label)
{
	local int Col, Row, AreaH, AreaV, ChildH, ChildV;

	Area.clipWindow.GetChildPosition(Col, Row);
	Area.clipWindow.GetUnitSize(AreaH, AreaV, ChildH, ChildV);
	Log("DXSCROLL: " $ Label $ ": tick " $ Area.vScale.GetTickPosition() $ " span " $ Area.vScale.GetThumbSpan()
		$ " of " $ Area.vScale.GetNumTicks() $ ", clip at " $ Col $ "," $ Row $ " showing " $ AreaH $ "x" $ AreaV
		$ " of " $ ChildH $ "x" $ ChildV $ ", list y " $ int(List.y) $ ", up " $ Area.upButton.bIsSensitive
		$ ", down " $ Area.downButton.bIsSensitive);
}

function LogSlider(string Label)
{
	Log("DXSCROLL: " $ Label $ ": slider tick " $ Slider.GetTickPosition() $ " of " $ Slider.GetNumTicks()
		$ ", value " $ Slider.GetValue() $ ", text '" $ Slider.GetValueString() $ "', value box '"
		$ SliderButton.winScaleText.buttonText $ "'");
}

// The UI cursor on a point of a window.
function PointAt(Window W, float X, float Y, string What)
{
	W.SetCursorPos(X, Y);
	Log("DXSCROLL: at " $ RunTime $ " s the cursor on " $ What);
}

// The next step once its time comes.
function bool Due(float Time)
{
	return RunTime >= Time;
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);

	// A drag in progress: the cursor moved along every 0.1 s.
	if (DragSteps > 0 && RunTime >= NextDrag)
	{
		if (Slider != None)
		{
			DragX += 12;
			Slider.SetCursorPos(DragX, Slider.height / 2);
		}
		else
		{
			DragY += 10;
			Area.vScale.SetCursorPos(Area.vScale.width / 2, DragY);
		}
		DragSteps--;
		NextDrag = RunTime + 0.1;
	}

	switch (Step)
	{
	case 0:
		if (!Due(3.0))
			return;
		DeusExPlayer(P).ShowMainMenu();
		Root.InvokeMenuScreen(class'MenuScreenCustomizeKeys');
		break;
	case 1:
		if (!Due(5.0))
			return;
		Area = ScrollAreaWindow(FindClass(Root.GetTopWindow(), class'ScrollAreaWindow'));
		if (Area == None)
		{
			Log("DXSCROLL: no scroll area, exiting");
			P.ConsoleCommand("exit");
			Step = 99;
			return;
		}
		List = ListWindow(Area.clipWindow.GetChild());
		LogState("opened");
		break;
	case 2:
		if (!Due(8.0))
			return;
		PointAt(Area.downButton, Area.downButton.width / 2, Area.downButton.height / 2, "the down arrow");
		break;
	case 3:
		if (!Due(11.0))
			return;
		LogState("after a click on the down arrow");
		break;
	case 4:
		if (!Due(12.0))
			return;
		PointAt(Area.downButton, Area.downButton.width / 2, Area.downButton.height / 2, "the down arrow");
		break;
	case 5:
		if (!Due(15.5))
			return;
		LogState("after holding the down arrow 1.5 s");
		break;
	case 6:
		if (!Due(16.0))
			return;
		PointAt(Area.upButton, Area.upButton.width / 2, Area.upButton.height / 2, "the up arrow");
		break;
	case 7:
		if (!Due(19.0))
			return;
		LogState("after a click on the up arrow");
		break;
	case 8:
		if (!Due(20.0))
			return;
		PointAt(Area.vScale, Area.vScale.width / 2, Area.vScale.height - 4, "the track's foot");
		break;
	case 9:
		if (!Due(23.0))
			return;
		LogState("after a click on the track below the thumb");
		break;
	case 10:
		if (!Due(24.0))
			return;
		PointAt(Area.vScale, Area.vScale.width / 2, 4, "the track's head");
		break;
	case 11:
		if (!Due(27.0))
			return;
		LogState("after a click on the track above the thumb");
		break;
	case 12:
		if (!Due(28.0))
			return;
		// The list back at its top: a few pixels down the track is the thumb.
		Area.vScale.SetTickPosition(0);
		DragY = 8;
		PointAt(Area.vScale, Area.vScale.width / 2, DragY, "the thumb");
		break;
	case 13:
		if (!Due(29.3))
			return;
		DragSteps = 10;
		NextDrag = RunTime;
		break;
	case 14:
		if (!Due(30.6))
			return;
		LogState("dragging the thumb 100 down");
		break;
	case 15:
		if (!Due(31.5))
			return;
		LogState("after letting the thumb go");
		break;
	case 16:
		if (!Due(32.0))
			return;
		PointAt(Area.clipWindow, 20, 20, "the list");
		break;
	case 17:
		if (!Due(35.0))
			return;
		LogState("after the wheel down 3");
		break;
	case 18:
		if (!Due(39.0))
			return;
		LogState("after the wheel up 1");
		break;
	case 19:
		if (!Due(40.0))
			return;
		List.SetFocusRow(List.IndexToRowId(List.GetNumRows() - 2), true);
		break;
	case 20:
		if (!Due(41.0))
			return;
		LogState("after the focus row went to the second to last");
		List.SetFocusRow(List.IndexToRowId(3), true);
		break;
	case 21:
		if (!Due(43.0))
			return;
		LogState("after the focus row went to the fourth");
		break;
	case 22:
		if (!Due(44.0))
			return;
		Root.PopWindow();
		Root.InvokeMenuScreen(class'MenuScreenSound');
		break;
	case 23:
		if (!Due(46.0))
			return;
		SliderButton = MenuUISliderButtonWindow(FindClass(Root.GetTopWindow(), class'MenuUISliderButtonWindow'));
		if (SliderButton == None)
		{
			Log("DXSCROLL: no slider, exiting");
			P.ConsoleCommand("exit");
			Step = 99;
			return;
		}
		Slider = SliderButton.winSlider;
		LogSlider("opened");
		PointAt(Slider, 30, Slider.height / 2, "the slider's track near its start");
		break;
	case 24:
		if (!Due(49.0))
			return;
		LogSlider("after a click near the start");
		break;
	case 25:
		if (!Due(50.0))
			return;
		DragX = 30;
		PointAt(Slider, DragX, Slider.height / 2, "the slider's thumb");
		break;
	case 26:
		if (!Due(51.3))
			return;
		DragSteps = 8;
		NextDrag = RunTime;
		break;
	case 27:
		if (!Due(52.4))
			return;
		LogSlider("dragging the thumb 96 right");
		break;
	case 28:
		if (!Due(54.0))
			return;
		LogSlider("after letting the thumb go");
		break;
	case 29:
		if (!Due(55.0))
			return;
		Root.PopWindow();
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	default:
		return;
	}
	Step++;
}
