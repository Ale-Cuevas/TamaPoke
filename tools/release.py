#!/usr/bin/env python3
"""Release helper for TamaPoke (used by the tamapoke-release skill).

  python3 tools/release.py bump 2.7            FW_VERSION + web/manifest.json + a CHANGELOG skeleton
  python3 tools/release.py row "check" "[CHANGELOG 2.7](../CHANGELOG.md)" "build ✓. Board: pending"
                                               append the next numbered row to docs/PENDING_TESTS.md
  python3 tools/release.py pointers [--fix]    re-resolve `name` `file:line` pointers in docs/GAME_LOGIC.md
  python3 tools/release.py check               versions agree, no TODO left, pointers fresh (exit 1 if not)

Only the standard library; run it with python3 from the repo root (or anywhere: it finds the root itself).
"""
import datetime
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INO = ROOT / "TamaPoke.ino"
MANIFEST = ROOT / "web" / "manifest.json"
CHANGELOG = ROOT / "CHANGELOG.md"
PENDING = ROOT / "docs" / "PENDING_TESTS.md"
GUIDE = ROOT / "docs" / "GAME_LOGIC.md"

VER_RE = re.compile(r'#define FW_VERSION "([^"]+)"')


def fw_version():
    return VER_RE.search(INO.read_text()).group(1)


def manifest_version():
    return json.loads(MANIFEST.read_text())["version"]


# ---------------------------------------------------------------- bump

def cmd_bump(args):
    if len(args) != 1 or not re.fullmatch(r"\d+\.\d+", args[0]):
        sys.exit("usage: release.py bump X.Y")
    new = args[0]
    old = fw_version()
    INO.write_text(VER_RE.sub(f'#define FW_VERSION "{new}"', INO.read_text(), count=1))
    m = MANIFEST.read_text()
    MANIFEST.write_text(re.sub(r'("version":\s*")[^"]+(")', rf"\g<1>{new}\g<2>", m, count=1))
    cl = CHANGELOG.read_text()
    if f"## [{new}]" not in cl:
        today = datetime.date.today().isoformat()
        i = cl.index("## [")
        cl = cl[:i] + f"## [{new}] - {today}\n\n### Changed\n\n- TODO\n\n" + cl[i:]
        CHANGELOG.write_text(cl)
    print(f"FW_VERSION {old} -> {new}; web/manifest.json {new}; CHANGELOG has ## [{new}] (fill in the TODO)")
    print("Still by hand: the version + commit line at the top of docs/GAME_LOGIC.md")


# ---------------------------------------------------------------- PENDING_TESTS row

def cmd_row(args):
    if len(args) != 3:
        sys.exit('usage: release.py row "check" "details" "result"')
    lines = PENDING.read_text().split("\n")
    last_i, last_n = None, 0
    for i, ln in enumerate(lines):
        m = re.match(r"\|\s*(\d+)\s*\|", ln)
        if m:
            last_i, last_n = i, int(m.group(1))
    if last_i is None:
        sys.exit("no numbered row found in PENDING_TESTS.md")
    n = last_n + 1
    lines.insert(last_i + 1, f"| {n} | {args[0]} | {args[1]} | {args[2]} |")
    PENDING.write_text("\n".join(lines))
    print(f"added row {n}")


# ---------------------------------------------------------------- GAME_LOGIC pointers

TOKEN = re.compile(r"`([^`\n]+)`")
PTR = re.compile(r"^(?:([A-Za-z0-9_./]+\.(?:ino|cpp|h|py|sh|ini|csv)))?:(\d+)(?:-(\d+))?$")
FILE_ONLY = re.compile(r"^[A-Za-z0-9_./]+\.(?:ino|cpp|h)$")
SRC_CACHE = {}


def src_lines(name):
    if name not in SRC_CACHE:
        p = ROOT / name
        SRC_CACHE[name] = p.read_text(errors="replace").split("\n") if p.is_file() else None
    return SRC_CACHE[name]


def ident_of(tok):
    """`Pet::store` -> store, `yardDropFood(kind)` -> yardDropFood, `BDROP_*` -> None"""
    t = tok.strip().split("(")[0].strip()
    t = t.split("::")[-1]
    if t.startswith("enum") or t.startswith("struct"):
        t = t.split()[-1]
    return t if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", t) else None


def def_candidates(lines, name):
    """1-based lines that look like the definition of name (function, method, constant, macro, type)."""
    n = re.escape(name)
    pats = [
        re.compile(rf"^[A-Za-z_][^;]*?\b(?:\w+::)?{n}\s*\([^;]*$"),            # top-level function definition
        re.compile(rf"^\s+(?:[\w:<>*&]+\s+)+[*&]?{n}\s*\([^;]*\)\s*(?:const\s*)?\{{"),  # inline method
        re.compile(rf"^\s*#define\s+{n}\b"),
        re.compile(rf"^\s*(?:typedef\s+)?(?:enum|struct|class|union)\b[^;]*\b{n}\b(?!\s*;)"),
        re.compile(rf"^[A-Za-z_][\w:<>*&\s]*?[\s*&]{n}\s*(?:\[[^\]]*\])*\s*(?:=|;|\{{)"),  # global / constant
        re.compile(rf"^\s*(?:enum\b.*\{{\s*)?{n}\s*(?:=|,)"),                   # enum value at line start
    ]
    out = []
    for i, ln in enumerate(lines):
        if name not in ln or ln.lstrip().startswith("//"):
            continue
        if any(p.search(ln) for p in pats):
            out.append(i + 1)
    return out


