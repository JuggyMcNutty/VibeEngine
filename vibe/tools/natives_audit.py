#!/usr/bin/env python3
"""Every native of the game's DLLs, against the engine fork's.

For each native a Deus Ex package declares -- its script declaration, its
`exec` export in the original DLL -- which function the fork registers for
Deus Ex, whether that function is a stub (it, or the package method it hands
over to, logs `Unimplemented`), and how often the game's scripts call it.
Surreal registers some natives per game (`if (engine->LaunchInfo.IsDeusEx())`);
the conditions are evaluated as Deus Ex 1112fm.

    python3 vibe/tools/natives_audit.py [--game DIR] [--engine DIR] [--tsv FILE]
                                        [--runs LOG...] [--all]

It prints a tally per package, then the natives that are not simply
implemented (--all: every native):

- stubs, and partial ones: some code, or code compiled out with `#if 0` and
  nothing real in an `#else`;
- empty bodies;
- iterators that make no iterator (a foreach over one stops the game);
- declared natives nothing registers (a call to one stops the game too).

--runs takes engine logs and adds which stubs fired, and where from; --tsv
writes every native with its original's address. The judgement the listing
cannot make -- whether an implemented native does what the original does --
is vibe/docs/NATIVES.md's.

It reads the game's packages with dx-reverse-info's tools/ida/ue1_types.py,
from beside this clone in the parent folder of Port Ex Machina's repositories
(dx-reverse-info/ and gamefiles/): $DX_ROOT, or the folder the clone is in.
"""
import argparse
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
DX_ROOT = os.environ.get("DX_ROOT") or os.path.normpath(os.path.join(REPO, ".."))

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(DX_ROOT, "dx-reverse-info", "tools", "ida"))
import ue1_types as u  # noqa: E402

DEUS_EX_PACKAGES = ["Core", "Engine", "Extension", "ConSys", "DeusExText", "DeusEx", "IpDrv", "Fire"]

NATIVE_DECL = re.compile(r"""
    (?P<mods>(?:\b\w+\b(?:\s*\(\s*\d+\s*\))?\s+)*?)
    (?P<kind>function|event|operator|preoperator|postoperator)\b\s*(?:\(\s*\d+\s*\))?\s*
    (?:(?P<ret>\w+(?:\s*<\s*\w+\s*>)?)\s+)?
    (?P<name>[A-Za-z_]\w*|[^\s\w(]+)\s*\(
""", re.X)


# ---------------------------------------------------------------------------
# The script's natives

class Native:
    def __init__(self, cls, name, number, kind, mods):
        self.cls, self.name, self.number, self.kind, self.mods = cls, name, number, kind, mods
        self.dll = None
        self.addr = None
        self.exec_name = None
        self.reg = None          # the fork's registration for Deus Ex
        self.status = None
        self.calls = 0
        self.ambiguous = False
        self.fired = []          # (map, caller)


def script_natives(model):
    out = []
    for cls in model.classes.values():
        if cls.package not in DEUS_EX_PACKAGES:
            continue
        for m in NATIVE_DECL.finditer(cls.text):
            mods = m.group("mods").lower()
            mm = re.search(r"\b(?:native|intrinsic)\b\s*(?:\(\s*(\d+)\s*\))?", mods)
            if not mm:
                continue
            number = int(mm.group(1)) if mm.group(1) else 0
            kind = m.group("kind").lower()
            name = m.group("name")
            if kind == "function" and name.lower() in ("function", "event"):
                continue
            out.append(Native(cls, name, number, kind, mods))
    return out


def script_calls(model, natives):
    """Call sites per name over every script, less the declarations; a name a
    script also declares as a non-native function is ambiguous."""
    counts, declared_script = {}, set()
    call = re.compile(r"(?<![\w.])(?:\.\s*)?([A-Za-z_]\w*)\s*\(")
    call_any = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
    decl = re.compile(r"\b(?:function|event)\s+(?:\w+\s+)?([A-Za-z_]\w*)\s*\(")
    for cls in model.classes.values():
        text = cls.text
        decls = {}
        for m in decl.finditer(text):
            decls[m.group(1).lower()] = decls.get(m.group(1).lower(), 0) + 1
            if "native" not in text[max(0, m.start() - 80):m.start()].split(";")[-1].lower():
                declared_script.add(m.group(1).lower())
        for m in call_any.finditer(text):
            k = m.group(1).lower()
            counts[k] = counts.get(k, 0) + 1
        for k, n in decls.items():
            counts[k] -= n
    for nat in natives:
        if nat.kind != "function" and nat.kind != "event":
            continue
        k = nat.name.lower()
        nat.calls = max(counts.get(k, 0), 0)
        nat.ambiguous = k in declared_script
    del call


# ---------------------------------------------------------------------------
# The original's exec exports

