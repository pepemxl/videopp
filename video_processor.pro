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

# --- OpenCV Configuration ---
# Note: You will need to configure OpenCV paths for your local system here
# if you decide to use qmake instead of CMake.
#
# For example, on Windows (adjust paths as necessary):
# INCLUDEPATH += "C:/opencv/build/include"
# LIBS += -L"C:/opencv/build/x64/vc15/lib" -lopencv_world450
#
# On Linux/macOS using pkg-config:
# unix: CONFIG += link_pkgconfig
# unix: PKGCONFIG += opencv4
