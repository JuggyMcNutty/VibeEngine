#!/usr/bin/env bash
# Scripted runs of both engines -- the original game under Proton and the
# engine fork -- each driven by a console class of the DXCapture package
# (vibe/tools/dxcap), named in a private ini made from the game's own for every
# run: the game's inis are never touched. vibe/docs/DEVELOPMENT.md, scripted runs.
#
#   vibe/tools/dxcap.sh setup                      build/dxcap: the SDK's UCC among links to the game's files
#   vibe/tools/dxcap.sh compile                    vibe/tools/dxcap -> build/dxcap/System/DXCapture.u (UCC, under Proton)
#   vibe/tools/dxcap.sh original <console> [<secs>] the original, from its menu map (it takes no map to start)
#   vibe/tools/dxcap.sh fork <console> <map> [<secs>] the fork's linux-x86_64 build, straight into <map>
#                                                  (or joining a server: <map> an address, as 127.0.0.1:7790);
#                                                  <console> - keeps the stock console, and DXCAP_TIMELINE=<file>
#                                                  drives the run (vibe/tools/dxcap/timelines)
#   vibe/tools/dxcap.sh prove <map>                the fork's proving run: shots at 20 s and 60 s, exit at 65 s
#   vibe/tools/dxcap.sh live <address> [<secs>]    the fork joining a live server with the stock console its
#                                                  game wants, driven by a timeline (the fork's --timeline):
#                                                  DXCAP_TIMELINE=<file>, else JoinConsole's walk
#
# The workspace is Port Ex Machina's: this clone sits in its repositories'
# parent folder ($DX_ROOT, or the one the clone is in), with the game, the SDK
# and the builds beside it. A run's shots and log land in
# build/dxcap/runs/<engine>-<console>-<time>/.
# The original runs in this container, never on the host, under the Proton
# build's own wine, in a Wine prefix of its own, build/dxcap/prefix (made on
# first use), never IDA's: a prefix's Wine desktop is on the display of the
# program that started its wineserver, and IDA's headless server starts one
# on the desktop's, where the game's first window fails with BadWindow.
# DXCAP_PREFIX and DXCAP_PROTON (default "Proton-CachyOS Latest") pick another
# prefix and the Proton under ~/.local/share/Steam/compatibilitytools.d.
# Both engines draw on the desktop's X display, Xwayland ($DISPLAY, or
# DXCAP_DISPLAY), each in a 1280x720 window, the fork through its window's
# X11 backend: one X server and the GPU for both. DXCAP_RENDERER=D3D draws the
# original through D3DDrv, the game's own renderer, instead of OpenGLDrv:
# the same frames, gamma ramp and all. DXCAP_AUDIO=1 gives
# the fork real audio; it is silent otherwise. DXCAP_RECORD=1 sends either
# engine's audio to a private sink instead of the speakers and records it into
# the run's audio.wav, with the music off (vibe/tools/dxcap/sound.py reads it).
# DXCAP_UPLINK=<host>:<port> has a run's server announce itself there -- a
# master on this machine (vibe/tools/dxcap/fakemaster.py), its uplink's
# DoUplink set --, where it otherwise announces itself nowhere.
# DXCAP_PORT=<port> has a run's server listen there ([URL] Port), as for
# vibe/tools/dxcap/netrelay.py between it and a client opening 7790.
# DXCAP_STATS=<password> has a run's server log world stats (bWorldLog) and
# its player hold that world stats password, so a join carries the player's
# checksum (the server logs the login's URL). DXCAP_SERVERPKGS=<dir> has a
# run's server find packages in <dir> too, each named in its ServerPackages,
# so that a client, whose paths lack <dir>, downloads them.
# DXCAP_MISSION_MAP=<map> names, through the run's ini, the map a run's
# console travels to (MissionConsole's and PerfConsole's TargetMap): an
# original run starts at its menu map, a fork run at the map it is given.
# DXCAP_ENGINE=<binary> runs another build of the fork than linux-x86_64's
# (an ASan build). DXCAP_MEMLOG=1 logs a fork run's resident memory each
# second into the run's memlog.txt: seconds since the start, then MB.
# DXCAP_SETTINGS=<file> gives a fork run that Settings.json in place of
# ~/.config/SurrealEngine's, through a HOME of the run's own (the caches stay
# the usual ones). DXCAP_NOVSYNC=1 has Mesa's GL draw either engine without
# waiting for the display (vblank_mode=0), whatever the engine asks: the
# original's OpenGLDrv asks nothing and gets the driver's vsync.
# DXCAP_FPS=<n> holds either engine to n frames a second:
# MangoHud's limiter, its display off (mangohud, and lib32-mangohud for the
# original's 32-bit process). DXCAP_PERF=1 samples the engine's main thread
# with the parent folder's deps/perf (vibe/tools/host-tools.sh) into the
# run's perf.data, on the wall clock, with the process's maps.txt;
# vibe/tools/perf/frame-report.py reads them against PerfConsole's windows.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DX_ROOT="${DX_ROOT:-$(cd "$HERE/../../.." && pwd)}"
say() { printf '%s\n' "$*" >&2; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

GAME="$DX_ROOT/gamefiles"
CAP="$DX_ROOT/build/dxcap"
SDK="$DX_ROOT/reference/ReleaseSDK1112f/System"
ENGINE_BIN="${DXCAP_ENGINE:-$DX_ROOT/build/linux-x86_64/engine/SurrealEngine}"

usage() { sed -n '2,/^set -euo/p' "${BASH_SOURCE[0]}" | sed '$d; s/^# \{0,1\}//'; exit 2; }
# Every command but help needs the game.
need_game() { [ -d "$GAME/System" ] || die "no game install at $GAME"; }

# The recording: a null sink that the game's stream goes to (PULSE_SINK),
# and parecord on its monitor, detached so it outlives the call.
rec_start() {
    local wav="$1"
    pactl list short sinks | grep -q '[[:space:]]dxcap[[:space:]]' ||
        pactl load-module module-null-sink sink_name=dxcap sink_properties=device.description=dxcap > "$CAP/sink.module"
    setsid nohup parecord --device=dxcap.monitor --file-format=wav "$wav" > /dev/null 2>&1 < /dev/null &
    echo $! > "$CAP/rec.pid"
    sleep 1
}

rec_stop() {
    [ -f "$CAP/rec.pid" ] && kill "$(cat "$CAP/rec.pid")" 2>/dev/null
    rm -f "$CAP/rec.pid"
    sleep 1
    if [ -f "$CAP/sink.module" ]; then
        pactl unload-module "$(cat "$CAP/sink.module")" || true
        rm -f "$CAP/sink.module"
    fi
}

# DXCAP_NOVSYNC: Mesa's vblank_mode. DXCAP_FPS: MangoHud preloaded into the
# engine's process, its limiter on and its display off; the dynamic linker
# takes lib32's copy for the original's 32-bit wine process ($LIB).
fps_env() {
    [ "${DXCAP_NOVSYNC:-0}" != 1 ] || export vblank_mode=0
    [ -n "${DXCAP_FPS:-}" ] || return 0
    [ -f /usr/lib/mangohud/libMangoHud_shim.so ] || die "DXCAP_FPS needs MangoHud (mangohud, and lib32-mangohud for the original)"
    export MANGOHUD=1 MANGOHUD_CONFIG="no_display,fps_limit=$DXCAP_FPS"
    export LD_PRELOAD="${LD_PRELOAD:+$LD_PRELOAD:}/usr/\$LIB/mangohud/libMangoHud_shim.so"
}

# DXCAP_PERF: the engine's main thread (its process's first) sampled until it
# exits, its samples stamped with the wall clock (-k realtime) so that they
# can be cut to PerfConsole's windows; its cycles and instructions counted
# each 100 ms (stat.csv, from the wall-clock time in stat.start), which a
# CPU's clock speed does not change; the process's maps once its libraries
# are in, for the original's DLLs. Prints perf's pids, for perf_wait.
PERF="$DX_ROOT/deps/perf/usr/bin/perf"
perf_attach() {
    local pid="$1" dir="$2"
    [ -n "$pid" ] || { say "perf: no engine process to record"; return 0; }
    LD_LIBRARY_PATH="$DX_ROOT/deps/perf/usr/lib" "$PERF" record -q -e cpu-clock -t "$pid" -k realtime -F 999 \
        -o "$dir/perf.data" > "$dir/perf.log" 2>&1 &
    local ppid=$!
    date +%s.%N > "$dir/stat.start"
    LD_LIBRARY_PATH="$DX_ROOT/deps/perf/usr/lib" "$PERF" stat -e cycles:u,instructions:u -t "$pid" -I 100 -x , \
        -o "$dir/stat.csv" > /dev/null 2>&1 &
    local spid=$!
    ( sleep 20; cp "/proc/$pid/maps" "$dir/maps.txt" 2>/dev/null || true ) > /dev/null 2>&1 &
    printf '%s %s\n' "$ppid" "$spid"
}
# perf_wait <pids>: until perf has written its files (not this shell's
# children: perf_attach runs in a command substitution).
perf_wait() {
    local p
    for p in "$@"; do
        while kill -0 "$p" 2>/dev/null; do sleep 0.2; done
    done
}
need_perf() {
    [ "${DXCAP_PERF:-0}" != 1 ] || [ -x "$PERF" ] || die "DXCAP_PERF needs perf in $DX_ROOT/deps -- vibe/tools/host-tools.sh"
}

# The prefix's Windows programs -- the original and the SDK's UCC -- run
# here with the Proton build's own wine, as dx-reverse-info's tools/ida/idalib-mcp.sh runs IDA.
WINE="$HOME/.local/share/Steam/compatibilitytools.d/${DXCAP_PROTON:-Proton-CachyOS Latest}/files/bin/wine"
PREFIX="${DXCAP_PREFIX:-$CAP/prefix}"
XDISPLAY="${DXCAP_DISPLAY:-${DISPLAY:-:0}}"
need_display() {
    DISPLAY="$XDISPLAY" xdotool getdisplaygeometry > /dev/null 2>&1 ||
        die "no X display at $XDISPLAY (the desktop's Xwayland; xdotool is needed too)"
}
wine_run() {
    env WINEPREFIX="$PREFIX" WINEDEBUG=-all \
        WINEDLLOVERRIDES="winemenubuilder.exe=d" "$WINE" "$@"
}

# The prefix is made on first use, by the same wine's wineboot on the
# desktop's X display, without Mono and Gecko, which
# the game and UCC do not need. Wine's wined3d, which D3DDrv's DirectDraw
# runs on, imports vkd3d's libraries, which Proton's own prefix has and a
# bare wineboot's lacks: without them ddraw.dll does not load, and the game
# falls back from D3DDrv to SoftDrv (its log: "DirectDraw not installed").
# So they are copied in from the Proton build's lib/vkd3d when missing.
ensure_prefix() {
    [ -f "$PREFIX/system.reg" ] || make_prefix
    local vkd3d="${WINE%/bin/wine}/lib/vkd3d" arch dir f
    for arch in x86_64:system32 i386:syswow64; do
        dir="$PREFIX/drive_c/windows/${arch#*:}"
        for f in "$vkd3d/${arch%%:*}-windows"/libvkd3d*.dll; do
            [ -f "$f" ] || die "no vkd3d libraries in $vkd3d"
            [ -f "$dir/${f##*/}" ] || cp "$f" "$dir/"
        done
    done
}

make_prefix() {
    need_display
    say "making the Wine prefix $PREFIX"
    mkdir -p "$(dirname "$PREFIX")"
    env -u WAYLAND_DISPLAY DISPLAY="$XDISPLAY" WINEPREFIX="$PREFIX" WINEDEBUG=-all \
        WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=" "$WINE" wineboot -i > /dev/null 2>&1 || true
    env WINEPREFIX="$PREFIX" "${WINE%/wine}/wineserver" -w || true
    [ -f "$PREFIX/system.reg" ] || die "wineboot made no prefix at $PREFIX"
}

# Wine's view of an absolute path: Z: is the root.
winpath() { printf 'Z:%s' "${1//\//\\}"; }

# make_ini <original|fork> <console> <out>: the game's DeusEx.ini with the
# console class, the package's path, a 1280x720 window, the music off when
# recording, and for the original the OpenGL renderer (DXCAP_RENDERER=D3D:
# D3DDrv), whose window its frames are grabbed from.
make_ini() {
    local engine="$1" console="$2" out="$3" path
    if [ "$engine" = original ]; then
        path="$(winpath "$CAP/System")\\*.u"
    else
        path="$CAP/System/*.u"
    fi
    python3 - "$GAME/System/DeusEx.ini" "$out" "$engine" "$console" "$path" <<'EOF'
import os, re, sys
src, out, engine, console, path = sys.argv[1:]
s = open(src, encoding='latin1', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'
def put(key, value):
    global s
    s, n = re.subn(r'^%s=.*$' % re.escape(key), lambda m: '%s=%s' % (key, value), s, count=1, flags=re.M)
    if n != 1:
        sys.exit('no %s= in %s' % (key, src))
# A live run keeps the stock console: a server's game drops any other.
if console:
    put('Console', 'DXCapture.' + console)
s, n = re.subn(r'^(Paths=\.\.\\System\\\*\.u)\r?$', lambda m: m.group(1) + nl + 'Paths=' + path, s, count=1, flags=re.M)
if n != 1:
    sys.exit('no Paths=..\\System\\*.u in ' + src)
if os.environ.get('DXCAP_RECORD') == '1':
    put('MusicVolume', '0')
# The Join Internet screen asks a live master server: the game's names
# GameSpy's, closed.
put('MasterServerAddress', 'master.333networks.com')
# A run's server never tells the master servers about itself -- only, with
# DXCAP_UPLINK=<host>:<port>, a master on this machine (fakemaster.py).
uplink = os.environ.get('DXCAP_UPLINK', '')
first = [True]
def one_uplink(m):
    if not uplink or not first[0]:
        return ''
    first[0] = False
    host, port = uplink.rsplit(':', 1)
    return 'ServerActors=IpServer.UdpServerUplink DoUplink=True MasterServerAddress=%s MasterServerPort=%s%s' % (host, port, nl)
s, n = re.subn(r'^ServerActors=IpServer\.UdpServerUplink.*\r?\n', one_uplink, s, flags=re.M)
if n == 0:
    sys.exit('no ServerActors=IpServer.UdpServerUplink in ' + src)
# A run's server listens on DXCAP_PORT's port.
if os.environ.get('DXCAP_PORT'):
    put('Port', os.environ['DXCAP_PORT'])
# A run's server offers the packages of DXCAP_SERVERPKGS's folder, found
# there by its paths.
serverpkgs = os.environ.get('DXCAP_SERVERPKGS', '')
if serverpkgs:
    import glob
    files = sorted(glob.glob(os.path.join(serverpkgs, '*.u*')))
    if not files:
        sys.exit('no packages in ' + serverpkgs)
    exts = sorted(set(os.path.splitext(f)[1] for f in files))
    folder = os.path.abspath(serverpkgs)
    if engine == 'original':
        folder = 'Z:' + folder.replace('/', '\\')
        lines = ''.join('Paths=%s\\*%s%s' % (folder, e, nl) for e in exts)
    else:
        lines = ''.join('Paths=%s/*%s%s' % (folder, e, nl) for e in exts)
    s, n = re.subn(r'^(Paths=\.\.\\System\\\*\.u\r?\n)', lambda m: m.group(1) + lines, s, count=1, flags=re.M)
    if n != 1:
        sys.exit('no Paths=..\\System\\*.u in ' + src)
    names = ''.join('ServerPackages=%s%s' % (os.path.splitext(os.path.basename(f))[0], nl) for f in files)
    s, n = re.subn(r'^\[DeusEx\.DeusExGameEngine\]\r?\n', lambda m: m.group(0) + names, s, count=1, flags=re.M)
    if n != 1:
        sys.exit('no [DeusEx.DeusExGameEngine] in ' + src)
# A run's server logs world stats only with DXCAP_STATS.
if os.environ.get('DXCAP_STATS'):
    s, n = re.subn(r'^\[Engine\.GameInfo\]\r?\n', lambda m: m.group(0) + 'bWorldLog=True' + nl, s, count=1, flags=re.M)
    if n != 1:
        sys.exit('no [Engine.GameInfo] in ' + src)
# Both engines take the game's settings from it; both run in a window of
# the same size.
put('WindowedViewportX', '1280')
put('WindowedViewportY', '720')
put('StartupFullscreen', 'False')
if engine == 'original':
    if os.environ.get('DXCAP_RENDERER', '') == 'D3D':
        put('GameRenderDevice', 'D3DDrv.D3DRenderDevice')
    else:
        put('GameRenderDevice', 'OpenGLDrv.OpenGLRenderDevice')
open(out, 'w', encoding='latin1', newline='').write(s)
EOF
}

# user_ini <out>: the game's User.ini, with DXCAP_STATS's world stats
# password for the player.
user_ini() {
    cp "$GAME/System/User.ini" "$1"
    [ -n "${DXCAP_STATS:-}" ] || return 0
    python3 - "$1" "$DXCAP_STATS" <<'EOF'
import re, sys
path, secret = sys.argv[1:]
s = open(path, encoding='latin1', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'
# The game's User.ini has both keys, empty: set in place, as a second
# key's value would not count in the original (its last one does).
for key, value in (('ngWorldSecret', secret), ('ngSecretSet', 'True')):
    s, n = re.subn(r'^%s=[^\r\n]*' % key, lambda m: '%s=%s' % (key, value), s, count=1, flags=re.M)
    if n != 1:
        sys.exit('no %s= in %s' % (key, path))
open(path, 'w', encoding='latin1', newline='').write(s)
EOF
}

cmd_setup() {
    [ -f "$SDK/UCC.exe" ] || die "no SDK at $SDK (reference/ReleaseSDK1112f)"
    mkdir -p "$CAP/System" "$CAP/DXCapture" "$CAP/runs"
    local f d
    for f in "$GAME"/System/*.u "$GAME"/System/*.dll "$GAME"/System/*.int; do
        ln -sfn "$f" "$CAP/System/$(basename "$f")"
    done
    # The SDK's UCC with the Core and Window it was built against.
    for f in UCC.exe Core.dll Window.dll; do
        rm -f "$CAP/System/$f"
        cp "$SDK/$f" "$CAP/System/$f"
    done
    for d in Textures Sounds Music Maps; do
        ln -sfn "$GAME/$d" "$CAP/$d"
    done
    ln -sfn "$HERE/dxcap/Classes" "$CAP/DXCapture/Classes"
    # UCC wants both UCC.ini and DeusEx.ini, and the defaults beside them:
    # the game's, with the package to compile last.
    ln -sfn "$GAME/System/Default.ini" "$CAP/System/Default.ini"
    ln -sfn "$GAME/System/DefUser.ini" "$CAP/System/DefUser.ini"
    python3 - "$GAME/System/DeusEx.ini" "$CAP/System/UCC.ini" <<'EOF'
import re, sys
s = open(sys.argv[1], encoding='latin1', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'
s, n = re.subn(r'^(EditPackages=DeusEx)\r?$', lambda m: m.group(1) + nl + 'EditPackages=DXCapture', s, count=1, flags=re.M)
if n != 1:
    sys.exit('no EditPackages=DeusEx in ' + sys.argv[1])
open(sys.argv[2], 'w', encoding='latin1', newline='').write(s)
EOF
    cp "$CAP/System/UCC.ini" "$CAP/System/DeusEx.ini"
    cp "$GAME/System/User.ini" "$CAP/System/User.ini"
    ensure_prefix

    say "build/dxcap is ready -- vibe/tools/dxcap.sh compile next"
}

cmd_compile() {
    [ -f "$CAP/System/UCC.exe" ] || die "vibe/tools/dxcap.sh setup first"
    ensure_prefix
    rm -f "$CAP/System/DXCapture.u"
    (cd "$CAP/System" && wine_run UCC.exe make > "$CAP/ucc.out" 2>&1) || true
    grep -E "Error|error\(s\)" "$CAP/System/UCC.log" >&2 || true
    [ -f "$CAP/System/DXCapture.u" ] || die "UCC made no DXCapture.u -- build/dxcap/System/UCC.log"
    say "compiled: build/dxcap/System/DXCapture.u"
}

# Shots the game writes into its System folder, but not ones that were
# already there before the run.
shots_before() { (cd "$GAME/System" && ls Shot*.bmp 2>/dev/null) || true; }

collect() {
    local dir="$1" before="$2" f
    mkdir -p "$dir"
    for f in $( (cd "$GAME/System" && ls Shot*.bmp 2>/dev/null) || true); do
        if ! printf '%s\n' "$before" | grep -qx "$f"; then
            mv "$GAME/System/$f" "$dir/$f"
        fi
    done
}

# The original's own SHOT reads back nothing under Proton, so its window
# is grabbed instead and the frames the console class marks kept
# (vibe/tools/dxcap/grab.py).

# The original runs in a view of the game, build/dxcap/game, made afresh for
# each run: the game's folders linked, and its System folder's files, but for
# what the game writes there -- its log, Running.ini, shots -- and any file
# with no extension. The original takes a package's bare name in its folder
# before any of its paths (appFindPackageFile), so the recreated launcher's
# DeusEx, installed beside DeusEx.exe, stood in for the DeusEx package. What
# the game writes stays in the view.
make_view() {
    local view="$CAP/game" f name d
    rm -rf "$view"
    mkdir -p "$view/System"
    for f in "$GAME"/System/*; do
        [ -f "$f" ] || continue
        name="${f##*/}"
        case "$name" in
            *.*) ;;
            *) continue ;;
        esac
        case "$name" in
            *.log|*.bmp|*.i64|Running.ini) continue ;;
        esac
        ln -s "$f" "$view/System/$name"
    done
    for d in "$GAME"/*/; do
        d="${d%/}"
        [ "${d##*/}" = System ] || ln -s "$d" "$view/${d##*/}"
    done
}

