#!/usr/bin/env python3
"""standin_check.py -- two ways a verbatim N64 line crashes the port.

Both have taken a real match down, and neither shows on the host, whose
models are raw file bytes and whose .bss neighbours happen to be zero.

  payload  A DObj's `dv` is the port's DCDisplay -- what gcSubmitDObj
           (src/dc/objdisplay.c) calls through -- and `dl`, `dl_link`,
           `dist_dl` and `dist_dl_link` are the SAME union member
           (sys/objtypes.h). So a decomp `dobj->dl = <display list>`
           replaces the baked model with N64 bytes, and the next draw
           calls 0xE7 (gsDPPipeSync). The same goes for building a DObj
           out of N64 data: gcAddDObjForGObj(gobj, attr->data) and its
           kin. Five items did the first (src/dc/itemmodel.h
           itemModelSetDisplayList).

  standin  `int llFoo;` is a reloc offset the port has no file for. The
           decomp's read, lbRelocGetFileData(T*, file, &llFoo), is
           `file + &llFoo` -- with the file pointer unbound that is the
           address of a 4-byte int, and anything read past it is the
           neighbouring .bss. The Thunder Jolt walked eight joints' worth
           of it as animation scripts and hung (src/dc/wpattrs.c).
           A stand-in named only inside a data table's initializer is
           not reported: those are EFDesc offsets the port's pack path
           never adds to a file.

Every site the rules match must be in ALLOWED below with the reason it is
safe, and every ALLOWED entry must still match something, so the list
cannot outlive the code. The sources scanned are exactly the ones
linked: src/game/ssb64/Makefile's OBJS, expanded, each object to the .c
its rule compiles.

`./run.sh test standin` runs it alone; `./run.sh test` runs it with the rest.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GAME = os.path.join(ROOT, "src", "game", "ssb64")
MAKEFILE = os.path.join(GAME, "Makefile")
DECOMP = os.environ.get("SSB_DECOMP_DIR", "/opt/ssb-decomp-re")
if not os.path.isdir(DECOMP):
    DECOMP = os.path.join(ROOT, "ssb-decomp-re")

# (rule, repo-relative file, a substring of the flagged code, why it is safe)
ALLOWED = [
    # -- payload writes --------------------------------------------------
    ("payload", "ssb-decomp-re/src/sys/objman.c", "new_dobj->dv = dvar",
     "the constructors themselves; what they are given is checked at "
     "their call sites"),
    ("payload", "src/dc/itbox.c", "dc_model_hidden_payload",
     "a payload the port built"),
    ("payload", "src/dc/ittarubomb.c", "dc_model_hidden_payload",
     "the same, one stage over: the barrel's smash debris"),
    ("payload", "src/dc/itemmodel.c", "dobj->dv = &alt->disp",
     "a baked alternate's DCDisplay, filled by dc_model_init_payload"),
    ("payload", "src/dc/itemmodel.c", "dobj->dl = dl",
     "the FT_HOSTTEST arm of itemModelSetDisplayList: the host's item "
     "models are raw bytes and nothing draws them"),
    ("payload", "src/dc/fighter.c", "bc.dv = dv",
     "BatchCtx.dv, the batch's texture V offset -- not a DObj's"),
    ("payload", "src/dc/ftmanager.c", "fp->dl_link",
     "FTStruct.dl_link, a camera link number, not the DObj union"),
    ("payload", "src/dc/ftparam.c", "fp->dl_link",
     "FTStruct.dl_link, as above"),
    ("payload", "src/dc/stage.c", "effect_desc.dl_link",
     "EFDesc.dl_link, a camera link number"),
    ("payload", "src/dc/objmodel.c", "c->dv =",
     "a material colour's texture-scroll delta, not a DObj"),
    # -- DObjs built out of data -----------------------------------------
    ("payload", "src/dc/objmodel.c", "disp",
     "dc_model_add_dobjs: the pack's own DCDisplay array"),
    ("payload", "src/dc/objmodel.c", ", dv)",
     "dc_model_add_dobjs: an entry of the same array, or NULL for a tree "
     "entry with no display list"),
    ("payload", "src/dc/ftmanager.c", "accesspart->dl",
     "the accessory GObj's DObj: FTAccessPart.dl is NULL, the accessory "
     "being a tag of the fighter's pack"),
    ("payload", "src/dc/ftparam.c", "accesspart->dl",
     "the accessory GObj re-made on a costume change, NULL as in "
     "ftManagerMakeFighter"),
    ("payload", "src/dc/ftparam.c", "dc_model_part_display",
     "a model part's payload out of dc_model_add_dobjs's array, or NULL"),
    ("payload", "src/dc/ftcommon.c", "gcAddDObjForGObj(fp->fighter_gobj, dl)",
     "`dl` is dc_model_hidden_payload's, a few lines up"),
    ("payload", "src/dc/scstaffroll.c", "gcAddChildForDObj(parent,",
     "a credit glyph's DObj: &sSCStaffrollNameAndJobDisplayLists[id], one "
     "of the port's own DCDisplays, filled by dc_model_init_payload over "
     "a scglyphs pack joint and then grafted"),
    # -- stand-in reads --------------------------------------------------
    ("standin", "ssb-decomp-re/src/ft/ftcommon/ftcommoncapturecaptain.c",
     "llCaptainMainMotionSpecialHiVec2h",
     "gFTDataCaptainMainMotion is bound so this lands on the copied table "
     "(src/dc/ftcommon.c ftCommonCaptureCaptainBindOffsets)"),
    ("standin", "src/dc/ftcommon.c", "llCaptainMainMotionSpecialHiVec2h",
     "the binding itself, and its overlay reset"),
    ("standin", "ssb-decomp-re/src/ft/ftchar/ftfox/ftfoxspeciallw.c",
     "llFoxMainMotionLwReflectorFTSpecialColl",
     "gFTDataFoxMainMotion is bound (src/dc/ftcommon.c "
     "ftFoxReflectorBindOffsets)"),
    ("standin", "src/dc/ftcommon.c", "llFoxMainMotionLwReflectorFTSpecialColl",
     "the binding itself, and its overlay reset"),
]

# The DObj constructors are found, not listed. sys/objman.c's three take
# the payload as their second argument; any linked function that hands one
# of its own parameters to a constructor in that slot is a constructor too
# (the RpyR and TraRotSca wrappers, gcSetupCustomDObjs, the port's
# itManagerSetupItemDObjs...), to a fixed point. Such a forward is not a
# site; the forwarder's callers are.
BASE_CTORS = {"gcAddDObjForGObj": 1, "gcAddChildForDObj": 1,
              "gcAddSiblingForDObj": 1}
UNION = ("dv", "dl", "dl_link", "dist_dl", "dist_dl_link")


def rel(p):
    """Repo-relative, with the decomp's files as ssb-decomp-re/... wherever
    the checkout lives, so the ALLOWED rows above match either way."""
    if os.path.abspath(p).startswith(os.path.abspath(DECOMP) + os.sep):
        return os.path.join("ssb-decomp-re", os.path.relpath(p, DECOMP))
    return os.path.relpath(p, ROOT)


def make_vars(text):
    """The Makefile's plain `VAR = ...` / `VAR += ...` assignments, with
    continuations joined. Conditionals are read as if taken, which only
    adds objects (romdisk.o has no source and is skipped)."""
    text = text.replace("\\\n", " ")
    out = {}
    for m in re.finditer(r"^([A-Z_][A-Z0-9_]*)\s*(\+?=|:=)(.*)$", text, re.M):
        name, op, val = m.group(1), m.group(2), m.group(3).split("#")[0]
        out[name] = (out.get(name, "") + " " + val) if op == "+=" else val
    return out


def expand(val, env, depth=0):
    if depth > 20:
        sys.exit("standin_check: Makefile variables nest too deep")
    return re.sub(r"\$\((\w+)\)",
                  lambda m: expand(env.get(m.group(1), ""), env, depth + 1),
                  val)


def linked_sources():
    text = open(MAKEFILE).read()
    env = make_vars(text)
    objs = expand(env.get("OBJS", ""), env).split()
    rules = {}
    for m in re.finditer(r"^([\w.-]+)\.o:\s*(\S+\.c)\b", text, re.M):
        rules[m.group(1)] = m.group(2)
    srcs = []
    for o in objs:
        if not o.endswith(".o"):
            continue
        name = o[:-2]
        if name == "romdisk":
            continue
        if re.search(r"^%s\.o:\s*\S+\.S\b" % re.escape(name), text, re.M):
            continue            # assembled data (aica_fwblob.o): no C
        if name not in rules:
            sys.exit("standin_check: %s.o is linked but no rule names its "
                     "source" % name)
        p = rules[name].replace("$(SSB_DECOMP_DIR)", DECOMP)
        p = p if os.path.isabs(p) else os.path.join(GAME, p)
        p = os.path.normpath(p)
        if not os.path.exists(p):
            if re.search(r"^%s:" % re.escape(os.path.basename(p)), text,
                         re.M):
                continue            # generated by a rule (sintable.c): data
            sys.exit("standin_check: %s.o's source %s is missing"
                     % (name, rel(p)))
        srcs.append(p)
        # A port file that stands in front of a decomp file it #includes
        # whole (src/dc/objanim.c): the included file is
        # linked too, and is where all but two of its names are defined.
        for inc in re.findall(r'^#include\s*[<"]([\w/]+\.c)[>"]',
                              open(p).read(), re.M):
            q = os.path.join(DECOMP, "src", inc)
            if not os.path.exists(q):
                sys.exit("standin_check: %s includes %s, which is not a "
                         "decomp source" % (rel(p), inc))
            srcs.append(q)
    if len(srcs) < 100:
        sys.exit("standin_check: only %d linked sources found; the "
                 "Makefile's OBJS no longer reads the way this expects"
                 % len(srcs))
    return sorted(set(srcs))


def strip_c(src):
    """Comments and string/char literals blanked, newlines kept, so
    offsets still map to the same lines."""
    out, i, n = [], 0, len(src)
    while i < n:
        c = src[i]
        if src.startswith("/*", i):
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", src[i:j]))
            i = j
        elif src.startswith("//", i):
            j = src.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(c + re.sub(r"[^\n]", " ", src[i + 1:j - 1]) + c
                       if j - i >= 2 else src[i:j])
            i = j
        elif c == "#" and (i == 0 or src[i - 1] == "\n"):
            # a preprocessor line: keep #if arms' code, drop the directive
            j = i
            while True:
                k = src.find("\n", j)
                if k < 0:
                    k = n
                    break
                if src[k - 1] != "\\":
                    break
                j = k + 1
            out.append(re.sub(r"[^\n]", " ", src[i:k]))
            i = k
        else:
            out.append(c)
            i += 1
    return "".join(out)


def brace_kinds(code):
    """For every offset, whether it is inside a function body ('func'), a
    data initializer ('init') or neither (None)."""
    kinds = [None] * (len(code) + 1)
    stack = []
    for i, c in enumerate(code):
        if c == "{":
            if stack:
                stack.append(stack[-1] if stack[-1] == "func" else
                             ("init" if stack[-1] == "init" else
                              _opener(code, i)))
            else:
                stack.append(_opener(code, i))
        elif c == "}" and stack:
            stack.pop()
        kinds[i] = stack[-1] if stack else None
    return kinds


def _opener(code, i):
    j = i - 1
    while j >= 0 and code[j].isspace():
        j -= 1
    if j >= 0 and code[j] == ")":
        return "func"
    if j >= 0 and code[j] in "=,{":
        return "init"
    return "type"


def functions(code):
    """[(name, params text, body start, body end)] for every function
    definition in the (stripped) file."""
    out = []
    for m in re.finditer(r"^[A-Za-z_][\w\s\*]*?\b(\w+)\s*\(([^;{}]*)\)\s*\{",
                         code, re.M):
        start = m.end() - 1
        depth = 0
        for i in range(start, len(code)):
            if code[i] == "{":
                depth += 1
            elif code[i] == "}":
                depth -= 1
                if depth == 0:
                    out.append((m.group(1), m.group(2), start, i))
                    break
    return out


def param_names(params):
    """A parameter list's names, in order ("void" and "..." give none)."""
    out = []
    for part in params.split(","):
        m = re.search(r"(\w+)\s*(?:\[[^\]]*\])?\s*$", part.strip())
        if m and m.group(1) != "void":
            out.append(m.group(1))
    return out


