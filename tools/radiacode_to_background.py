#!/usr/bin/env python3
"""
Convert a background spectrum exported by the official RadiaCode software
into the background-reference CSV this firmware reads from the SD card.

Why convert on a PC rather than on the device: the RadiaCode export is XML,
and parsing XML on the ESP32 would be both heavy and brittle. The firmware
reads a trivial `channel,cps` CSV instead, and the files are already on a PC.

Usage
-----
    python radiacode_to_background.py MySpectrum.xml --slot HOME
    python radiacode_to_background.py *.xml --slot CAR --out E:/background

Then copy the resulting <SLOT>.csv into /background/ on the SD card, and on
the device: Data screen -> BKG LIBRARY -> MODE: LOAD -> tap the slot.

Valid slots (fixed, because the device has no keyboard):
    HOME  CAR  BASEMENT  GARDEN  FIELD  SPARE
"""

import argparse
import glob
import os
import sys
import xml.etree.ElementTree as ET

CHANNELS = 1024
SLOTS = ["HOME", "CAR", "BASEMENT", "GARDEN", "FIELD", "SPARE"]

# The device's own calibration, for a sanity check. A stored background is
# only meaningful if its channels map to the same energies as the live
# spectrum, so a mismatch here means the reference will be subtracted from
# the wrong places.
DEVICE_CALIB = (3.839503, 2.375420, 0.000364)


def localname(tag):
    """Strip any XML namespace so tag matching is namespace-agnostic."""
    return tag.rsplit("}", 1)[-1]


def find_first(root, name):
    for el in root.iter():
        if localname(el.tag) == name:
            return el
    return None


def find_all(root, name):
    return [el for el in root.iter() if localname(el.tag) == name]


def parse_spectrum(path, prefer_background):
    """Returns (counts, duration_seconds, coefficients)."""
    tree = ET.parse(path)
    root = tree.getroot()

    # RadiaCode exports can carry both the measurement and a separately
    # recorded background in one file.
    wanted = "BackgroundEnergySpectrum" if prefer_background else "EnergySpectrum"
    spec_parent = find_first(root, wanted)
    if spec_parent is None:
        if prefer_background:
            raise ValueError(
                "no <BackgroundEnergySpectrum> in this file - omit --from-background-element"
            )
        raise ValueError("no <EnergySpectrum> found - is this a RadiaCode export?")

    # Duration. MeasurementTime is seconds in the exports seen so far.
    duration = None
    for name in ("MeasurementTime", "RealTime", "LiveTime"):
        el = find_first(spec_parent, name)
        if el is not None and el.text and el.text.strip():
            try:
                duration = float(el.text.strip())
                break
            except ValueError:
                pass
    if duration is None or duration <= 0:
        raise ValueError("could not read a positive MeasurementTime")

    coeffs = [float(c.text) for c in find_all(spec_parent, "Coefficient") if c.text]

    points = find_all(spec_parent, "DataPoint")
    counts = []
    for p in points:
        if p.text is None:
            continue
        try:
            counts.append(int(float(p.text.strip())))
        except ValueError:
            pass

    if not counts:
        raise ValueError("no <DataPoint> values found")

    return counts, duration, coeffs


def write_reference(counts, duration, out_path):
    if len(counts) != CHANNELS:
        # Rebinning would silently distort the energy mapping, so refuse
        # rather than produce a reference that quietly subtracts the wrong
        # channels.
        raise ValueError(
            f"expected {CHANNELS} channels, got {len(counts)} - this firmware "
            f"assumes the RC-110's native 1024-channel spectrum"
        )

    total = sum(counts)
    with open(out_path, "w", newline="\n") as f:
        f.write("# background reference\n")
        f.write(f"# duration_s,{int(round(duration))}\n")
        f.write(f"# total_counts,{total}\n")
        f.write("channel,cps\n")
        for ch, c in enumerate(counts):
            f.write(f"{ch},{c / duration:.4f}\n")
    return total


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="+", help="RadiaCode XML export(s)")
    ap.add_argument("--slot", default="HOME",
                    help="target slot name (default HOME). One file per slot.")
    ap.add_argument("--out", default=".",
                    help="output directory - point this at <SD>/background")
    ap.add_argument("--from-background-element", action="store_true",
                    help="read <BackgroundEnergySpectrum> instead of <EnergySpectrum>")
    args = ap.parse_args()

    slot = args.slot.upper()
    if slot not in SLOTS:
        print(f"warning: '{slot}' is not one of the device's slots "
              f"({', '.join(SLOTS)}); it will not be loadable from the UI.",
              file=sys.stderr)

    paths = []
    for pattern in args.files:
        paths.extend(glob.glob(pattern))
    if not paths:
        print("no input files matched", file=sys.stderr)
        return 1

    if len(paths) > 1:
        print(f"note: {len(paths)} files matched but a slot holds one reference; "
              f"only the first ({os.path.basename(paths[0])}) will be written.",
              file=sys.stderr)

    path = paths[0]
    try:
        counts, duration, coeffs = parse_spectrum(path, args.from_background_element)
    except Exception as e:
        print(f"{os.path.basename(path)}: {e}", file=sys.stderr)
        return 1

    os.makedirs(args.out, exist_ok=True)
    out_path = os.path.join(args.out, f"{slot}.csv")
    try:
        total = write_reference(counts, duration, out_path)
    except Exception as e:
        print(f"{os.path.basename(path)}: {e}", file=sys.stderr)
        return 1

    print(f"{os.path.basename(path)} -> {out_path}")
    print(f"  {len(counts)} channels, {duration:.0f} s, {total} counts, "
          f"{total / duration:.2f} cps")

    if coeffs:
        shown = ", ".join(f"{c:g}" for c in coeffs[:3])
        print(f"  calibration: {shown}")
        if len(coeffs) >= 3:
            drift = [abs(coeffs[i] - DEVICE_CALIB[i]) for i in range(3)]
            # a1 dominates the energy mapping; ~1% of it is a few keV at the
            # top of the range, which is well inside one FWHM.
            if drift[1] > DEVICE_CALIB[1] * 0.01:
                print("  WARNING: calibration differs noticeably from this device's "
                      f"({', '.join(f'{c:g}' for c in DEVICE_CALIB)}).")
                print("  The reference channels will not line up in energy; "
                      "subtracting it would remove counts from the wrong places.")

    if duration < 60:
        print("  WARNING: shorter than a minute - a noisy reference adds noise "
              "to every measurement it is subtracted from.")

    print(f"\nCopy {out_path} to <SD>/background/{slot}.csv, then on the device:")
    print("  Data screen -> BKG LIBRARY -> MODE: LOAD -> tap " + slot)
    return 0


if __name__ == "__main__":
    sys.exit(main())