cmd_original() {
    local console="${1:?console class}" secs="${2:-120}"
    [ -f "$CAP/System/DXCapture.u" ] || die "vibe/tools/dxcap.sh compile first"
    command -v import >/dev/null || die "ImageMagick's import is needed to grab the original's window"
    need_display
    need_perf
    ensure_prefix
    local ini="$CAP/System/Original.ini" userini="$CAP/System/OriginalUser.ini"
    local dir="$CAP/runs/original-$console-$(date +%H%M%S)"
    make_ini original "$console" "$ini"
    user_ini "$userini"
    # A console that travels to a named map (the sweep's MissionConsole):
    # the game starts at the menu map, so the map comes through the ini.
    if [ -n "${DXCAP_MISSION_MAP:-}" ]; then
        printf '[DXCapture.%s]\nTargetMap=%s\n' "$console" "$DXCAP_MISSION_MAP" >> "$ini"
    fi
    make_view
    mkdir -p "$dir"

    local gpid=""
    python3 "$HERE/dxcap/grab.py" "$dir" "$XDISPLAY" '^Deus Ex$' 1280 720 2> "$dir/grab.log" &
    gpid=$!

    [ "${DXCAP_RECORD:-0}" != 1 ] || rec_start "$dir/audio.wav"
    # Started straight on the desktop's X display (no Wine desktop), its
    # command line's first word taken for the start URL, so the menu map
    # goes first; waited for until it exits
    # (each console class ends its run with EXIT) or the time runs out; then
    # only its own process is stopped.
    (
        unset WAYLAND_DISPLAY
        export DISPLAY="$XDISPLAY"
        [ "${DXCAP_RECORD:-0}" != 1 ] || export PULSE_SINK=dxcap
        cd "$CAP/game/System"
        (fps_env; wine_run DeusEx.exe DX.dx "INI=$(winpath "$ini")" "USERINI=$(winpath "$userini")" > "$dir/wine.log" 2>&1) &
        i=0
        while [ $i -lt 60 ] && ! pgrep -f "^DeusEx.exe" > /dev/null; do sleep 1; i=$((i+1)); done
        perfpid=""
        [ "${DXCAP_PERF:-0}" != 1 ] || perfpid="$(perf_attach "$(pgrep -f "^DeusEx.exe" | head -1)" "$dir")"
        i=0
        while [ $i -lt "$secs" ] && pgrep -f "^DeusEx.exe" > /dev/null; do sleep 1; i=$((i+1)); done
        if pgrep -f "^DeusEx.exe" > /dev/null; then
            kill $(pgrep -f "^DeusEx.exe")
            echo "killed after $secs s"
        else
            echo "exited after $i s"
        fi
        sleep 3
        [ -z "$perfpid" ] || perf_wait $perfpid
    ) || true
    [ "${DXCAP_RECORD:-0}" != 1 ] || rec_stop

    kill "$gpid" 2>/dev/null || true
    wait "$gpid" 2>/dev/null || true
    # The game's own shots, black here, stay in the view; the grabbed frames
    # stand for them.
    local f
    for f in "$dir"/Shot*.ppm; do
        [ -f "$f" ] || continue
        magick "$f" "${f%.ppm}.png" && rm -f "$f"
    done
    cp "$CAP/game/System/DeusEx.log" "$dir/DeusEx.log" 2>/dev/null || true
    say "original: $dir"
    # A renderer that fails to start is replaced by SoftDrv without a word
    # on the screen: its frames would pass for D3DDrv's.
    if grep -a -q "Bound to SoftDrv" "$dir/DeusEx.log" 2>/dev/null; then
        die "the game fell back to SoftDrv (its renderer did not start): $dir/DeusEx.log"
    fi
}

