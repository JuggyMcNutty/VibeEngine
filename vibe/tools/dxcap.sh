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
#                                                  (or joining a server: <map> an address, as 127.0.0.1:7790)
#   vibe/tools/dxcap.sh prove <map>                the fork's proving run: shots at 20 s and 60 s, exit at 65 s
#
# The workspace is Port Ex Machina's, where this clone is engine/SurrealEngine
# ($DX_ROOT, or two directories above the clone): the game, the SDK and the
# builds are there. A run's shots and log land in
# build/dxcap/runs/<engine>-<console>-<time>/.
# The original runs in this container, never on the host, under the Proton
# build's own wine; DXCAP_PREFIX (default ~/Games/umu/umu-default) and
# DXCAP_PROTON (default "Proton-CachyOS Latest") pick the Wine prefix and the
# Proton under ~/.local/share/Steam/compatibilitytools.d. DXCAP_AUDIO=1 gives
# the fork real audio; it is silent otherwise. DXCAP_RECORD=1 sends either
# engine's audio to a private sink instead of the speakers and records it into
# the run's audio.wav, with the music off (vibe/tools/dxcap/sound.py reads it).
# DXCAP_UPLINK=<host>:<port> has a run's server announce itself there -- a
# master on this machine (vibe/tools/dxcap/fakemaster.py), its uplink's
# DoUplink set --, where it otherwise announces itself nowhere.
# DXCAP_STATS=<password> has a run's server log world stats (bWorldLog) and
# its player hold that world stats password, so a join carries the player's
# checksum (the server logs the login's URL). DXCAP_SERVERPKGS=<dir> has a
# run's server find packages in <dir> too, each named in its ServerPackages,
# so that a client, whose paths lack <dir>, downloads them.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DX_ROOT="${DX_ROOT:-$(cd "$HERE/../../../.." && pwd)}"
say() { printf '%s\n' "$*" >&2; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

GAME="$DX_ROOT/gamefiles"
CAP="$DX_ROOT/build/dxcap"
SDK="$DX_ROOT/reference/ReleaseSDK1112f/System"
ENGINE_BIN="$DX_ROOT/build/linux-x86_64/engine/SurrealEngine"

usage() { sed -n '2,33p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 2; }

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

[ -d "$GAME/System" ] || die "no game install at $GAME"

# The prefix's Windows programs -- the original and the SDK's UCC -- run
# here with the Proton build's own wine, as dx-reverse-info's tools/ida/idalib-mcp.sh runs IDA
# (a wineserver it has running is shared, never stopped).
WINE="$HOME/.local/share/Steam/compatibilitytools.d/${DXCAP_PROTON:-Proton-CachyOS Latest}/files/bin/wine"
wine_run() {
    env WINEPREFIX="${DXCAP_PREFIX:-$HOME/Games/umu/umu-default}" WINEDEBUG=-all \
        WINEDLLOVERRIDES="winemenubuilder.exe=d" "$WINE" "$@"
}

# Wine's view of an absolute path: Z: is the root.
winpath() { printf 'Z:%s' "${1//\//\\}"; }

# make_ini <original|fork> <console> <out>: the game's DeusEx.ini with the
# console class, the package's path, a 1280x720 window, the music off when
# recording, and for the original the OpenGL renderer, which draws on the
# hidden display its frames are grabbed from.
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

    say "build/dxcap is ready -- vibe/tools/dxcap.sh compile next"
}

cmd_compile() {
    [ -f "$CAP/System/UCC.exe" ] || die "vibe/tools/dxcap.sh setup first"
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

# The original draws on a private X display (Xvfb, here; its socket in the
# shared /tmp): nothing on the desktop, and a screen that can be read -- its
# own SHOT reads back nothing under Proton, so the screen is grabbed instead
# and the frames the console class marks kept (vibe/tools/dxcap/grab.py).
XDISPLAY=:99
XSIZE=1344x800

cmd_original() {
    local console="${1:?console class}" secs="${2:-120}"
    [ -f "$CAP/System/DXCapture.u" ] || die "vibe/tools/dxcap.sh compile first"
    command -v Xvfb >/dev/null || die "Xvfb is needed for the original's display"
    command -v import >/dev/null || die "ImageMagick's import is needed to grab the original's display"
    command -v xdotool >/dev/null || die "xdotool is needed to place the original's window"
    local ini="$CAP/System/Original.ini" userini="$CAP/System/OriginalUser.ini"
    local dir="$CAP/runs/original-$console-$(date +%H%M%S)"
    make_ini original "$console" "$ini"
    user_ini "$userini"
    mkdir -p "$dir"
    local before; before="$(shots_before)"

    local xpid="" gpid=""
    if [ ! -S "/tmp/.X11-unix/X${XDISPLAY#:}" ]; then
        Xvfb "$XDISPLAY" -screen 0 "${XSIZE}x24" -ac -nolisten tcp > "$dir/xvfb.log" 2>&1 &
        xpid=$!
        sleep 2
    fi
    python3 "$HERE/dxcap/grab.py" "$dir" "$XDISPLAY" "${XSIZE%x*}" "${XSIZE#*x}" 1280 720 2> "$dir/grab.log" &
    gpid=$!

    [ "${DXCAP_RECORD:-0}" != 1 ] || rec_start "$dir/audio.wav"
    # Started straight on the hidden display -- a Wine desktop fails to set
    # its display up on Xvfb here --, its command line's first word taken for
    # the start URL, so the menu map goes first; waited for until it exits
    # (each console class ends its run with EXIT) or the time runs out; then
    # only its own process is stopped.
    (
        export DISPLAY="$XDISPLAY"
        [ "${DXCAP_RECORD:-0}" != 1 ] || export PULSE_SINK=dxcap
        cd "$GAME/System"
        rm -f Running.ini
        wine_run DeusEx.exe DX.dx "INI=$(winpath "$ini")" "USERINI=$(winpath "$userini")" > "$dir/wine.log" 2>&1 &
        i=0
        while [ $i -lt 60 ] && ! pgrep -f "^DeusEx.exe" > /dev/null; do sleep 1; i=$((i+1)); done
        # Wine places a new window a step further on each time while its
        # server stays up (IDA's keeps it up), and the grabber reads the view
        # from the display's corner: the window goes there once it is up.
        i=0
        while [ $i -lt 30 ] && ! xdotool search --onlyvisible --name '^Deus Ex$' windowmove 0 0 > /dev/null 2>&1; do sleep 1; i=$((i+1)); done
        i=0
        while [ $i -lt "$secs" ] && pgrep -f "^DeusEx.exe" > /dev/null; do sleep 1; i=$((i+1)); done
        if pgrep -f "^DeusEx.exe" > /dev/null; then
            kill $(pgrep -f "^DeusEx.exe")
            echo "killed after $secs s"
        else
            echo "exited after $i s"
        fi
        sleep 3
    ) || true
    [ "${DXCAP_RECORD:-0}" != 1 ] || rec_stop

    kill "$gpid" 2>/dev/null || true
    wait "$gpid" 2>/dev/null || true
    [ -n "$xpid" ] && kill "$xpid" 2>/dev/null
    # The game's own shots are black here; the grabbed frames stand for them.
    local f
    for f in $( (cd "$GAME/System" && ls Shot*.bmp 2>/dev/null) || true); do
        printf '%s\n' "$before" | grep -qx "$f" || rm -f "$GAME/System/$f"
    done
    for f in "$dir"/Shot*.ppm; do
        [ -f "$f" ] || continue
        magick "$f" "${f%.ppm}.png" && rm -f "$f"
    done
    cp "$GAME/System/DeusEx.log" "$dir/DeusEx.log" 2>/dev/null || true
    say "original: $dir"
}

cmd_fork() {
    local console="${1:?console class}" map="${2:?map}" secs="${3:-120}"
    [ -f "$CAP/System/DXCapture.u" ] || die "vibe/tools/dxcap.sh compile first"
    [ -x "$ENGINE_BIN" ] || die "no engine build at $ENGINE_BIN"
    local ini="$CAP/System/Fork.ini" userini="$CAP/System/ForkUser.ini"
    local dir="$CAP/runs/fork-$console-$(date +%H%M%S)"
    make_ini fork "$console" "$ini"
    user_ini "$userini"
    local before; before="$(shots_before)"
    mkdir -p "$dir"
    [ "${DXCAP_RECORD:-0}" != 1 ] || rec_start "$dir/audio.wav"
    local rc=0
    (
        cd "$GAME"
        if [ "${DXCAP_RECORD:-0}" = 1 ]; then
            printf '[general]\ndrivers = pulse\n' > "$CAP/alsoft-pulse.conf"
            export ALSOFT_CONF="$CAP/alsoft-pulse.conf" PULSE_SINK=dxcap
        elif [ "${DXCAP_AUDIO:-0}" != 1 ]; then
            printf '[general]\ndrivers = null\n' > "$CAP/alsoft-null.conf"
            export ALSOFT_CONF="$CAP/alsoft-null.conf"
        fi
        timeout -s KILL "$secs" "$ENGINE_BIN" --no-launcher "$GAME" --ini="$ini" --userini="$userini" --url="$map" > "$dir/engine.log" 2>&1
    ) || rc=$?
    [ "${DXCAP_RECORD:-0}" != 1 ] || rec_stop
    collect "$dir" "$before"
    printf '%s\n' "$rc" > "$dir/exit"
    say "fork: $dir (exit $rc)"
    printf '%s\n' "$dir"
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
    setup)    cmd_setup ;;
    compile)  cmd_compile ;;
    original) cmd_original "$@" ;;
    fork)     cmd_fork "$@" ;;
    prove)    cmd_prove "$@" ;;
    -h|--help|help) usage ;;
    *)        die "unknown command '$cmd' (vibe/tools/dxcap.sh help)" ;;
esac
