#!/usr/bin/env python3
"""Generate synthetic RC-110 spectra in the device's own CSV export format.

Purpose: let the analysis be developed and regression-tested before real
captures exist, and give exact ground truth that a real sample can never
provide (you know precisely what went in).

DELIBERATELY INDEPENDENT of the firmware. The line energies and intensities
below come from published NNDC/ENSDF values, and the detector response is
modelled here from scratch - photopeak, Compton continuum, backscatter. It
does NOT import or mirror relEfficiency()/fwhmKeVAt() from peak_search.cpp.

That matters: analysis_selftest.cpp generates its spectra with an efficiency()
that mirrors the analyser's own relEfficiency(), so it cannot fail on a
physics assumption the two share. Anything agreeing here agrees across two
independent implementations.

Usage:
  python make_synthetic.py background  spectra/synth_background.csv 3600
  python make_synthetic.py uranium     spectra/synth_uranium.csv    600
  python make_synthetic.py thorium     spectra/synth_thorium.csv    600
  python make_synthetic.py caesium     spectra/synth_caesium.csv    300
"""

import math
import random
import sys

CHANNELS = 1024
# Real coefficients read from the user's RC-110 (serial RC-110-000983).
A0, A1, A2 = 3.839503, 2.375420, 0.000364

# 8.4% FWHM at 662 keV, measured. Scintillator resolution is photon-statistics
# limited, so FWHM scales as sqrt(E).
def fwhm_kev(e):
    return 0.084 * math.sqrt(662.0 * max(e, 20.0))

def energy_of(ch):
    return A0 + A1 * ch + A2 * ch * ch

def channel_of(e):
    # Invert the quadratic; A2 is small so a couple of Newton steps suffice.
    ch = (e - A0) / A1
    for _ in range(4):
        f = A0 + A1 * ch + A2 * ch * ch - e
        ch -= f / (A1 + 2 * A2 * ch)
    return ch

def intrinsic_efficiency(e):
    """Full-energy peak efficiency of a small CsI(Tl) crystal, normalised at
    200 keV. Photoelectric absorption falls steeply with energy; below ~80 keV
    the housing attenuates. Modelled independently of the firmware."""
    if e < 40.0:
        return 0.15
    if e < 90.0:
        return 0.15 + 0.85 * (e - 40.0) / 50.0
    return (200.0 / e) ** 1.15 if e >= 200.0 else 1.0

# --- nuclide data (NNDC/ENSDF absolute intensities per decay) ---------------
NUCLIDES = {
    # U-238 chain, in secular equilibrium
    "Bi-214": [(609.31, 0.4549), (768.36, 0.0489), (934.06, 0.0303),
               (1120.29, 0.1491), (1238.11, 0.0579), (1377.67, 0.0400),
               (1764.49, 0.1531), (2204.21, 0.0486)],
    "Pb-214": [(241.99, 0.0743), (295.22, 0.1842), (351.93, 0.3560)],
    "Ra-226": [(186.21, 0.0359)],
    "Pa-234m": [(1001.03, 0.0084), (766.36, 0.0032)],
    "Th-234": [(63.29, 0.0448), (92.38, 0.0510)],
    "U-235": [(185.72, 0.5720), (143.76, 0.1096), (163.33, 0.0508)],
    # Th-232 chain
    "Tl-208": [(583.19, 0.8500), (510.77, 0.2260), (860.56, 0.1250),
               (2614.51, 0.9975)],
    "Ac-228": [(338.32, 0.1127), (911.20, 0.2580), (968.97, 0.1580),
               (964.77, 0.0499), (794.95, 0.0425), (209.25, 0.0389)],
    "Pb-212": [(238.63, 0.4355), (300.09, 0.0318)],
    "Bi-212": [(727.33, 0.0667), (1620.50, 0.0149)],
    # Common singles
    "K-40":   [(1460.82, 0.1055)],
    "Cs-137": [(661.66, 0.8510)],
    "Co-60":  [(1173.23, 0.9985), (1332.49, 0.9998)],
    "Am-241": [(59.54, 0.3592)],
}

# Branching relative to the chain parent. Tl-208 only appears in 35.94% of
# Th-232 decays (the Bi-212 alpha branch).
BRANCH = {"Tl-208": 0.3594, "Bi-212": 0.6406, "U-235": 0.0072,
          "Pa-234m": 0.9980}

CHAINS = {
    "uranium": ["Bi-214", "Pb-214", "Ra-226", "Pa-234m", "Th-234", "U-235"],
    "thorium": ["Tl-208", "Ac-228", "Pb-212", "Bi-212"],
}


