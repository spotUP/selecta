#!/usr/bin/env python3
"""Measure each Amiga replayer's RAM peak for its SEL_RP_MEMORY manifest tag (I1313).

The registry hides a player whose SEL_RP_MEMORY is above the RAM free after Selecta has started
(SEL_MACHINE_FREE_RAM). This script produces the measured peak each manifest declares.

Method (vamos, Amiga-built players):
  selecta-cli render <song> --seconds 2 under `vamos -l mem_alloc:info`. Every allocation line carries
  the free memory left ("(free XXXXXX #n)"); the lowest value is the run's peak use. A baseline run
  renders a file no player claims, so no player loads. Player peak = baseline min free - song min free
  (module hunk plus every allocation the player makes, including the decoder's working buffers).
  Declared value = peak * 1.25, rounded up to 64 KB.

Method (host, players vamos cannot run, e.g. FPU code under vamos -C 68020):
  host selecta-cli render with the count_malloc shim (tests/m0/count_malloc.c, DYLD_INSERT_LIBRARIES,
  SEL_ALLOC_PEAK_BYTES=...). Peak = song peak - baseline peak. Host sizes are 64-bit, so the value is an
  upper bound for the 32-bit Amiga build.

Results go to a TSV (one row per player, written as each run finishes); rows already in the TSV are
skipped, so a rerun resumes. --only <player> reruns one row.

Usage:
  tools/player_ram.py --amiga-bin <dir with selecta-cli and Players/> --songs <spec file> --out <tsv>
      [--host-cli <host selecta-cli> --shim <libcount_malloc.dylib>] [--only NAME] [--ram-kb 16384]
Spec file lines: <player> <vamos|host> <song path>
"""
import argparse
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile

FREE_RE = re.compile(r"\(free ([0-9a-f]+) #")
PEAK_RE = re.compile(r"SEL_ALLOC_PEAK_BYTES=(\d+)")
GRAIN = 65536


def declared(peak):
    return int(math.ceil(peak * 1.25 / GRAIN) * GRAIN)


def vamos_min_free(binary_dir, song, ram_kb):
    work = tempfile.mkdtemp(prefix="player_ram_")
    try:
        os.mkdir(os.path.join(work, "ram"))
        songs = os.path.join(work, "songs")
        os.mkdir(songs)
        name = os.path.basename(song)
        shutil.copy(song, os.path.join(songs, name))
        cmd = ["nice", "-n", "19", "vamos", "-C", "68020", "-m", str(ram_kb), "-H", "disable",
               "-l", "mem_alloc:info", "-V", "selbin:" + binary_dir, "-a", "PROGDIR:selbin:",
               "-V", "ram:" + os.path.join(work, "ram"), "-V", "songs:" + songs, "--",
               os.path.join(binary_dir, "selecta-cli"), "render", "songs:" + name,
               "--output", "ram:o.wav", "--seconds", "2"]
        out = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                             errors="replace").stdout
    finally:
        shutil.rmtree(work, ignore_errors=True)
    frees = [int(m.group(1), 16) for m in FREE_RE.finditer(out)]
    if not frees:
        raise RuntimeError("no mem_alloc lines for " + song)
    tail = [l for l in out.splitlines() if "replayer=" in l or "error" in l.lower()]
    return min(frees), (tail[-1].strip() if tail else "")


def host_peak(cli, shim, song):
    env = dict(os.environ, DYLD_INSERT_LIBRARIES=shim, SEL_ALLOC_PEAK_AT_EXIT="1")
    work = tempfile.mkdtemp(prefix="player_ram_")
    try:
        out = subprocess.run([cli, "render", song, "--output", os.path.join(work, "o.wav"),
                              "--seconds", "2"], env=env, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True, errors="replace").stdout
    finally:
        shutil.rmtree(work, ignore_errors=True)
    m = PEAK_RE.search(out)
    if not m:
        raise RuntimeError("no SEL_ALLOC_PEAK_BYTES for " + song)
    tail = [l for l in out.splitlines() if "replayer=" in l]
    return int(m.group(1)), (tail[-1].strip() if tail else "")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--amiga-bin", required=True)
    ap.add_argument("--songs", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--host-cli")
    ap.add_argument("--shim")
    ap.add_argument("--only")
    ap.add_argument("--ram-kb", type=int, default=16384)
    a = ap.parse_args()

    done = {}
    if os.path.exists(a.out):
        for line in open(a.out):
            f = line.rstrip("\n").split("\t")
            if f and f[0] != "player":
                done[f[0]] = f
    if a.only:
        done.pop(a.only, None)
    spec = [l.split(None, 2) for l in open(a.songs) if l.strip() and not l.startswith("#")]
    rows = dict(done)

    base = {}
    def baseline(kind):
        if kind not in base:
            junk = os.path.join(tempfile.mkdtemp(prefix="player_ram_"), "baseline.zzz")
            open(junk, "wb").write(b"\0" * 4096)
            base[kind] = (vamos_min_free(a.amiga_bin, junk, a.ram_kb)[0] if kind == "vamos"
                          else host_peak(a.host_cli, a.shim, junk)[0])
        return base[kind]

    for player, kind, song in spec:
        song = song.strip()
        if player in rows or (a.only and player != a.only):
            continue
        if kind == "vamos":
            low, note = vamos_min_free(a.amiga_bin, song, a.ram_kb)
            peak = baseline("vamos") - low
        else:
            high, note = host_peak(a.host_cli, a.shim, song)
            peak = high - baseline("host")
        rows[player] = [player, kind, os.path.basename(song), str(peak), str(declared(peak)), note]
        with open(a.out, "w") as f:
            f.write("player\tmethod\tsong\tpeak_bytes\tdeclared_bytes\tlast_line\n")
            for r in rows.values():
                f.write("\t".join(r) + "\n")
        print("\t".join(rows[player][:5]), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
