#!/usr/bin/env python3
"""Where a frame's main-thread CPU time goes, for a PerfConsole run of either engine.

Reads a run folder that vibe/tools/dxcap.sh wrote with DXCAP_PERF=1: the
engine's log (engine.log, the fork's; DeusEx.log, the original's), whose
"DXPERF: window" lines give the measured windows on the wall clock and the
frames in each, and perf.data, the main thread's samples on the same clock
(cpu-clock at 999 a second). Only the samples inside the windows count. Each
is put in an area and a function; the result is milliseconds a frame.

The original's code is the game's DLLs, mapped by Wine: a sample's DLL is
perf's, its function the nearest export at or below its address (maps.txt
gives the DLL's base). Render.dll exports few of its functions, so the names
in it are only near. The fork's functions are perf's own symbols.

    frame-report.py [--top N] <run dir> [<run dir> ...]
"""
import argparse
import bisect
import datetime
import os
import re
import struct
import subprocess
import sys
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
DX_ROOT = os.environ.get('DX_ROOT') or os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
PERF = os.path.join(DX_ROOT, 'deps', 'perf', 'usr', 'bin', 'perf')
PERF_LIB = os.path.join(DX_ROOT, 'deps', 'perf', 'usr', 'lib')
RATE = 999.0

WINDOW = re.compile(r'DXPERF: window (\d+) from (\d+) to (\d+) frames (\d+) ms (\d+)')
TOTAL = re.compile(r'DXPERF: total frames (\d+) ms (\d+) fps ([\d.]+) mean ([\d.]+)(.*)')

# The original's areas, by DLL (lower case).
ORIGINAL_AREAS = [
    ('script VM and objects (Core)', ['core.dll']),
    ('game: tick, physics, AI, natives (Engine, DeusEx)', ['engine.dll', 'deusex.dll']),
    ('UI windows (Extension, Window, ConSys)', ['extension.dll', 'window.dll', 'consys.dll', 'deusextext.dll']),
    ('scene render (Render)', ['render.dll']),
    ('fractal textures (Fire)', ['fire.dll']),
    ('render device (OpenGlDrv, D3DDrv)', ['opengldrv.dll', 'd3ddrv.dll', 'softdrv.dll']),
    ('audio (Galaxy)', ['galaxy.dll']),
    ('platform and launcher (WinDrv, DeusEx.exe)', ['windrv.dll', 'deusex.exe']),
]

# The fork's areas, by a name anywhere in the symbol (a template's arguments
# included); the first that matches wins.
FORK_AREAS = [
    ('render device', r'^(GL|OpenGL|Vulkan|SurrealGPU|VkTexture|CommandBuffer|TextureManager|UploadManager|'
                      r'BufferManager|DescriptorSetManager|RenderPassManager|SamplerManager|ShaderManager|'
                      r'FramebufferManager)'),
    ('scene render', r'(VisibleFrame|VisibleMesh|VisibleNode|BspClipper|PortalSpan|DrawPortal|LightSystem|'
                     r'LightActorTree|Lightmap|FogSystem|Corona|RenderSubsystem|UpdateBspInfo|'
                     r'UpdateRenderInterface|FireEngine|UFireTexture|UFractalTexture|UWaveTexture|UWetTexture|'
                     r'UIceTexture|UWaterTexture|FrustumPlanes|NCanvas|UCanvas|UFont|UMesh::|ULodMesh|'
                     r'USkeletalMesh|SceneNode)'),
    ('collision', r'(Collision|TraceTester|TraceAABB|TraceRay|TraceSphere|TraceCylinder|NodeRayIntersect|'
                  r'Overlap\w*Tester)'),
    ('actor iterators', r'(Iterator|NActor::(AllActors|ZoneActors|RadiusActors|VisibleActors|'
                        r'VisibleCollidingActors|TouchingActors|TraceActors|ChildActors|BasedActors))'),
    ('script VM and objects', r'^(Frame::|ExpressionEvaluator|ExpressionValue|Expression\b|ScriptCall|CallEvent|'
                              r'UFunction|Bytecode|VirtualFunctionCache|FindScriptFunction|NObject::|UObject::|'
                              r'UStruct::|UClass::|U\w*Property::|PropertyDataBlock|GC::|GCObject|GCMarker|'
                              r'Package::|PackageManager|ObjectStream)'),
    ('game: tick, physics, AI, natives', r'^(ULevel::|ULevelBase::|UActor|UPawn|UPlayerPawn|UScriptedPawn|'
                                         r'UDeusEx|UEventManager|UDecoration|UInventory|UWeapon|UProjectile|'
                                         r'UMover|UTrigger|UZoneInfo|ULevelInfo|UGameInfo|UNavigationPoint|'
                                         r'UCarcass|UAugmentation|USkill|N(Actor|Pawn|PlayerPawn|ScriptedPawn|'
                                         r'DeusExPlayer|DeusExDecoration|Decoration|ZoneInfo|LevelInfo|GameInfo|'
                                         r'Inventory|Weapon|NavigationPoint|Mover|DeusExCarcass|Augmentation|'
                                         r'AugmentationManager|DeusExLevelInfo|DeusExMover|DeusExWeapon|'
                                         r'DeusExProjectile)::|Engine::(Tick|CallEvent|LoadMap)|Timeline)'),
    ('UI windows', r'^(U\w*Window|N\w*Window|NGC|UGC|UDeusExRootWindow|URootWindow|UFlagBase|NFlagBase|'
                   r'UConEvent|UConversation|UConPlay|NConPlay|NConversation)'),
    ('audio', r'^(Audio|Galaxy|USurrealAudioDevice|USound|UMusic|OpenAL|alc|al[A-Z])'),
    ('maths, callers vary', r'^(mat[234]|vec[234]|Coords|Rotator|quaternion|BBox|dvec)'),
    ('containers and names, callers vary', r'(^std::|^void std::|^Array<|^NameString|^__gnu|_Hashtable|_Map_base)'),
]
FORK_AREA_RES = [(name, re.compile(rx)) for name, rx in FORK_AREAS]

