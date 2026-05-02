QT += core gui widgets

CONFIG += c++17

TARGET = video_processor
TEMPLATE = app

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    VideoProcessor.cpp

HEADERS += \
    MainWindow.h \
    VideoProcessor.h

# --- OpenCV configuration ---
# Linux / macOS: rely on pkg-config (apt: libopencv-dev, brew: opencv).
unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += opencv4
}

# Windows: vcpkg-installed OpenCV (x64-windows triplet).
# vcpkg is expected to live inside the project directory.
win32 {
    # Manifest mode: vcpkg installs into <project>/vcpkg_installed/<triplet>/.
    VCPKG_DIR = $$PWD/vcpkg_installed/x64-mingw-dynamic

    INCLUDEPATH += "$$VCPKG_DIR/include/opencv4"

    # Match the modules actually built by vcpkg (opencv4[core,ffmpeg]).
    OPENCV_LIBS = -lopencv_core4 -lopencv_imgproc4 \
                  -lopencv_imgcodecs4 -lopencv_videoio4

    # vcpkg appends 'd' to debug lib names for both MSVC and MinGW triplets.
    CONFIG(debug, debug|release) {
        LIBS += -L"$$VCPKG_DIR/debug/lib"
        LIBS += $$replace(OPENCV_LIBS, "4", "4d")
    } else {
        LIBS += -L"$$VCPKG_DIR/lib"
        LIBS += $$OPENCV_LIBS
    }
}

DISTFILES +=

RESOURCES += \
    recursos.qrc
