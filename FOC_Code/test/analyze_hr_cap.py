# -*- coding: utf-8 -*-
"""Parse Keil hr_cap.log (BKP + 16-byte samples) and compare to PC ratchet."""
from __future__ import print_function
import os
import re
import sys

FOC_2PI = 6283
DETENTS = 12
WIDTH = FOC_2PI // DETENTS


def parse_log(path):
    text = open(path, "r").read()
    bkp = {}
    for m in re.finditer(r"DR(\d+)=([0-9A-Fa-f]+)", text):
        bkp[int(m.group(1))] = int(m.group(2), 16)
    flash = {}
    m = re.search(r"FMAGIC=([0-9A-Fa-f]+)", text)
    if m:
        flash["magic"] = int(m.group(1), 16)
    m = re.search(r"Fn=(\d+) snap=(\d+)", text)
    if m:
        flash["n"] = int(m.group(1))
        flash["snap"] = int(m.group(2))
    rows = []
    for m in re.finditer(
            r"CAP,(-?\d+),(-?\d+),(-?\d+),(-?\d+),(\d+),(\d+)", text):
        rec = {
            "pos": int(m.group(1)),
            "err": int(m.group(2)),
            "uq": int(m.group(3)),
            "index": int(m.group(4)),
            "enc": int(m.group(5)),
            "snap": int(m.group(6)),
        }
        if rec["pos"] == -1 and rec["enc"] == 65535:
            continue
        rows.append(rec)
    n = flash.get("n", len(rows))
    if n and n < len(rows):
        rows = rows[:n]
    while rows and rows[-1]["pos"] == 0 and rows[-1]["enc"] == 0 and rows[-1]["uq"] == 0:
        if len(rows) == 1:
            break
        rows.pop()
    return bkp, rows, flash


def s16(u):
    u &= 0xFFFF
    return u - 0x10000 if u >= 0x8000 else u


def analyze(bkp, rows, flash):
    print("BKP", {k: "0x%04X" % v for k, v in sorted(bkp.items())})
    print("FLASH", flash)
    magic = bkp.get(1, 0)
    snap_n = bkp.get(2, flash.get("snap", 0)) & 0xFFFF
    dpos = bkp.get(3, 0)
    index = s16(bkp.get(4, 0))
    uq_min = 0
    uq_max = 0
    if dpos > 20000:
        uq_min = s16(bkp.get(3, 0))
        uq_max = s16(bkp.get(4, 0))
        index = s16(bkp.get(5, 0))
        dpos = bkp.get(6, 0)
    print("magic=0x%04X dpos=%d snap_n=%d index=%d uq=[%d,%d] rows=%d" % (
        magic, dpos, snap_n, index, uq_min, uq_max, len(rows)))
    bad = []
    if flash.get("magic") != 0x48524331 and magic != 0xA55A:
        bad.append("no flash/BKP capture magic")
    if rows:
        pos = [r["pos"] for r in rows]
        enc = [r["enc"] for r in rows]
        uqs = [r["uq"] for r in rows]
        snaps = sum(r["snap"] for r in rows)
        denc = max(enc) - min(enc)
        print("cap pos=[%d,%d] enc=[%d,%d] denc=%d uq=[%d,%d] snaps=%d idx0=%d idx1=%d" % (
            min(pos), max(pos), min(enc), max(enc), denc,
            min(uqs), max(uqs), snaps, rows[0]["index"], rows[-1]["index"]))
        duq_ns = []
        for i in range(1, len(rows)):
            if rows[i]["snap"] or rows[i - 1]["snap"]:
                continue
            duq_ns.append(abs(rows[i]["uq"] - rows[i - 1]["uq"]))
        if duq_ns:
            print("max_duq_nonsnap=%d" % max(duq_ns))
            if max(duq_ns) > 400:
                bad.append("non-snap uq jump %d" % max(duq_ns))
        if denc < 200:
            bad.append("encoder barely moved denc=%d" % denc)
        if snaps > abs(rows[-1]["index"]) + 3:
            bad.append("series chatter snaps=%d index=%d" % (snaps, rows[-1]["index"]))
    if magic == 0xA55A and dpos < 2000 and not rows:
        print("note: short travel dpos=%d snap=%d (hold or weak crawl)" % (dpos, snap_n))
    if snap_n > abs(index) + 2:
        bad.append("chatter snap_n=%d vs |index|=%d" % (snap_n, abs(index)))
    if abs(index) < 4 and dpos >= 4000:
        bad.append("too few detents for motion index=%d" % index)
    packed = []
    for n in range(5, 11):
        if n not in bkp:
            continue
        w = bkp[n]
        eq = w & 0xFF
        if eq >= 128:
            eq -= 256
        uq8 = (w >> 8) & 0xFF
        if uq8 >= 128:
            uq8 -= 256
        packed.append((eq * 8, uq8 * 16))
    if packed:
        print("packed_err_uq", packed)
        uqs = [p[1] for p in packed]
        duq = [abs(uqs[i] - uqs[i - 1]) for i in range(1, len(uqs))]
        if duq:
            print("packed_max_duq=%d" % max(duq))
    return bad


def path_dir(argv):
    return os.path.dirname(os.path.abspath(__file__))


def write_csv(rows, dest):
    with open(dest, "w") as f:
        f.write("pos,err,uq,index,enc,snap\n")
        for r in rows:
            f.write("%d,%d,%d,%d,%d,%d\n" % (
                r["pos"], r["err"], r["uq"], r["index"], r["enc"], r["snap"]))
    print("wrote", dest)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "..", "Objects", "hr_cap.log")
    path = os.path.abspath(path)
    if not os.path.isfile(path):
        print("missing", path)
        return 2
    bkp, rows, flash = parse_log(path)
    bad = analyze(bkp, rows, flash)
    if rows:
        write_csv(rows, os.path.join(os.path.dirname(path),
                                     "haptic_on_target.csv"))
    if bad:
        print("FAIL: " + "; ".join(bad))
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