DRIVER_DSO = re.compile(r'(libgallium|libGL|libEGL|libGLX|libGLdispatch|libglapi|libdrm|libvulkan|'
                        r'libX11|libxcb|libXext|libXrender|libXfixes|libXcomposite|libxshmfence|'
                        r'libwayland|opengl32\.|win32u\.|winex11\.|libvulkan_radeon|radeonsi|libLLVM|'
                        r'libMangoHud|ddraw\.|wined3d\.|d3d)', re.I)
RUNTIME_DSO = re.compile(r'(libc\.so|libm\.so|libstdc\+\+|libgcc|ld-linux|\[vdso\]|\[kernel|libpthread|'
                         r'msvcrt\.|ntdll\.|kernel32\.|kernelbase\.|ucrtbase\.|user32\.|gdi32\.|'
                         r'libz\.|libpcre|libglib|libffi)', re.I)
AUDIO_DSO = re.compile(r'(libopenal|libpulse|libpipewire|libasound|dsound\.|winepulse\.|mmdevapi\.)', re.I)


def demangle_msvc(name):
    """Class::Function from an MSVC export's decorated name, enough to read."""
    if not name.startswith('?'):
        return name
    body = name[1:]
    special = {'?0': '{ctor}', '?1': '{dtor}', '?4': 'operator=', '?_G': '{scalar deleting dtor}',
               '?_E': '{vector deleting dtor}', '?2': 'operator new', '?3': 'operator delete'}
    prefix = None
    for code, label in special.items():
        if body.startswith(code):
            prefix, body = label, body[len(code):]
            break
    parts = body.split('@@', 1)[0].split('@')
    parts = [p for p in parts if p]
    if prefix:
        parts = [prefix] + parts
    if not parts:
        return name
    return '::'.join(reversed(parts))


class PeExports:
    """A PE file's sections and named exports, by address offset (RVA)."""

    def __init__(self, path):
        data = open(path, 'rb').read()
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        nsec = struct.unpack_from('<H', data, pe + 6)[0]
        optsz = struct.unpack_from('<H', data, pe + 20)[0]
        opt = pe + 24
        self.base = struct.unpack_from('<I', data, opt + 28)[0]
        self.size = struct.unpack_from('<I', data, opt + 56)[0]
        exp_rva, exp_size = struct.unpack_from('<II', data, opt + 96)
        self.sections = []
        for i in range(nsec):
            s = opt + optsz + i * 40
            vsize, va, rsize, rptr = struct.unpack_from('<IIII', data, s + 8)
            self.sections.append((va, max(vsize, rsize), rptr))
        names = []
        if exp_rva:
            e = self.offset(exp_rva)
            _, _, nfun, nnames, afun, aname, aord = struct.unpack_from('<7I', data, e + 12)
            funcs = [struct.unpack_from('<I', data, self.offset(afun) + 4 * i)[0] for i in range(nfun)]
            for i in range(nnames):
                name_rva = struct.unpack_from('<I', data, self.offset(aname) + 4 * i)[0]
                ordinal = struct.unpack_from('<H', data, self.offset(aord) + 2 * i)[0]
                o = self.offset(name_rva)
                n = data[o:data.index(b'\0', o)].decode('latin1')
                rva = funcs[ordinal]
                if exp_rva <= rva < exp_rva + exp_size:
                    continue  # a forwarder
                names.append((rva, demangle_msvc(n)))
        names.sort()
        self.rvas = [r for r, _ in names]
        self.names = [n for _, n in names]

    def offset(self, rva):
        for va, size, rptr in self.sections:
            if va <= rva < va + size:
                return rva - va + rptr
        raise ValueError('rva %#x in no section' % rva)

    def name(self, rva):
        i = bisect.bisect_right(self.rvas, rva) - 1
        if i < 0:
            return '%#x' % rva
        return self.names[i]


