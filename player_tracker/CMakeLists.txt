cmake_minimum_required(VERSION 3.16)

if(NOT PROJECT_NAME OR NOT "${PROJECT_NAME}" STREQUAL "video_processor")
    project(player_tracker LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT OpenCV_FOUND)
    find_package(OpenCV REQUIRED)
endif()

# The pose backend requires opencv_dnn. Unlike disc_tracker, there is no
# classical fallback for human pose — fail fast if dnn is unavailable.
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
if(NOT _HAVE_DNN)
    message(FATAL_ERROR
        "player_tracker: opencv_dnn is not available in this OpenCV install. "
        "Add the 'dnn' feature in vcpkg.json (already done by disc_tracker's "
        "setup) and re-run vcpkg install for your triplet.")
endif()

set(_PT_SOURCES
    main.cpp
    Config.h
    Config.cpp
    Markers.h
    Markers.cpp
    Skeleton.h
    KalmanTracker.h
    KalmanTracker.cpp
    PoseDetector.h
    PoseDetector.cpp
    PlayerTracker.h
    PlayerTracker.cpp
)

add_executable(player_tracker ${_PT_SOURCES})

target_include_directories(player_tracker PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(player_tracker PRIVATE ${OpenCV_LIBS})

if(MSVC)
    target_compile_options(player_tracker PRIVATE /W4 /permissive-)
else()
    target_compile_options(player_tracker PRIVATE -Wall -Wextra -Wpedantic)
endif()