def call_args(code, start):
    """The argument text of the call whose '(' is at `start`, split at
    top-level commas."""
    depth, args, cur = 0, [], []
    for c in code[start:]:
        if c == "(":
            depth += 1
            if depth == 1:
                continue
        elif c == ")":
            depth -= 1
            if depth == 0:
                args.append("".join(cur).strip())
                return args
        if c == "," and depth == 1:
            args.append("".join(cur).strip())
            cur = []
        else:
            cur.append(c)
    return args


def line_of(code, off):
    return code.count("\n", 0, off) + 1


def main():
    srcs = linked_sources()
    texts = {p: strip_c(open(p, errors="replace").read()) for p in srcs}

    standins = {}
    for p, code in texts.items():
        for m in re.finditer(r"^(?:int|intptr_t|s32|u32)\s+(ll\w+)\s*;",
                             code, re.M):
            standins[m.group(1)] = (p, m.start())

    found = []      # (rule, path, line, code line)
    union = re.compile(r"(?:->|\.)\s*(%s)\s*=(?!=)\s*([^;]*);"
                       % "|".join(UNION))
    ident = re.compile(r"\bll\w+\b")

    funcs = {p: functions(code) for p, code in texts.items()}
    ctors = dict(BASE_CTORS)
    changed = True
    while changed:
        changed = False
        pat = re.compile(r"\b(%s)\s*\(" % "|".join(sorted(ctors)))
        for p, code in texts.items():
            for name, params, fs, fe in funcs[p]:
                if name in ctors:
                    continue
                names = param_names(params)
                for m in pat.finditer(code, fs, fe):
                    args = call_args(code, m.end() - 1)
                    k = ctors[m.group(1)]
                    root = re.match(r"\w+", args[k]) if len(args) > k else None
                    if root and root.group(0) in names:
                        ctors[name] = names.index(root.group(0))
                        changed = True
                        break
    ctor = re.compile(r"\b(%s)\s*\(" % "|".join(sorted(ctors)))

    for p, code in texts.items():
        lines = code.split("\n")
        kinds = None
        for m in union.finditer(code):
            if m.group(2).strip() in ("NULL", "0", "(void*)0", "NULL, 0"):
                continue
            ln = line_of(code, m.start())
            found.append(("payload", p, ln, lines[ln - 1].strip()))
        for m in ctor.finditer(code):
            inside = [f for f in funcs[p] if f[2] < m.start() < f[3]]
            if not inside:
                continue            # a definition or a prototype
            args = call_args(code, m.end() - 1)
            k = ctors[m.group(1)]
            if len(args) <= k or args[k] in ("NULL", "0"):
                continue
            root = re.match(r"&?\s*(\w+)", args[k])
            if inside[-1][0] in ctors and root and \
                    root.group(1) in param_names(inside[-1][1]):
                continue            # a forward of the caller's own argument
            ln = line_of(code, m.start())
            found.append(("payload", p, ln, lines[ln - 1].strip()))
        for m in ident.finditer(code):
            name = m.group(0)
            if name not in standins or standins[name] == (p, m.start()):
                continue
            if kinds is None:
                kinds = brace_kinds(code)
            if kinds[m.start()] != "func":
                continue
            ln = line_of(code, m.start())
            found.append(("standin", p, ln, lines[ln - 1].strip()))

    # The absolute offsets a source defines in top-level asm (Kirby's motion
    # file, src/dc/ftcommon.c) must be the reloc header's, the oracle
    # wpattr_check.py is for the weapons' .ld.
    truth = {n: int(v, 16) for (n, v) in re.findall(
        r"^extern int (ll\w+); // (0x[0-9a-fA-F]+)$",
        open(os.path.join(ROOT, "src", "dc", "decomp", "reloc_data.us.h")).read(),
        re.M)}
    sets = 0
    for p in srcs:
        raw = open(p, errors="replace").read()
        for m in re.finditer(r"\.set \"\s*\w+\((ll\w+)\)\s*\", (0x[0-9a-fA-F]+)",
                             raw):
            sets += 1
            name, val = m.group(1), int(m.group(2), 16)
            if truth.get(name) != val:
                print("%s: .set %s = 0x%x, reloc_data.us.h says %s"
                      % (rel(p), name, val,
                         "nothing" if name not in truth else
                         "0x%x" % truth[name]))
                found.append(("set", p, 0, name))

    used = set()
    bad = []
    for rule, p, ln, text in found:
        hit = None
        for k, (arule, apath, needle, _why) in enumerate(ALLOWED):
            if arule == rule and apath == rel(p) and needle in text:
                hit = k
                break
        if hit is None:
            bad.append((rule, p, ln, text))
        else:
            used.add(hit)

    stale = [ALLOWED[k] for k in range(len(ALLOWED)) if k not in used]
    for rule, p, ln, text in bad:
        print("%s:%d: %s: %s" % (rel(p), ln, rule, text))
    for arule, apath, needle, _why in stale:
        print("standin_check: ALLOWED entry matches nothing now: "
              "(%s, %s, %r)" % (arule, apath, needle))
    if bad or stale:
        sys.exit("standin_check: %d unexplained site(s), %d stale "
                 "allowance(s) -- see the docstring for the two rules"
                 % (len(bad), len(stale)))
    print("standin_check: %d linked sources, %d stand-ins, %d sites, all "
          "explained; %d absolute offsets match reloc_data.us.h"
          % (len(srcs), len(standins), len(found), sets))
    print("standin_check: DObj constructors: %s" % ", ".join(sorted(ctors)))


if __name__ == "__main__":
    main()