# memlog <timeout's pid> <file>: the engine's resident memory (/proc's statm)
# each second, as "<seconds> <MB>", until it exits.
memlog() {
    local tpid="$1" out="$2" pid="" t=0 pages
    local page; page="$(getconf PAGESIZE)"
    : > "$out"
    while kill -0 "$tpid" 2>/dev/null; do
        [ -n "$pid" ] || pid="$(pgrep -P "$tpid" | head -1)" || true
        if [ -n "$pid" ] && [ -r "/proc/$pid/statm" ]; then
            read -r _ pages _ < "/proc/$pid/statm" || break
            awk -v t="$t" -v p="$pages" -v s="$page" 'BEGIN { printf "%d %.1f\n", t, p * s / 1048576 }' >> "$out"
        fi
        sleep 1
        t=$((t + 1))
    done
}

# A fork run into <map> or a server's address, its log into <dir>; any
# further arguments go to the engine.
run_fork() {
    local dir="$1" ini="$2" userini="$3" map="$4" secs="$5"
    shift 5
    local before; before="$(shots_before)"
    mkdir -p "$dir"
    need_perf
    local settings=""
    if [ -n "${DXCAP_SETTINGS:-}" ]; then
        settings="$(realpath "$DXCAP_SETTINGS")"
        [ -f "$settings" ] || die "no settings file $DXCAP_SETTINGS"
    fi
    need_display
    [ "${DXCAP_RECORD:-0}" != 1 ] || rec_start "$dir/audio.wav"
    local rc=0
    (
        cd "$GAME"
        # The desktop's X display through the window's X11 backend, where
        # the original draws through Wine's.
        unset WAYLAND_DISPLAY
        export DISPLAY="$XDISPLAY" SURREALWIDGETS_DISPLAY_BACKEND=X11 SDL_VIDEODRIVER=x11
        if [ "${DXCAP_RECORD:-0}" = 1 ]; then
            printf '[general]\ndrivers = pulse\n' > "$CAP/alsoft-pulse.conf"
            export ALSOFT_CONF="$CAP/alsoft-pulse.conf" PULSE_SINK=dxcap
        elif [ "${DXCAP_AUDIO:-0}" != 1 ]; then
            printf '[general]\ndrivers = null\n' > "$CAP/alsoft-null.conf"
            export ALSOFT_CONF="$CAP/alsoft-null.conf"
        fi
        # The run's own Settings.json, where the engine looks for it
        # ($HOME/.config/SurrealEngine); the caches stay where they are.
        if [ -n "$settings" ]; then
            mkdir -p "$dir/home/.config/SurrealEngine"
            cp "$settings" "$dir/home/.config/SurrealEngine/Settings.json"
            export XDG_CACHE_HOME="${XDG_CACHE_HOME:-$HOME/.cache}"
            export HOME="$dir/home"
        fi
        fps_env
        timeout -s KILL "$secs" "$ENGINE_BIN" --no-launcher "$GAME" --ini="$ini" --userini="$userini" --url="$map" "$@" > "$dir/engine.log" 2>&1 &
        local tpid=$! mpid="" perfpid="" epid="" i=0
        if [ "${DXCAP_MEMLOG:-0}" = 1 ]; then
            memlog "$tpid" "$dir/memlog.txt" &
            mpid=$!
        fi
        if [ "${DXCAP_PERF:-0}" = 1 ]; then
            while [ $i -lt 50 ] && [ -z "$epid" ]; do
                epid="$(pgrep -P "$tpid" | head -1)" || true
                [ -n "$epid" ] || sleep 0.1
                i=$((i + 1))
            done
            perfpid="$(perf_attach "$epid" "$dir")"
        fi
        local erc=0
        wait "$tpid" || erc=$?
        [ -z "$mpid" ] || kill "$mpid" 2>/dev/null || true
        [ -z "$perfpid" ] || perf_wait $perfpid
        exit "$erc"
    ) || rc=$?
    [ "${DXCAP_RECORD:-0}" != 1 ] || rec_stop
    collect "$dir" "$before"
    printf '%s\n' "$rc" > "$dir/exit"
    say "fork: $dir (exit $rc)"
    printf '%s\n' "$dir"
}