def dll_execs(system_dir):
    """(script class lower, exec name lower) -> (dll, address, exec name)"""
    out = {}
    for dll in ["Core", "Engine", "Extension", "ConSys", "DeusExText", "DeusEx", "IpDrv", "Fire"]:
        pe = u.PE(os.path.join(system_dir, dll + ".dll"))
        for n, va in pe.exports.items():
            m = re.match(r"^\?exec(\w+)@[AUXDF](\w+)@@", n)
            if m:
                out[(m.group(2).lower(), m.group(1).lower())] = (dll, follow_jump(pe, va), m.group(1))
    return out


def follow_jump(pe, va):
    """An incrementally linked export is a jmp to the code."""
    try:
        o = pe.off(va - pe.base)
    except ValueError:
        return va
    if pe.data[o] == 0xE9:
        rel = int.from_bytes(pe.data[o + 1:o + 5], "little", signed=True)
        return va + 5 + rel
    return va


# ---------------------------------------------------------------------------
# The fork's registrations

REG = re.compile(r'RegisterVMNativeFunc_\d+\(\s*"(\w+)"\s*,\s*"(\w+)"\s*,\s*&(\w+)::(\w+)\s*,\s*(\d+)\s*\)')


def cond_true_for_deusex(cond):
    c = cond
    c = re.sub(r"(?:engine->)?[lL]aunchInfo\.", "", c)
    c = c.replace("IsDeusEx()", "True")
    c = re.sub(r"\bIs\w+\(\)", "False", c)
    c = re.sub(r"\bue1Version\b", "500", c)
    c = re.sub(r"\bgameVersion\b", "1112", c)
    c = re.sub(r'\bgameExecutableName\b', '"DeusEx"', c)
    c = re.sub(r'\bgameVersionString\b', '"1112fm"', c)
    c = c.replace("&&", " and ").replace("||", " or ")
    c = re.sub(r"!(?!=)", " not ", c)
    try:
        return bool(eval(c, {"__builtins__": {}}, {"True": True, "False": False}))
    except Exception:
        return None


def parse_registrations(engine_src):
    """(class lower, name lower) -> [(func owner, func, number, active for Deus Ex, file)]"""
    regs = {}
    for path in glob.glob(os.path.join(engine_src, "Native", "*.cpp")):
        text = open(path, encoding="utf-8", errors="replace").read()
        for body in function_bodies(text, r"void\s+\w+::RegisterFunctions\s*\(\s*\)"):
            walk_statements(tokens_cpp(body), 0, [True], regs, path)
    return regs


def function_bodies(text, head_re):
    for m in re.finditer(head_re, text):
        i = text.index("{", m.end())
        j = match_brace(text, i)
        yield text[i + 1:j]


def match_brace(text, i):
    depth = 0
    for k in range(i, len(text)):
        if text[k] == "{":
            depth += 1
        elif text[k] == "}":
            depth -= 1
            if depth == 0:
                return k
    return len(text)


def tokens_cpp(text):
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.findall(r'"(?:[^"\\]|\\.)*"|\w+|->|::|&&|\|\||[!=<>]=|\S', text)


def walk_statements(toks, i, active, regs, path):
    """Walks statements from toks[i] to the end (or a closing brace), with
    `active` the enclosing conditions' truth for Deus Ex."""
    while i < len(toks):
        if toks[i] == "}":
            return i + 1
        i = statement(toks, i, active, regs, path)
    return i


def statement(toks, i, active, regs, path):
    t = toks[i]
    if t == "{":
        return walk_statements(toks, i + 1, active, regs, path)
    if t == "if":
        chain_taken = False
        while True:
            j = paren_end(toks, i + 1)
            cond = cond_true_for_deusex("".join(toks[i + 2:j]))
            this = (cond is True) and not chain_taken
            if cond is None:
                this = None
            i = statement(toks, j + 1, active + [this], regs, path)
            chain_taken = chain_taken or cond is True
            if i < len(toks) and toks[i] == "else":
                if i + 1 < len(toks) and toks[i + 1] == "if":
                    i += 1
                    continue
                return statement(toks, i + 1, active + [not chain_taken], regs, path)
            return i
    # an ordinary statement, to its ';'
    j = i
    depth = 0
    while j < len(toks):
        if toks[j] in "([{":
            depth += 1
        elif toks[j] in ")]}":
            depth -= 1
        elif toks[j] == ";" and depth == 0:
            break
        j += 1
    stmt = " ".join(toks[i:j])
    m = REG.search(stmt.replace(" ", "").replace('RegisterVMNativeFunc', ' RegisterVMNativeFunc').replace(",", ", "))
    if m is None:
        m2 = re.search(r'RegisterVMNativeFunc_\d+\s*\(\s*("\w+")\s*,\s*("\w+")\s*,\s*&\s*(\w+)\s*::\s*(\w+)\s*,\s*(\d+)\s*\)', stmt)
        m = m2
    if m:
        g = [x.strip('"') for x in m.groups()]
        act = None if any(a is None for a in active) else all(active)
        regs.setdefault((g[0].lower(), g[1].lower()), []).append((g[2], g[3], int(g[4]), act, os.path.basename(path)))
    return j + 1