def add_line(spec, energy, area):
    """Photopeak plus the Compton continuum that produced it."""
    if area <= 0:
        return
    sigma_kev = fwhm_kev(energy) / 2.3548
    centre = channel_of(energy)
    sigma_ch = sigma_kev / A1
    lo = max(0, int(centre - 5 * sigma_ch))
    hi = min(CHANNELS - 1, int(centre + 5 * sigma_ch))
    for ch in range(lo, hi + 1):
        d = (ch - centre) / sigma_ch
        spec[ch] += area * math.exp(-0.5 * d * d) / (sigma_ch * math.sqrt(2 * math.pi))

    # Compton plateau from the edge down, plus a backscatter bump. The
    # peak-to-total ratio falls with energy for a small crystal.
    edge = energy * (1.0 - 1.0 / (1.0 + 2.0 * energy / 511.0))
    comp_total = area * min(4.0, 0.4 + energy / 500.0)
    edge_ch = int(channel_of(edge))
    if edge_ch > 2:
        per = comp_total / edge_ch
        for ch in range(2, min(edge_ch, CHANNELS)):
            # Mild rise toward the edge, as the Klein-Nishina shape gives.
            spec[ch] += per * (0.6 + 0.8 * ch / edge_ch)
    bs = energy / (1.0 + 2.0 * energy / 511.0)
    if 20.0 < bs < energy:
        add_gauss(spec, bs, comp_total * 0.12)


def add_gauss(spec, energy, area):
    sigma_ch = (fwhm_kev(energy) / 2.3548) / A1
    centre = channel_of(energy)
    lo = max(0, int(centre - 4 * sigma_ch))
    hi = min(CHANNELS - 1, int(centre + 4 * sigma_ch))
    for ch in range(lo, hi + 1):
        d = (ch - centre) / sigma_ch
        spec[ch] += area * math.exp(-0.5 * d * d) / (sigma_ch * math.sqrt(2 * math.pi))


def add_nuclide(spec, symbol, decays):
    """decays = number of parent decays; branching and efficiency applied."""
    br = BRANCH.get(symbol, 1.0)
    for energy, intensity in NUCLIDES[symbol]:
        area = decays * br * intensity * intrinsic_efficiency(energy)
        add_line(spec, energy, area)


def add_background(spec, seconds):
    """Ordinary indoor background: radon daughters in air, K-40 in the
    building, cosmic continuum. About 24 cps total, matching this detector."""
    for sym, rate in (("Bi-214", 3.3), ("Pb-214", 3.3), ("K-40", 9.6),
                      ("Tl-208", 0.6), ("Ac-228", 0.6), ("Pb-212", 0.6)):
        add_nuclide(spec, sym, rate * seconds)
    # Cosmic / scattered continuum, falling exponentially with energy.
    for ch in range(CHANNELS):
        e = energy_of(ch)
        spec[ch] += seconds * 0.060 * math.exp(-e / 480.0)
    # Low-energy X-ray fluorescence forest, which the analyser does not model.
    for ch in range(CHANNELS):
        e = energy_of(ch)
        if e < 110.0:
            spec[ch] += seconds * 0.30 * math.exp(-(e - 40.0) ** 2 / 900.0)


def poisson(mean, rng):
    if mean <= 0:
        return 0
    if mean > 60:  # normal approximation, fine this far out
        return max(0, int(rng.gauss(mean, math.sqrt(mean)) + 0.5))
    # Knuth
    l, k, p = math.exp(-mean), 0, 1.0
    while True:
        k += 1
        p *= rng.random()
        if p <= l:
            return k - 1


def build(kind, seconds, rng):
    spec = [0.0] * CHANNELS
    add_background(spec, seconds)

    if kind == "background":
        dose = 0.16
    elif kind == "uranium":
        # Ore roughly 150x background, in secular equilibrium.
        for sym in CHAINS["uranium"]:
            add_nuclide(spec, sym, 1440.0 * seconds)
        dose = 27.7
    elif kind == "thorium":
        for sym in CHAINS["thorium"]:
            add_nuclide(spec, sym, 160.0 * seconds)
        dose = 3.2
    elif kind == "caesium":
        add_nuclide(spec, "Cs-137", 120.0 * seconds)
        dose = 0.9
    elif kind == "cobalt":
        add_nuclide(spec, "Co-60", 20.0 * seconds)
        dose = 1.4
    elif kind == "americium":
        add_nuclide(spec, "Am-241", 300.0 * seconds)
        dose = 0.5
    else:
        raise SystemExit("unknown kind: " + kind)

    counts = [poisson(v, rng) for v in spec]
    return counts, dose


def write_csv(path, counts, seconds, dose):
    total = sum(counts)
    with open(path, "w", newline="\n") as f:
        f.write("# RadiaCode-110 spectrum export (SYNTHETIC)\n")
        f.write("# duration_s,%d\n" % seconds)
        f.write("# total_counts,%d\n" % total)
        f.write("# cps,%.3f\n" % (total / seconds))
        f.write("# dose_uSv_h,%.4f\n" % dose)
        f.write("# calib_a0,%.6f\n" % A0)
        f.write("# calib_a1,%.6f\n" % A1)
        f.write("# calib_a2,%.8f\n" % A2)
        f.write("# background_subtracted,0\n")
        f.write("channel,energy_keV,counts\n")
        for i, c in enumerate(counts):
            f.write("%d,%.2f,%d\n" % (i, energy_of(i), c))
    print("wrote %s: %d s, %d counts, %.1f cps" % (path, seconds, total, total / seconds))


if __name__ == "__main__":
    if len(sys.argv) < 4:
        raise SystemExit(__doc__)
    kind, path, seconds = sys.argv[1], sys.argv[2], int(sys.argv[3])
    rng = random.Random(int(sys.argv[4]) if len(sys.argv) > 4 else 12345)
    counts, dose = build(kind, seconds, rng)
    write_csv(path, counts, seconds, dose)