cmd_fork() {
    local console="${1:?console class}" map="${2:?map}" secs="${3:-120}"
    [ -f "$CAP/System/DXCapture.u" ] || die "vibe/tools/dxcap.sh compile first"
    [ -x "$ENGINE_BIN" ] || die "no engine build at $ENGINE_BIN"
    local ini="$CAP/System/Fork.ini" userini="$CAP/System/ForkUser.ini"
    local name="$console"
    [ "$console" != - ] || { console=""; name=stock; }
    local dir="$CAP/runs/fork-$name-$(date +%H%M%S)"
    make_ini fork "$console" "$ini"
    user_ini "$userini"
    if [ -n "${DXCAP_MISSION_MAP:-}" ] && [ -n "$console" ]; then
        printf '[DXCapture.%s]\nTargetMap=%s\n' "$console" "$DXCAP_MISSION_MAP" >> "$ini"
    fi
    local args=()
    if [ -n "${DXCAP_TIMELINE:-}" ]; then
        mkdir -p "$dir"
        cp "$DXCAP_TIMELINE" "$dir/timeline.txt"
        args=(--timeline="$dir/timeline.txt")
    fi
    run_fork "$dir" "$ini" "$userini" "$map" "$secs" "${args[@]}"
}

# The fork on a live server: the stock console, and a timeline for what a
# console class would do -- by default JoinConsole's walk, the player's
# place and the others' logged each second of the game (DXLIVE lines).
cmd_live() {
    local address="${1:?server address}" secs="${2:-120}"
    [ -x "$ENGINE_BIN" ] || die "no engine build at $ENGINE_BIN"
    local ini="$CAP/System/Fork.ini" userini="$CAP/System/ForkUser.ini"
    local dir="$CAP/runs/fork-live-$(date +%H%M%S)"
    make_ini fork "" "$ini"
    user_ini "$userini"
    mkdir -p "$dir"
    local timeline="$dir/timeline.txt"
    if [ -n "${DXCAP_TIMELINE:-}" ]; then
        cp "$DXCAP_TIMELINE" "$timeline"
    else
        cat > "$timeline" <<'EOF'
# JoinConsole's walk: once in the game the player stands 5 s, walks forward
# 5 s with the key held (W, MoveForward in the game's User.ini) and stands
# again; shots at the stops; the run exits 25 s into the game.
game 5 shot
game 5 press W
game 10 release W
game 11 shot
game 25 exit
EOF
    fi
    run_fork "$dir" "$ini" "$userini" "$address" "$secs" --timeline="$timeline"
}