def read_maps(path):
    """Each mapped file's base: its mapping at file offset 0."""
    bases = {}
    if not os.path.exists(path):
        return bases
    for line in open(path):
        parts = line.split(None, 5)
        if len(parts) < 6:
            continue
        start = int(parts[0].split('-')[0], 16)
        offset = int(parts[2], 16)
        f = parts[5].strip()
        if offset == 0 and f not in bases:
            bases[f] = start
    return bases


def read_log(run):
    for name in ('engine.log', 'DeusEx.log'):
        path = os.path.join(run, name)
        if os.path.exists(path):
            return name, open(path, encoding='latin1').read()
    sys.exit('%s: no engine.log or DeusEx.log' % run)


def ms_of_day(epoch):
    t = datetime.datetime.fromtimestamp(epoch)
    return ((t.hour * 60 + t.minute) * 60 + t.second) * 1000 + t.microsecond / 1000.0


def samples(run):
    env = dict(os.environ, LD_LIBRARY_PATH=PERF_LIB)
    out = subprocess.run([PERF, 'script', '-i', os.path.join(run, 'perf.data'), '-F', 'time,ip,sym,dso', '--ns'],
                         env=env, capture_output=True, text=True, errors='replace').stdout
    line_re = re.compile(r'^\s*(\d+\.\d+):\s+([0-9a-f]+)\s+(.*?)\s+\((.*)\)\s*$')
    for line in out.splitlines():
        m = line_re.match(line)
        if m:
            yield float(m.group(1)), int(m.group(2), 16), m.group(3), m.group(4)


def fork_area(sym, dso):
    if '/SurrealEngine' in dso or dso.endswith('SurrealEngine'):
        for name, rx in FORK_AREA_RES:
            if rx.search(sym):
                return name
        return 'other engine code'
    return dso_area(dso)


def dso_area(dso):
    if DRIVER_DSO.search(dso):
        return 'GL driver and window system'
    if AUDIO_DSO.search(dso):
        return 'audio'
    if RUNTIME_DSO.search(dso):
        return 'C runtime and OS'
    return 'other'


def report(run, top):
    logname, log = read_log(run)
    windows = [tuple(int(x) for x in m.groups()) for m in WINDOW.finditer(log)]
    if not windows:
        sys.exit('%s: no DXPERF window lines in %s' % (run, logname))
    total = TOTAL.search(log)
    frames = sum(w[3] for w in windows)
    spans = [(w[1], w[2]) for w in windows]
    start, end = spans[0][0], spans[-1][1]
    original = logname == 'DeusEx.log'
    bases = read_maps(os.path.join(run, 'maps.txt'))
    pe_cache = {}

    areas, funcs = Counter(), Counter()
    kept = 0
    for epoch, ip, sym, dso in samples(run):
        t = ms_of_day(epoch)
        if not (start <= t <= end):
            continue
        kept += 1
        low = os.path.basename(dso).lower()
        if original and low.endswith(('.dll', '.exe')) and '/gamefiles/' in dso:
            area = next((name for name, dlls in ORIGINAL_AREAS if low in dlls), 'other game DLLs')
            if dso not in pe_cache:
                pe_cache[dso] = PeExports(dso)
            pe = pe_cache[dso]
            base = bases.get(dso, pe.base)
            func = '%s!%s' % (os.path.basename(dso), pe.name(ip - base))
        elif original:
            area = dso_area(dso)
            func = '%s!%s' % (os.path.basename(dso), sym)
        else:
            area = fork_area(sym, dso)
            func = sym if '/SurrealEngine' in dso else '%s!%s' % (os.path.basename(dso), sym)
        areas[area] += 1
        funcs[func] += 1

    per_frame = 1000.0 / RATE / max(frames, 1)
    wall = (total and float(total.group(4))) or 0.0
    print('== %s (%s)' % (run, 'original' if original else 'fork'))
    print('   %d frames in %d windows, %.0f s; %s' % (frames, len(windows), (end - start) / 1000.0,
                                                     total.group(0).replace('DXPERF: total ', '') if total else ''))
    print('   main thread on the CPU %.3f ms a frame of the frame\'s %.3f (the rest: waits, the kernel)'
          % (kept * per_frame, wall))
    print('   %-52s %8s' % ('area', 'ms/frame'))
    for name, n in areas.most_common():
        print('   %-52s %8.3f' % (name, n * per_frame))
    print('   %-52s %8s' % ('function (self)', 'ms/frame'))
    for name, n in funcs.most_common(top):
        print('   %-70.70s %8.3f' % (name, n * per_frame))
    print()


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--top', type=int, default=30, help='functions to list (30)')
    ap.add_argument('runs', nargs='+')
    args = ap.parse_args()
    if not os.access(PERF, os.X_OK):
        sys.exit('no perf at %s -- vibe/tools/host-tools.sh' % PERF)
    for run in args.runs:
        report(run, args.top)


if __name__ == '__main__':
    main()