def paren_end(toks, i):
    depth = 0
    for k in range(i, len(toks)):
        if toks[k] == "(":
            depth += 1
        elif toks[k] == ")":
            depth -= 1
            if depth == 0:
                return k
    return len(toks)


# ---------------------------------------------------------------------------
# Stubs

def definitions(engine_src):
    """'Owner::Func' -> body text, over the fork's sources."""
    defs = {}
    head = re.compile(r"^[\w:<>*&\s,]*?\b(\w+)::(\w+)\s*\(([^;{}]*)\)\s*(?:const\s*)?\{", re.M)
    for path in glob.glob(os.path.join(engine_src, "**", "*.cpp"), recursive=True):
        text = open(path, encoding="utf-8", errors="replace").read()
        for m in head.finditer(text):
            i = m.end() - 1
            j = match_brace(text, i)
            defs.setdefault("%s::%s" % (m.group(1), m.group(2)), []).append(text[i + 1:j])
    return defs


def handed_over_texts(reg, defs):
    """A registered function's body and the bodies it hands over to: what it
    calls through -> or ::, and in those, their own class's methods called
    by name."""
    owner, func = reg[0], reg[1]
    bodies = defs.get("%s::%s" % (owner, func), [])
    if not bodies:
        return []
    texts = [bodies[0]]
    seen = {"%s::%s" % (owner, func)}
    for m in re.finditer(r"(?:->|::)\s*(\w+)\s*\(", bodies[0]):
        name = m.group(1)
        if name in ("Cast", "TryCast", "Value", "RegisterVMNativeFunc"):
            continue
        for key, bs in defs.items():
            if key.endswith("::" + name) and not key.startswith(owner + "::") and key not in seen:
                seen.add(key)
                texts.append(bs[0])
                cls = key.split("::")[0]
                for m2 in re.finditer(r"(?<![\w>:.])(\w+)\s*\(", bs[0]):
                    k2 = "%s::%s" % (cls, m2.group(1))
                    if k2 in defs and k2 not in seen:
                        seen.add(k2)
                        texts.append(defs[k2][0])
    return texts


def handed_over(reg, defs):
    return "\n".join(handed_over_texts(reg, defs))


TRIVIAL = [
    re.compile(r"^\s*LogUnimplemented\s*\(.*\)\s*$", re.S),
    re.compile(r"^\s*return\s*(?:\w+|\{\s*\}|nullptr|false|true|0|0\.0f?|std::string\(\)|\"\")?\s*$"),
    re.compile(r"^\s*ReturnValue\s*=\s*(?:\{\s*\}|nullptr|false|true|-?\d+(?:\.\d*)?f?|\"[^\"]*\"|std::string\(\)|\w+\(\))\s*$"),
    re.compile(r"^\s*(?:auto|U\w+\s*\*|const\s+\w+\s*&?)\s*\w+\s*=\s*(?:UObject::)?(?:Cast|TryCast)\s*<.*>\s*\(.*\)\s*$", re.S),
    re.compile(r"^\s*$"),
]


def substantive(body):
    body = re.sub(r"//[^\n]*", "", body)
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    stmts = [s for s in re.split(r";|\{|\}", body)]
    return [s.strip() for s in stmts if not any(t.match(s) for t in TRIVIAL)]


def compiled_out(text):
    """An `#if 0` block whose `#else`, if any, does nothing: code compiled
    out with nothing real in its place."""
    for m in re.finditer(r"^\s*#\s*if\s+0\b(.*?)^\s*#\s*endif", text, re.M | re.S):
        parts = re.split(r"^\s*#\s*(?:else|elif)\b[^\n]*", m.group(1), maxsplit=1, flags=re.M)
        if len(parts) == 1 or not substantive(parts[1]):
            return True
    return False


def classify(reg, defs):
    texts = handed_over_texts(reg, defs)
    if not texts:
        return "no-body", ""
    body = texts[0]
    texts = [re.sub(r"/\*.*?\*/", "", re.sub(r"//[^\n]*", "", t), flags=re.S) for t in texts]
    unimpl = any("LogUnimplemented" in t for t in texts)
    disabled = any(compiled_out(t) for t in texts)
    subst = [s for t in texts for s in substantive(t)]
    if unimpl:
        return ("stub" if len(subst) <= 1 else "partial"), body
    if disabled:
        return "partial", body
    if not subst:
        return "empty", body
    return "implemented", body


