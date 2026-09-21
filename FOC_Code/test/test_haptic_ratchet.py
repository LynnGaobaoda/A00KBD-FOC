# -*- coding: utf-8 -*-
"""PC self-test for integer haptic ratchet (same units as firmware).

Slow-rotates a virtual shaft, records uq/err/snap, and checks detent
count plus torque smoothness. Tune constants here and in HapticRatchet.c
together.
"""
from __future__ import print_function
import os
import sys

FOC_2PI = 6283
DETENTS = 12
WIDTH = FOC_2PI // DETENTS
SNAP = WIDTH * 13 // 20
DEAD = 2 * FOC_2PI // 360
KP = 3200
UQ_LIM = 1100
LPF_N = 0
LPF_D = 1
SLEW_P = 1
SLEW_N = 2


def clamp(x, lo, hi):
    return hi if x > hi else lo if x < lo else x


class Ratchet(object):
    def __init__(self, pos, snap=SNAP, ge=False, lock_n=40):
        self.center = pos
        self.index = 0
        self.pos_f = pos
        self.uq_prev = 0
        self.snap_th = snap
        self.ge = ge
        self.lock_n = lock_n
        self.lock = 0

    def step(self, pos_now):
        snap = 0
        if LPF_D <= 1:
            self.pos_f = pos_now
        else:
            self.pos_f = (self.pos_f * LPF_N + pos_now) // LPF_D
        err = self.pos_f - self.center
        if self.lock > 0:
            self.lock -= 1
            hit_pos = False
            hit_neg = False
        else:
            hit_pos = (err >= self.snap_th) if self.ge else (err > self.snap_th)
            hit_neg = (err <= -self.snap_th) if self.ge else (err < -self.snap_th)
        if hit_pos:
            self.center += WIDTH
            err -= WIDTH
            self.index -= 1
            snap = 1
            self.lock = self.lock_n
        elif hit_neg:
            self.center -= WIDTH
            err += WIDTH
            self.index += 1
            snap = 1
            self.lock = self.lock_n
        if -DEAD <= err <= DEAD:
            inp = 0
        elif err > 0:
            inp = -(err - DEAD)
        else:
            inp = -(err + DEAD)
        uq = clamp(KP * inp // 1000, -UQ_LIM, UQ_LIM)
        uq = (self.uq_prev * SLEW_P + uq) // SLEW_N if SLEW_N else uq
        self.uq_prev = uq
        return err, uq, snap, self.index


def slow_rotate(turns=2, step_mrad=8, hr=None):
    if hr is None:
        hr = Ratchet(0)
    rows = []
    pos = 0
    end = turns * FOC_2PI
    while pos <= end:
        err, uq, snap, idx = hr.step(pos)
        rows.append((pos, err, uq, snap, idx))
        pos += step_mrad
    return rows


def metrics(rows):
    snaps = sum(r[3] for r in rows)
    uqs = [r[2] for r in rows]
    duq = [abs(uqs[i] - uqs[i - 1]) for i in range(1, len(rows))]
    duq_nonsnap = []
    hold = 0
    for i in range(1, len(rows)):
        if rows[i][3] or rows[i - 1][3]:
            hold = 6
        if hold > 0:
            hold -= 1
            continue
        duq_nonsnap.append(abs(rows[i][2] - rows[i - 1][2]))
    sat = sum(1 for u in uqs if abs(u) >= UQ_LIM) * 100.0 / len(uqs)
    return {
        "n": len(rows),
        "snaps": snaps,
        "uq_min": min(uqs),
        "uq_max": max(uqs),
        "max_duq": max(duq) if duq else 0,
        "max_duq_nonsnap": max(duq_nonsnap) if duq_nonsnap else 0,
        "sat_pct": sat,
    }


def main():
    rows = slow_rotate(2, 8)
    m = metrics(rows)
    expect_snaps = DETENTS * 2
    print("Haptic slow-rotate self-test")
    print("samples=%(n)d snaps=%(snaps)d uq=[%(uq_min)d,%(uq_max)d] "
          "max_duq=%(max_duq)d max_duq_nonsnap=%(max_duq_nonsnap)d "
          "sat%%=%(sat_pct).1f" % m)
    bad = []
    if m["snaps"] != expect_snaps:
        bad.append("snap count %d != %d" % (m["snaps"], expect_snaps))
    if m["max_duq_nonsnap"] > 80:
        bad.append("non-snap uq jump %d > 80" % m["max_duq_nonsnap"])
    if m["sat_pct"] > 35.0:
        bad.append("uq saturated %.1f%% of travel" % m["sat_pct"])
    if m["uq_max"] <= 200 or m["uq_min"] >= -200:
        bad.append("detent pull too weak")
    chatter = 0
    hr_bug = Ratchet(0, snap=WIDTH // 2, ge=True, lock_n=0)
    th = WIDTH // 2
    for i in range(40):
        chatter += hr_bug.step(th + (1 if (i % 2) == 0 else -1))[2]
    print("chatter_at_50pct_ge_boundary snaps=%d / 40 (board saw 34 vs index 6)" % chatter)
    if chatter < 20:
        bad.append("failed to reproduce on-target chatter (snaps=%d)" % chatter)
    hold = 0
    hr_ok = Ratchet(0)
    for i in range(40):
        hold += hr_ok.step(SNAP + (1 if (i % 2) == 0 else -1))[2]
    print("hold_at_55pct_gt_boundary snaps=%d / 40 (expect <= 1)" % hold)
    if hold > 1:
        bad.append("55%% hysteresis still chatters at boundary (snaps=%d)" % hold)
    out = os.path.join(os.path.dirname(__file__), "haptic_slow_rotate.csv")
    with open(out, "w") as f:
        f.write("pos,err,uq,snap,index\n")
        for r in rows:
            f.write("%d,%d,%d,%d,%d\n" % r)
    print("wrote %s" % out)
    if bad:
        print("FAIL: " + "; ".join(bad))
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
