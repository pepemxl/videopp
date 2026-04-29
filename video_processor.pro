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

# Windows: point at your local OpenCV install.
# Tip: define OPENCV_DIR as an environment variable to avoid editing this file.
win32 {
    OPENCV_DIR_ENV = $$(OPENCV_DIR)
    isEmpty(OPENCV_DIR_ENV) {
        OPENCV_DIR = "C:/opencv/build"
    } else {
        OPENCV_DIR = $$OPENCV_DIR_ENV
    }

    INCLUDEPATH += "$$OPENCV_DIR/include"

    *-msvc* {
        LIBS += -L"$$OPENCV_DIR/x64/vc16/lib"
    } else {
        LIBS += -L"$$OPENCV_DIR/x64/mingw/lib"
    }

    CONFIG(debug, debug|release) {
        LIBS += -lopencv_world490d
    } else {
        LIBS += -lopencv_world490
    }
}