def scan_pointers(text):
    """Yield (start, end, file, line, line2, name, tok, explicit) for every `:N` / `file:N` token.

    name is the identifier in the backtick token right before the pointer (only spaces or "(" between),
    else None. A bare `:N` takes the file of the last `file` or `file:N` token in the same paragraph or
    table row."""
    blocks = re.split(r"(\n\s*\n|\n(?=\|))", text)  # context resets at a blank line or a new table row
    pos = 0
    for blk in blocks:
        cur_file, prev_name, prev_end = None, None, 0
        for m in TOKEN.finditer(blk):
            tok = m.group(1)
            pm = PTR.match(tok)
            if pm:
                if pm.group(1):
                    cur_file = pm.group(1)
                name = prev_name if re.fullmatch(r"[\s(]*", blk[prev_end:m.start()]) else None
                if cur_file:
                    yield (pos + m.start(), pos + m.end(), cur_file, int(pm.group(2)),
                           int(pm.group(3)) if pm.group(3) else None, name, tok, bool(pm.group(1)))
                prev_name = None
            elif FILE_ONLY.match(tok):
                cur_file, prev_name = tok, None
            else:
                prev_name = ident_of(tok)
            prev_end = m.end()
        pos += len(blk)


def source_files():
    return sorted(str(p.relative_to(ROOT)) for p in ROOT.glob("*") if p.suffix in (".ino", ".cpp", ".h"))


def resolve(text):
    """-> (new_text, changes, unresolved)"""
    edits, changes, unresolved = [], [], []
    for start, end, f, ln, ln2, name, tok, explicit in scan_pointers(text):
        if not name:
            continue  # a range or a line with no name in front: nothing to look up
        # a bare `:N` may point into another file than the one named last (e.g. a header, then .ino lines)
        files = [f] if explicit else [f, "TamaPoke.ino", "pet.cpp"] + source_files()
        hit = None
        for cand_file in dict.fromkeys(files):
            lines = src_lines(cand_file)
            cands = def_candidates(lines, name) if lines else []
            if cands:
                hit = (cand_file, min(cands, key=lambda c: abs(c - ln)))
                break
        if not hit:
            unresolved.append(f"`{name}` {tok}: no definition found (looked in {f}{'' if explicit else ' and others'})")
            continue
        new = hit[1]
        if new == ln:
            continue
        rng = f"-{ln2 + new - ln}" if ln2 else ""
        new_tok = f"{f if explicit else ''}:{new}{rng}"
        edits.append((start, end, f"`{new_tok}`"))
        where = "" if explicit or hit[0] == f else f" (in {hit[0]})"
        changes.append(f"`{name}` {tok} -> {new_tok}{where}")
    for start, end, rep in reversed(edits):
        text = text[:start] + rep + text[end:]
    return text, changes, unresolved


def cmd_pointers(args):
    fix = "--fix" in args
    text = GUIDE.read_text()
    new, changes, unresolved = resolve(text)
    for c in changes:
        print(("fixed  " if fix else "stale  ") + c)
    for u in unresolved:
        print("check  " + u)
    if fix and changes:
        GUIDE.write_text(new)
    print(f"{len(changes)} {'fixed' if fix else 'stale'}, {len(unresolved)} to check by hand")
    return 1 if changes and not fix else 0


# ---------------------------------------------------------------- check

def cmd_check(_args):
    bad = []
    fw, mf = fw_version(), manifest_version()
    if fw != mf:
        bad.append(f"FW_VERSION {fw} != manifest {mf}")
    cl = CHANGELOG.read_text()
    top = re.search(r"^## \[([^\]]+)\]", cl, re.M)
    if not top or top.group(1) != fw:
        bad.append(f"top CHANGELOG entry is {top.group(1) if top else None}, not {fw}")
    sect = cl[top.start():cl.find("\n## [", top.end())] if top else ""
    if "TODO" in sect:
        bad.append("CHANGELOG entry still has TODO")
    head = "\n".join(GUIDE.read_text().split("\n")[:8])
    if f"v{fw}" not in head:
        bad.append(f"GAME_LOGIC.md header does not mention v{fw}")
    _, changes, _ = resolve(GUIDE.read_text())
    if changes:
        bad.append(f"{len(changes)} stale GAME_LOGIC pointers (run: release.py pointers --fix)")
    for b in bad:
        print("FAIL  " + b)
    if not bad:
        print(f"OK  v{fw}: manifest, CHANGELOG, guide header and pointers agree")
    return 1 if bad else 0


def main():
    cmds = {"bump": cmd_bump, "row": cmd_row, "pointers": cmd_pointers, "check": cmd_check}
    if len(sys.argv) < 2 or sys.argv[1] not in cmds:
        sys.exit(__doc__)
    sys.exit(cmds[sys.argv[1]](sys.argv[2:]) or 0)


if __name__ == "__main__":
    main()
