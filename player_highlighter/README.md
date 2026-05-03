# player_highlighter — standalone player-highlighter subservice

Consumes a video plus the per-frame skeleton CSV emitted by
`player_tracker`, segments the player out of the background using
GrabCut seeded by the skeleton joints, and writes:

1. **Overlay video** at original size — the player highlighted in one of
   four configurable styles (outline / dim / silhouette / depth_overlay).
2. **Zoom video** (optional) — a smoothly-cropped, resized track that
   follows the player at higher effective resolution for analytics.

The binary has **no Qt dependency** — it links only OpenCV and is built
as a sibling target alongside `disc_tracker/` and `player_tracker/`.

---

## Pipeline

```
player_tracker.csv ──┐
                     │
video frame ──┬─► row N (bbox + 17 keypoints)
              │      │
              │      ▼
              │   Segmenter (GrabCut)
              │      │  rect = bbox + margin
              │      │  seeds: visible joints = GC_FGD,
              │      │         bbox interior  = GC_PR_FGD,
              │      │         outside        = GC_PR_BGD
              │      ▼
              │   binary mask (player silhouette)
              │      │
              ├──────┼─► (optional) DepthEstimator (MiDaS ONNX) ─► depth8u
              │      │                                              │
              ▼      ▼                                              │
         composeOverlay(frame, mask, depth8u, mode) ◄───────────────┘
                  │
                  ├─► overlay_video_path  (original size)
                  │
                  └─► ZoomTracker.update(bbox)
                          │
                          ▼
                      crop = expand(smoothed_bbox, zoom_factor)
                          │
                          ▼
                      resize → zoom_video_path
```

- **Segmenter (GrabCut + skeleton seeds)**: sets the bbox interior to
  `GC_PR_FGD` (probable foreground), drops `GC_FGD` disks at every
  visible joint, and runs GrabCut for `grabcut_iterations` iterations.
  Joint-seeded GrabCut converges ~2× faster and produces noticeably
  cleaner silhouettes than a rect-only run.

- **Highlight modes**:
  - `outline`: draw the segmentation contour over the original frame
  - `dim`: keep the player full-brightness, multiply everything else by
    `background_dim` (use 0.0 to fully blacken the background)
  - `silhouette`: color-tint the player region with a configurable
    BGR + alpha
  - `depth_overlay`: blend a colormap of the depth estimate inside the
    player region (requires `enable_depth: 1` and a MiDaS ONNX)

- **Zoom**: EMA-smoothed bbox center and size drive a crop rectangle
  expanded by `zoom_factor`. The crop respects the output aspect ratio,
  is clipped to the frame, and is then resized to
  `zoom_output_width × zoom_output_height`. Lower `zoom_smoothing_alpha`
  → steadier crop (more lag); higher → snappier (more jitter).

---

## Build

`vcpkg.json` already declares `opencv4[ffmpeg, dnn]`, which is what this
target needs. Builds with the same MinGW invocation as the other
subservices:

```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;" + $env:PATH

cmake -S player_highlighter -B build_mingw_highlighter `
  -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic `
  -DVCPKG_INSTALLED_DIR="$PWD/vcpkg_installed" `
  -DVCPKG_MANIFEST_INSTALL=OFF `
  -DVCPKG_MANIFEST_MODE=OFF

cmake --build build_mingw_highlighter --target player_highlighter -j 4
# binary: build_mingw_highlighter/player_highlighter.exe
```

The target is also wired into the parent project under the
`BUILD_PLAYER_HIGHLIGHTER` CMake option (ON by default).

---

## Run

Produce the player CSV first via `player_tracker`, then feed it in:

```powershell
$env:PATH = "D:\SANDBOX\videopp\vcpkg_installed\x64-mingw-dynamic\bin;" `
            + "C:\Qt\Tools\mingw1310_64\bin;" + $env:PATH

# 1. Skeleton + bbox CSV from player_tracker (already in player_tracker/out/).
.\build_mingw_player\player_tracker.exe `
    --config player_tracker\configs\test_hole14_player.yaml

# 2. Highlight + zoom on top of that CSV.
.\build_mingw_highlighter\player_highlighter.exe `
    --config player_highlighter\configs\test_hole14_highlighter.yaml
```

Expected output:

```
Frames processed: 115
  with pose:     115  (100.0%)
  segmented:     113  (98.3%)
  zoom frames:   115  (100.0%)
  Overlay: D:/SANDBOX/videopp/player_highlighter/out/hole14_highlight.mp4
  Zoom:    D:/SANDBOX/videopp/player_highlighter/out/hole14_zoom.mp4
