# Video Processor

A C++ application built with Qt (Qt6 preferred, Qt5 supported) and OpenCV that processes video from a file or webcam by converting it to grayscale and applying a Gaussian blur in a separate worker thread.

## Status

Verified working on Windows with the following toolchain:

| Component | Version / Path |
| --- | --- |
| Qt | `C:\Qt\6.11.0\mingw_64` |
| MinGW | `C:\Qt\Tools\mingw1310_64` (GCC 13.1.0) |
| OpenCV | 4.12.0 via vcpkg, triplet `x64-mingw-dynamic`, features `[core, ffmpeg]` |
| Build outputs | `build_qmake/release/video_processor.exe` (qmake) |

## Prerequisites

- **C++17** compatible compiler (MSVC, MinGW, GCC, or Clang)
- **Qt 6** (with the `Widgets` component) — Qt 5.15 also works
- **OpenCV 4**
- **CMake** 3.16+ (recommended) or **qmake**

---

## Building on Linux

Install the dependencies via your distro's package manager.

### Debian / Ubuntu

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config \
    qt6-base-dev qt6-base-dev-tools \
    libopencv-dev
```

> If your distro only ships Qt 5, replace the Qt packages with `qtbase5-dev qttools5-dev`.

### Fedora

```bash
sudo dnf install -y gcc-c++ cmake pkgconf-pkg-config \
    qt6-qtbase-devel opencv-devel
```

### Arch

```bash
sudo pacman -S --needed base-devel cmake qt6-base opencv
```

### Build & run

```bash
./build.sh                # configures + builds in ./build/
./build/bin/video_processor
```

---

## Building on macOS

```bash
brew install cmake qt opencv
./build.sh -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
./build/bin/video_processor.app/Contents/MacOS/video_processor
```

---

## Building on Windows

OpenCV is a third-party dependency. Pick **one** of the methods below.

### Method A — vcpkg (recommended)

```powershell
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

Then from this project's directory:

```powershell
set CMAKE_PREFIX_PATH=C:\Qt\6.7.0\msvc2019_64
build.bat -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

The provided `vcpkg.json` manifest declares the OpenCV dependency, so `vcpkg` will install it automatically the first time you configure.

### Method B — Manual OpenCV download

1. Download a Windows release from the [OpenCV Releases Page](https://opencv.org/releases/) and extract it to e.g. `C:\opencv`.
2. Tell CMake where Qt and OpenCV live, then build:

   ```powershell
   set CMAKE_PREFIX_PATH=C:\Qt\6.7.0\msvc2019_64
   set OpenCV_DIR=C:\opencv\build
   build.bat
   ```

3. **Runtime DLLs:** the build runs `windeployqt` automatically to copy the Qt DLLs next to the executable. Make sure the OpenCV DLL (e.g. `opencv_world490.dll` from `C:\opencv\build\x64\vc16\bin`) is either on your `PATH` or copied next to `video_processor.exe`.

### Building from Qt Creator (Windows, MinGW + vcpkg)

This is the verified flow for Qt Creator with the bundled MinGW kit.

**One-time vcpkg setup** (only needed if `vcpkg_installed/x64-mingw-dynamic/` does not yet exist):

1. Open a terminal where MinGW is on the `PATH` so vcpkg picks it up as the host compiler:

   ```powershell
   $env:PATH = "C:\Qt\Tools\mingw1310_64\bin;" + $env:PATH
   ```

2. From the project root, install OpenCV with the MinGW triplet (compiles from source — expect ~1.5–2 hours, dominated by ffmpeg):

   ```powershell
   .\vcpkg\vcpkg.exe install --triplet x64-mingw-dynamic --host-triplet x64-mingw-dynamic
   ```

   Output goes to `vcpkg_installed/x64-mingw-dynamic/`. The `.pro` file already points there.

**Open and build in Qt Creator:**

1. **File → Open File or Project…** → select `video_processor.pro`.
2. Pick the **Desktop Qt 6.x.x MinGW 64-bit** kit when prompted (any matching MinGW kit works; do **not** pick an MSVC kit — the libs were built with MinGW and linking across ABIs will fail).
3. Hit **Build** (Ctrl+B). The output binary lands in `build-video_processor-Desktop_Qt_…-Release/release/video_processor.exe`.

**Run from Qt Creator:**

The executable depends on Qt, OpenCV, and MinGW runtime DLLs. Either:

- **Easiest** — In **Projects → Run → Run Environment**, prepend these to `PATH`:

  ```
  D:\SANDBOX\videopp\vcpkg_installed\x64-mingw-dynamic\bin
  C:\Qt\6.11.0\mingw_64\bin
  C:\Qt\Tools\mingw1310_64\bin
  ```

  Qt Creator typically adds the Qt and MinGW paths automatically; only the vcpkg bin entry usually needs to be added.

- **Or** copy the OpenCV DLLs (`vcpkg_installed/x64-mingw-dynamic/bin/*.dll`, ~18 files) next to the .exe and run `windeployqt.exe video_processor.exe` from a Qt-enabled shell to deploy Qt DLLs.

**Linux / macOS:** the `.pro` file uses `pkg-config opencv4` automatically — just open it in Qt Creator and build, no extra setup.

---

## Project layout

| File | Purpose |
| --- | --- |
| `CMakeLists.txt` | Cross-platform CMake build (Qt6/Qt5, OpenCV, Windows windeployqt) |
| `video_processor.pro` | qmake alternative |
| `vcpkg.json` | vcpkg manifest used by Method A on Windows |
| `build.sh` / `build.bat` | One-shot build helpers |
| `main.cpp` | Application entry point |
| `MainWindow.{h,cpp}` | UI: video display + Start/Stop buttons |
| `VideoProcessor.{h,cpp}` | `QThread` worker that grabs and processes frames |
| `disc_tracker/` | Standalone (no-Qt) console binary that consumes a video + marker file, tracks the disc, and writes a CSV trajectory + overlay video. See [`disc_tracker/README.md`](disc_tracker/README.md). |

---

## Disc-trajectory subservice

`disc_tracker/` is a sibling target — a small console binary that reads
a config file pointing at a video (and optionally a marker file saved
by this Qt app), runs a per-frame detector (Ultralytics YOLOv8/11 ONNX
or classical Hough), gates and Kalman-smooths the result, and writes
the trajectory.

The marker-anchored Hough mode is the working out-of-the-box path on
disc-golf footage; the YOLO mode is the recommended endpoint once you
have a fine-tuned model. Full docs and the verified MinGW build
invocation are in [`disc_tracker/README.md`](disc_tracker/README.md).
