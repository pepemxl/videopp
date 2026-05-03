cmake_minimum_required(VERSION 3.16)

# When this directory is configured standalone (not via the parent's
# add_subdirectory), declare a project so it can be built on its own.
if(NOT PROJECT_NAME OR NOT "${PROJECT_NAME}" STREQUAL "video_processor")
    project(disc_tracker LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Reuse the parent's OpenCV find when available; otherwise locate it ourselves.
if(NOT OpenCV_FOUND)
    find_package(OpenCV REQUIRED)
endif()

set(_DT_SOURCES
    main.cpp
    Config.h
    Config.cpp
    Detection.h
    DiscTracker.h
    DiscTracker.cpp
    HoughDetector.h
    HoughDetector.cpp
    KalmanTracker.h
    KalmanTracker.cpp
    Markers.h
    Markers.cpp
)

# Auto-detect whether the linked OpenCV exposes the dnn module. This is
# the gating switch for the YOLO backend; without it we still build with
# only the Hough fallback.
set(_HAVE_DNN FALSE)
foreach(_lib IN LISTS OpenCV_LIBS)
    if("${_lib}" MATCHES "opencv_dnn")
        set(_HAVE_DNN TRUE)
        break()
    endif()
endforeach()
if(NOT _HAVE_DNN AND TARGET opencv_dnn)
    set(_HAVE_DNN TRUE)
endif()

if(_HAVE_DNN)
    list(APPEND _DT_SOURCES YoloDetector.h YoloDetector.cpp)
    message(STATUS "disc_tracker: opencv_dnn detected — YOLO backend ENABLED")
else()
    message(STATUS "disc_tracker: opencv_dnn NOT detected — only Hough fallback available. "
                   "To enable YOLO, add the 'dnn' feature to vcpkg.json (or build OpenCV with "
                   "the dnn module) and reconfigure.")
endif()

add_executable(disc_tracker ${_DT_SOURCES})

target_include_directories(disc_tracker PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(disc_tracker PRIVATE ${OpenCV_LIBS})

if(_HAVE_DNN)
    target_compile_definitions(disc_tracker PRIVATE HAVE_OPENCV_DNN=1)
endif()

if(MSVC)
    target_compile_options(disc_tracker PRIVATE /W4 /permissive-)
else()
    target_compile_options(disc_tracker PRIVATE -Wall -Wextra -Wpedantic)
endif()