# ---------------------------------------------------------------------------

def main(argv):
    import signal
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)   # quiet under | head
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--game", default=os.path.join(DX_ROOT, "gamefiles"))
    ap.add_argument("--engine", default=os.path.join(REPO, "SurrealEngine"))
    ap.add_argument("--tsv", metavar="FILE", help="every native, as tab-separated values")
    ap.add_argument("--runs", nargs="*", default=[], metavar="LOG", help="engine logs: which stubs fired")
    ap.add_argument("--all", action="store_true", help="list the implemented natives too")
    a = ap.parse_args(argv)

    model = u.Model()
    u.read_packages(os.path.join(a.game, "System"), model)
    natives = script_natives(model)
    script_calls(model, natives)
    execs = dll_execs(os.path.join(a.game, "System"))
    regs = parse_registrations(a.engine)
    defs = definitions(a.engine)

    by_number = {}
    for key, rs in regs.items():
        for r in rs:
            if r[3] is not False and r[2]:
                by_number.setdefault((key[0], r[2]), []).append((key, r))

    for nat in natives:
        ck = nat.cls.name.lower()
        e = execs.get((ck, nat.name.lower()))
        if e:
            nat.dll, nat.addr, nat.exec_name = e
        cands = [r for r in regs.get((ck, nat.name.lower()), []) if r[3] is not False]
        if not cands and nat.number:
            cands = [r for _, r in by_number.get((ck, nat.number), [])]
        active = [r for r in cands if r[3] is True] or cands
        if not active:
            nat.status = "unregistered"
            continue
        nat.reg = active[-1]
        nat.status, body = classify(nat.reg, defs)
        # A foreach over an iterator native that makes no iterator is a
        # script error: the VM stops the game.
        if "iterator" in nat.mods and "Iterator" not in handed_over(nat.reg, defs):
            nat.status = "no-iterator"

    # Runs: "Unimplemented: Class.Name" lines, with the caller when logged,
    # and the map then up: the last whose game started (GameInfo.InitGame's
    # "Base Mutator is <map>.") or that a client loaded ("Net: loading <map>
    # as a client"). Before either, the log's own name -- or, for a log the
    # harness named (engine.log, DeusEx.log), its run's folder.
    fired = {}
    for log in a.runs:
        mapname = os.path.basename(log).rsplit(".", 1)[0]
        if mapname in ("engine", "DeusEx"):
            mapname = os.path.basename(os.path.dirname(os.path.abspath(log)))
        for line in open(log, encoding="utf-8", errors="replace"):
            m = re.search(r"Base Mutator is (\w+)\.|Net: loading (\w+) as a client", line)
            if m:
                mapname = m.group(1) or m.group(2)
                continue
            m = re.search(r"Unimplemented:\s*([\w]+)\.(\w+)", line)
            if m:
                caller = line.split("Unimplemented:")[0].strip(" :[]\t")
                name = re.sub(r"_(?:Deus|HP|U227k?|UT469|219)$", "", m.group(2))   # the fork's variant names
                fired.setdefault((m.group(1).lower(), name.lower()), []).append((mapname, caller))
    for nat in natives:
        nat.fired = fired.get((nat.cls.name.lower(), nat.name.lower()), [])

    rows = []
    for nat in sorted(natives, key=lambda n: (DEUS_EX_PACKAGES.index(n.cls.package), n.cls.name.lower(), n.name.lower())):
        rows.append([
            nat.cls.package, nat.cls.name, nat.name, str(nat.number), nat.kind,
            nat.dll or "", "0x%08x" % nat.addr if nat.addr else "",
            nat.status, "%s::%s" % (nat.reg[0], nat.reg[1]) if nat.reg else "",
            str(nat.calls), "ambiguous" if nat.ambiguous else "",
            ",".join(sorted({m for m, _ in nat.fired})),
        ])
    if a.tsv:
        with open(a.tsv, "w") as f:
            f.write("package\tclass\tnative\tnumber\tkind\tdll\taddress\tstatus\tfork function\tscript calls\tnote\tfired in\n")
            for r in rows:
                f.write("\t".join(r) + "\n")
    tally = {}
    for nat in natives:
        tally.setdefault(nat.cls.package, {}).setdefault(nat.status, 0)
        tally[nat.cls.package][nat.status] += 1
    for pkg in DEUS_EX_PACKAGES:
        if pkg in tally:
            print("%-11s %s" % (pkg, ", ".join("%s %d" % kv for kv in sorted(tally[pkg].items()))))
    for r in rows:
        if a.all or r[7] != "implemented":
            print("  %-11s %-26s %-28s %5s %-12s %-40s calls %-4s %s %s" % (r[0], r[1], r[2], r[3], r[7], r[8], r[9], r[10], r[11]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
