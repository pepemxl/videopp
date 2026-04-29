# Video Processor

A C++ application built with Qt (Qt6 preferred, Qt5 supported) and OpenCV that processes video from a file or webcam by converting it to grayscale and applying a Gaussian blur in a separate worker thread.

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

### Using qmake instead of CMake

Open `video_processor.pro` in **Qt Creator**. The file is already set up to use `pkg-config opencv4` on Linux/macOS. On Windows it reads the `OPENCV_DIR` environment variable (defaulting to `C:/opencv/build`); set it before launching Qt Creator, or edit the path directly in the `.pro` file.

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
