# Video Processor

A C++ application built with Qt6 and OpenCV that processes video from a file or webcam by converting it to grayscale and applying a Gaussian blur in a separate processing thread.

## Prerequisites

- **C++17** compatible compiler (e.g., MSVC, MinGW, or GCC)
- **Qt 6** (with the `Widgets` component)
- **OpenCV 4** (see setup instructions below)
- **CMake** 3.16+ or **qmake**

## Setting up OpenCV on Windows

OpenCV is a third-party dependency. Because Windows does not have a native C++ package manager, you must provide the OpenCV binaries to the compiler. You can choose one of the two methods below:

### Method A: Manual Download (Easiest for Beginners)

1. Go to the [OpenCV Releases Page](https://opencv.org/releases/) and download the **Windows** version (an `.exe` self-extracting archive).
2. Extract the contents to a simple path, for example, `C:\opencv`.
3. **For CMake:** Open `CMakeLists.txt` and add the following line *before* `find_package(OpenCV REQUIRED)`:
   ```cmake
   set(OpenCV_DIR "C:/opencv/build")
   ```
4. **For qmake:** Open `video_processor.pro` and edit the OpenCV paths at the bottom of the file to match your setup:
   ```pro
   INCLUDEPATH += "C:/opencv/build/include"
   LIBS += -L"C:/opencv/build/x64/vc16/lib" -lopencv_world490
   ```
5. **Runtime Requirement:** Ensure that the compiled `.exe` can find the OpenCV DLL file. You can either:
   - Add `C:\opencv\build\x64\vc16\bin` to your system's `PATH` environment variable.
   - Or copy `opencv_world490.dll` (or whatever version you downloaded) directly into the folder containing your compiled `.exe`.

### Method B: Using vcpkg (Professional Standard)

1. Install `vcpkg` and OpenCV by running in PowerShell:
   ```powershell
   git clone https://github.com/microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   .\vcpkg install opencv4:x64-windows
   ```
2. When configuring CMake (in Qt Creator or Visual Studio), pass the vcpkg toolchain file:
   `-DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake`

## Building the Project

### Using CMake (Recommended)
You can open the `CMakeLists.txt` file directly in **Qt Creator** or **Visual Studio**. Ensure you have configured the OpenCV path as mentioned above.

### Using qmake
You can double-click `video_processor.pro` to open the project in **Qt Creator**. Adjust the `INCLUDEPATH` and `LIBS` in the file to point to your local OpenCV build.