```

---

## Optional: depth map via MiDaS

Depth is gated by `enable_depth: 1` (or `highlight_mode: "depth_overlay"`,
which implies it). The MiDaS project publishes pre-exported ONNX files
on the `v2_1` release page —
<https://github.com/isl-org/MiDaS/releases/tag/v2_1>:

| File | Size | Notes |
| --- | ---: | --- |
| `model-small.onnx`     | ~67 MB | MiDaS-small, 256² input. Recommended default. |
| `model-f6b98070.onnx`  | ~417 MB | Full MiDaS, 384² input. Slower, sharper. |

Download the small variant and rename it to whatever your config points
at (the example uses `midas_small.onnx`):

```powershell
curl -L `
    -o LOCAL_DATA\models\midas_small.onnx `
    https://github.com/isl-org/MiDaS/releases/download/v2_1/model-small.onnx
```

Or export from PyTorch yourself if you prefer fresher weights:

```python
import torch
midas = torch.hub.load("intel-isl/MiDaS", "MiDaS_small")
midas.eval()
dummy = torch.rand(1, 3, 256, 256)
torch.onnx.export(midas, dummy, "LOCAL_DATA/models/midas_small.onnx",
                  input_names=["input"], output_names=["depth"], opset_version=12)
```

Drop the `.onnx` into `LOCAL_DATA/models/` (project convention — see
[disc_tracker/README.md](../disc_tracker/README.md)) and set
`depth_model_path` accordingly.

The estimator normalizes via ImageNet mean/std, runs at
`depth_input_size × depth_input_size`, upsamples back to the frame
resolution, and emits a `CV_8UC1` depth image that's then run through
`cv::applyColorMap(depth_colormap)`. Useful colormap codes:

| Value | Constant | Looks like |
| ---: | --- | --- |
| 2  | `COLORMAP_JET`     | Blue → Yellow → Red |
| 13 | `COLORMAP_MAGMA`   | Black → Purple → White |
| 14 | `COLORMAP_INFERNO` | Black → Red → Yellow |
| 20 | `COLORMAP_TURBO`   | Modern, perceptually uniform |

---

## Config schema

See [`configs/example.yaml`](configs/example.yaml) for the documented
template and [`configs/test_hole14_highlighter.yaml`](configs/test_hole14_highlighter.yaml)
for the working hole-14 test setup.

| Field | Default | Meaning |
| --- | --- | --- |
| `video_path` | — (required) | Source video |
| `player_csv_path` | — (required) | CSV from `player_tracker` |
| `highlight_mode` | `dim` | `outline` / `dim` / `silhouette` / `depth_overlay` |
| `silhouette_color` | `[0, 200, 255]` | BGR for silhouette mode |
| `silhouette_alpha` | `0.45` | Blend factor for silhouette mode |
| `background_dim` | `0.35` | Multiplier for non-player pixels in `dim` mode |
| `draw_skeleton` | `1` | Draw the player_tracker skeleton on top |
| `draw_bbox` | `1` | Draw the green bbox on top |
| `contour_thickness` | `2` | Outline mode line thickness |
| `grabcut_iterations` | `3` | GrabCut iters (more = sharper, slower) |
| `grabcut_margin` | `20` | Px around bbox passed to GrabCut |
| `joint_seed_radius` | `4` | Definite-FG disk radius at each visible joint |
| `keypoint_vis_threshold` | `0.40` | Joint visibility floor |
| `enable_depth` | `0` | Run MiDaS for depth on each frame |
| `depth_model_path` | — | MiDaS ONNX (required if depth enabled) |
| `depth_input_size` | `256` | MiDaS input resolution |
| `depth_mean` / `depth_std` | ImageNet | Normalization for MiDaS |
| `depth_colormap` | `2` | `cv::COLORMAP_*` enum |
| `depth_alpha` | `0.55` | Blend factor for depth_overlay mode |
| `enable_zoom` | `1` | Render the zoom track |
| `zoom_factor` | `1.6` | Crop = bbox × this, centered |
| `zoom_output_width` | `960` | Output width of the zoom video |
| `zoom_output_height` | `540` | Output height of the zoom video |
| `zoom_smoothing_alpha` | `0.25` | EMA on bbox; lower = steadier |
| `overlay_video_path` | — | Output highlighted video (empty = skip) |
| `zoom_video_path` | — | Output zoomed video (empty = skip) |

---

## Performance notes

GrabCut is the dominant cost — typical numbers on a 720p clip with the
default 3 iterations and a 20 px margin:

| step | per-frame ms | for 4 s @ 30 fps |
|---|---:|---:|
| CSV row lookup | ~0.01 | negligible |
| GrabCut on bbox crop | 30 – 60 | ~5 s total |
| MiDaS-small (256²) | 25 – 40 | ~4 s total |
| Composite + write | 5 – 10 | ~1 s total |

If you need faster runs, drop `grabcut_iterations` to 1, or compute
GrabCut on a downsampled bbox crop and upsample the resulting mask.

---

## Reusing as a library

`Highlighter` is a plain C++ class
(`player_highlighter::Highlighter`). Link the same `.cpp` files into
another target and call `highlighter.run()` to get a `RunReport` with
per-stage frame counts.
