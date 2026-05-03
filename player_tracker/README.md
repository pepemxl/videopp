# player_tracker — standalone player-skeleton subservice

A console binary that tracks the player's body through the wind-up and
throw of a disc-golf shot. Reads a video plus a marker file from the Qt
app, runs YOLOv8/11-pose per frame, locks onto the marked player, and
writes a per-frame skeleton CSV (plus an optional overlay video).

The binary has **no Qt dependency** — it links only OpenCV and is built
as a sibling target alongside `disc_tracker/`.

---

## Recommended ML model: YOLOv8-pose

| Property | Value |
| --- | --- |
| Architecture | Ultralytics YOLOv8/11-pose (single-stage detector + keypoints) |
| Size | ~13 MB ONNX (`yolov8n-pose`); ~25 MB (`yolov8s-pose`) |
| CPU speed (640 input) | 20 – 40 FPS on a modern desktop CPU |
| Runtime dependency | **OpenCV DNN only** — no PyTorch / TensorFlow needed |
| Output | 17 COCO keypoints per person + bbox + visibility |
| Fine-tuning | Same Ultralytics flow as the disc detector |

The 17 COCO keypoints map directly onto our priority joints:

| Priority joint | KP idx |
| --- | ---: |
| left_shoulder / right_shoulder | 5 / 6 |
| left_elbow / right_elbow       | 7 / 8 |
| left_knee / right_knee         | 13 / 14 |

Other joints (nose, eyes, ears, wrists, hips, ankles) are tracked but
de-prioritized — they're rendered smaller in the overlay and not
labeled.

### Get the model

```bash
pip install ultralytics
yolo export model=yolov8n-pose.pt format=onnx imgsz=640 dynamic=False
# produces yolov8n-pose.onnx
```

The project convention is to keep all Ultralytics weights and ONNX
exports under `LOCAL_DATA/models/` (gitignored, alongside the source
clips):

```
LOCAL_DATA/
└── models/
    ├── yolov8n-pose.pt
    └── yolov8n-pose.onnx
```

Point `model_path` at the file in there. For higher accuracy at the
cost of ~2× runtime, use `yolov8s-pose.pt` instead.

Unlike disc detection, **the pretrained pose model works out of the
box** — the COCO pose dataset is full of human-like postures, and a
disc-golf player isn't a domain shift. Fine-tuning is only needed if
you want sport-specific keypoints (e.g., disc grip points).

---

## Pipeline

```
                     ┌─────────────────────────────────────┐
markers (Qt) ──►   first player marker  + first disc marker│
                     │                                      │
                     │  derive [start, end] window:         │
                     │  start = player.time - pre_pad       │
                     │  end   = disc.time   (= release)     │
                     │  clamp duration into [min, max]      │
                     └──────────────────┬──────────────────┘
                                        │
video frame  ──►   YOLOv8-pose ─►  candidate persons + 17 KPs each
                                        │
                     first frame? ─yes─► gate by distance to player marker
                          │ no
                          ▼
                       max-IoU vs. previous-frame bbox
                                        │
                          chosen detection
                                        │
                     ┌─────────────────────────────┐
                     │ per-keypoint Kalman smooth  │   (optional)
                     └─────────────────────────────┘
                                        │
                              FrameRecord (bbox + 17 keypoints)
                                        │
                                        ▼
                              CSV  +  skeleton overlay
```

- **Time window** is the wind-up + throw, capped at 4–15 s by default.
  `start = max(0, player.time − pre_pad)`. `end = disc.time` (the first
  disc marker is the release moment). If the marker-derived duration is
  shorter than `min_duration_sec`, the start is padded backwards (then
  the end forwards if the start hits 0). If longer than
  `max_duration_sec`, the end is trimmed.

- **Person identity**: on the first frame the marker's (x, y) is used
  to gate detections — the closest person inside `person_gate_radius_px`
  is locked. After that, IoU vs. the previously tracked bbox carries
  identity (with a center-distance fallback for low-IoU cases).

- **Keypoint smoothing**: optional 4-state Kalman per keypoint
  (`[x, y, vx, vy]`, dt = 1 frame). Visibility-gated — invisible
  keypoints don't update the filter, so the smoothed position holds
  through occlusions.

---

## Build

The target is wired into the top-level `CMakeLists.txt` under the
`BUILD_PLAYER_TRACKER` option (ON by default). It requires
`opencv_dnn`; vcpkg.json already declares the `dnn` feature, so any
recent OpenCV build for this project will have it.

### Verified MinGW build (fast — reuses the existing vcpkg install)

Same pattern as disc_tracker — point CMake at the source subdir and at
the existing vcpkg install, with `VCPKG_MANIFEST_INSTALL=OFF`:

