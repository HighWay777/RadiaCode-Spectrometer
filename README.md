# RadiaCode Spectrometer Display

A standalone touchscreen companion for [RadiaCode](https://www.radiacode.com/)
gamma spectrometers. An ESP32-S3 board with a 4.3" display connects to the
detector over Bluetooth LE, analyses the live gamma spectrum on-device, and
tells you in plain language what it thinks you are looking at — no phone, no
app, no cloud.

![Isotope candidates screen](docs/screen-isotopes.jpg)

## What it does

- **Connects over BLE** to RadiaCode-102 / 103 / 103G / 110 and reads the live
  1024-channel spectrum, dose rate and count rate.
- **Identifies nuclides on-device** with a classical peak-search and
  energy-library match, tuned to what a scintillator can actually resolve.
- **Answers the real question** — "URANIUM ORE", "THORIUM", "BACKGROUND" —
  rather than only listing daughter nuclides.
- **Tells you how long to keep measuring.** Peak significance grows as √time,
  so the device extrapolates from your current statistics and says
  *"keep measuring ~4m 30s"* instead of leaving you guessing.
- **Background subtraction** with named references per location, so weak
  samples can be separated from room radon and building-material K-40.
- **Keeps measuring through interruptions.** The detector accumulates
  internally, so a dropped BLE link, a disconnect, or a reboot of the display
  can all resume the same run. Enable **auto-reconnect** in Settings — it is
  off by default so a new device never connects to anything unasked.
- **Logs to microSD** — CSV spectra with calibration coefficients, ready for
  offline analysis in [Gamma-MCA](https://github.com/OpenGammaProject/Gamma-MCA).

## What it does not do

Being honest about the physics matters more than a longer feature list.

The RadiaCode-110 measures about **8.4% FWHM at 662 keV**. Gamma lines closer
together than roughly 60–100 keV blur into a single bump. Strong, isolated
sources (Cs-137, Co-60, Am-241, K-40, and the natural uranium and thorium
chains) are identifiable. **Weak mixtures are not reliably separable**, and no
amount of software changes that. Treat the output as *ranked probable
candidates*, not a laboratory result.

Near background level, results depend entirely on having recorded a background
reference — ambient air genuinely contains radon daughters (real U-238 chain
members) and walls genuinely contain K-40.

## Hardware

| Part | Notes |
|---|---|
| **Guition JC4827W543** ESP32-S3 board | 4.3" 480×272 IPS, NV3041A over QSPI, GT911 capacitive touch, 4 MB flash / 8 MB PSRAM. The **capacitive** variant — the resistive one wires its touch controller to the SD card's SPI pins. |
| **A RadiaCode detector** | 102 / 103 / 103G / 110. Developed and tested against an RC-110. |
| microSD card | Optional. **Must be FAT32** — Windows formats cards over 32 GB as exFAT, which the ESP32 cannot read. The device can reformat a card for you. |
| USB-C cable | Power and flashing. |

Nothing needs soldering; the board is used as sold.

## Flashing

Built with [PlatformIO](https://platformio.org/).

```bash
git clone https://git.vhelectronics.com/highway/Radiacode110.git
cd Radiacode110/RadiaCode-Spectrometer
pio run -t upload
```

To watch the diagnostic log:

```bash
pio device monitor
```

The project uses a **custom partition table** (`partitions.csv`) giving two
1.9 MB OTA slots instead of the stock 1.31 MB, reclaimed from the unused
SPIFFS region. Both app slots are required for over-the-air updates.

## First run

On first power-up the device shows a **setup screen**:

1. Tap **WiFi network (SSID)** and type it on the on-screen keyboard.
2. Tap **WiFi password** and enter it.
3. Tap **DONE**.

WiFi is optional and only used for firmware updates — the spectrometer works
fully without it. Credentials are stored in the ESP32's NVS flash and are
never part of the source or the firmware image.

Setup does not reappear on later boots. It stays reachable as the **last
screen** in the swipe cycle.

## Using it

Swipe left/right to move between seven screens:

| Screen | Purpose |
|---|---|
| **Home** | Dose rate, count rate, sample verdict, device status, connect |
| **Isotopes** | Ranked candidate nuclides; tap one for its full line list |
| **Spectrum** | Live chart, log/linear (tap to toggle), marked peaks |
| **Analysis** | Verdict, confidence in σ, chain evidence, how long to keep measuring |
| **Background** | Record / enable / manage the background reference |
| **Data** | SD card status, CSV export, card formatting |
| **Settings** | WiFi, auto-reconnect, updates, brightness, factory reset |

### Settings and their defaults

| Setting | Default | Notes |
|---|---|---|
| Auto-reconnect / resume after reboot | **off** | Resumes an interrupted run. Off out of the box so nothing connects unasked. |
| Connect to detector at startup | **off** | Links as soon as the board powers up. |
| Check for firmware updates at startup | **off** | Needs WiFi; the row stays disabled until a network is set. |
| Display brightness | 78% | Tap to cycle. |

All of these persist in NVS flash until a factory reset.

### A typical measurement

1. **Home → CONNECT.**
2. Optionally record a background: **Background → START RECORDING** with
   nothing near the detector, 10–60 minutes, then **STOP & SAVE**.
3. Place your sample and tap **RESET** on Home to start a fresh run.
4. Watch **Analysis** — it tells you when the result is statistically solid.

Longer is better, but with diminishing returns: significance grows as √time,
so quadrupling the time only doubles your confidence.

## Firmware updates

**Settings → Firmware / updates**:

- **CHECK NOW** queries the latest release from this repository.
- If a newer version exists the row becomes **INSTALL x.y.z**; tapping it
  downloads and flashes it, then reboots.
- **Check for firmware updates at startup** does this automatically.

Updates are written to the *inactive* OTA slot — the running firmware is never
overwritten, so a failed or interrupted download leaves the device working on
the current version. Requires WiFi.

## Factory reset

**Settings → Factory defaults** (tap twice to confirm) erases WiFi
credentials, all preferences and the stored background reference, then
reboots into first-run setup. It does not touch files on the SD card.

## The microSD card

Entirely optional — measurement and identification never depend on it.

| Path | Contents |
|---|---|
| `/spectra/` | Exported CSV spectra, plus a 5-minute autosave checkpoint |
| `/background/` | Named background references (HOME, CAR, BASEMENT…) |
| `/import/` | Drop RadiaCode `.xml` exports here to import as backgrounds |
| `/isotopes.csv` | Optional replacement nuclide library |

### Bringing in RadiaCode app backgrounds

Copy XML exports from the official RadiaCode software into `/import`, then
**Data → BKG LIBRARY → IMPORT XML**. Pick a target slot and tap the file; it
is converted and activated in one step.

### Extending the nuclide library

On first run with a card the device writes `/isotopes_template.csv` containing
its built-in table. Rename it to `isotopes.csv`, add nuclides, and reboot —
up to 300 nuclides. One row per gamma line:

```csv
symbol,name,category,chain,energy_keV,intensity
Bi-214,Bismuth-214,natural,U238,609.31,0.4549
Cs-137,Caesium-137,artificial,none,661.66,0.851
```

`category` is `natural|artificial|medical|calibration`, `chain` is
`U238|Th232|none`, and `intensity` is **absolute** emission probability per
decay (0–1), not relative.

## How identification works

1. Background reference (if any) is subtracted.
2. The spectrum is smoothed with a resolution-aware window — a peak is ~9
   channels wide at 100 keV but ~46 at 2614 keV.
3. A continuum is estimated from side-bands either side of each peak.
4. Peaks are found on the *net* spectrum and ranked by statistical
   significance, `net / √(sample + background + continuum)`.
5. Lines are matched within ±0.6 FWHM, weighted by **detectability**
   (emission × detector efficiency) rather than raw emission — a small CsI
   crystal sees Tl-208's 583 keV line far more readily than its 2614 keV one.
6. Each peak is awarded to the nuclide that best explains it, so one peak
   cannot confirm several nuclides at once.
7. Decay-chain members are combined: uranium is identified through Bi-214,
   Pb-214 and Ra-226, because U-238 itself emits essentially no detectable
   gammas.

## Safety note

This is a hobbyist instrument, not a certified radiation safety device. Do not
rely on it for occupational or medical decisions. If you collect radioactive
minerals, the practical hazards are dust and radon rather than external dose:
don't grind specimens, wash your hands, and store them ventilated.

## Credits

- [mkgeiger/RadiaCode](https://github.com/mkgeiger/RadiaCode) — Arduino BLE
  library for RadiaCode devices (MIT), vendored with documented patches.
- [cdump/radiacode](https://github.com/cdump/radiacode) — the reverse-engineered
  protocol everything else builds on.
- [moononournation/Arduino_GFX](https://github.com/moononournation/Arduino_GFX)
  — display driver, vendored with an NV3041A QSPI fix.
- Nuclear data follows published NNDC/ENSDF values.

RadiaCode publishes no official API; all BLE integration is community
reverse-engineering. Not affiliated with or endorsed by RadiaCode.

## License

MIT — see [LICENSE](LICENSE).
