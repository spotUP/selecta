#!/usr/bin/env python3
"""AutoDoc extractor of the UP-MCC package (design 2026-11-06-mui-mcc-package.md section 8.5). Stdlib only. MIT licence.

  mcc_autodoc.py extract OUTDIR   write OUTDIR/<name>.doc for every /****** <name>/<entry> ... ******/ block found under
                                  plugins/shared/mcc (src, prefs, docs; .c .h .autodoc)
  mcc_autodoc.py check            fail (exit 1) when a UPA_/UPM_ name in include/mcc/up.h or a UP_ function in include/mcc/up_lib.h
                                  has no block: the docs cannot drift from the headers
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / "plugins" / "shared" / "mcc"
BLOCK = re.compile(r"^/\*{6} ([^/\s]+)/(\S+) \*{6}\n(.*?)^\*{6}/", re.S | re.M)


def blocks():
    found = []
    for sub in ("src", "prefs", "docs"):
        for path in sorted((ROOT / sub).rglob("*")):
            if path.suffix in (".c", ".h", ".autodoc"):
                for m in BLOCK.finditer(path.read_text(encoding="utf-8")):
                    found.append((m.group(1), m.group(2), m.group(0)))
    return found


def extract(outdir):
    out = Path(outdir)
    out.mkdir(parents=True, exist_ok=True)
    by_name = {}
    for name, entry, text in blocks():
        by_name.setdefault(name, []).append((entry, text))
    for name, items in by_name.items():
        toc = "".join("%s/%s\n" % (name, e) for e, _ in items)
        body = "\f\n".join(t + "\n" for _, t in items)
        (out / (name + ".doc")).write_text("TABLE OF CONTENTS\n\n" + toc + "\f\n" + body, encoding="utf-8")
    return len(by_name)


def public_names():
    head = (ROOT / "include/mcc/up.h").read_text(encoding="utf-8")
    lib = (ROOT / "include/mcc/up_lib.h").read_text(encoding="utf-8")
    names = set(re.findall(r"^#define (UP[AM]_\w+)\s", head, re.M))
    names |= set(re.findall(r"^[A-Za-z_][\w\s*]*?\b(UP_\w+)\(", lib, re.M))
    return names


def check():
    documented = {e for _, e, _ in blocks()}
    names = public_names()
    missing = sorted(names - documented)
    for n in missing:
        print("[ERROR] no AutoDoc entry for %s" % n)
    print("[%s] %d of %d public names documented" % ("ERROR" if missing else "OK", len(names) - len(missing), len(names)))
    return 1 if missing else 0


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "extract":
        print("%d files written" % extract(sys.argv[2]))
    elif len(sys.argv) == 2 and sys.argv[1] == "check":
        sys.exit(check())
    else:
        sys.exit(__doc__)