```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;" + $env:PATH

cmake -S player_tracker -B build_mingw_player `
  -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic `
  -DVCPKG_INSTALLED_DIR="$PWD/vcpkg_installed" `
  -DVCPKG_MANIFEST_INSTALL=OFF `
  -DVCPKG_MANIFEST_MODE=OFF

cmake --build build_mingw_player --target player_tracker -j 4
# binary: build_mingw_player/player_tracker.exe
```

### Building the whole repo

`cmake --build build` from the parent project also produces
`player_tracker.exe`. Use this if you're rebuilding the Qt app at the
same time.

---

## Run

```powershell
$env:PATH = "D:\SANDBOX\videopp\vcpkg_installed\x64-mingw-dynamic\bin;" `
            + "C:\Qt\Tools\mingw1310_64\bin;" + $env:PATH

.\build_mingw_player\player_tracker.exe `
    --config player_tracker\configs\test_hole14_player.yaml
```

Output prints the resolved tracking window, frame-with-pose count, and
visibility percentages for each keypoint (priority joints first):

```
Tracking window: [0.00s, 4.00s]  (duration 4.00s)
Frames processed: 120  with pose: 118

Keypoint visibility (priority joints first):
  left_shoulder: 117/120 (97.5%)
  right_shoulder: 116/120 (96.7%)
  left_elbow: 110/120 (91.7%)
  right_elbow: 113/120 (94.2%)
  left_knee: 102/120 (85.0%)
  right_knee: 104/120 (86.7%)
...
```

---

## CSV schema

Wide format — one row per video frame, 8 metadata columns + 51 keypoint
columns (17 × `_x`, `_y`, `_v`):

```
frame_idx,time_ms,has_pose,bbox_x,bbox_y,bbox_w,bbox_h,person_conf,
nose_x,nose_y,nose_v,
left_eye_x,left_eye_y,left_eye_v,
... (all 17 COCO keypoints in standard order)
right_ankle_x,right_ankle_y,right_ankle_v
```

- `has_pose`: `1` if a tracked detection exists for this frame, `0` if
  the player was lost briefly.
- `<joint>_v`: visibility 0..1 from the network. Below
  `keypoint_vis_threshold` the pose overlay treats the joint as unseen
  (no edge or dot drawn) — but the raw value is still in the CSV.
- `<joint>_x`, `<joint>_y`: smoothed (or raw, if `use_keypoint_kalman:
  0`) position in original-frame pixel coordinates.

For analysis, the priority columns are:

```
left_shoulder_*, right_shoulder_*,
left_elbow_*,    right_elbow_*,
left_knee_*,     right_knee_*
```

---

## Config schema

See [`configs/example.yaml`](configs/example.yaml) for the documented
template and [`configs/test_hole14_player.yaml`](configs/test_hole14_player.yaml)
for a working hole-14 setup.

| Field | Default | Meaning |
| --- | --- | --- |
| `video_path` | — (required) | Source video |
| `markers_path` | — (required) | Marker YAML from the Qt app |
| `model_path` | — (required) | YOLOv8/11-pose ONNX |
| `input_size` | `640` | Letterbox input size |
| `person_conf_threshold` | `0.30` | Min person score |
| `nms_threshold` | `0.45` | NMS IoU |
| `keypoint_vis_threshold` | `0.40` | Below this a joint is unseen |
| `min_duration_sec` | `4.0` | Lower bound on tracking-window length |
| `max_duration_sec` | `15.0` | Upper bound on tracking-window length |
| `pre_pad_sec` | `0.0` | Extra pre-roll before the player marker |
| `person_gate_radius_px` | `200` | First-frame distance gate vs. player marker |
| `use_iou_tracking` | `1` | Subsequent frames: lock by max-IoU |
| `use_keypoint_kalman` | `1` | Per-joint constant-velocity smoothing |
| `csv_path` | — | Output CSV (empty = skip) |
| `overlay_video_path` | — | Output annotated video (empty = skip) |
| `draw_all_keypoints` | `1` | Draw non-priority joints as small dots |
| `label_priority_joints` | `1` | Draw the joint name next to each priority dot |

---

## Reusing the tracker as a library

`PlayerTracker` is a plain C++ class (`player_tracker::PlayerTracker`).
Link the same `.cpp` files into another target and call `tracker.run()`
— it returns a `RunReport` containing `std::vector<FrameRecord>` plus
per-keypoint visibility counts and the resolved time window. The
`FrameRecord` carries a `std::array<Keypoint, 17>` you can index with
the names from `Skeleton.h` (`KP_LEFT_ELBOW`, etc.).
