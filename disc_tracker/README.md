# disc_tracker — standalone disc-trajectory subservice

A console binary that takes a config file, opens the referenced video,
runs a per-frame detector, gates and smooths the result, and writes the
disc's trajectory to CSV plus an optional annotated overlay video.

The binary has **no Qt dependency** — it links only OpenCV and is built
as a sibling target in this repo's CMake project.

---

## Three ways to run it

`disc_tracker` supports two detection backends and an optional
marker-anchoring layer that can ride on top of either one. Pick by
what you have available:

| | What it needs | What it does | When to use |
| --- | --- | --- | --- |
| **YOLO (fine-tuned)** | Custom-trained YOLOv8/11 ONNX | Per-frame detection of a real disc-golf disc | Production: best accuracy, no markers needed |
| **YOLO (stock COCO)** | `yolov8n.onnx` from Ultralytics | Detects nothing useful — see "Realistic expectations" | Don't — fails on disc-golf footage |
| **Hough + markers** | Pre-clicked disc markers in the Qt app | Hough-circle detection gated to a corridor along the marker spline; falls back to anchor when Hough fails | Today, no training: produces a fully populated trajectory |

The marker-anchored Hough path is the working option out of the box.
The fine-tuned YOLO path is the recommended endpoint once you have
labeled data.

---

## Realistic expectations: stock YOLOv8n + COCO does not see disc-golf discs

Empirical result on the hole-14 test clip
(`LOCAL_DATA/WhatsApp Video 2026-04-19 at 6.52.41 PM.mp4`),
no class filter, `conf_threshold=0.05`:

| frames | classes that fired | what it actually found |
|---:|---|---|
| 221 / 221 | only class 0 (`person`) | the player |
| 0 | class 29 (`frisbee`) | — |
| 0 | class 32 (`sports ball`) | — |

COCO's `frisbee` class was trained on beach-toy frisbees (rendered
horizontally on grass, bright colors) and `sports ball` on round 3D
balls. A thin, fast, motion-blurred disc-golf disc looks like neither.
**You must fine-tune** to get the YOLO backend to be useful here.

The Ultralytics one-liner once you have ~200–500 labeled frames:

```bash
pip install ultralytics
yolo train  model=yolov8n.pt data=disc.yaml epochs=80 imgsz=640
yolo export model=runs/detect/train/weights/best.pt format=onnx imgsz=640
```

Drop `best.onnx` into `LOCAL_DATA/models/` (the project convention —
all Ultralytics weights and ONNX exports live there) and point
`model_path` at it. Set `class_ids: [0]` (or whichever class indices
your dataset uses).

Stock weights / exports follow the same convention:

```
LOCAL_DATA/
└── models/
    ├── yolov8n.pt
    ├── yolov8n.onnx
    ├── yolov8n-pose.pt
    └── yolov8n-pose.onnx
```

The `LOCAL_DATA/` tree is gitignored, so models stay out of the repo.

---

## Pipeline

```
                          ┌─────────────────────────┐
video frame  ──►  detector (YOLO ONNX | Hough)      │
                          │  candidates              │
                          ▼                          │
        ┌─────────── chooseBest ───────────┐         │
        │                                  │         │
        │   if anchor_to_markers:          │         │
        │     anchor = lerp(disc markers,  │         │
        │                    frame_time)   │         │
        │     gate to anchor_radius_px,    │         │
        │     strict (no fallback)         │         │
        │   else:                          │         │
        │     gate to last position w/     │         │
        │     search_radius_px, soft       │         │
        │                                  │         │
        └──────────────┬───────────────────┘         │
                       │                              │
       ┌───────────────┴──────────────┐              │
       │ candidate?                   │              │
       ▼                              ▼              │
  Kalman.correct(z)        anchor_fallback?          │
       │                   ▼               ▼          │
       │            anchor as point     Kalman.predict()
       │                   │               │          │
       │                   │               │ (≤ max_missed_frames)
       └─►   per-frame PathPoint  ◄────────┴──────────┘
                       │
                       ▼
              CSV  +  overlay video
```

- **Kalman state**: `[x, y, vx, vy]`, constant-velocity, dt = 1 frame.
  Smooths jitter and bridges short detection drop-outs.
- **Marker anchor**: when `anchor_to_markers: 1` and `markers_path` is
  set, each frame's expected disc position is computed by linear
  interpolation along the disc-marker timeline. The anchor restricts
  detection acceptance to a tight gate, and (if `anchor_fallback: 1`)
  is used as the position itself when nothing is found in the gate —
  guaranteeing a complete trajectory.

---

## Build

`vcpkg.json` declares OpenCV with features `["ffmpeg", "dnn"]` —
required for the YOLO backend. After modifying `vcpkg.json`, re-run
the appropriate vcpkg install for your triplet (this rebuilds OpenCV
the first time):

```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;" + $env:PATH
.\vcpkg\vcpkg.exe install --triplet x64-mingw-dynamic --host-triplet x64-mingw-dynamic
```

> `core` is **not** a feature — it's the base package and is always
> installed. The `features` array lists only optional add-ons.

### Verified MinGW build (fast — reuses the existing vcpkg install)

This is the pattern that works on the user's machine without forcing a
fresh OpenCV rebuild into a new build dir. Run from the project root:

```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;" + $env:PATH

cmake -S disc_tracker -B build_mingw `
  -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic `
  -DVCPKG_INSTALLED_DIR="$PWD/vcpkg_installed" `
  -DVCPKG_MANIFEST_INSTALL=OFF `
  -DVCPKG_MANIFEST_MODE=OFF

cmake --build build_mingw --target disc_tracker -j 4
# binary lands at: build_mingw/disc_tracker.exe
```

