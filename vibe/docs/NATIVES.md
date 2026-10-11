# Differences from the original

Where the engine fork still differs from the original Deus Ex, feature by
feature. Each section opens with its status -- whether the fork matches the
original, the original's write-up, the console that compares the two --
then lists what still differs. What each engine patch changes is
[what the fork changes](ENGINE.md#what-the-fork-changes).

## How it is known

- **The audit**, [`vibe/tools/natives_audit.py`](../tools/natives_audit.py):
  every native the game's packages declare against the fork's -- a stub, a
  partial one, an iterator that makes none, or none registered -- with its
  call sites in the game's scripts; `--runs` adds which stubs a run's log
  fired. A stub logs once a session, naming the script function that called
  it, so a run shows which stubs a map reaches, not how often. Run it for
  the current list of stubs; this doc does not copy it.
- **Reading the DLLs**
  ([dx-reverse-info](https://github.com/JuggyMcNutty/dx-reverse-info)): C++
  that is not a native leaves no stub, and only the original shows whether
  an implemented native does what it does.
- **Scripted runs of both engines**
  ([scripted runs](DEVELOPMENT.md#scripted-runs-of-both-engines)), by the
  consoles named below.

## Stops the game

Differs. An error the fork does not catch -- a script error, an unknown
native, a failed save -- exits the engine with 1, and the launcher shows its
crash banner. No map or action is known to reach one; the audit lists the
natives whose call would.

## Saving, loading and travel

Matches the original ([travel and saving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-game-engine-travel-and-saving),
[the save directory](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-save-directory)):
each engine loads the other's saves (`SaveConsole`, `LoadConsole`), and a
level returned to keeps its game (`ReturnConsole`). A level's start
([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-levels-tick))
runs each pass over its actors as the list grows, so a map's carcass holds
its items hidden in `Idle2` and its search hands them over whole
(`CarcassConsole`), and after `BeginPlay` finds every actor's zone where it
is, whatever region the map saved: DXMP_Silo's items, crates and robots
start in their zones, where the map's regions put them in none and their
first fall took them out of the world (`VisibleConsole` there); every level
loaded, a save's or one returned to, then
calls `PostPostBeginPlay` (`LoadConsole`, `ReturnConsole`). A save's load
logs the saved player in as the original's does, with `?loadgame`: the
game's `Login` keeps it as saved, and its `TravelPostAccept` spawns the
level's mission script, which no save holds -- an object of a transient
class is not written (`LoadConsole` over either engine's saves and the
original's hub save, `ReturnConsole`). Before a level is saved into
`Current`, what travels with the player goes from it, a decoration in its
hands too (`PruneTravelActors`; `ReturnConsole`). A map opened at a portal
(`open <map>#<portal>`) starts the player at the teleporter of that tag
(`PortalConsole`). Differs: a level returned to from `Current` keeps the
map's name in its URL, where the original's names the saved file
(`..\Save\Current\<map>.dxs`), as `Level.GetLocalURL` shows
(`ReturnConsole`).

## Flags

Matches the original ([flags](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#flags)):
each engine finds the other's saved flags. Mission scripts come up as the
original's: `MissionConsole` over all 83 maps finds every mission map's
script running. `DX`, `DXOnly` and `Entry` are not mission maps, and
`12_Vandenberg_Tunnels` (no `MissionScript` actor) and `99_Endgame4` (no
`DeusExLevelInfo`) run none in the original either.

## Conversations

Matches the original ([`ConSys.dll`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/consys-dll.md)):
`SkipConsole` compares a skipped line's speech, `CaptureConsole` a jump to a
comment's label, `EndgameConsole` an ending skipped to its credits: 99_Endgame2's
conversation stopped, its Played flag, the mission script's timer and quote,
and the credits up at the level's fifteenth second, alike.

## What the player reads

Matches the original's parser ([`DeusExText.dll`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusextext-dll.md)),
but for one quirk left out on purpose: at a comment (`NOTE`, `GOAL`,
`COMMENT`) with no end tag the original reads on past the text's end, so
`09_EmailMenu_ShipOps`'s listing depends on the memory after it. The fork
stops at the end, hiding the rest.

## Every NPC

### The native tick: `AScriptedPawn::Tick`

Matches the original ([the native tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-native-tick)),
run before the actor tick, in the original's order.

### Starting up

Matches the original ([a level's tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-levels-tick)):
every actor starts a level 10 s undrawn and every dynamic one ticks in its
first tick, so a pawn out of sight starts from its state code (`AIConsole`).

### Hearing: the AI event system

Matches the original ([the AI event system](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-ai-event-system)),
saved in its layout ([saved](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#saved)):
each engine loads the other's saves with every listener. The fork also reads
the manager as its own older saves lay it out.

### Moving: wandering and tactical movement

Matches the original ([moving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#moving),
[reaching](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#reaching),
[the search](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-search)):
`MoveConsole` (`move.py`) has all 52 of Liberty Island's pawns move within
15% (or 200 units) of the original's distance. Each that walks to a spot and
stops there ends where the original's does, within a unit, though 25 to 110
units short of its distance: the console counts from its first line, 2 s in
by its own clock, which in the original's run takes in the island's first
frame, the load's, some 1.5 s of which the actors get 0.4 s
([a long frame](#small)). By then the fork's pawns are further along their
first walk. `ReachConsole`'s 144 answers, twelve pawns each asked from its
home spot, are the original's to the unit. A dead robot frozen in `Dying`
for good is the original's own behaviour, and the rest of a death is alike
too (`DeathConsole`).

## Out of sight

Matches the original ([render time](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#render-time),
[which actors are drawn](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#which-actors-are-drawn),
[stasis and render time](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#stasis-and-render-time)):
whether an actor counts as drawn is tested piece by piece in the BSP walk,
and the proxies' rectangles are the original's: at Liberty Island's start
the same three NPCs count as drawn in both engines, every other one hidden
from the camera by the level, as the traces have it (`AIConsole`). An actor drawn
on its own (`GC.DrawActor`, `Canvas.DrawActor`) counts as drawn whenever its
rectangle is on the frame, whatever is in front of it: an NPC the vision
augmentation draws through a wall stays drawn while it is in view
(`AugVisionConsole`). Beside it the fork keeps Distant AI's own
`LastVisibleFrame`
([its patches](ENGINE.md#settings-the-launcher-exposes)). Differs:

- **The pixels.** The fork's grid takes a pixel whose centre a polygon
  covers; the original's span buffer, one whose lower right corner it covers
  ([the pixels](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-frame)).
  A sliver under a pixel tall can show in one and not the other.
- **One span buffer** for the frame, where the original keeps one per zone.
  A buffer per zone would change no test in 15,500 at the starts of Liberty
  Island, UNATCO HQ and Battery Park: the walls round a portal are drawn
  before what lies beyond it.
- **Drawing** is the fork's own box test, the depth buffer hiding the rest;
  only whether an actor counts as drawn is the original's.

## On screen

### The view's width

Differs by choice: on a screen wider than 4:3 the view keeps the height a
4:3 screen shows and sees more at the sides (Hor+), where the original keeps
its field of view across the width and loses height. The game's scenes are
framed for 4:3: at 16:9, with the original's angle, the intro's "Deus Ex"
under the logo falls off the bottom; with the fork's it shows. At 4:3 and
narrower the view is the original's. The scene, the coronas, what the HUD
draws in 3D, the frob highlight's box and the meshes' level of detail
([mesh detail](#mesh-detail)) all take the one angle (`HorPlusFovAngle`), so
on a wide screen a mesh keeps less detail than the original's at the same
distance.

### Particles and lasers: render iterators

Matches the original ([render iterators](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#render-iterators),
[particles and lasers](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#particles-and-lasers)):
`LaserConsole` has the beams' red over the floor within 0.5% of `D3DDrv`'s,
and `ParticleConsole`'s particles rise, fall, grow and fade as the
original's: its smoke's light row by row 6 to 17% over `D3DDrv`'s, its puffs
under gravity in the same rows. Differs:

- **Occlusion.** The original occludes each item's sprite through its span
  buffer; the fork clips items to the view (and a portal's spans) before its
  BSP walk and leaves the rest to the depth buffer. The look is the same; a
  hidden item's cost is paid. Whether the proxy counts as drawn is the
  original's.

### Coronas

Matches the original ([coronas](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#coronas);
`CoronaConsole`): the static lights reaching the player's leaf get coronas,
and no dynamic light does, the original's pass reading the leaf's dynamic
lights only after its occlusion pass has emptied them. `CoronaConsole`'s
own movable lamp shows none in either engine, in the player's leaf or out
of it, and logs the same BSP leaves in both. A light behind a glass mover
shows none in either: the line to it is checked against the level and the
movers with every surface, the invisible collision hull round the glass's
pane among them, which a sight test passes (`GlassConsole` at
03_NYC_Airfield's `BreakableGlass2`, `Light56` beyond it).

### What a pawn holds

Matches the original ([a pawn's attachments](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-pawns-attachments));
`HeldConsole`.

### Mesh detail

Matches the original's vertex budget and its morph ([mesh detail](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#mesh-detail)):
`MeshConsole` with `LODBias` 0.004 and 0.0015, Paul Denton cut down to a
few vertices, has his shape at a correlation of 0.93 and 0.95 to
`D3DDrv`'s. On a screen wider than 4:3 the budget takes the wider angle
([the view's width](#the-views-width)).

### Lighting

Matches the original's light maps, mesh lighting and the faces a mesh draws ([lighting](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#lighting)):
`ViewConsole`, `LaserConsole`, `MeshConsole` and `CoronaConsole` come out
within a level or two of 256 of `D3DDrv`'s frames. A spotlight's cone is the
original's, its cosine `1 − LightCone ÷ 256`, its value rounded down, and a
light or fog map lights texel i at its centre, `Pan.X + i × UScale` along
U, where both engines draw it
([a texel's place](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#light-maps)):
`LightConsole` has 03_NYC_Airfield's `Spotlight7` and Liberty Island's
`Spotlight0` within 0.90 to 1.05 of `D3DDrv`'s frames across their pools,
in blocks of 16 pixels ([brightness](#brightness)).
The other effects' shapes are Render.dll's own, on the level's time.
`LightConsole`, shooting both engines at one moment of it (`ShotAt`), has
06_HongKong_WanChai_Canal's disco `Light343` at 1.00 of `D3DDrv`'s frame,
02_NYC_BatteryPark's cylinder `Light81` at a correlation of 0.98 to it, and
09_NYC_ShipBelow's slow wave `Light16` at 1.01 of its brightness. The rest
are the reading's alone: the stock views of the searchlight, non-incidence
and the rotor come out the same under other shapes too, and no stock light
shows a fast wave (99_Endgame2's one is dark), shock, shell or interference.
Still the fork's own:

- **Light maps**
  - floats, converted on the CPU for the Smart Pro's GPU (engine patches
    0002 and 0024);
  - a map whose animating or moving lights changed built, converted and sent
    to the GPU again only where they reach now or reached at its last build,
    where the original builds and uploads it whole every frame;
  - the lookup each surface's own last map, else a `std::map`, where the
    original's cache hashes and first checks the item it found last;
  - a still-shaped animated light re-run, where the original keeps it as its
    shadowed light and rescales it;
  - blink and strobe on the fork's timing; the original's blink goes by the
    lowest bit of its turn count and its strobe turns over each frame, both
    at the frame rate;
  - a flicker drawn at most 25 times a second;
  - `LE_CloudCast`, the plain shape, built once, where the original re-runs
    it every frame (its table counts the shape as changing): the same light.
- **Meshes**
  - the moving lights come from the fork's light tree near the actor, not
    the leaf's own list;
  - the `AmbientGlow` pulse runs on the level's time, the original's on the
    viewport's.

### Brightness

Matches the original's picture ([gamma](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#gamma),
[the light maps' brightness](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#the-light-maps-brightness),
[textures](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#textures)):
the Brightness slider raises the picture to 1 / (2.5 × Brightness), as
`D3DDrv` sets its ramp, and the launcher's other gamma mode (XOpenGL's
curve) takes the same product. Deus Ex's shaders take nothing off a
texture's colours (`NO_DARKCLAMP`), as `D3DDrv` draws 32-bit textures on any
display of 24 bits or more, on every device: `MeshConsole`'s shots under
Vulkan, with descriptor indexing and without (the Smart Pro's), are within
0.2% of the GL device's. Other games keep upstream's `darkClamp`. How a
fork shot meets the original's frames:
[scripted runs](DEVELOPMENT.md#scripted-runs-of-both-engines). Differs:

- `MeshConsole`'s Liberty Island pier, where nothing is in hand, is 1.6%
  under `D3DDrv`'s frames with the detail textures on, and 0.7% over with
  them off in both (`NoDetail`): the fork draws the pier's detail texture,
  the original none ([the menus' settings](#the-menus-settings)). Its crate,
  box and barrel are within 0.8%, Paul Denton a level over, and
  `LaserConsole`'s corridor within 0.9%.
- 06_HongKong_WanChai_Underworld's floor under its two disco lights
  (`LightConsole` on `Light2` and `Light21`) is 0.35 to 0.57 of `D3DDrv`'s
  frames, and the same under other light shapes. The disco lights do not
  light it: they are `bSpecialLit`, and its surfaces (BSP surface 1251
  under `Light2`, 1230 under `Light21`; `CoreTexStone.Stone.ShnyBlacMarbl_A`
  with the detail texture `DStone_A`) are not `PF_SpecialLit`. Their light
  maps hold `ZoneInfo1`'s ambient (brightness 20, saturation 50) and, on
  1251, the steady `Light63`; why the fork's floor is darker is unread.

### The screen flash

Matches the original ([the rest of a frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#the-rest-of-a-frame));
`FlashConsole`.

### Draw order and movers

Matches the original's order
([a frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-frame)):
after the portals, the opaque surfaces with their decals and the solid
actors; then, each back to front, the parts of the actors not drawn
translucent (a mesh's translucent faces, a sprite of another style), the
translucent and modulated surfaces with their decals, the movers' among
them, and the actors drawn translucent (`STY_Translucent`), a render
iterator's items by its proxy's style. So glass blends over a mesh or a
sprite behind it in both engines. `GlassConsole`'s glass movers come out
as the original's frames: the room seen through 05_NYC_UNATCOMJ12lab's
`BreakableGlass16` and 03_NYC_Airfield's `BreakableGlass2` within 2%, and
behind the first an unlit crate within 0.3% and an unlit sprite drawn
`STY_Normal` within 0.5%, the glass adding 7.8 of 255 over the sprite to
the original's 8.5. Differs, read from the code (`VisibleFrame::Draw`,
`VisibleMesh`, `VisibleBrush`):

- **Back to front** by the distance of one point of each, where the
  original's lists run in the order its BSP walk met them.
- **A mesh's translucent faces** are drawn after all its opaque ones. The
  original draws them in face order among them, so one drawn before an
  opaque face behind it is lost where that face writes over it.
- **Movers** are drawn as their brushes, through their own matrix, with no
  depth bias: their opaque polygons with the solid actors, their
  translucent ones among the surfaces. The original files their polygons
  into the level's BSP and drops what lands in solid. A mover
  hidden after it is filed stays drawn there in the original, where the
  fork stops drawing it (`GlassConsole`); no stock script hides a mover.

### Fire, water and ice textures

Matches `Fire.dll` step for step ([`fire-dll.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/fire-dll.md)),
checked against its own routines run in an emulator. How a mesh shows its
textures is the original's too
([mesh textures](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#mesh-textures)).
Differs:

- **The pace** ([stepping](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/fire-dll.md#stepping)):
  a texture with no `MaxFrameRate`, as most of Deus Ex's have, steps every
  frame in the original, faster at a higher frame rate; the fork steps it at
  most 60 times a second, the original's pace at 60 frames.
- **The wave lighting table** can be one step off at a single entry, in 3 of 200 settings: the
  fork takes the C library's cosine, the original the x87's, and they differ in the last bit.
- The original's fire repeats itself from run to run; the fork's varies
  (`LaserConsole`).

### Head turns and lip sync: blend animations

Matches the original ([blend animations](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#blend-animations)).

### Lists

Matches the original ([lists](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#lists)):
the rows, fields, sort order and selections that `MenuConsole` and
`ScreenConsole` log are alike in both engines. A list shown with rows
selects and focuses its first, so the Images screen opens on its first
image; a float field reads as its shown number. The hot keys and a drag's
selection are the original's, read from its code; a list in a clip window
fills it, and scrolls a row at a time (`ScrollConsole`).

### The UI

Matches the original ([the UI in front of the game](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#the-ui-in-front-of-the-game),
[layout](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#layout),
[text](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#text),
[buttons](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#buttons),
[scales and scrolling](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#scales-and-scrolling),
[tiles and tab groups](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#tiles-and-tab-groups),
[drawing](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#drawing),
[small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#small)):
`GetConsole`, `BeltConsole`, `BorderConsole`, `ColorsConsole`,
`ChoiceConsole` for a conversation's choices, `SizeConsole` for a new
window's size, `ScrollConsole` for a scroll area and a slider,
`ClickConsole` for a button, a choice and a checkbox clicked, a menu
dragged by its title bar and Alt with a letter, `EditConsole` for a
script's text in an edit field, a text window and a button, and
`MenuConsole` and `ScreenConsole`, which log every menu and game screen's
windows: their places, sizes, text, focus, a slider's value text, a scroll
area's bars, a tile's children and a list's rows read alike in both engines
but for what is listed below. The same, read from the original's code:

- **A new window** starts hidden, 10 by 10; its parent and ancestors hear of
  it before its `InitWindow`, and it is shown after, so `VisibilityChanged`
  reaches everything its `InitWindow` made.
- **Places**: `SetPos` makes a window left- and top-aligned with its x and
  y as margins; `SetWindowAlignments` sets both margins of each side, the
  second the first's unless given. A parent's `ConfigureChild` sets just
  what it gives; a window no parent places, or that the base script's
  answer to its asking hands back (`ResizeChild`), sits by its alignment at
  its preferred size -- centred with the half dropped, plus its margin --
  and its `x` and `y` are that place. So a menu dragged by its title bar
  follows the pointer and the credits start below the screen
  (`ClickConsole`, `MenuConsole`).
- **Sizes**: a side a window was given (`SetSize`) counts as asked for; a
  side it leaves unset is its background's size, else its own -- an empty
  text window is 0 wide and keeps its height. A large text window and an
  edit field measure rows of the font's height with their vertical spacing
  between (`ScreenConsole`'s skill text and note), an edit field a space
  wider when no width is given. A window asking to be laid out again is
  laid out itself too.
- **Text from a script** (`EditConsole`): a text window takes no text that
  differs only in case. An edit field's `SetText` replaces its text as
  typing would -- its undo cleared, `TextChanged` announced up its
  parents -- and leaves the insertion point at the start, as `AppendText`
  does after adding at the end; so the save screen's Save Game button
  wakes for the name it fills in. An edit leaves the insertion point after
  what it put in.
- **Buttons** activate on the release, a repeating one on the press and
  again while held; a right click where the button takes them; each with
  its sounds, and a press from the keyboard shown pressed for its delay. A
  toggle's state is its pressed look; a radio box keeps one of its toggles
  on.
- **Scales** keep a tick among their ticks: a slider's value is its tick's,
  its text the tick's own or the value printed; the thumb drags, a click
  beside a scrollbar's thumb pages, held, again and again. A scroll area
  shows a bar only while what it holds does not fit, its clip window moving
  it a unit at a time (a list's row, a text's line) and bringing the focus
  into view; the wheel steps it.
- **Keys** go up from the focus window to the root, so Esc and F1 close a
  screen; **mouse buttons**, only while a modal shows, to the window the
  mouse acts on (the grab, else the one under the pointer in the topmost
  modal, else that modal), then up its parents. A press grabs the mouse for
  that window, so a drag's moves and its drop reach the item it started on,
  and counts as a multiple click within 0.5 s and 10 units of the first.
  The root marks every press and release: a press of a key already down is
  a repeat, and a release of a key or button not down is taken while a
  modal shows and goes to no window (`KeypadConsole` with
  `stray-release.txt`, the fork's alone; with no modal up the game gets it).
  `IsKeyDown` reads those marks.
  `LootConsole` searches two carcasses alike in both engines; the keys and
  the drags between the searches (`loot-drags.txt`) are a timeline's, so the
  fork's alone, as are `ScrollConsole`'s clicks, drags and wheel
  (`scroll.txt`) and `ClickConsole`'s clicks, drag and keys (`clicks.txt`).
- **Accelerators**: a text's `|&` marks its window's key while the text is
  its accelerator -- every text window's from its start, but a large text
  window's. Alt and a key the scripts leave press, in the modal on top, the
  first window with that key -- the modal, then its children bottom to
  top, each in turn -- that shows, is sensitive and takes the focus, a
  letter in either case (`ClickConsole`: Alt+B opens Brightness from the
  Display screen, Alt+C cancels it).
- **Keyboard focus**
  ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#keyboard-focus)):
  a tab group's windows in order of where they lie on screen; `MoveFocus`,
  `MoveTabGroup`, `SetFocusWindow` (only a window that can take the focus
  takes it, its modal keeping it to return to), the focus moving off a
  window hidden, made unselectable or destroyed, and the root seeding it --
  none while the keypad (`MFOCUS_EnterLeave`) is up, none with no modal up
  (`KeypadConsole`).
- **The windows** tick by real time since their last tick, not the level's,
  and a held button's first repeat counts what of the frame had gone.
- **A GC's style** sets only how tiles draw: text stays masked, translucent
  only by `EnableTranslucentText`, so the frob label over its modulated
  backing reads as the original's (`FrobConsole`'s shots).
- **The scale** is the original's, its root's one multiplier: the largest
  whole multiple of 640x480 the screen holds, at least 1
  ([the root window](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#the-root-window)).
  `SizeConsole` at 1600x900 and 1276x960 (a scale of 1 at both),
  `MenuConsole` and `ScreenConsole` at 1600x900: the windows' places and
  sizes alike.
- **The frob highlight's box** (`ConvertVectorToCoordinates`) is projected by
  the main scene's view into the root window's coordinates: `FrobConsole`'s
  boxes round two decorations, the crosshair on their middle and turned 5
  degrees four ways, lie within 7 pixels of the original's at 1280x720 with
  the original's [view width](#the-views-width).

Differs:

- **`GC.DrawActor` ignores `bConstrain`**: the vision augmentation's calls
  cover the whole view. Its render time: [out of sight](#out-of-sight).
- **Keys**, read from both codes: the keyboard counts as grabbed while a
  modal shows, where the original counts its grabs (every modal grabs while
  shown). With Alt down the fork takes a letter or digit from the key's
  press, not from the characters the platform sends, and it forgets the
  keys held as its window loses the keyboard.
- **Keyboard focus**: the fork places windows in its layout pass, after the
  level's tick, and seeds the first focus there; the original places them
  as they are made and seeds it at its windows' next tick, so a
  conversation's first choice has the focus a frame sooner (`ChoiceConsole`).
  Read from both codes, the tables pass no window over as clipped away, the
  fork keeping no clip rectangles.
- **Layout** (`MenuConsole`, `ScreenConsole`, `SizeConsole`): the fork lays
  windows out once a frame, the original as each asks, so a window made this
  frame is 10 by 10 until the next; a window that shows sits at the margins
  `SetPos` gives it at once, where a parent that places it again does so the
  next frame. A window a parent placed keeps that place until it moves or
  asks again, where the original's goes back to its alignment when the
  parent's next layout leaves it out; read from both codes, a hidden window
  takes a parent's `ConfigureChild`, where the original's only counts itself
  placed. The root window is 4:3 (960 wide at 1280x720) by choice, where the
  original's is the whole viewport: the stock
  `ConWindowActive.CalculateWindowSizes` gives a cutscene's subtitles no
  height on a root 16:9 or wider, the Smart Pro's among them, so a cutscene
  letterboxes at 16:9. The root's children lie across the screen up to 16:9,
  their `x` the screen's.
- **A large text window and an edit field** draw their lines through the GC,
  read from both codes, whose block of lines ends in one more spacing than
  the original's rows: in a window taller than its text, lines centred or
  at the bottom sit up to a pixel higher.

## The menus' settings

Matches the original: every setting the Controls, Game Options, Display,
Sound, Colors and multiplayer Player Setup screens change, stepped through
its menu and saved with OK, reads back alike in both engines -- the menu's
value and its setting's `GET` -- and steps back (`SettingsConsole`). A
volume slider moved but not yet saved plays its new level while `GET` still
reads the saved one, as the original's subsystem and class default differ.
With Decals off no new decal attaches
([small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#small)) and the
ones on the walls are not drawn
([a frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-frame);
`DecalConsole`). Differs:

- **Screen Resolution** offers the fork's sizes, not the display's modes: in
  a window the common sizes from 640x480 that fit the screen, with the
  window's own and the screen's, a choice resizing the window and kept as
  its size; full screen, the screen's size alone, the fork's full screen
  being a borderless window that size.
- **Detail Textures** draws or leaves the world's detail textures in one
  pass in the shaders, not `D3DDrv`'s three; the original's draws none
  either way, run here under Wine, `D3DDrv` and `OpenGLDrv` alike: its
  frames are the same with the setting on and off, though each condition
  read holds but one, the client it reads through a static pointer
  ([detail textures](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#detail-textures);
  `MeshConsole` with `NoDetail`). The fork's darken Liberty Island's pier
  2.3% ([brightness](#brightness)).
- **World and Object Texture Detail** are kept and read back, but every
  texture draws at full detail; **Low Sound Quality** is kept, but every
  sound plays at its own; 16-bit **Texture Color Depth** is kept, the fork
  drawing in 32 bits. **Use 3D Hardware** finds no hardware, as the original
  finds none without an A3D or EAX card.
- **Rendering Device**, and a dedicated server from the Host screen, send
  `RELAUNCH`, which starts the launcher again with the rest of the line once
  the engine has exited, as the original's starts `DeusEx.exe`
  ([`RELAUNCH`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#relaunch)):
  the recreated launcher's renderer page, or a dedicated server. The fork
  passes the line to the launcher on its line
  ([running it](ENGINE.md#running-it)), which starts again once the game
  has ended, where the original's new process starts while the old one is
  still ending. With no launcher on the line -- the ports' launcher, or the
  engine started by hand -- the dedicated server's starts the engine again
  itself, as that server, and Rendering Device does nothing.

## Sound

Matches `Galaxy.dll` ([`galaxy-dll.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md)):
Deus Ex's sounds are mixed as its mixer mixes them (pan, volumes,
resampling and reverb, at `OutputRate`) into one OpenAL source, one behind
the level's BSP muffled -- a closed door's too, the movers in it where it
filed them ([the movers in a line's way](#implemented-not-as-the-original)).
Who hears a sound an actor plays is the original's too
([sounds](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#sounds)):
the player within the radius ÷ √1.3, and with the level's BSP between, at
0.35 of the volume within 0.77 of that, but for the actor's instigator.
`SoundConsole` (its hearing test) and `SkipConsole` record both engines. A
window's sound is the original's too
([window sounds](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#window-sounds)):
in the world, one unit from the player, turned by its place on the screen,
at the window's own volume, which is 0, unheard, unless the window sets it.
`SoundConsole`'s root beeps from the screen's left edge, centre and right
edge come out within 0.2 dB of the original's, panned alike, the one at the
root's own volume unheard in both. Differs:

- The music plays on an OpenAL source of its own.
- Not carried: Galaxy's quirk with the Sound and Speech sliders equal, both
  kinds of sound scaled by the slider once more.
- Not carried: the original's 1/256 volume floor, an integer artifact.
- A change to `EffectsChannels` (the Sound menu's Effects Channels) applies at
  once, held to 0-32, as the original's. The sounds on channels past a lowered
  count stop, where the original's play on out of its reach, and a raised
  count's channels are voices of their own, where the original's are its
  music's ([settings](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#settings)).
- Not carried: the holds a `SET` puts on `DopplerSpeed` and `AmbientFactor`,
  which no menu sets.
- An ambient sound on a light follows its blink and strobe on the fork's
  timing ([lighting](#lighting)).
- A sound's ID packs a number the fork gives each object that plays one,
  never given out twice, where the original packs the object's index
  ([sounds](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#sounds));
  the slot and `bNoOverride` are packed alike.

## Implemented, not as the original

Every other native the scripted runs compare matches the original
([the natives](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#the-natives),
[`Engine.dll`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md)):

- **The traces** (`TraceConsole`, `VisibleConsole`; `TraceActors`, the
  player's frob target, through decorations), and `RadiusActors`, which
  reaches each actor's own collision radius further (`VisibleConsole` on
  Liberty Island and DXMP_Silo).
- **The movers in a line's way**: a line meets each mover where the level's
  BSP last filed it
  ([traces](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#traces)),
  whatever it collides with, hidden or not. A map's load and a save file
  every mover where it is; each frame drawn files again where it is every
  mover not hidden that has moved or turned since; a destroyed one stays
  filed until the next load or save. `FastTrace` and the checks on its line
  (`LineOfSightTo`, hearing, the reach tests and path searches, the
  visible-actor iterators, network relevance, mesh lights' shadows, the
  AI's sight and the mixer's walls, [sound](#sound)) meet the filed
  polygons, a line's `Trace` and the multi-hit check meet them as the
  level's, after the mover's own hit where it still is, and a box meets
  none (`MoverConsole` on Liberty Island and DXMP_Silo: a door made not to
  collide, then hidden and moved, then saved; a window broken, at once and
  a frame on; `VisibleConsole` there). Not carried: the refiling of the
  movers filed under one filed again, which the original files again
  wherever they are; and the polygons are filed whole, where the original's
  tree drops what lands in solid, which no line in the open meets. A
  dedicated server, drawing no frame, never files a mover again: unproven.
- **Sight** (`SightConsole`), **the reach tests** (`ReachConsole`), and
  **the look up and down stairs** (`StairConsole`: every answer of
  `Pawn.FindStairRotation` alike on Liberty Island's pier and
  `SlideConsole`'s slope,
  [small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#small)).
- **Falls**, a whole frame at once in the original's steps, each by its
  mean velocity (a pawn's air control, a bounce left to the script, a wall
  slid along, a landing ending the frame's move), coming to rest where the
  original's do at 60 frames a second and at 20, and their landing on a
  ledge -- a crate nudged off it, a pawn fitted clear of it and pushed on at
  random (`RestConsole`,
  [moving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#moving)).
- **A pawn landing on another** or on a decoration that cannot be its base,
  which the scripts' `SupportActor` bounces off 50 up and 25 to 50 sideways
  at random, again until it slips off, and the one under it stomped but for
  a human NPC not hostile to the player (`StompConsole`: the player and Paul
  Denton on each other, crates).
- **Water**: falls in it (`FloatConsole`: a buoyant crate rises and bobs at
  the surface at the original's speeds) and swimming (`SwimConsole`: the
  player and a troop falling or thrown into water are put back at the water
  line and swim on, the player stopped as its head goes under and held to
  0.3 of `WaterSpeed`, as it walks whenever it swims; up out of the water it
  hops and falls back in, bobbing).
- **Walking**, a whole frame at once in the original's steps, a slope slid
  down where the zone's ground friction is low, and its ledges: a pawn that
  cannot jump stopped there, its move given up, any other given `Falling`
  (`WadeConsole`: a troop walking off the pier given the jump
  `ScriptedPawn`'s `FallingState` aims at its destination, `EAdjustJump`,
  into the water or across it, with `bCanSwim` off, and stopped at the edge
  with `bCanJump` off; one walking down 14_OceanLab_Lab's steps into the
  sea, the stock maps' one shore). The speed a pawn walks and swims at
  (`calcVelocity`, once a frame; one that stops brakes to a standstill);
  ladders (`LadderConsole`: a player at a ladder's foot hangs there, climbs
  as it pushes into it, goes down looking down and steps off onto the top);
  and the same check's slide of a walking player on a texture with a
  `Friction` under 1 (`SlideConsole`: on a slope at a `Friction` of 0.5 and
  0, the glide to a stop and the slide down a slope in a zone of ground
  friction 0.2, at 60 and 20 frames a second; the game's one such texture,
  `Slippery_Metal`, covers two slopes in 14_Oceanlab_Silo).
- **Flying**, by which a player climbs (`FlightConsole`: a gull flying into
  the pier's wall, level and rising, toward the player, taking off as it
  walks off the pier, and its `LongFall` 2.5 s into a drop into the water;
  the stock gull's own flight plausible), and the pawns' turning (the pitch
  at half a turn a second toward level when walking or falling, held within
  `RotationRate`'s).
- **Small ones**: `SetLocation`; the rotators' conversions
  (`RotatorConsole`, 702 of them); `Object.Mid` 127 at its edges, its end's
  clamp known from `MidConsole`'s ten probes of both;
  `Object.DynamicLoadObject` with a group in the name, its own, another or
  none, and a class that is not the object's own (`DynLoadConsole`); and
  `Object.GetPropertyText` and `SetPropertyText`, through which the server's
  query answerer reads what a remote asks for: a public property's text as
  `GET` writes it, a const one never set, a private one or a name the class
  has not read empty and set nothing (`GetConsole`). `LevelInfo`'s clock is
  the original's
  ([small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#small)).

Differs:

- **A line wholly inside the level's solid** meets nothing: the fork's line
  test is of the surfaces a line crosses, where the original's BSP stops it
  at once (`MoverConsole`: a line through the wall over a door).
  `VisibleConsole`'s lines across DXMP_Silo's `DeusExMover19` and `25` meet
  the level in the original and the mover in the fork; why is unread.
- **`DeusExPlayer.GetDeusExVersion`**: the fork's own string, by choice, so a player can tell
  which engine they run ([the version](ENGINE.md#running-on-our-devices)). The original's is
  "Mon Mar 19 12:06:14 2001 v1.112fm".
- **The engine's own rotations** (the view, movement) take exact angles;
  the scripts' conversions take the original's sine table
  ([the natives](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#the-natives)).
  At 20 frames a second `FlightConsole`'s gull, flown level into the pier's
  wall, turns and slides some 18 units along it in the original, where the
  fork's stays put.
- **A box started inside a cylinder** keeps the least penetration's normal;
  the original's is level, out from the axis, as a line's is in both
  ([traces](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#traces)).

## Housekeeping, not seen directly

**The level's tick** is the original's
([a level's tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-levels-tick)):
at every load the actor list is put in the original's order, static actors
first, and every pass of the tick starts after them, so a static actor never
ticks -- on Liberty Island 1,833 of some 2,600, none of which animates or
keeps a timer (`StaticConsole`). Each dynamic actor's distance to the player
comes first, in single player; an actor whose owner has not ticked that frame
waits for it; one spawned during the pass is ticked in it; a class whose
defaults are static or not to be deleted is not spawned; and stasis is for
single player only.

**Garbage** is collected as the original's map load collects it
([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-map-loads-collection);
the fork's: [objects and memory](ENGINE.md#objects-and-memory)), the new
level's actors that are not in its `Actors` (an editor brush, a camera)
with it. `GarbageConsole` and `DeleteConsole` run both engines. Differs:

- **When**: at the end of the frame of the load, once the new level has
  begun and the player is in, where the original's runs before the new
  level begins.
- **A level left behind** is eliminated: every reference to one of its
  objects is made None, where the original's keeps the objects a live
  reference reaches. Without it a conversation event's `speaker`, the game
  replication info's `PRIArray` or a HUD's `clientObject` keeps a whole
  level. No stock script reads such a reference before setting it again.
- **Code is never freed**, where the original's frees a script class nothing
  uses and loads it again when asked for.

On linux-x86_64 under software GL (llvmpipe), Liberty Island and Battery Park
loaded in turn (`reload-fresh.txt`), twenty loads: 522 MB at the end, 528 MB at most;
each island load's collection leaves the same 43,054 objects and frees some
130 textures and 20 sounds. Liberty Island and UNATCO HQ through `Current`
(`reload-current.txt`): 522 MB at most throughout. A collection takes 28 to
66 ms there; on the Smart Pro 200 to 265 ms without the name walk.

**Names** ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#names-hashed-and-compared)):
a spawned actor, an AI event's object, an object a script makes with `new`
and a new window are named as the original names them
(`MakeUniqueObjectName`: the class's name without trailing digits and the
class's count, past any number an object of its package already has), and
the collection deletes such a name once nothing holds it (an object's own
name, its properties', a class's defaults, a state frame's locals), its
slot taken by the next name made. Differs: every other name -- from a
package, an ini, the script or C++, or a spawned actor's looked up by its
spelling -- is kept for the session, where the original's deletes any name
nothing reaches but the hard-coded ones. `ChurnConsole` (2,200 actors a
second for two minutes) stays at 358 MB resident, where keeping every name
grew it by 77 MB; the name walk adds 5 to 8 ms to a collection on
linux-x86_64.

**Destroyed actors** are the original's
([destroyed actors](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#destroyed-actors)):
each joins the level's chain (`FirstDeleted`, through `Deleted`), and at the
end of a level's tick, once 128 wait, or before a save, every live actor's
references to them are made None and the event manager is told. The
original then deletes them; the fork frees them at the next collection,
which makes every other reference to them None (a window's, a
conversation's), where the original's delete leaves those pointing at
freed memory, and the fork lets them gather between collections
([when](ENGINE.md#objects-and-memory)): a collection is a hitch on the
Smart Pro (above). A map load does not clean
the chain of the level it leaves, as the original's does only for `?push`:
that level is freed whole. A decal attached again after its `Destroyed`
(which detaches it) stays on its surfaces until that cleanup, as the
original's; there the fork takes it off them, where the original's surfaces
keep pointing at the deleted decal.

**`Object.CriticalDelete` 751** (20 call sites). The original frees the
object at once, whatever still refers to it
([`CriticalDelete`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#criticaldelete)).
The fork's flags it (`EliminateObject`), destroying an actor first: it goes
at the next collection, which makes every reference to it None, and until
then it stays as it was, which `NanoKeyRing.RemoveAllKeys` relies on as it
reads the next key from the one it deleted. Gone to the script at once as
the original's: `AllObjects`, the console's `SET` and a find by name pass it
over, and a save writes a reference to it as None. Code, a package and the
engine's own objects are not deleted (the log says so).

## Small

- **A long frame**: the actors' step is at most 0.4 s, as the original's
  ([a level's tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-levels-tick)).
  The frame's time on the level's clock is held to 1 s; the original's
  first frame on a level it travels to takes in the whole load, some 1.5 s
  on Liberty Island, of which its actors get 0.4 (`AIConsole`), where the
  fork's is a frame of its own, a tenth of a second or so. So the player's
  fall at the island's start lands elsewhere: the original's first step,
  0.1 s, a move of 4.75 units down, meets the floor too near to go anywhere
  ([moving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#moving)),
  where the fork's shorter step lands it lower, and the walk lifts it twice,
  not once: the fork's player stands 4.80 over the floor to the original's
  4.75 (`StandConsole`; a line straight down meets the floor half a unit
  higher). Not carried: the original's floor of 5 ms, the step of every tick
  past 200 frames a second, which would run the game fast there. The level's
  `TimeDilation` is held to 0.0025 to 25, where the original's is not held
  (the game sets it only through `SetGameSpeed`, to 0.1 or more).
- **Input axes**: an axis a console command sets is used as it is; the
  original's input scales it (the harness's `StandConsole` and `JoinConsole`
  allow for it).
- **The field of view** is the original's setting: the player's
  `DefaultFOV` is Deus Ex's own config, 75 (`[Engine.PlayerPawn]` in
  `User.ini`), the angle across a 4:3 view ([the view's width](#the-views-width)). Upstream's own key,
  `MainFOV`, which the game's ini lacks (it would make the view 90), is
  neither read nor written for Deus Ex.

## The command line

The original's command line reaches the fork as one string, `--cmdline=`,
which the recreated launcher's `run-game.sh` passes on as given
([running it](ENGINE.md#running-it)) but for its quotes, which that launcher
drops ([its known defects](https://github.com/JuggyMcNutty/deusex-launcher/blob/main/README.md#known-defects)).
Its flags are found as the original finds them
([cli-flags.md](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/cli-flags.md)),
with one exception, by choice: a flag's name (`-X` or `/X`) must end at a
space, a `?` (a URL's next option, where the Host screen's
[`RELAUNCH`](#the-menus-settings) puts `-server`) or the line's end, where
the original's `ParseParam` checks nothing after it. Both take `/` as a switch, and a Linux path is full of them: the
original's would find `-server` in `INI=/srv/server/x.ini`. The recreated
launcher parses alike
([where it differs](https://github.com/JuggyMcNutty/deusex-launcher/blob/main/README.md#where-it-differs-from-the-original)).
What the fork does with each:

- **The start URL**: with `-hax0r` or `-server`, the first word (the second
  after `SERVER`) unless it starts with `-`; else the game's own start,
  `DX.dx` ([starting the game engine](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#starting-the-game-engine)).
- **`-server`**: a dedicated server, as `--server`.
- **`INI=` and `USERINI=`**: the inis read and written back, as `--ini` and
  `--userini`.
- **`EXEC=<file>`**: `exec <file>` on the player once the first map is in;
  `exec` runs a file's lines as commands, as the original's does.
- **Safe mode's flags** ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/cli-flags.md#flags-the-launcher-emits-safe-mode)):
  - `-nosound`: no sound and no music. The original makes no audio
    subsystem; the fork keeps its device, on OpenAL Soft's null driver.
  - `-defaultres`: the window and fullscreen sizes 640×480 for the run, not
    saved.
  - `-nohard -noddraw` ("Run the game in a window"): a window. The original
    gets there through its software renderer, which has no fullscreen mode
    without DirectDraw; the fork has no software renderer, so either flag
    alone does nothing.
  - `-nojoy`: no pad at all, neither the fork's pad handling nor its buttons
    as keys.
  - **Nothing to do:** `-no3dsound` (the original's turns off A3D or EAX
    hardware; the fork's mixer is software, [sound](#sound)), `-nommx`,
    `-nokni` and `-nok6` (the original's mixer picks its routines by them),
    and `-safe`'s no DirectInput (the fork's input is SDL's).

## Mods

A mod gets the original's loader:

- **Its folders.** Its packages and maps are found by the `Paths` of its own
  ini (`--ini`), relative to `System` and written with backslashes as the
  game's are, and by their extensions in any case, as the original's
  ([a package's file](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#packages-and-linkers)).
- **Protected packages.** An export whose flags give it no context (client,
  server or editor) is never made, and a reference to it is None, as the
  original's linker has it: the live servers' anti-cheat packages (ANNA,
  DXNMS) point their classes' `ScriptText` at such exports. A save's exports
  are all made whatever their flags, for the fork's older saves wrote
  spawned actors without them.
- **Config files.** A class's config file that is not there is empty, its
  properties keeping their defaults, until the class writes it
  ([configuration](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#configuration)):
  DXMTL's classes read a `DXMTL.ini` no player without the mod has. A key
  with an empty value is a value, what the original's `ImportText` makes of
  no text: a string empty; a name, object or class None; a float 0; an int,
  byte or bool its default. Of a key given twice in a section the last is
  read and the last written, wherever the engine reads the ini: a class's
  config, the default URL, the settings it reads again by hand -- the
  player's as it spawns one, the client's, the audio and render devices'
  (`IniConsole`). Those read by hand, though, take an empty value for none:
  a float or a string keeps the fork's default where the original's is 0 or
  empty.

Differs:

- **Macro textures**, which no stock texture has: the fork multiplies a
  surface by its macro texture once, at the one-pass light map's scale.
  `D3DDrv` draws it in the modulated blend, twice its colour, and the light
  map in a second pass, twice as bright
  ([the light maps' brightness](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#the-light-maps-brightness)).

## Multiplayer

Matches the original's client and server ([the network](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md)):
joining and playing, listen and dedicated servers, replication with the
original's native lists, the server's calls, downloads both ways, server
travel, the Entry level for a lost server, the login, merged bunches, the
uplink and query answerer, the scripts' sockets
([the script's links](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/ipdrv-dll.md#the-scripts-links)),
the addresses they ask for, a player's and the server's own, and a server's
URL with the address it bound (`ServeConsole`'s `serving` line;
[the scripts' addresses](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#addresses)),
and the sounds the server plays, which a remote player's client plays
through `Pawn.ClientHearSound`
([sounds](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#sounds);
`ServeConsole`'s beeps, the client recorded).
Each runs against the original as client and as server: `ServeConsole`,
`JoinConsole`, `RejoinConsole`, `TravelServeConsole`, `TravelJoinConsole`,
`NetConsole`. The fork joins live public servers, the mods they run included
(MTL, ANNA, CDX, DXAG, SG), and stays in. Until its pawn arrives, a joining
fork draws no world. The
server-only stubs are under
[not needed for single player](#not-needed-for-single-player). Differs:

- **The net driver** is the fork's own, over UDP with
  `[IpDrv.TcpNetDriver]`'s timeouts; the connection, its channels and the
  handshake are the original's.
- **The download cache.** A package is cached as its GUID with `CacheExt`,
  where the original writes `.uxx` whatever `CacheExt` says (the game's ini
  names `.uxx`). A package a join loaded from the cache is let go by name at
  the next map load that does not use it, and that load's collection frees
  what nothing uses of it but its code, which stays loaded, where the
  original's collects it all ([housekeeping](#housekeeping-not-seen-directly)).
- **A listen server runs at most 144 frames a second**, where the original's
  runs as fast as it draws: a remote client's queue drains its rate over a
  tick, so the faster the ticks, the less room an unreliable call finds
  ([packets](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#packets)).
  `ServeConsole`'s 8 beeps reach the original's client 7 of 8 at 100 and at
  144 frames a second, 4 at 200 and none at some 1,300 (uncapped on the
  desktop with no vsync), as the original server's reach the fork's, 6 to 8:
  the first goes as the host stops, its last step's packet just sent.
- **A server's corrections** (`ClientAdjustPosition`; the fork a client of
  the original's server, a stock game): one as the player starts walking,
  some 5 units, its first moves' latency; one at its spawn, 2.4 units up. A
  client runs no level's `SupportActor`, a simulated proxy running only
  simulated functions, so its player's floor is never its base, and the
  walk, as the original's, lifts a pawn only over its base: landed, it rests
  2.4 lower than the server's. At a wall 0.02 units; standing, none.
- **The player's name in a net game**: a name the player never chose -- none,
  or the game's stock `Player` -- logs in as `VibePlayer`, a client's to its
  server and a listen server's host's, by choice, so the fork's players show
  as such (`Engine::SetMultiplayerName`). A name the player chose is kept;
  single player, the menus and the inis keep the game's.

As in the original:

- For a player's first seconds in, until the server's augmentation manager
  reaches its client, the client walks at 100 where the server runs it at 230
  (`GetCurrentGroundSpeed` gives 0 without one), and the server corrects a
  walk by up to 36 units.
- A game hosted from the menus is never listed on a master server: an uplink
  announces a server only with its `DoUplink` set, which the game's
  `DeusEx.ini` does not set ([the master server](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/ipdrv-dll.md#the-master-server)).
- The Join Internet screen lists servers only once `MasterServerAddress`
  (`[DeusEx.MenuScreenJoinGame]` in `DeusEx.ini`) names a live master; the
  game's names GameSpy's, which is closed.

## Not needed for single player

Stubs a single-player game does without; the audit lists each:

- `DumpLocation`'s 21: Ion Storm's bug-location tool, though
  `DeusExGameInfo.Login` calls `HasLocationBeenSaved` on every map but a
  save's.
- `DebugInfo` (compiled out in the original too:
  [`DebugInfo`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#debuginfo)),
  `SaveTimeDemo`, `Commandlet.Main`, and `Object`'s `clock`, `unclock` and
  `CyclesToSeconds` ([the timing natives](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#clock-unclock-and-cyclestoseconds)).
- `StatLog`'s and `StatLogFile`'s natives, which do nothing, so a fork
  server logging world stats writes no log of them.
- `ComputerWindow`'s 22, which no script calls: the InfoLink's text window,
  its only user, calls only implemented ones.
- `GC`'s `PushGC`, `PopGC`, `CopyGC` and `Intersect`, and 3 more of the
  windows' stubs, which no script calls.
- Five more that no script of the game calls, so only a mod reaches them:
  `Object.ResetConfig`, which in the original copies the class's section
  back from `Default.ini` or `DefUser.ini` and reads it again
  ([configuration](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#configuration)),
  `DeusExPlayer.SetBoolFlagFromString` 3001
  ([the player](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-player)),
  `ExtString.GetNextTextPart` 1146
  ([small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#small)),
  `Canvas.DrawPortal` 480, which draws nothing, and `Pawn.FindRandomDest`
  525, upstream's own pick of a random navigation point the pawn can reach;
  the original's two are unread.