cmd_prove() {
    local map="${1:?map}" dir shots=0 f mean bad=0
    dir="$(cmd_fork ProveConsole "$map" 90 | tail -1)"
    [ "$(cat "$dir/exit")" = 0 ] || { say "the run did not exit cleanly: $dir/engine.log"; bad=1; }
    for f in "$dir"/Shot*.bmp; do
        [ -f "$f" ] || continue
        shots=$((shots + 1))
        mean="$(magick "$f" -format '%[fx:mean]' info:)"
        say "$(basename "$f"): mean brightness $mean"
        awk -v m="$mean" 'BEGIN { exit !(m < 0.01) }' && { say "  -- nearly black"; bad=1; }
    done
    [ "$shots" = 2 ] || { say "expected 2 shots, got $shots"; bad=1; }
    grep -E "Script error|SurrealEngine error|Unimplemented" "$dir/engine.log" | sort | uniq -c >&2 || true
    [ "$bad" = 0 ] || die "proving run failed: $dir"
    say "proving run clean: $dir"
}

cmd="${1:-help}"; shift || true
case "$cmd" in
    setup)    need_game; cmd_setup ;;
    compile)  need_game; cmd_compile ;;
    original) need_game; cmd_original "$@" ;;
    fork)     need_game; cmd_fork "$@" ;;
    prove)    need_game; cmd_prove "$@" ;;
    live)     need_game; cmd_live "$@" ;;
    -h|--help|help) usage ;;
    *)        die "unknown command '$cmd' (vibe/tools/dxcap.sh help)" ;;
esac