The `VCPKG_MANIFEST_INSTALL=OFF` + `VCPKG_INSTALLED_DIR` combo is the
difference between a 2-second configure and a multi-hour rebuild.

CMake configure should print this, confirming the YOLO backend is
linked in:

```
-- disc_tracker: opencv_dnn detected — YOLO backend ENABLED
```

### Building the whole repo (parent CMakeLists)

The target is also wired into the top-level `CMakeLists.txt` under the
`BUILD_DISC_TRACKER` option (ON by default), so building the parent
project builds `disc_tracker` too. Use this if you're building the Qt
app at the same time. Note that the parent's CMake invocation triggers
a full vcpkg manifest install for whichever triplet your generator
implies.

---

## Run

```powershell
$env:PATH = "D:\SANDBOX\videopp\vcpkg_installed\x64-mingw-dynamic\bin;" `
            + "C:\Qt\Tools\mingw1310_64\bin;" + $env:PATH

.\build_mingw\disc_tracker.exe --config disc_tracker\configs\test_hole14_anchored.yaml
```

The first PATH entry is needed at runtime so the OpenCV DLLs (and
FFmpeg) resolve. Alternatively copy them next to the .exe.

### Output filenames

**Outputs carry a `_<YYYYMMDD>_<HHMMSS>` suffix** (local time at launch),
inserted before the extension. All outputs from one run share the same
timestamp so a CSV is easy to group with its overlay. The
`csv_path` / `overlay_video_path` in the config are templates — the
binary stamps them at runtime. So `out/hole14_anchored_path.csv` in
the config becomes `out/hole14_anchored_path_20260503_152200.csv` on
disk.

### Hole-14 test result (marker-anchored Hough)

```
Processed 238 frames (detections=84, kalman_bridged=0, anchor=154, lost=0)
  CSV:     disc_tracker/out/hole14_anchored_path_20260503_152200.csv
  Overlay: disc_tracker/out/hole14_anchored_overlay_20260503_152200.mp4

Marker residuals (px error vs. ground-truth):
  -> matched 4/8 markers, mean=23.35 px, max=39.88 px
```

35% of frames have a real Hough detection inside the marker gate; 65%
fall back to the anchor itself. Trajectory is fully populated (no
`lost` frames). Mean pixel error against the 4 ground-truth markers
that fall within the 2-frame matching window is ~23 px — about 2% of
frame width.

---

## CSV schema

```
frame_idx,time_ms,x,y,w,h,cx,cy,confidence,class_id,source
```

- `cx, cy` — smoothed center in original-frame pixel coordinates.
  This is the trajectory to plot.
- `x, y, w, h` — raw detection bbox (zero-sized for `kalman_predict` /
  `anchor` rows).
- `class_id` — detector class index (YOLO only); `-1` for Hough/anchor.
- `source` is one of:
  - `detector` — real detection accepted by the gate
  - `kalman_predict` — short detection gap bridged by Kalman
  - `anchor` — no detection in the marker gate; anchor used directly
    (only when `anchor_to_markers` + `anchor_fallback` are both on)
  - `lost` — no position estimate available

When markers are loaded, the binary also prints per-marker residuals:
the pixel distance between each ground-truth marker and the closest
non-anchor tracker frame (within `marker_tolerance_frames` of the
marker time). Anchor rows are excluded from this so the residuals
report honest detector quality.

---

## Config schema

See [`configs/test_hole14_anchored.yaml`](configs/test_hole14_anchored.yaml)
for the full marker-anchored Hough setup, and
[`configs/example.yaml`](configs/example.yaml) for the YOLO setup.

The parser is `cv::FileStorage`, so both YAML (`*.yaml`) and JSON
(`*.json`) are accepted in OpenCV's standard dialect.

| Field | Default | Meaning |
| --- | --- | --- |
| `video_path` | — (required) | Source video |
| `start_sec` / `end_sec` | `0` / `-1` | Time window; `-1` = end of stream |
| `backend` | `yolo` | `yolo` or `hough` |
| `model_path` | — (required for yolo) | YOLOv8/11 ONNX |
| `input_size` | `640` | YOLO input letterbox size |
| `class_ids` | `[29, 32]` | YOLO class filter (empty = all) |
| `conf_threshold` | `0.30` | YOLO confidence floor |
| `nms_threshold` | `0.45` | YOLO NMS IoU |
| `hough_*` | (see example) | Hough-circle params |
| `use_kalman` | `1` | Smooth + bridge gaps |
| `max_missed_frames` | `15` | Kalman bridge length |
| `search_radius_px` | `0` | Soft gate to last position; `0` = disabled |
| `markers_path` | — | YAML markers from the Qt app |
| `seed_from_markers` | `1` | Init Kalman from first disc marker |
| `clip_to_marker_window` | `1` | Auto start/end from marker times |
| `marker_window_pad_sec` | `0.3` | Pad before/after marker window |
| `marker_tolerance_frames` | `2` | Residual matching window |
| `anchor_to_markers` | `0` | Gate detector to marker-spline anchor |
| `anchor_radius_px` | `60` | Anchor gate radius |
| `anchor_fallback` | `1` | Emit anchor when no detection in gate |
| `csv_path` | — | Output CSV (empty = skip) |
| `overlay_video_path` | — | Output annotated video (empty = skip) |
| `draw_trajectory` | `1` | Draw the running polyline on overlay |

All fields except `video_path` are optional; `model_path` is required
when `backend: yolo`.

---

## Reusing the tracker as a library

`DiscTracker` is a plain C++ class. If you want to surface trajectories
inside the Qt app or behind an HTTP service, link the same `.cpp` files
into another target and call `tracker.run()` — it returns a `RunReport`
containing `std::vector<PathPoint>` plus marker residuals and source
counts.
