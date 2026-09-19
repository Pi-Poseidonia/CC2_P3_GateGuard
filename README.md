# CC2_P3_GateGuard

**GateGuard** is an automated License Plate Recognition (LPR) system for garage access control, built in **C++** using **openFrameworks**. It detects a vehicle's license plate from a photo, reads the plate text using OCR, and checks it against an authorized list to decide whether to grant access. The system supports both **European Union (EU)** plates and **Indian High Security Registration Plates (HSRP)** through a pluggable Strategy-pattern architecture.

---

## Features

- **Dual-region plate detection** — separate, independently-tuned detection strategies for EU and Indian plate formats, selectable via the UI or keyboard shortcuts.
- **Robust detection** — BFS flood-fill connected-component analysis distinguishes a real plate from reflections, glare, and background noise.
- **OCR via Tesseract** — plate text is read using Tesseract's LSTM-based recognition engine, configured specifically for plate-style text (single-line mode, uppercase/digit whitelist).
- **Fuzzy access matching** — recognized text is compared against an authorized plate list using Levenshtein edit distance, tolerating small, realistic OCR errors without weakening access control.
- **Polymorphic logging** — every access decision is dispatched to multiple logging channels (console, simulated email alert) through a shared logger interface.
- **Flexible image input** — load a test image via the native file dialog, drag-and-drop a file onto the window, or preload a fixed test set.

---

## Architecture

Plate detection is built around the **Strategy pattern**:

```
PlateDetectionStrategy (abstract)
        ├── EUDetectionStrategy
        └── IndianDetectionStrategy
```

`PlateDetector` holds a `std::shared_ptr<PlateDetectionStrategy>` and delegates every detection request to whichever concrete strategy is active, without needing to know which one it is.

**Data flow:**

```
ofApp  →  PlateDetector  →  active Strategy  →  LicensePlate (result)
       →  TesseractPlateReader  →  GateAccessController
       →  AccessDecision  →  AccessLog + registered loggers
```

**Key classes:**

| Class | Responsibility |
|---|---|
| `EUDetectionStrategy` / `IndianDetectionStrategy` | Region-specific plate detection |
| `PlateDetector` | Strategy context; orchestrates detection + OCR |
| `TesseractPlateReader` | Wraps Tesseract's C++ API for OCR |
| `GateAccessController` | Fuzzy-matches OCR text against the authorized list |
| `AccessLog` | Records every access decision |
| `AccessLogger` (abstract) → `ConsoleAccessLogger`, `EmailSecurityAlert` | Polymorphic notification on each decision |
| `GarageUI` | Gate status indicator + strategy toggle buttons |
| `PlateDisplayer` | Renders the detection overlay, OCR crop, and result text |
| `StartScreen` | Welcome screen and image-import entry point |

**Libraries:** openFrameworks (windowing, graphics, image I/O) · Tesseract OCR + Leptonica, via vcpkg · C++ Standard Library.

---

## Getting Started

### Prerequisites
- Visual Studio 2022 (or later) with the C++ desktop development workload
- [openFrameworks](https://openframeworks.cc/download/) (v0.12.x)
- [vcpkg](https://github.com/microsoft/vcpkg)

### Setup

1. **Install Tesseract via vcpkg:**
   ```powershell
   vcpkg install tesseract:x64-windows
   vcpkg integrate install
   ```

2. **Add the English trained-data file:**
   Download [`eng.traineddata`](https://github.com/tesseract-ocr/tessdata/raw/main/eng.traineddata) and place it at:
   ```
   bin/tessdata/eng.traineddata
   ```
   > Note: `bin/tessdata/`, **not** `bin/data/tessdata/`.

3. **Set up the authorized plate list** at `bin/data/users.csv`, one plate per line, no header row:
   ```
   PLATE,OwnerName
   BRL702,Max Mueller
   HR26DK8337,Anaya Saha
   ```

4. **Clone this repository** and open `GateGuard.sln` in Visual Studio.

5. **Build in Release configuration.** (Tesseract's pre-built vcpkg libraries can crash in Debug mode due to a C-runtime mismatch — see [Known Issues](#known-issues).)

6. **Run** the application.

---

## Usage

| Action | Input |
|---|---|
| Switch to EU detection mode | `1`, or click the **EU** button |
| Switch to Indian detection mode | `2`, or click the **Indian** button |
| Run both strategies simultaneously | `3` |
| Cycle test images | `←` / `→` |
| Import an image | Click **Browse & Import** on the start screen, or drag-and-drop a file onto the window |

The interface displays the source image with a bounding-box overlay around the detected plate, the binarized OCR crop, the recognized text, and a **GATE OPEN** / **GATE CLOSED** indicator based on the access decision.

---

## Known Issues

- **Debug build crash on Tesseract init** — caused by a Debug/Release C-runtime mismatch with vcpkg's pre-built library. Build in **Release** configuration.
- **`tessdata` path resolution** — must resolve relative to the executable's own directory (`ofFilePath::getCurrentExeDir()`), not the debugger's assumed working directory.
- **Strategy toggle buttons vs. active mode** — the on-screen EU/Indian buttons and the keyboard shortcuts do not currently share a single state; confirm the active mode via the `Mode:` label in the status bar.
- **Secondary marker strips** — some plates (e.g. certain Italian regional plates) carry a second marker strip on the right edge that is not yet trimmed before OCR, which can introduce extra characters.
- **`IndianDetectionStrategy`** can fail to detect a plate under strong glare, dirt, or an off-angle photo.

---

## Future Work

- Unify the strategy toggle buttons and keyboard shortcuts into a single shared state.
- Extend strip/marker trimming to handle secondary marker strips on either edge of the plate.
- Improve `IndianDetectionStrategy`'s robustness to glare, dirt, and off-angle photos.
- Integrate live camera capture via `ofVideoGrabber` for real-time gate operation.
- Move access history from in-memory logging to persistent storage.
- Extend the Strategy pattern to additional regions/plate formats.

---
