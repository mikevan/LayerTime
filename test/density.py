"""Measures Michael's unit test density: for every executable line, how many
DISTINCT test cases exercised it, against how many decisions gate reaching it.

Coverage engine is gcov (built into g++). Nothing else is installed.
The per-case isolation and the depth denominator are custom, because no
off-the-shelf tool computes this metric.
"""
import subprocess, glob, os, re, collections

CASES = [l.split("(")[1].split(")")[0]
         for l in open("test_geogrid.cpp") if l.strip().startswith("CASE(")]

SRC = "GeoGrid.cpp"

def hits_for(case):
    [os.remove(f) for f in glob.glob("*.gcda")]
    subprocess.run(["./test_cov", case], capture_output=True)
    subprocess.run(["gcov","-o",".","GeoGrid.gcno"], capture_output=True)
    got = set()
    for line in open(SRC + ".gcov"):
        parts = line.split(":", 2)
        if len(parts) < 3: continue
        cnt, num = parts[0].strip(), parts[1].strip()
        if cnt in ("-", "#####", "$$$$$") or not num.isdigit(): continue
        cnt = cnt.rstrip("*")
        if cnt.isdigit() and int(cnt) > 0: got.add(int(num))
    return got

density = collections.Counter()
for c in CASES:
    for ln in hits_for(c): density[ln] += 1

# Denominator: decisions that gate each line. Brace-depth walker over control
# keywords, plus short-circuit operands counted separately the way MC/DC does.
# Crude next to a clang AST walk, and honest about it.
depth, out = {}, []
lvl = 0
stack = []
for i, raw in enumerate(open(SRC, encoding="utf-8", errors="replace"), 1):
    line = raw.split("//")[0]
    opens = line.count("{"); closes = line.count("}")
    ctrl = bool(re.search(r'\b(if|else if|for|while|switch|case)\b', line))
    extra = line.count("&&") + line.count("||") + line.count("?")
    depth[i] = lvl + (1 if ctrl else 0) + extra
    while closes and stack:
        stack.pop(); lvl = max(0, lvl - 1); closes -= 1
    for _ in range(opens):
        stack.append(ctrl); lvl += 1 if ctrl else 0

covered = [l for l in density if density[l] > 0]
print(f"cases run in isolation: {len(CASES)}")
print(f"executable lines covered in {SRC}: {len(covered)}")
print()
print("lines where DENSITY < DECISION DEPTH (your bar):")
print(f"{'line':>5} {'density':>8} {'depth':>6}   source")
short = 0
for ln in sorted(covered):
    d, k = density[ln], depth.get(ln, 0)
    if d < k:
        short += 1
        src = open(SRC, encoding="utf-8", errors="replace").readlines()[ln-1].rstrip()[:70]
        print(f"{ln:>5} {d:>8} {k:>6}   {src.strip()}")
print()
print(f"{short} lines below bar, {len(covered)-short} at or above")
uncov = []
for line in open(SRC + ".gcov"):
    p = line.split(":", 2)
    if len(p) >= 3 and p[0].strip() == "#####":
        uncov.append(int(p[1].strip()))
print(f"lines never executed by any test: {len(uncov)}" + (f" -> {uncov}" if uncov else ""))
